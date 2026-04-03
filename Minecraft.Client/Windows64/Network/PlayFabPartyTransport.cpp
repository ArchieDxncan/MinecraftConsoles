#include "stdafx.h"

#ifdef _WINDOWS64

#include "PlayFabPartyTransport.h"
#include "PlayFabLobbyWin64.h"
#include "../Leaderboards/PlayFabConfig.h"
#include "Common/Consoles_App.h"

#include <string>
#include <vector>
#include <cstring>

#if MINECRAFT_PLAYFAB_PARTY_SDK_AVAILABLE
#include <Party_c.h>
#include <objbase.h>
#pragma comment(lib, "ole32.lib")
#include "WinsockNetLayer.h"
#include "../../Common/Network/PlatformNetworkManagerStub.h"
#include "../../Minecraft.h"

extern QNET_STATE _iQNetStubState;
extern CPlatformNetworkManagerStub *g_pPlatformNetworkManager;
#endif

#if MINECRAFT_PLAYFAB_PARTY_SDK_AVAILABLE

namespace
{
CRITICAL_SECTION g_partyCs;
bool g_partyCsInit = false;

PARTY_HANDLE g_partyHandle = nullptr;

PARTY_LOCAL_USER_HANDLE g_hostLocalUser = nullptr;
PARTY_LOCAL_USER_HANDLE g_joinLocalUser = nullptr;
PARTY_NETWORK_HANDLE g_hostNetwork = nullptr;
PARTY_NETWORK_HANDLE g_joinNetwork = nullptr;

PARTY_ENDPOINT_HANDLE g_hostGameEndpoint = nullptr;
PARTY_ENDPOINT_HANDLE g_joinGameEndpoint = nullptr;
PARTY_ENDPOINT_HANDLE g_joinHostEndpoint = nullptr;
PARTY_ENDPOINT_HANDLE g_partyRemoteEndpointBySmallId[256] = {};

volatile bool g_partyJoinThreadOwnsPump = false;
volatile bool g_partyJoinWaitingForAssign = false;
volatile bool g_partyJoinAssignSuccess = false;
volatile bool g_partyJoinRejectReceived = false;
BYTE g_partyJoinAssignByte = 0;
bool g_partyClientGameTransportActive = false;

bool g_hostSessionActive = false;

static void GameLayerStateChange(const PARTY_STATE_CHANGE *sc);

struct HostCreateState
{
	bool createCompleted = false;
	bool createSucceeded = false;
	uint32_t createResult = 0;
	PARTY_NETWORK_DESCRIPTOR networkDescriptor = {};
	char invitationId[PARTY_MAX_INVITATION_IDENTIFIER_STRING_LENGTH + 1] = {};
};

struct JoinHandshakeState
{
	bool connectCompleted = false;
	bool connectSucceeded = false;
	bool authCompleted = false;
	bool authSucceeded = false;
	// CONNECT_TO_NETWORK_COMPLETED: set g_hostNetwork (host after create) vs g_joinNetwork (client join).
	bool hostConnect = false;
	// Only accept AUTHENTICATE_LOCAL_USER_COMPLETED for this network+user (ignore unrelated/stale completions).
	bool filterAuthByHandles = false;
	PARTY_NETWORK_HANDLE expectedAuthNetwork = nullptr;
	PARTY_LOCAL_USER_HANDLE expectedAuthLocalUser = nullptr;
};

void EnsurePartyCs()
{
	if (!g_partyCsInit)
	{
		InitializeCriticalSection(&g_partyCs);
		g_partyCsInit = true;
	}
}

const char *PartyStateChangeResultName(PARTY_STATE_CHANGE_RESULT r)
{
	switch (r)
	{
	case PARTY_STATE_CHANGE_RESULT_SUCCEEDED:
		return "SUCCEEDED";
	case PARTY_STATE_CHANGE_RESULT_UNKNOWN_ERROR:
		return "UNKNOWN_ERROR";
	case PARTY_STATE_CHANGE_RESULT_CANCELED_BY_TITLE:
		return "CANCELED_BY_TITLE";
	case PARTY_STATE_CHANGE_RESULT_INTERNET_CONNECTIVITY_ERROR:
		return "INTERNET_CONNECTIVITY_ERROR";
	case PARTY_STATE_CHANGE_RESULT_PARTY_SERVICE_ERROR:
		return "PARTY_SERVICE_ERROR";
	case PARTY_STATE_CHANGE_RESULT_NO_SERVERS_AVAILABLE:
		return "NO_SERVERS_AVAILABLE";
	case PARTY_STATE_CHANGE_RESULT_USER_NOT_AUTHORIZED:
		return "USER_NOT_AUTHORIZED";
	case PARTY_STATE_CHANGE_RESULT_USER_CREATE_NETWORK_THROTTLED:
		return "USER_CREATE_NETWORK_THROTTLED";
	case PARTY_STATE_CHANGE_RESULT_TITLE_NOT_ENABLED_FOR_PARTY:
		return "TITLE_NOT_ENABLED_FOR_PARTY";
	case PARTY_STATE_CHANGE_RESULT_NETWORK_LIMIT_REACHED:
		return "NETWORK_LIMIT_REACHED";
	case PARTY_STATE_CHANGE_RESULT_NETWORK_NO_LONGER_EXISTS:
		return "NETWORK_NO_LONGER_EXISTS";
	case PARTY_STATE_CHANGE_RESULT_NETWORK_NOT_JOINABLE:
		return "NETWORK_NOT_JOINABLE";
	case PARTY_STATE_CHANGE_RESULT_VERSION_MISMATCH:
		return "VERSION_MISMATCH";
	case PARTY_STATE_CHANGE_RESULT_LEAVE_NETWORK_CALLED:
		return "LEAVE_NETWORK_CALLED";
	case PARTY_STATE_CHANGE_RESULT_FAILED_TO_BIND_TO_LOCAL_UDP_SOCKET:
		return "FAILED_TO_BIND_TO_LOCAL_UDP_SOCKET";
	default:
		return "?";
	}
}

void LogPartyErrorDetail(const char *context, PartyError err)
{
	PartyString msg = nullptr;
	if (PartyGetErrorMessage(err, &msg) == c_partyErrorSuccess && msg != nullptr)
		app.DebugPrintf("[Party] %s: %s\n", context, msg);
	else
		app.DebugPrintf("[Party] %s (no message) err=0x%08X\n", context, (unsigned int)err);
}

static PARTY_SEND_MESSAGE_OPTIONS PartyTcpStyleSendOptions()
{
	return static_cast<PARTY_SEND_MESSAGE_OPTIONS>(
		static_cast<uint32_t>(PARTY_SEND_MESSAGE_OPTIONS_GUARANTEED_DELIVERY) |
		static_cast<uint32_t>(PARTY_SEND_MESSAGE_OPTIONS_SEQUENTIAL_DELIVERY));
}

bool RefreshLocalUserEntityToken(PARTY_LOCAL_USER_HANDLE user)
{
	if (user == nullptr)
		return false;
	// Cached g_entityToken expires; must call PlayFab GetEntityToken again (not only read memory).
	if (!PlayFabLobbyWin64::RefreshPlayFabEntityToken())
	{
		app.DebugPrintf("[Party] RefreshLocalUserEntityToken: GetEntityToken API failed\n");
		return false;
	}
	std::string eid, etype, etok;
	if (!PlayFabLobbyWin64::GetPlayFabEntityCredentials(&eid, &etype, &etok) || etok.empty())
	{
		app.DebugPrintf("[Party] RefreshLocalUserEntityToken: missing credentials after refresh\n");
		return false;
	}
	PartyError e = PartyLocalUserUpdateEntityToken(user, etok.c_str());
	if (e != c_partyErrorSuccess)
	{
		app.DebugPrintf("[Party] PartyLocalUserUpdateEntityToken failed (err=0x%08X)\n", (unsigned int)e);
		LogPartyErrorDetail("PartyLocalUserUpdateEntityToken", e);
		return false;
	}
	app.DebugPrintf("[Party] Refreshed Party local user entity token from PlayFab before AuthenticateLocalUser\n");
	return true;
}

void ProcessHostCreateStateChange(const PARTY_STATE_CHANGE *sc, HostCreateState *out)
{
	if (sc == nullptr || out == nullptr)
		return;
	switch (sc->stateChangeType)
	{
	case PARTY_STATE_CHANGE_TYPE_CREATE_NEW_NETWORK_COMPLETED:
	{
		const auto *p = reinterpret_cast<const PARTY_CREATE_NEW_NETWORK_COMPLETED_STATE_CHANGE *>(sc);
		out->createCompleted = true;
		out->createSucceeded = (p->result == PARTY_STATE_CHANGE_RESULT_SUCCEEDED);
		out->createResult = static_cast<uint32_t>(p->result);
		app.DebugPrintf("[Party] CREATE_NEW_NETWORK_COMPLETED result=%u\n", out->createResult);
		if (out->createSucceeded)
		{
			out->networkDescriptor = p->networkDescriptor;
			if (p->appliedInitialInvitationIdentifier != nullptr)
			{
				strncpy_s(out->invitationId, _countof(out->invitationId), p->appliedInitialInvitationIdentifier,
					_TRUNCATE);
				app.DebugPrintf("[Party] CREATE_NEW_NETWORK_COMPLETED invite='%s'\n", out->invitationId);
			}
			else
			{
				out->invitationId[0] = 0;
				app.DebugPrintf("[Party] CREATE_NEW_NETWORK_COMPLETED invite=<null>\n");
			}
		}
		else
		{
			app.DebugPrintf("[Party] CREATE_NEW_NETWORK_COMPLETED failed errorDetail=0x%08X\n",
				(unsigned int)p->errorDetail);
		}
		break;
	}
	default:
		break;
	}
}

void ProcessJoinConnectStateChange(const PARTY_STATE_CHANGE *sc, JoinHandshakeState *out)
{
	if (sc == nullptr || out == nullptr)
		return;
	switch (sc->stateChangeType)
	{
	case PARTY_STATE_CHANGE_TYPE_CONNECT_TO_NETWORK_COMPLETED:
	{
		const auto *p = reinterpret_cast<const PARTY_CONNECT_TO_NETWORK_COMPLETED_STATE_CHANGE *>(sc);
		out->connectCompleted = true;
		out->connectSucceeded = (p->result == PARTY_STATE_CHANGE_RESULT_SUCCEEDED);
		app.DebugPrintf("[Party] CONNECT_TO_NETWORK_COMPLETED result=%u (%s) hostConnect=%d\n",
			(unsigned int)p->result, PartyStateChangeResultName(p->result), out->hostConnect ? 1 : 0);
		if (out->connectSucceeded)
		{
			if (out->hostConnect)
				g_hostNetwork = p->network;
			else
				g_joinNetwork = p->network;
		}
		break;
	}
	case PARTY_STATE_CHANGE_TYPE_AUTHENTICATE_LOCAL_USER_COMPLETED:
	{
		const auto *p = reinterpret_cast<const PARTY_AUTHENTICATE_LOCAL_USER_COMPLETED_STATE_CHANGE *>(sc);
		if (out->filterAuthByHandles)
		{
			if (p->network != out->expectedAuthNetwork || p->localUser != out->expectedAuthLocalUser)
			{
				app.DebugPrintf(
					"[Party] AUTHENTICATE_COMPLETED skipped (not our network/user) result=%u errDetail=0x%08X\n",
					(unsigned int)p->result, (unsigned int)p->errorDetail);
				break;
			}
		}
		out->authCompleted = true;
		out->authSucceeded = (p->result == PARTY_STATE_CHANGE_RESULT_SUCCEEDED);
		app.DebugPrintf("[Party] AUTHENTICATE_COMPLETED result=%u (%s) inviteOut='%s'\n",
			(unsigned int)p->result, PartyStateChangeResultName(p->result),
			p->invitationIdentifier != nullptr ? p->invitationIdentifier : "(null)");
		if (!out->authSucceeded)
			LogPartyErrorDetail("AuthenticateLocalUser errorDetail", p->errorDetail);
		break;
	}
	default:
		break;
	}
}

void PumpStateChanges(PARTY_HANDLE h, void (*onChange)(const PARTY_STATE_CHANGE *, void *), void *ctx)
{
	// Drive both Party internal task queues (see GDK Party_c.h PARTY_THREAD_ID).
	(void)PartyDoWork(h, PARTY_THREAD_ID_AUDIO);
	(void)PartyDoWork(h, PARTY_THREAD_ID_NETWORKING);
	uint32_t count = 0;
	const PARTY_STATE_CHANGE *const *changes = nullptr;
	PartyError e = PartyStartProcessingStateChanges(h, &count, &changes);
	if (e != c_partyErrorSuccess || count == 0)
		return;
	for (uint32_t i = 0; i < count; ++i)
	{
		if (onChange != nullptr)
			onChange(changes[i], ctx);
	}
	PartyFinishProcessingStateChanges(h, count, changes);
}

static void PumpGameLayerOnlyCb(const PARTY_STATE_CHANGE *sc, void *)
{
	GameLayerStateChange(sc);
}

struct HostCreatePumpCtx
{
	HostCreateState *hs = nullptr;
};

static void HostCreatePumpCb(const PARTY_STATE_CHANGE *sc, void *ctx)
{
	auto *p = static_cast<HostCreatePumpCtx *>(ctx);
	ProcessHostCreateStateChange(sc, p->hs);
	GameLayerStateChange(sc);
}

static void JoinHandshakePumpCb(const PARTY_STATE_CHANGE *sc, void *ctx)
{
	ProcessJoinConnectStateChange(sc, static_cast<JoinHandshakeState *>(ctx));
	GameLayerStateChange(sc);
}

static bool GameMessageTargetsLocal(const PARTY_ENDPOINT_MESSAGE_RECEIVED_STATE_CHANGE *p,
	PARTY_ENDPOINT_HANDLE localEp)
{
	if (localEp == nullptr || p->receiverEndpointCount == 0)
		return false;
	for (uint32_t i = 0; i < p->receiverEndpointCount; ++i)
	{
		if (p->receiverEndpoints[i] == localEp)
			return true;
	}
	return false;
}

static void PartySendTcpStyleReject(PARTY_ENDPOINT_HANDLE remote, DisconnectPacket::eDisconnectReason reason)
{
	if (g_hostGameEndpoint == nullptr || remote == nullptr)
		return;
	BYTE buf[6];
	buf[0] = WIN64_SMALLID_REJECT;
	buf[1] = (BYTE)255;
	const int r = (int)reason;
	buf[2] = (BYTE)((r >> 24) & 0xff);
	buf[3] = (BYTE)((r >> 16) & 0xff);
	buf[4] = (BYTE)((r >> 8) & 0xff);
	buf[5] = (BYTE)(r & 0xff);
	PARTY_DATA_BUFFER db = { buf, sizeof(buf) };
	(void)PartyEndpointSendMessage(g_hostGameEndpoint, 1u, &remote, PartyTcpStyleSendOptions(), nullptr, 1u,
		&db, nullptr);
}

static void HostPartyOnRemoteGameEndpoint(PARTY_ENDPOINT_HANDLE remoteEp)
{
	if (g_hostNetwork == nullptr || g_hostGameEndpoint == nullptr || remoteEp == nullptr)
		return;
	if (remoteEp == g_hostGameEndpoint)
		return;
	PartyBool isLocal = 0;
	if (PartyEndpointIsLocal(remoteEp, &isLocal) != c_partyErrorSuccess || isLocal)
		return;

	if (_iQNetStubState != QNET_STATE_GAME_PLAY)
	{
		app.DebugPrintf("[Party] Host: rejecting Party client (game not in play state)\n");
		PartySendTcpStyleReject(remoteEp, DisconnectPacket::eDisconnect_Quitting);
		return;
	}

	if (g_pPlatformNetworkManager != nullptr && !g_pPlatformNetworkManager->CanAcceptMoreConnections())
	{
		app.DebugPrintf("[Party] Host: rejecting Party client (server full)\n");
		PartySendTcpStyleReject(remoteEp, DisconnectPacket::eDisconnect_ServerFull);
		return;
	}

	BYTE sid = 0;
	if (!WinsockNetLayer::TryAllocateRemoteJoinSmallId(&sid))
	{
		app.DebugPrintf("[Party] Host: no free smallId for Party client\n");
		PartySendTcpStyleReject(remoteEp, DisconnectPacket::eDisconnect_ServerFull);
		return;
	}

	(void)PartyEndpointSetCustomContext(remoteEp, reinterpret_cast<void *>(static_cast<uintptr_t>(sid)));
	g_partyRemoteEndpointBySmallId[sid] = remoteEp;

	const BYTE assignBuf[1] = { sid };
	PARTY_DATA_BUFFER db = { assignBuf, sizeof(assignBuf) };
	const PartyError se = PartyEndpointSendMessage(g_hostGameEndpoint, 1u, &remoteEp, PartyTcpStyleSendOptions(),
		nullptr, 1u, &db, nullptr);
	if (se != c_partyErrorSuccess)
	{
		app.DebugPrintf("[Party] Host: PartyEndpointSendMessage(smallId assign) failed (err=0x%08X)\n",
			(unsigned int)se);
		g_partyRemoteEndpointBySmallId[sid] = nullptr;
		(void)PartyEndpointSetCustomContext(remoteEp, nullptr);
		WinsockNetLayer::PushFreeSmallId(sid);
		return;
	}

	WinsockNetLayer::CompletePartyRemotePlayerSetup(sid);
	app.DebugPrintf("[Party] Host: Party remote player accepted smallId=%u\n", (unsigned)sid);
}

void GameLayerStateChange(const PARTY_STATE_CHANGE *sc)
{
	if (sc == nullptr)
		return;
	switch (sc->stateChangeType)
	{
	case PARTY_STATE_CHANGE_TYPE_ENDPOINT_CREATED:
	{
		const auto *p = reinterpret_cast<const PARTY_ENDPOINT_CREATED_STATE_CHANGE *>(sc);
		if (p->network == g_hostNetwork && g_hostSessionActive && g_hostGameEndpoint != nullptr)
			HostPartyOnRemoteGameEndpoint(p->endpoint);
		break;
	}
	case PARTY_STATE_CHANGE_TYPE_ENDPOINT_DESTROYED:
	{
		const auto *p = reinterpret_cast<const PARTY_ENDPOINT_DESTROYED_STATE_CHANGE *>(sc);
		if (p->network != g_hostNetwork || p->endpoint == g_hostGameEndpoint)
			break;
		void *ctx = nullptr;
		if (PartyEndpointGetCustomContext(p->endpoint, &ctx) != c_partyErrorSuccess || ctx == nullptr)
			break;
		const BYTE sid = static_cast<BYTE>(reinterpret_cast<uintptr_t>(ctx));
		g_partyRemoteEndpointBySmallId[sid] = nullptr;
		WinsockNetLayer::NotifyPartyRemoteDisconnected(sid);
		break;
	}
	case PARTY_STATE_CHANGE_TYPE_ENDPOINT_MESSAGE_RECEIVED:
	{
		const auto *p = reinterpret_cast<const PARTY_ENDPOINT_MESSAGE_RECEIVED_STATE_CHANGE *>(sc);
		if (p->network == g_hostNetwork && g_hostGameEndpoint != nullptr &&
			GameMessageTargetsLocal(p, g_hostGameEndpoint))
		{
			void *ctx = nullptr;
			if (PartyEndpointGetCustomContext(p->senderEndpoint, &ctx) != c_partyErrorSuccess || ctx == nullptr)
				break;
			const BYTE clientSmallId = static_cast<BYTE>(reinterpret_cast<uintptr_t>(ctx));
			if (p->messageSize == 0 || p->messageBuffer == nullptr)
				break;
			std::vector<BYTE> copy(static_cast<size_t>(p->messageSize));
			memcpy(copy.data(), p->messageBuffer, copy.size());
			WinsockNetLayer::HandleDataReceived(clientSmallId, WinsockNetLayer::GetHostSmallId(), copy.data(),
				static_cast<unsigned int>(p->messageSize));
			break;
		}
		if (p->network == g_joinNetwork && g_joinGameEndpoint != nullptr &&
			GameMessageTargetsLocal(p, g_joinGameEndpoint))
		{
			if (g_partyJoinWaitingForAssign)
			{
				bool handledAssignMsg = false;
				if (p->messageSize == 1 && p->messageBuffer != nullptr)
				{
					const BYTE *b = static_cast<const BYTE *>(p->messageBuffer);
					if (b[0] == WIN64_SMALLID_REJECT)
						g_partyJoinRejectReceived = true;
					else
					{
						g_partyJoinAssignByte = b[0];
						g_partyJoinAssignSuccess = true;
					}
					handledAssignMsg = true;
				}
				else if (p->messageSize == 6 && p->messageBuffer != nullptr)
				{
					const BYTE *b = static_cast<const BYTE *>(p->messageBuffer);
					if (b[0] == WIN64_SMALLID_REJECT && b[1] == (BYTE)255)
					{
						const int r2 = ((b[2] & 0xff) << 24) | ((b[3] & 0xff) << 16) | ((b[4] & 0xff) << 8) |
							(b[5] & 0xff);
						WinsockNetLayer::SetPartyJoinRejectReason(
							static_cast<DisconnectPacket::eDisconnectReason>(r2));
						g_partyJoinRejectReceived = true;
						handledAssignMsg = true;
					}
				}
				if (handledAssignMsg)
					g_partyJoinWaitingForAssign = false;
				break;
			}
			if (p->messageSize == 0 || p->messageBuffer == nullptr)
				break;
			std::vector<BYTE> copy(static_cast<size_t>(p->messageSize));
			memcpy(copy.data(), p->messageBuffer, copy.size());
			WinsockNetLayer::HandleDataReceived(WinsockNetLayer::GetHostSmallId(), WinsockNetLayer::GetLocalSmallId(),
				copy.data(), static_cast<unsigned int>(p->messageSize));
			break;
		}
		break;
	}
	default:
		break;
	}
}

struct PartyEndpointCreateWait
{
	bool completed = false;
	bool ok = false;
	PARTY_ENDPOINT_HANDLE endpoint = nullptr;
};

static void CreateEndpointCompletedPumpCb(const PARTY_STATE_CHANGE *sc, void *ctx)
{
	GameLayerStateChange(sc);
	auto *w = static_cast<PartyEndpointCreateWait *>(ctx);
	if (sc->stateChangeType != PARTY_STATE_CHANGE_TYPE_CREATE_ENDPOINT_COMPLETED)
		return;
	const auto *p = reinterpret_cast<const PARTY_CREATE_ENDPOINT_COMPLETED_STATE_CHANGE *>(sc);
	if (p->asyncIdentifier != w)
		return;
	w->completed = true;
	w->ok = (p->result == PARTY_STATE_CHANGE_RESULT_SUCCEEDED);
	if (w->ok)
		w->endpoint = p->localEndpoint;
	else
		LogPartyErrorDetail("CreateEndpointCompleted", p->errorDetail);
}

static bool PartyCreateGameEndpointBlocking(PARTY_NETWORK_HANDLE network, PARTY_LOCAL_USER_HANDLE user,
	PARTY_ENDPOINT_HANDLE *outEndpoint)
{
	if (outEndpoint == nullptr)
		return false;
	*outEndpoint = nullptr;
	PartyEndpointCreateWait wait = {};
	const PartyError qerr = PartyNetworkCreateEndpoint(network, user, 0, nullptr, nullptr, &wait, outEndpoint);
	if (qerr != c_partyErrorSuccess)
	{
		app.DebugPrintf("[Party] PartyNetworkCreateEndpoint failed (err=0x%08X)\n", (unsigned int)qerr);
		return false;
	}
	const DWORD deadline = GetTickCount() + 60000;
	while (GetTickCount() < deadline)
	{
		PumpStateChanges(g_partyHandle, CreateEndpointCompletedPumpCb, &wait);
		if (wait.completed)
		{
			if (!wait.ok)
				return false;
			if (*outEndpoint == nullptr && wait.endpoint != nullptr)
				*outEndpoint = wait.endpoint;
			return *outEndpoint != nullptr;
		}
		Sleep(5);
	}
	app.DebugPrintf("[Party] PartyCreateGameEndpointBlocking timed out\n");
	return false;
}

static bool DiscoverJoinPartyHostEndpoint()
{
	const DWORD deadline = GetTickCount() + 30000;
	while (GetTickCount() < deadline)
	{
		EnterCriticalSection(&g_partyCs);
		PumpStateChanges(g_partyHandle, PumpGameLayerOnlyCb, nullptr);
		uint32_t n = 0;
		const PARTY_ENDPOINT_HANDLE *eps = nullptr;
		if (g_joinNetwork != nullptr &&
			PartyNetworkGetEndpoints(g_joinNetwork, &n, &eps) == c_partyErrorSuccess && eps != nullptr && n > 0u)
		{
			for (uint32_t i = 0; i < n; ++i)
			{
				PartyBool loc = 0;
				if (PartyEndpointIsLocal(eps[i], &loc) != c_partyErrorSuccess)
					continue;
				if (!loc && eps[i] != g_joinGameEndpoint)
				{
					g_joinHostEndpoint = eps[i];
					LeaveCriticalSection(&g_partyCs);
					return true;
				}
			}
		}
		LeaveCriticalSection(&g_partyCs);
		Sleep(5);
	}
	return false;
}

void ResetHostHandlesLocked()
{
	g_hostLocalUser = nullptr;
	g_hostNetwork = nullptr;
	g_hostSessionActive = false;
	g_hostGameEndpoint = nullptr;
	memset(g_partyRemoteEndpointBySmallId, 0, sizeof(g_partyRemoteEndpointBySmallId));
}

void ResetJoinHandlesLocked()
{
	g_joinLocalUser = nullptr;
	g_joinNetwork = nullptr;
	g_joinGameEndpoint = nullptr;
	g_joinHostEndpoint = nullptr;
	g_partyClientGameTransportActive = false;
}

} // namespace

namespace PlayFabPartyTransport
{
bool IsRuntimeEnabled()
{
#if MINECRAFT_PLAYFAB_PARTY_ENABLED && MINECRAFT_PLAYFAB_PARTY_RUNTIME_ENABLED
	return true;
#else
	return false;
#endif
}

bool IsAvailable()
{
#if MINECRAFT_PLAYFAB_PARTY_ENABLED && MINECRAFT_PLAYFAB_PARTY_RUNTIME_ENABLED && MINECRAFT_PLAYFAB_PARTY_SDK_AVAILABLE
	return true;
#else
	return false;
#endif
}

static bool EnsureInitializedInternal()
{
	if (!IsAvailable())
		return false;
	EnsurePartyCs();
	if (g_partyHandle != nullptr)
		return true;
	// Party networking on Windows expects a multithreaded COM apartment (GDK / Party docs).
	static bool s_partyComInited = false;
	if (!s_partyComInited)
	{
		const HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
		if (SUCCEEDED(hr) || hr == RPC_E_CHANGED_MODE)
			s_partyComInited = true;
	}
	// Ensure region discovery runs at init (not deferred until CreateNewNetwork). Safe before PartyInitialize per SDK.
	{
		PARTY_REGION_UPDATE_CONFIGURATION regionCfg = {};
		regionCfg.mode = PARTY_REGION_UPDATE_MODE_IMMEDIATE;
		regionCfg.refreshIntervalInSeconds = 300u;
		PartyError optErr =
			PartySetOption(nullptr, PARTY_OPTION_REGION_UPDATE_CONFIGURATION, &regionCfg);
		if (optErr != c_partyErrorSuccess)
		{
			app.DebugPrintf("[Party] PartySetOption(REGION_UPDATE_CONFIGURATION) failed (err=0x%08X)\n",
				(unsigned int)optErr);
		}
	}
	PartyError err = PartyInitialize(MINECRAFT_PLAYFAB_TITLE_ID, &g_partyHandle);
	if (err != c_partyErrorSuccess || g_partyHandle == nullptr)
	{
		app.DebugPrintf("[Party] PartyInitialize failed (err=0x%08X)\n", (unsigned int)err);
		g_partyHandle = nullptr;
		return false;
	}
	app.DebugPrintf("[Party] PartyInitialize ok (title=%s)\n", MINECRAFT_PLAYFAB_TITLE_ID);
	return true;
}

bool HostCreateNetworkForLobby(std::string *outSerializedDescriptor, std::string *outInvitationId)
{
	if (outSerializedDescriptor)
		outSerializedDescriptor->clear();
	if (outInvitationId)
		outInvitationId->clear();
	if (!IsAvailable())
		return false;

	EnsurePartyCs();
	EnterCriticalSection(&g_partyCs);

	if (!EnsureInitializedInternal())
	{
		LeaveCriticalSection(&g_partyCs);
		return false;
	}

	// Tear down previous host session if any
	if (g_hostNetwork != nullptr)
	{
		PartyNetworkLeaveNetwork(g_hostNetwork, nullptr);
		for (int i = 0; i < 200; ++i)
		{
			PumpStateChanges(g_partyHandle, nullptr, nullptr);
			Sleep(5);
		}
		g_hostNetwork = nullptr;
	}
	if (g_hostLocalUser != nullptr)
	{
		PartyDestroyLocalUser(g_partyHandle, g_hostLocalUser, nullptr);
		for (int i = 0; i < 200; ++i)
		{
			PumpStateChanges(g_partyHandle, nullptr, nullptr);
			Sleep(5);
		}
		g_hostLocalUser = nullptr;
	}

	if (!PlayFabLobbyWin64::RefreshPlayFabEntityToken())
	{
		app.DebugPrintf("[Party] HostCreateNetworkForLobby: RefreshPlayFabEntityToken failed (need PlayFab login)\n");
		LeaveCriticalSection(&g_partyCs);
		return false;
	}

	std::string entityId, entityType, entityToken;
	if (!PlayFabLobbyWin64::GetPlayFabEntityCredentials(&entityId, &entityType, &entityToken) ||
		entityId.empty() || entityToken.empty())
	{
		app.DebugPrintf("[Party] HostCreateNetworkForLobby: missing PlayFab entity credentials\n");
		LeaveCriticalSection(&g_partyCs);
		return false;
	}

	PartyError err = PartyCreateLocalUserWithEntityType(g_partyHandle, entityId.c_str(), entityType.c_str(),
		entityToken.c_str(), &g_hostLocalUser);
	if (err != c_partyErrorSuccess || g_hostLocalUser == nullptr)
	{
		app.DebugPrintf("[Party] PartyCreateLocalUserWithEntityType failed (err=0x%08X)\n", (unsigned int)err);
		LeaveCriticalSection(&g_partyCs);
		return false;
	}

	{
		PartyString partyEid = nullptr;
		if (PartyLocalUserGetEntityId(g_hostLocalUser, &partyEid) == c_partyErrorSuccess && partyEid != nullptr &&
			entityId != std::string(partyEid))
		{
			app.DebugPrintf("[Party] Warning: PlayFab entity id != Party local user entity id (check title / login)\n");
		}
	}

	PARTY_NETWORK_CONFIGURATION cfg = {};
	cfg.maxUserCount = 32;
	cfg.maxDeviceCount = 16;
	cfg.maxUsersPerDeviceCount = 8;
	cfg.maxDevicesPerUserCount = 4;
	cfg.maxEndpointsPerDeviceCount = 8;
	cfg.directPeerConnectivityOptions = (PARTY_DIRECT_PEER_CONNECTIVITY_OPTIONS)(
		PARTY_DIRECT_PEER_CONNECTIVITY_OPTIONS_ANY_PLATFORM_TYPE | PARTY_DIRECT_PEER_CONNECTIVITY_OPTIONS_ANY_ENTITY_LOGIN_PROVIDER);

	// Optional OUT params: omit buffers; completion arrives via PARTY_STATE_CHANGE_TYPE_CREATE_NEW_NETWORK_COMPLETED.
	err = PartyCreateNewNetwork(g_partyHandle, g_hostLocalUser, &cfg, 0, nullptr, nullptr, nullptr, nullptr, nullptr);
	if (err != c_partyErrorSuccess)
	{
		app.DebugPrintf("[Party] PartyCreateNewNetwork failed (err=0x%08X)\n", (unsigned int)err);
		PartyDestroyLocalUser(g_partyHandle, g_hostLocalUser, nullptr);
		g_hostLocalUser = nullptr;
		LeaveCriticalSection(&g_partyCs);
		return false;
	}

	HostCreateState hs = {};
	HostCreatePumpCtx hccp = { &hs };
	// Create may wait on PartyRegionsChanged + QoS before CreateNewNetworkCompleted (see Party.h).
	const DWORD deadline = GetTickCount() + 60000;
	while (GetTickCount() < deadline)
	{
		PumpStateChanges(g_partyHandle, HostCreatePumpCb, &hccp);
		if (hs.createCompleted)
			break;
		Sleep(5);
	}

	if (!hs.createCompleted || !hs.createSucceeded)
	{
		if (!hs.createCompleted)
			app.DebugPrintf("[Party] CreateNewNetwork timed out waiting for completion state change\n");
		else
			app.DebugPrintf("[Party] CreateNewNetwork completed with failure result=%u\n", hs.createResult);
		PartyDestroyLocalUser(g_partyHandle, g_hostLocalUser, nullptr);
		g_hostLocalUser = nullptr;
		LeaveCriticalSection(&g_partyCs);
		return false;
	}

	// Creator must still ConnectToNetwork (DTLS to relay); GetNetworks() may stay empty until then (Party.h).
	g_hostNetwork = nullptr;
	err = PartyConnectToNetwork(g_partyHandle, &hs.networkDescriptor, nullptr, &g_hostNetwork);
	if (err != c_partyErrorSuccess)
	{
		app.DebugPrintf("[Party] PartyConnectToNetwork (host) failed (err=0x%08X)\n", (unsigned int)err);
		PartyDestroyLocalUser(g_partyHandle, g_hostLocalUser, nullptr);
		g_hostLocalUser = nullptr;
		LeaveCriticalSection(&g_partyCs);
		return false;
	}

	JoinHandshakeState connectWait = {};
	connectWait.hostConnect = true;
	const DWORD connectDeadline = GetTickCount() + 30000;
	while (GetTickCount() < connectDeadline)
	{
		PumpStateChanges(g_partyHandle, JoinHandshakePumpCb, &connectWait);
		if (connectWait.connectCompleted)
			break;
		Sleep(5);
	}
	if (!connectWait.connectCompleted || !connectWait.connectSucceeded || g_hostNetwork == nullptr)
	{
		app.DebugPrintf("[Party] Host ConnectToNetwork did not complete successfully\n");
		if (g_hostNetwork != nullptr)
		{
			PartyNetworkLeaveNetwork(g_hostNetwork, nullptr);
			g_hostNetwork = nullptr;
		}
		PartyDestroyLocalUser(g_partyHandle, g_hostLocalUser, nullptr);
		g_hostLocalUser = nullptr;
		LeaveCriticalSection(&g_partyCs);
		return false;
	}

	(void)RefreshLocalUserEntityToken(g_hostLocalUser);

	const char *inviteForAuth = hs.invitationId[0] != 0 ? hs.invitationId : "";
	app.DebugPrintf("[Party] PartyNetworkAuthenticateLocalUser (host) invite='%s'\n", inviteForAuth);
	err = PartyNetworkAuthenticateLocalUser(g_hostNetwork, g_hostLocalUser, inviteForAuth, nullptr);
	if (err != c_partyErrorSuccess)
	{
		app.DebugPrintf("[Party] PartyNetworkAuthenticateLocalUser (host) failed (err=0x%08X)\n", (unsigned int)err);
		PartyNetworkLeaveNetwork(g_hostNetwork, nullptr);
		g_hostNetwork = nullptr;
		PartyDestroyLocalUser(g_partyHandle, g_hostLocalUser, nullptr);
		g_hostLocalUser = nullptr;
		LeaveCriticalSection(&g_partyCs);
		return false;
	}

	JoinHandshakeState authWait = {};
	authWait.filterAuthByHandles = true;
	authWait.expectedAuthNetwork = g_hostNetwork;
	authWait.expectedAuthLocalUser = g_hostLocalUser;
	const DWORD authDeadline = GetTickCount() + 60000;
	while (GetTickCount() < authDeadline)
	{
		PumpStateChanges(g_partyHandle, JoinHandshakePumpCb, &authWait);
		if (authWait.authCompleted)
			break;
		Sleep(5);
	}
	if (!authWait.authCompleted || !authWait.authSucceeded)
	{
		app.DebugPrintf(
			"[Party] Host authenticate did not complete successfully (completed=%d ok=%d)\n",
			authWait.authCompleted ? 1 : 0, authWait.authSucceeded ? 1 : 0);
		PartyNetworkLeaveNetwork(g_hostNetwork, nullptr);
		g_hostNetwork = nullptr;
		PartyDestroyLocalUser(g_partyHandle, g_hostLocalUser, nullptr);
		g_hostLocalUser = nullptr;
		LeaveCriticalSection(&g_partyCs);
		return false;
	}

	memset(g_partyRemoteEndpointBySmallId, 0, sizeof(g_partyRemoteEndpointBySmallId));
	if (g_hostGameEndpoint != nullptr)
	{
		PartyNetworkDestroyEndpoint(g_hostNetwork, g_hostGameEndpoint, nullptr);
		for (int i = 0; i < 120; ++i)
		{
			PumpStateChanges(g_partyHandle, PumpGameLayerOnlyCb, nullptr);
			Sleep(5);
		}
		g_hostGameEndpoint = nullptr;
	}
	if (!PartyCreateGameEndpointBlocking(g_hostNetwork, g_hostLocalUser, &g_hostGameEndpoint))
	{
		app.DebugPrintf("[Party] Host: failed to create Party game endpoint\n");
		PartyNetworkLeaveNetwork(g_hostNetwork, nullptr);
		g_hostNetwork = nullptr;
		PartyDestroyLocalUser(g_partyHandle, g_hostLocalUser, nullptr);
		g_hostLocalUser = nullptr;
		LeaveCriticalSection(&g_partyCs);
		return false;
	}

	char serialized[PARTY_MAX_SERIALIZED_NETWORK_DESCRIPTOR_STRING_LENGTH + 1] = {};
	err = PartySerializeNetworkDescriptor(&hs.networkDescriptor, serialized);
	if (err != c_partyErrorSuccess)
	{
		app.DebugPrintf("[Party] PartySerializeNetworkDescriptor failed (err=0x%08X)\n", (unsigned int)err);
		PartyNetworkLeaveNetwork(g_hostNetwork, nullptr);
		g_hostNetwork = nullptr;
		PartyDestroyLocalUser(g_partyHandle, g_hostLocalUser, nullptr);
		g_hostLocalUser = nullptr;
		LeaveCriticalSection(&g_partyCs);
		return false;
	}

	if (outSerializedDescriptor)
		*outSerializedDescriptor = serialized;
	if (outInvitationId && hs.invitationId[0] != 0)
		*outInvitationId = hs.invitationId;

	g_hostSessionActive = true;
	app.DebugPrintf("[Party] Host network ready (serialized len=%zu)\n", outSerializedDescriptor ? outSerializedDescriptor->size() : 0);

	LeaveCriticalSection(&g_partyCs);
	return true;
}

void StopHostSession()
{
#if MINECRAFT_PLAYFAB_PARTY_SDK_AVAILABLE
	if (!g_partyCsInit)
		return;
	EnterCriticalSection(&g_partyCs);
	if (g_partyHandle != nullptr)
	{
		memset(g_partyRemoteEndpointBySmallId, 0, sizeof(g_partyRemoteEndpointBySmallId));
		if (g_joinGameEndpoint != nullptr && g_joinNetwork != nullptr)
		{
			PartyNetworkDestroyEndpoint(g_joinNetwork, g_joinGameEndpoint, nullptr);
			for (int i = 0; i < 100; ++i)
			{
				PumpStateChanges(g_partyHandle, PumpGameLayerOnlyCb, nullptr);
				Sleep(5);
			}
			g_joinGameEndpoint = nullptr;
		}
		g_joinHostEndpoint = nullptr;
		g_partyClientGameTransportActive = false;
		if (g_hostGameEndpoint != nullptr && g_hostNetwork != nullptr)
		{
			PartyNetworkDestroyEndpoint(g_hostNetwork, g_hostGameEndpoint, nullptr);
			for (int i = 0; i < 120; ++i)
			{
				PumpStateChanges(g_partyHandle, PumpGameLayerOnlyCb, nullptr);
				Sleep(5);
			}
			g_hostGameEndpoint = nullptr;
		}
		if (g_joinNetwork != nullptr)
		{
			PartyNetworkLeaveNetwork(g_joinNetwork, nullptr);
			for (int i = 0; i < 100; ++i)
			{
				PumpStateChanges(g_partyHandle, PumpGameLayerOnlyCb, nullptr);
				Sleep(5);
			}
			g_joinNetwork = nullptr;
		}
		if (g_joinLocalUser != nullptr)
		{
			PartyDestroyLocalUser(g_partyHandle, g_joinLocalUser, nullptr);
			for (int i = 0; i < 100; ++i)
			{
				PumpStateChanges(g_partyHandle, PumpGameLayerOnlyCb, nullptr);
				Sleep(5);
			}
			g_joinLocalUser = nullptr;
		}
		if (g_hostNetwork != nullptr)
		{
			PartyNetworkLeaveNetwork(g_hostNetwork, nullptr);
			for (int i = 0; i < 200; ++i)
			{
				PumpStateChanges(g_partyHandle, PumpGameLayerOnlyCb, nullptr);
				Sleep(5);
			}
			g_hostNetwork = nullptr;
		}
		if (g_hostLocalUser != nullptr)
		{
			PartyDestroyLocalUser(g_partyHandle, g_hostLocalUser, nullptr);
			for (int i = 0; i < 200; ++i)
			{
				PumpStateChanges(g_partyHandle, PumpGameLayerOnlyCb, nullptr);
				Sleep(5);
			}
			g_hostLocalUser = nullptr;
		}
		PartyCleanup(g_partyHandle);
		g_partyHandle = nullptr;
		app.DebugPrintf("[Party] PartyCleanup (host stop)\n");
	}
	ResetHostHandlesLocked();
	LeaveCriticalSection(&g_partyCs);
#endif
}

bool JoinRunPartyHandshake(const char *serializedNetworkDescriptor, const char *invitationId,
	const char *entityId, const char *entityType, const char *entityToken)
{
#if MINECRAFT_PLAYFAB_PARTY_SDK_AVAILABLE
	if (!IsAvailable())
		return false;
	if (serializedNetworkDescriptor == nullptr || serializedNetworkDescriptor[0] == 0)
		return false;

	EnsurePartyCs();
	EnterCriticalSection(&g_partyCs);

	if (!EnsureInitializedInternal())
	{
		LeaveCriticalSection(&g_partyCs);
		return false;
	}

	if (g_joinNetwork != nullptr)
	{
		PartyNetworkLeaveNetwork(g_joinNetwork, nullptr);
		for (int i = 0; i < 150; ++i)
		{
			PumpStateChanges(g_partyHandle, nullptr, nullptr);
			Sleep(5);
		}
		g_joinNetwork = nullptr;
	}
	if (g_joinLocalUser != nullptr)
	{
		PartyDestroyLocalUser(g_partyHandle, g_joinLocalUser, nullptr);
		for (int i = 0; i < 150; ++i)
		{
			PumpStateChanges(g_partyHandle, nullptr, nullptr);
			Sleep(5);
		}
		g_joinLocalUser = nullptr;
	}

	(void)entityId;
	(void)entityType;
	(void)entityToken;

	if (!PlayFabLobbyWin64::RefreshPlayFabEntityToken())
	{
		app.DebugPrintf("[Party] JoinRunPartyHandshake: RefreshPlayFabEntityToken failed\n");
		LeaveCriticalSection(&g_partyCs);
		return false;
	}

	std::string joinEid, joinEtype, joinEtok;
	if (!PlayFabLobbyWin64::GetPlayFabEntityCredentials(&joinEid, &joinEtype, &joinEtok) || joinEid.empty() ||
		joinEtok.empty())
	{
		app.DebugPrintf("[Party] JoinRunPartyHandshake: missing PlayFab entity credentials after refresh\n");
		LeaveCriticalSection(&g_partyCs);
		return false;
	}

	const char *etype =
		joinEtype.empty() ? "title_player_account" : joinEtype.c_str();

	PartyError err = PartyCreateLocalUserWithEntityType(g_partyHandle, joinEid.c_str(), etype, joinEtok.c_str(),
		&g_joinLocalUser);
	if (err != c_partyErrorSuccess || g_joinLocalUser == nullptr)
	{
		app.DebugPrintf("[Party] Join: PartyCreateLocalUserWithEntityType failed (err=0x%08X)\n", (unsigned int)err);
		LeaveCriticalSection(&g_partyCs);
		return false;
	}

	PARTY_NETWORK_DESCRIPTOR desc = {};
	err = PartyDeserializeNetworkDescriptor(serializedNetworkDescriptor, &desc);
	if (err != c_partyErrorSuccess)
	{
		app.DebugPrintf("[Party] PartyDeserializeNetworkDescriptor failed (err=0x%08X)\n", (unsigned int)err);
		PartyDestroyLocalUser(g_partyHandle, g_joinLocalUser, nullptr);
		g_joinLocalUser = nullptr;
		LeaveCriticalSection(&g_partyCs);
		return false;
	}

	g_joinNetwork = nullptr;
	err = PartyConnectToNetwork(g_partyHandle, &desc, nullptr, &g_joinNetwork);
	if (err != c_partyErrorSuccess)
	{
		app.DebugPrintf("[Party] PartyConnectToNetwork (join) failed (err=0x%08X)\n", (unsigned int)err);
		PartyDestroyLocalUser(g_partyHandle, g_joinLocalUser, nullptr);
		g_joinLocalUser = nullptr;
		LeaveCriticalSection(&g_partyCs);
		return false;
	}

	JoinHandshakeState st = {};
	st.hostConnect = false;
	const DWORD deadline = GetTickCount() + 30000;
	while (GetTickCount() < deadline)
	{
		PumpStateChanges(g_partyHandle, JoinHandshakePumpCb, &st);
		if (st.connectCompleted)
			break;
		Sleep(5);
	}
	if (!st.connectCompleted || !st.connectSucceeded || g_joinNetwork == nullptr)
	{
		app.DebugPrintf("[Party] ConnectToNetwork did not succeed\n");
		PartyDestroyLocalUser(g_partyHandle, g_joinLocalUser, nullptr);
		g_joinLocalUser = nullptr;
		LeaveCriticalSection(&g_partyCs);
		return false;
	}

	(void)RefreshLocalUserEntityToken(g_joinLocalUser);

	const char *inv = (invitationId != nullptr && invitationId[0] != 0) ? invitationId : "";
	err = PartyNetworkAuthenticateLocalUser(g_joinNetwork, g_joinLocalUser, inv, nullptr);
	if (err != c_partyErrorSuccess)
	{
		app.DebugPrintf("[Party] PartyNetworkAuthenticateLocalUser (join) failed (err=0x%08X)\n", (unsigned int)err);
		PartyNetworkLeaveNetwork(g_joinNetwork, nullptr);
		g_joinNetwork = nullptr;
		PartyDestroyLocalUser(g_partyHandle, g_joinLocalUser, nullptr);
		g_joinLocalUser = nullptr;
		LeaveCriticalSection(&g_partyCs);
		return false;
	}

	st = JoinHandshakeState();
	st.filterAuthByHandles = true;
	st.expectedAuthNetwork = g_joinNetwork;
	st.expectedAuthLocalUser = g_joinLocalUser;
	const DWORD authDeadline = GetTickCount() + 60000;
	while (GetTickCount() < authDeadline)
	{
		PumpStateChanges(g_partyHandle, JoinHandshakePumpCb, &st);
		if (st.authCompleted)
			break;
		Sleep(5);
	}
	if (!st.authCompleted || !st.authSucceeded)
	{
		app.DebugPrintf("[Party] Join authenticate did not succeed (completed=%d ok=%d)\n",
			st.authCompleted ? 1 : 0, st.authSucceeded ? 1 : 0);
		PartyNetworkLeaveNetwork(g_joinNetwork, nullptr);
		g_joinNetwork = nullptr;
		PartyDestroyLocalUser(g_partyHandle, g_joinLocalUser, nullptr);
		g_joinLocalUser = nullptr;
		LeaveCriticalSection(&g_partyCs);
		return false;
	}

	g_joinHostEndpoint = nullptr;
	if (!PartyCreateGameEndpointBlocking(g_joinNetwork, g_joinLocalUser, &g_joinGameEndpoint))
	{
		app.DebugPrintf("[Party] Join: PartyCreateGameEndpointBlocking failed\n");
		PartyNetworkLeaveNetwork(g_joinNetwork, nullptr);
		g_joinNetwork = nullptr;
		PartyDestroyLocalUser(g_partyHandle, g_joinLocalUser, nullptr);
		g_joinLocalUser = nullptr;
		LeaveCriticalSection(&g_partyCs);
		return false;
	}

	LeaveCriticalSection(&g_partyCs);

	if (!DiscoverJoinPartyHostEndpoint())
	{
		app.DebugPrintf("[Party] Join: could not discover host Party game endpoint\n");
		EnterCriticalSection(&g_partyCs);
		if (g_joinGameEndpoint != nullptr && g_joinNetwork != nullptr)
		{
			PartyNetworkDestroyEndpoint(g_joinNetwork, g_joinGameEndpoint, nullptr);
			for (int i = 0; i < 120; ++i)
			{
				PumpStateChanges(g_partyHandle, PumpGameLayerOnlyCb, nullptr);
				Sleep(5);
			}
			g_joinGameEndpoint = nullptr;
		}
		g_joinHostEndpoint = nullptr;
		PartyNetworkLeaveNetwork(g_joinNetwork, nullptr);
		g_joinNetwork = nullptr;
		PartyDestroyLocalUser(g_partyHandle, g_joinLocalUser, nullptr);
		g_joinLocalUser = nullptr;
		LeaveCriticalSection(&g_partyCs);
		return false;
	}

	app.DebugPrintf("[Party] Join Party handshake succeeded (game endpoints ready)\n");
	return true;
#else
	(void)serializedNetworkDescriptor;
	(void)invitationId;
	(void)entityId;
	(void)entityType;
	(void)entityToken;
	return false;
#endif
}

bool IsPartyGameTransportActive()
{
#if MINECRAFT_PLAYFAB_PARTY_SDK_AVAILABLE
	return g_partyClientGameTransportActive ||
		(g_hostSessionActive && g_hostGameEndpoint != nullptr && g_hostNetwork != nullptr);
#else
	return false;
#endif
}

PartyJoinAssignWait WaitForPartyJoinSmallIdAssignment(unsigned char *outSmallId, unsigned int timeoutMs)
{
#if MINECRAFT_PLAYFAB_PARTY_SDK_AVAILABLE
	if (outSmallId == nullptr || g_joinGameEndpoint == nullptr || g_joinHostEndpoint == nullptr)
		return PartyJoinAssignWait::TimedOut;
	g_partyJoinThreadOwnsPump = true;
	g_partyJoinWaitingForAssign = true;
	g_partyJoinAssignSuccess = false;
	g_partyJoinRejectReceived = false;
	const DWORD deadline = GetTickCount() + static_cast<DWORD>(timeoutMs);
	while (GetTickCount() < deadline)
	{
		if (g_partyJoinRejectReceived)
		{
			g_partyJoinThreadOwnsPump = false;
			g_partyJoinWaitingForAssign = false;
			return PartyJoinAssignWait::Rejected;
		}
		if (g_partyJoinAssignSuccess)
		{
			*outSmallId = g_partyJoinAssignByte;
			g_partyClientGameTransportActive = true;
			g_partyJoinThreadOwnsPump = false;
			g_partyJoinWaitingForAssign = false;
			return PartyJoinAssignWait::Success;
		}
		EnterCriticalSection(&g_partyCs);
		PumpStateChanges(g_partyHandle, PumpGameLayerOnlyCb, nullptr);
		LeaveCriticalSection(&g_partyCs);
		Sleep(1);
	}
	g_partyJoinThreadOwnsPump = false;
	g_partyJoinWaitingForAssign = false;
	return PartyJoinAssignWait::TimedOut;
#else
	(void)outSmallId;
	(void)timeoutMs;
	return PartyJoinAssignWait::TimedOut;
#endif
}

bool SendPartyGameDataToHost(const void *data, int dataSize)
{
#if MINECRAFT_PLAYFAB_PARTY_SDK_AVAILABLE
	if (!g_partyClientGameTransportActive || g_joinGameEndpoint == nullptr || g_joinHostEndpoint == nullptr)
		return false;
	if (dataSize <= 0 || dataSize > WIN64_NET_MAX_PACKET_SIZE)
		return false;
	EnsurePartyCs();
	EnterCriticalSection(&g_partyCs);
	PARTY_DATA_BUFFER db = { data, static_cast<uint32_t>(dataSize) };
	const PartyError e = PartyEndpointSendMessage(g_joinGameEndpoint, 1u, &g_joinHostEndpoint,
		PartyTcpStyleSendOptions(), nullptr, 1u, &db, nullptr);
	LeaveCriticalSection(&g_partyCs);
	return e == c_partyErrorSuccess;
#else
	(void)data;
	(void)dataSize;
	return false;
#endif
}

bool SendPartyGameDataToClient(unsigned char targetSmallId, const void *data, int dataSize)
{
#if MINECRAFT_PLAYFAB_PARTY_SDK_AVAILABLE
	if (!g_hostSessionActive || g_hostGameEndpoint == nullptr || g_hostNetwork == nullptr)
		return false;
	if (dataSize <= 0 || dataSize > WIN64_NET_MAX_PACKET_SIZE)
		return false;
	PARTY_ENDPOINT_HANDLE remote = g_partyRemoteEndpointBySmallId[targetSmallId];
	if (remote == nullptr)
		return false;
	EnsurePartyCs();
	EnterCriticalSection(&g_partyCs);
	PARTY_DATA_BUFFER db = { data, static_cast<uint32_t>(dataSize) };
	const PartyError e = PartyEndpointSendMessage(g_hostGameEndpoint, 1u, &remote, PartyTcpStyleSendOptions(),
		nullptr, 1u, &db, nullptr);
	LeaveCriticalSection(&g_partyCs);
	return e == c_partyErrorSuccess;
#else
	(void)targetSmallId;
	(void)data;
	(void)dataSize;
	return false;
#endif
}

void ShutdownPartyClientTransport()
{
#if MINECRAFT_PLAYFAB_PARTY_SDK_AVAILABLE
	g_partyClientGameTransportActive = false;
	g_partyJoinWaitingForAssign = false;
	g_partyJoinThreadOwnsPump = false;
	if (!g_partyCsInit || g_partyHandle == nullptr)
		return;
	EnsurePartyCs();
	EnterCriticalSection(&g_partyCs);
	if (g_joinGameEndpoint != nullptr && g_joinNetwork != nullptr)
	{
		PartyNetworkDestroyEndpoint(g_joinNetwork, g_joinGameEndpoint, nullptr);
		for (int i = 0; i < 80; ++i)
		{
			PumpStateChanges(g_partyHandle, PumpGameLayerOnlyCb, nullptr);
			Sleep(5);
		}
		g_joinGameEndpoint = nullptr;
	}
	g_joinHostEndpoint = nullptr;
	LeaveCriticalSection(&g_partyCs);
#endif
}

void PumpNetworking()
{
#if MINECRAFT_PLAYFAB_PARTY_SDK_AVAILABLE
	if (!IsAvailable() || g_partyHandle == nullptr)
		return;
	if (g_partyJoinThreadOwnsPump)
		return;
	EnsurePartyCs();
	if (TryEnterCriticalSection(&g_partyCs))
	{
		PumpStateChanges(g_partyHandle, PumpGameLayerOnlyCb, nullptr);
		LeaveCriticalSection(&g_partyCs);
	}
#endif
}

bool StartHostSession(const char *announceHost, int announcePort)
{
	(void)announceHost;
	(void)announcePort;
	return EnsureInitializedInternal();
}

bool BeginJoinSession(const char *targetHost, int targetPort)
{
	(void)targetHost;
	(void)targetPort;
	return EnsureInitializedInternal();
}

} // namespace PlayFabPartyTransport

#else // !MINECRAFT_PLAYFAB_PARTY_SDK_AVAILABLE

namespace PlayFabPartyTransport
{
bool IsRuntimeEnabled()
{
#if MINECRAFT_PLAYFAB_PARTY_ENABLED && MINECRAFT_PLAYFAB_PARTY_RUNTIME_ENABLED
	return true;
#else
	return false;
#endif
}

bool IsAvailable() { return false; }

bool HostCreateNetworkForLobby(std::string *, std::string *) { return false; }

void StopHostSession() {}

bool JoinRunPartyHandshake(const char *, const char *, const char *, const char *, const char *) { return false; }

PartyJoinAssignWait WaitForPartyJoinSmallIdAssignment(unsigned char *, unsigned int)
{
	return PartyJoinAssignWait::TimedOut;
}

bool IsPartyGameTransportActive() { return false; }

bool SendPartyGameDataToHost(const void *, int) { return false; }

bool SendPartyGameDataToClient(unsigned char, const void *, int) { return false; }

void ShutdownPartyClientTransport() {}

void PumpNetworking() {}

bool StartHostSession(const char *, int) { return false; }

bool BeginJoinSession(const char *, int) { return false; }
}

#endif // MINECRAFT_PLAYFAB_PARTY_SDK_AVAILABLE

#endif // _WINDOWS64
