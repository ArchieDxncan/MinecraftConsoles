#include "stdafx.h"

#ifdef _WINDOWS64

#include "PlayFabLobbyWin64.h"
#include "../Leaderboards/PlayFabConfig.h"
#include "../Windows64_Xuid.h"

#include "Common/Consoles_App.h"
#include "Common/Network/PlatformNetworkManagerInterface.h"
#include "../../Minecraft.h"
#include "../../User.h"
#include "../4JLibs/inc/4J_Profile.h"

#include <nlohmann/json.hpp>

#include <iphlpapi.h>
#include <ipifcons.h>
#include <climits>
#include <cstdlib>
#include <cstring>
#include <atomic>
#include <mutex>
#include <unordered_set>
#include <vector>
#include <winhttp.h>
#include <cstdarg>
#include <cstdio>

#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "iphlpapi.lib")

#if MINECRAFT_PLAYFAB_LOBBY_UPNP && !defined(_UWP)
#include <natupnp.h>
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")
#endif
#if MINECRAFT_PLAYFAB_LOBBY_UPNP && defined(_UWP)
#include "UpnpIgdUwp.h"
#endif

#ifdef _UWP
extern "C" bool Uwp_GetPrimaryLanIPv4(char *out, size_t outSize);
// UWP_App.cpp — must be global ::LogMsg, not inside an anonymous namespace (linker).
void LogMsg(const char *fmt, ...);
#endif

namespace
{
	// UWP: write high-signal PlayFab lines to mc_debug.log (LogMsg). Win32: DebugPrintf only.
	static void PlayFabFileDiag(const char *fmt, ...)
	{
		char buf[768];
		va_list ap;
		va_start(ap, fmt);
		vsnprintf(buf, sizeof(buf), fmt, ap);
		va_end(ap);
		buf[sizeof(buf) - 1] = '\0';
#if defined(_UWP)
		::LogMsg("%s", buf);
#else
		app.DebugPrintf("%s", buf);
#endif
	}

	std::mutex g_mu;
	std::string g_titleId;
	std::string g_sessionTicket;
	std::string g_entityToken;
	std::string g_entityId;
	std::string g_entityType;
	std::string g_myPlayFabId;
	std::string g_activeLobbyId;

	// FindLobbies is polled from the join menu; PlayFab returns HTTP 429 if called too often.
	static std::vector<PlayFabListedGame> g_findLobbiesCache;
	static DWORD g_lastFindLobbiesHttpTick = 0;
	static DWORD g_findLobbiesBackoffUntilTick = 0;
	static const DWORD kFindLobbiesMinIntervalMs = 10000;
	static const DWORD kFindLobbies429BackoffMs = 60000;
	static DWORD g_lastFindLobbiesCacheLogTick = 0;
	static const DWORD kFindLobbiesCacheLogIntervalMs = 30000;

	std::wstring Utf8ToWide(const std::string &u8)
	{
		if (u8.empty())
			return std::wstring();
		int n = MultiByteToWideChar(CP_UTF8, 0, u8.c_str(), (int)u8.size(), nullptr, 0);
		if (n <= 0)
			return std::wstring();
		std::wstring w((size_t)n, L'\0');
		MultiByteToWideChar(CP_UTF8, 0, u8.c_str(), (int)u8.size(), &w[0], n);
		return w;
	}

	std::string WideToUtf8(const wchar_t *w)
	{
		if (!w || !*w)
			return std::string();
		int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, nullptr, 0, nullptr, nullptr);
		if (n <= 1)
			return std::string();
		std::string u8((size_t)n - 1, '\0');
		WideCharToMultiByte(CP_UTF8, 0, w, -1, &u8[0], n, nullptr, nullptr);
		return u8;
	}

	std::wstring Utf8ToWidePath(const std::string &u8)
	{
		return Utf8ToWide(u8);
	}

	bool HttpsPost(const std::wstring &host, const std::wstring &path, const std::string &extraHeadersUtf8,
		const std::string &bodyUtf8, long &httpStatus, std::string &responseUtf8, std::string &err)
	{
		httpStatus = 0;
		responseUtf8.clear();
		err.clear();

		HINTERNET hSession = WinHttpOpen(L"MinecraftConsoles/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
			WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
		if (!hSession)
		{
			err = "WinHttpOpen failed";
			return false;
		}

		HINTERNET hConnect = WinHttpConnect(hSession, host.c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0);
		if (!hConnect)
		{
			WinHttpCloseHandle(hSession);
			err = "WinHttpConnect failed";
			return false;
		}

		HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST", path.c_str(), nullptr, WINHTTP_NO_REFERER,
			WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
		if (!hRequest)
		{
			WinHttpCloseHandle(hConnect);
			WinHttpCloseHandle(hSession);
			err = "WinHttpOpenRequest failed";
			return false;
		}

		std::wstring hdr = Utf8ToWidePath("Content-Type: application/json\r\n" + extraHeadersUtf8);
		BOOL ok = WinHttpSendRequest(hRequest, hdr.empty() ? WINHTTP_NO_ADDITIONAL_HEADERS : hdr.c_str(),
			hdr.empty() ? 0 : (DWORD)-1, (LPVOID)bodyUtf8.data(), (DWORD)bodyUtf8.size(), (DWORD)bodyUtf8.size(), 0);

		if (!ok || !WinHttpReceiveResponse(hRequest, nullptr))
		{
			WinHttpCloseHandle(hRequest);
			WinHttpCloseHandle(hConnect);
			WinHttpCloseHandle(hSession);
			err = "WinHttpSendRequest/ReceiveResponse failed";
			return false;
		}

		DWORD statusCode = 0;
		DWORD sz = sizeof(statusCode);
		if (WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
				WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &sz, WINHTTP_NO_HEADER_INDEX))
		{
			httpStatus = (long)statusCode;
		}

		std::string acc;
		for (;;)
		{
			char buf[8192];
			DWORD read = 0;
			if (!WinHttpReadData(hRequest, buf, sizeof(buf), &read) || read == 0)
				break;
			acc.append(buf, read);
		}

		WinHttpCloseHandle(hRequest);
		WinHttpCloseHandle(hConnect);
		WinHttpCloseHandle(hSession);

		responseUtf8 = std::move(acc);
		return true;
	}

	bool PlayFabOk(const std::string &raw, nlohmann::json &outRoot, std::string &err)
	{
		try
		{
			outRoot = nlohmann::json::parse(raw);
		}
		catch (...)
		{
			err = "Invalid JSON";
			return false;
		}
		if (outRoot.contains("code") && outRoot["code"].is_number() && outRoot["code"].get<int>() != 200)
		{
			err = outRoot.value("errorMessage", outRoot.value("error", std::string("PlayFab error")));
			return false;
		}
		return true;
	}

	// Same scoring idea as UWP_App Uwp_GetPrimaryLanIPv4 — prefer real LAN over random interfaces.
	static int ScoreLanIpv4String(const char *ip)
	{
		unsigned a = 0, b = 0, c = 0, d = 0;
		if (sscanf_s(ip, "%u.%u.%u.%u", &a, &b, &c, &d) != 4)
			return -1;
		if (a == 127u)
			return -1;
		if (a == 169u && b == 254u)
			return -1;
		if (a == 0u)
			return -1;
		if (a == 192u && b == 168u)
			return 100;
		if (a == 10u)
			return 90;
		if (a == 172u && b >= 16u && b <= 31u)
			return 85;
		return 40;
	}

#ifndef _UWP
	static int VirtualAdapterDescriptionPenalty(const char *desc)
	{
		if (desc == nullptr || desc[0] == 0)
			return 0;
		// Deprioritize common virtual switches so we don't announce an IP the Xbox can't reach.
		char low[256];
		size_t i = 0;
		for (; desc[i] && i + 1 < sizeof(low); ++i)
		{
			char t = desc[i];
			if (t >= 'A' && t <= 'Z')
				t = (char)(t - 'A' + 'a');
			low[i] = t;
		}
		low[i] = 0;
		if (strstr(low, "hyper-v") != nullptr || strstr(low, "hyperv") != nullptr)
			return -35;
		if (strstr(low, "vmware") != nullptr)
			return -35;
		if (strstr(low, "virtualbox") != nullptr)
			return -35;
		if (strstr(low, "vethernet") != nullptr)
			return -35;
		if (strstr(low, "wsl") != nullptr)
			return -35;
		if (strstr(low, "virtual ") != nullptr)
			return -20;
		return 0;
	}
#endif

	// Uses GetAdaptersInfo (IPv4 string) so we do not need winsock2.h before windows.h (PCH order).
	bool PickLanIPv4(char *out, size_t outSize)
	{
		out[0] = 0;
#ifdef _UWP
		if (Uwp_GetPrimaryLanIPv4(out, outSize))
			return true;
#endif
		ULONG bufLen = sizeof(IP_ADAPTER_INFO);
		std::vector<BYTE> buf(bufLen);
		auto *info = reinterpret_cast<PIP_ADAPTER_INFO>(buf.data());
		if (GetAdaptersInfo(info, &bufLen) == ERROR_BUFFER_OVERFLOW)
		{
			buf.resize(bufLen);
			info = reinterpret_cast<PIP_ADAPTER_INFO>(buf.data());
		}
		if (GetAdaptersInfo(info, &bufLen) != NO_ERROR)
			return false;

#ifndef _UWP
		DWORD preferredIfIndex = 0;
		bool havePreferredIf = false;
		{
			// 8.8.8.8 as ULONG in the same form GetBestRoute expects (network byte order for IPv4).
			const ULONG dest8888 = (8u << 24) | (8u << 16) | (8u << 8) | 8u;
			MIB_IPFORWARDROW fr{};
			if (GetBestRoute(dest8888, 0, &fr) == NO_ERROR)
			{
				preferredIfIndex = fr.dwForwardIfIndex;
				havePreferredIf = true;
			}
		}

		int bestTotal = INT_MIN;
		const char *bestIp = nullptr;

		for (PIP_ADAPTER_INFO a = info; a != nullptr; a = a->Next)
		{
			if (a->Type == MIB_IF_TYPE_LOOPBACK)
				continue;

			const char *gw = a->GatewayList.IpAddress.String;
			const bool hasRealGateway =
				gw != nullptr && gw[0] != 0 && strcmp(gw, "0.0.0.0") != 0;

			const int virtPen = VirtualAdapterDescriptionPenalty(a->Description);
			const int routeBonus = (havePreferredIf && a->Index == preferredIfIndex) ? 50 : 0;
			const int gwBonus = hasRealGateway ? 25 : 0;

			for (PIP_ADDR_STRING p = &a->IpAddressList; p != nullptr; p = p->Next)
			{
				const char *ip = p->IpAddress.String;
				if (ip == nullptr || ip[0] == 0 || strcmp(ip, "0.0.0.0") == 0)
					continue;
				if (strncmp(ip, "127.", 4) == 0)
					continue;
				if (strncmp(ip, "169.254.", 8) == 0)
					continue;

				const int base = ScoreLanIpv4String(ip);
				if (base < 0)
					continue;

				const int total = base + routeBonus + gwBonus + virtPen;
				if (total > bestTotal)
				{
					bestTotal = total;
					bestIp = ip;
				}
			}
		}

		if (bestIp != nullptr)
		{
			strncpy_s(out, outSize, bestIp, _TRUNCATE);
			return out[0] != 0;
		}
		return false;
#else
		// UWP: Uwp_GetPrimaryLanIPv4 already handled; if it failed, fall back to first sane adapter.
		for (PIP_ADAPTER_INFO a = info; a != nullptr; a = a->Next)
		{
			if (a->Type == MIB_IF_TYPE_LOOPBACK)
				continue;
			for (PIP_ADDR_STRING p = &a->IpAddressList; p != nullptr; p = p->Next)
			{
				const char *ip = p->IpAddress.String;
				if (ip == nullptr || ip[0] == 0 || strcmp(ip, "0.0.0.0") == 0)
					continue;
				if (strncmp(ip, "127.", 4) == 0)
					continue;
				if (strncmp(ip, "169.254.", 8) == 0)
					continue;
				strncpy_s(out, outSize, ip, _TRUNCATE);
				return out[0] != 0;
			}
		}
		return false;
#endif
	}

#if MINECRAFT_PLAYFAB_LOBBY_UPNP && !defined(_UWP)
	static std::atomic<int> g_upnpMappedExternalPort{0};

	static bool LooksLikeIpv4(const char *s)
	{
		unsigned a = 0, b = 0, c = 0, d = 0;
		return s != nullptr && sscanf_s(s, "%u.%u.%u.%u", &a, &b, &c, &d) == 4 && a <= 255 && b <= 255 && c <= 255 && d <= 255;
	}

	static void UpnpRemoveMappingIfAny()
	{
		const int extPort = g_upnpMappedExternalPort.exchange(0);
		if (extPort <= 0)
			return;

		bool comInitialized = false;
		const HRESULT hrCo = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
		if (hrCo == RPC_E_CHANGED_MODE)
			;
		else if (SUCCEEDED(hrCo))
			comInitialized = true;
		else
			return;

		IUPnPNAT *pNat = nullptr;
		HRESULT hr = CoCreateInstance(__uuidof(UPnPNAT), nullptr, CLSCTX_INPROC_SERVER, __uuidof(IUPnPNAT), (void **)&pNat);
		if (SUCCEEDED(hr) && pNat)
		{
			IStaticPortMappingCollection *pColl = nullptr;
			hr = pNat->get_StaticPortMappingCollection(&pColl);
			pNat->Release();
			if (SUCCEEDED(hr) && pColl)
			{
				BSTR bTcp = SysAllocString(L"TCP");
				if (bTcp)
				{
					pColl->Remove((long)extPort, bTcp);
					SysFreeString(bTcp);
				}
				pColl->Release();
			}
		}

		if (comInitialized)
			CoUninitialize();
	}

	static bool TryUpnpAddTcpMapAndGetPublicIp(int gamePort, const char *lanIpUtf8, char *outAnnounceHost, size_t announceHostSize,
		int *outAnnouncePort)
	{
		if (outAnnouncePort != nullptr)
			*outAnnouncePort = gamePort;
		if (outAnnounceHost == nullptr || announceHostSize < 8)
			return false;
		outAnnounceHost[0] = 0;
		if (gamePort <= 0 || gamePort > 65535 || lanIpUtf8 == nullptr || lanIpUtf8[0] == 0)
			return false;

		bool comInitialized = false;
		const HRESULT hrCo = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
		if (hrCo == RPC_E_CHANGED_MODE)
			;
		else if (SUCCEEDED(hrCo))
			comInitialized = true;
		else
			return false;

		IUPnPNAT *pNat = nullptr;
		HRESULT hr = CoCreateInstance(__uuidof(UPnPNAT), nullptr, CLSCTX_INPROC_SERVER, __uuidof(IUPnPNAT), (void **)&pNat);
		if (FAILED(hr) || !pNat)
		{
			if (comInitialized)
				CoUninitialize();
			app.DebugPrintf("[PlayFabLobby] UPnP: CoCreate IUPnPNAT failed (HRESULT=0x%08X)\n", (unsigned)hr);
			return false;
		}

		IStaticPortMappingCollection *pColl = nullptr;
		hr = pNat->get_StaticPortMappingCollection(&pColl);
		pNat->Release();
		pNat = nullptr;
		if (FAILED(hr) || !pColl)
		{
			if (comInitialized)
				CoUninitialize();
			app.DebugPrintf("[PlayFabLobby] UPnP: StaticPortMappingCollection unavailable (HRESULT=0x%08X)\n", (unsigned)hr);
			return false;
		}

		wchar_t wClient[64];
		wClient[0] = 0;
		if (MultiByteToWideChar(CP_ACP, 0, lanIpUtf8, -1, wClient, (int)(sizeof(wClient) / sizeof(wClient[0]))) <= 0)
		{
			pColl->Release();
			if (comInitialized)
				CoUninitialize();
			return false;
		}

		{
			BSTR bTcpRm = SysAllocString(L"TCP");
			if (bTcpRm)
			{
				pColl->Remove((long)gamePort, bTcpRm);
				SysFreeString(bTcpRm);
			}
		}

		BSTR bTcp = SysAllocString(L"TCP");
		BSTR bClient = SysAllocString(wClient);
		BSTR bDesc = SysAllocString(L"MinecraftConsoles");
		IStaticPortMapping *pMap = nullptr;
		hr = pColl->Add((long)gamePort, bTcp, (long)gamePort, bClient, VARIANT_TRUE, bDesc, &pMap);
		if (bTcp)
			SysFreeString(bTcp);
		if (bClient)
			SysFreeString(bClient);
		if (bDesc)
			SysFreeString(bDesc);

		if (FAILED(hr) || !pMap)
		{
			pColl->Release();
			if (comInitialized)
				CoUninitialize();
			app.DebugPrintf("[PlayFabLobby] UPnP AddPortMapping failed (HRESULT=0x%08X)\n", (unsigned)hr);
			return false;
		}

		BSTR extIp = nullptr;
		hr = pMap->get_ExternalIPAddress(&extIp);
		pMap->Release();
		pMap = nullptr;
		if (FAILED(hr) || extIp == nullptr || SysStringLen(extIp) == 0)
		{
			BSTR bTcp2 = SysAllocString(L"TCP");
			if (bTcp2)
			{
				pColl->Remove((long)gamePort, bTcp2);
				SysFreeString(bTcp2);
			}
			pColl->Release();
			if (extIp)
				SysFreeString(extIp);
			if (comInitialized)
				CoUninitialize();
			app.DebugPrintf("[PlayFabLobby] UPnP GetExternalIPAddress failed (HRESULT=0x%08X)\n", (unsigned)hr);
			return false;
		}

		char pubAnsi[256];
		const int nPub = WideCharToMultiByte(CP_UTF8, 0, extIp, -1, pubAnsi, sizeof(pubAnsi) - 1, nullptr, nullptr);
		SysFreeString(extIp);
		if (nPub <= 0 || !LooksLikeIpv4(pubAnsi))
		{
			BSTR bTcp2 = SysAllocString(L"TCP");
			if (bTcp2)
			{
				pColl->Remove((long)gamePort, bTcp2);
				SysFreeString(bTcp2);
			}
			pColl->Release();
			if (comInitialized)
				CoUninitialize();
			app.DebugPrintf("[PlayFabLobby] UPnP external address missing or not IPv4; falling back to LAN announce\n");
			return false;
		}

		strncpy_s(outAnnounceHost, announceHostSize, pubAnsi, _TRUNCATE);
		g_upnpMappedExternalPort.store(gamePort);
		pColl->Release();
		if (comInitialized)
			CoUninitialize();
		app.DebugPrintf("[PlayFabLobby] UPnP mapped public %s:%d (TCP %d -> %s)\n", outAnnounceHost, gamePort, gamePort, lanIpUtf8);
		return true;
	}
#elif MINECRAFT_PLAYFAB_LOBBY_UPNP && defined(_UWP)

	static void UpnpRemoveMappingIfAny()
	{
		UpnpIgdUwp_RemoveMappingIfAny();
	}

	static bool TryUpnpAddTcpMapAndGetPublicIp(int gamePort, const char *lanIpUtf8, char *outAnnounceHost, size_t announceHostSize,
		int *outAnnouncePort)
	{
		return UpnpIgdUwp_AddTcpMappingAndGetExternalIPv4(gamePort, lanIpUtf8, outAnnounceHost, announceHostSize, outAnnouncePort);
	}
#else
	static void UpnpRemoveMappingIfAny() {}
#endif

	std::uint64_t HashLobbySessionId(const std::string &lobbyId)
	{
		std::uint64_t h = 14695981039346656037ULL;
		for (unsigned char c : lobbyId)
		{
			h ^= c;
			h *= 1099511628211ULL;
		}
		return 0xFE00000000000000ULL | (h & 0x00FFFFFFFFFFFFFFULL);
	}

	double JsonNumber(const nlohmann::json &o, const char *key, double def = 0.0)
	{
		if (!o.contains(key))
			return def;
		const auto &v = o[key];
		if (v.is_number_float())
			return v.get<double>();
		if (v.is_number_integer())
			return (double)v.get<long long>();
		if (v.is_number_unsigned())
			return (double)v.get<std::uint64_t>();
		if (v.is_string())
		{
			try
			{
				return std::stod(v.get<std::string>());
			}
			catch (...)
			{
				return def;
			}
		}
		return def;
	}

	std::string JsonString(const nlohmann::json &o, const char *key)
	{
		if (!o.contains(key) || !o[key].is_string())
			return std::string();
		return o[key].get<std::string>();
	}

	// PlayFab Lobby REST responses use camelCase; older code assumed PascalCase.
	const nlohmann::json *JsonObjectAlt(const nlohmann::json &o, const char *pascalKey, const char *camelKey)
	{
		if (o.contains(pascalKey) && o[pascalKey].is_object())
			return &o[pascalKey];
		if (camelKey != nullptr && o.contains(camelKey) && o[camelKey].is_object())
			return &o[camelKey];
		return nullptr;
	}

	std::string LobbyIdFromSummary(const nlohmann::json &lob)
	{
		static const char *keys[] = {"LobbyId", "lobbyId"};
		for (const char *k : keys)
		{
			if (lob.contains(k) && lob[k].is_string())
				return lob[k].get<std::string>();
		}
		return std::string();
	}

	std::string OwnerEntityIdFromSummary(const nlohmann::json &lob)
	{
		const nlohmann::json *owner = JsonObjectAlt(lob, "Owner", "owner");
		if (!owner)
			return std::string();
		static const char *idKeys[] = {"Id", "id"};
		for (const char *k : idKeys)
		{
			if (owner->contains(k) && (*owner)[k].is_string())
				return (*owner)[k].get<std::string>();
		}
		return std::string();
	}

	int IntFromSummary(const nlohmann::json &lob, const char *pascalKey, const char *camelKey, int def = 0)
	{
		for (const char *k : {pascalKey, camelKey})
		{
			if (k == nullptr || !lob.contains(k))
				continue;
			const auto &v = lob[k];
			if (v.is_number_integer())
				return v.get<int>();
			if (v.is_number_unsigned())
				return (int)v.get<unsigned>();
		}
		return def;
	}
}

namespace PlayFabLobbyWin64
{
	bool IsEnabled()
	{
#if MINECRAFT_PLAYFAB_LOBBY_ENABLED
		return MINECRAFT_PLAYFAB_TITLE_ID[0] != '\0';
#else
		return false;
#endif
	}

	static bool PostTitle(const char *path, const std::string &headers, const std::string &body, std::string &raw,
		std::string &err)
	{
		std::wstring whost = Utf8ToWidePath(std::string(MINECRAFT_PLAYFAB_TITLE_ID) + ".playfabapi.com");
		std::wstring wpath = Utf8ToWidePath(path);
		long http = 0;
		if (!HttpsPost(whost, wpath, headers, body, http, raw, err))
			return false;
		if (http < 200 || http >= 300)
		{
			err = "HTTP " + std::to_string(http);
			return false;
		}
		return true;
	}

#if MINECRAFT_PLAYFAB_LOBBY_MUTUAL_FRIENDS_ONLY
	// Mutual friend PlayFabIds — CloudScript LCE_GetFriendsMutualFlags (scripts/playfab-cloudscript-friend-requests.js).
	static bool TryGetMutualFriendPlayFabIds(const std::string &sessionTicket, std::unordered_set<std::string> &outMutual,
		std::string &err)
	{
		outMutual.clear();
		if (sessionTicket.empty())
		{
			err = "no session ticket";
			return false;
		}
		nlohmann::json body;
		body["FunctionName"] = "LCE_GetFriendsMutualFlags";
		body["FunctionParameter"] = nlohmann::json::object();
		const std::string hdr = "X-Authorization: " + sessionTicket + "\r\n";
		std::string raw;
		if (!PostTitle("/Client/ExecuteCloudScript", hdr, body.dump(), raw, err))
			return false;
		nlohmann::json root;
		if (!PlayFabOk(raw, root, err))
			return false;
		if (!root.contains("data") || !root["data"].is_object())
		{
			err = "ExecuteCloudScript missing data";
			return false;
		}
		const nlohmann::json &d = root["data"];
		if (d.contains("Error"))
		{
			err = "CloudScript execution error";
			return false;
		}
		nlohmann::json fr;
		if (d.contains("FunctionResult"))
			fr = d["FunctionResult"];
		else if (d.contains("functionResult"))
			fr = d["functionResult"];
		if (!fr.is_object())
		{
			err = "FunctionResult missing";
			return false;
		}
		const nlohmann::json *mbid = nullptr;
		if (fr.contains("MutualById") && fr["MutualById"].is_object())
			mbid = &fr["MutualById"];
		else if (fr.contains("mutualById") && fr["mutualById"].is_object())
			mbid = &fr["mutualById"];
		if (mbid == nullptr)
		{
			err = "MutualById missing";
			return false;
		}
		for (auto it = mbid->begin(); it != mbid->end(); ++it)
		{
			const std::string k = it.key();
			const auto &v = it.value();
			bool isMutual = false;
			if (v.is_boolean())
				isMutual = v.get<bool>();
			else if (v.is_number())
				isMutual = (v.get<int>() != 0);
			if (isMutual)
				outMutual.insert(k);
		}
		return true;
	}
#endif

	static bool EnsureAuth(std::string &err)
	{
		std::lock_guard<std::mutex> lock(g_mu);
		err.clear();
		g_titleId = MINECRAFT_PLAYFAB_TITLE_ID;

		if (!g_entityToken.empty() && !g_sessionTicket.empty())
			return true;

		PlayerUID uid = Win64Xuid::ResolvePersistentXuid();
		char customId[32];
		sprintf_s(customId, "%016llX", (unsigned long long)uid);

		nlohmann::json loginBody;
		loginBody["TitleId"] = g_titleId;
		loginBody["CustomId"] = customId;
		loginBody["CreateAccount"] = true;

		std::string rawLogin;
		if (!PostTitle("/Client/LoginWithCustomID", "", loginBody.dump(), rawLogin, err))
			return false;

		nlohmann::json lr;
		if (!PlayFabOk(rawLogin, lr, err))
			return false;
		if (!lr.contains("data"))
		{
			err = "Login missing data";
			return false;
		}
		const nlohmann::json &data = lr["data"];
		if (!data.contains("SessionTicket"))
		{
			err = "Login missing SessionTicket";
			return false;
		}
		g_sessionTicket = data["SessionTicket"].get<std::string>();
		if (data.contains("PlayFabId") && data["PlayFabId"].is_string())
			g_myPlayFabId = data["PlayFabId"].get<std::string>();
		else
			g_myPlayFabId.clear();

		nlohmann::json etRoot;
		std::string rawEt;
		std::string authHdr = "X-Authorization: " + g_sessionTicket + "\r\n";
		if (!PostTitle("/Authentication/GetEntityToken", authHdr, "{}", rawEt, err))
			return false;
		if (!PlayFabOk(rawEt, etRoot, err))
			return false;
		if (!etRoot.contains("data"))
		{
			err = "GetEntityToken missing data";
			return false;
		}
		const nlohmann::json &ed = etRoot["data"];
		if (!ed.contains("EntityToken") || !ed.contains("Entity"))
		{
			err = "GetEntityToken missing EntityToken/Entity";
			return false;
		}
		g_entityToken = ed["EntityToken"].get<std::string>();
		const nlohmann::json &ent = ed["Entity"];
		g_entityId = ent["Id"].get<std::string>();
		g_entityType = ent["Type"].get<std::string>();
		return true;
	}

	// FindLobbies (same filter as browse) and LeaveLobby every MC_WIN64 lobby owned by this entity so stale
	// cloud rows from crashes / failed LeaveLobby do not accumulate when hosting again.
	static void LeaveAllOwnedMcWin64LobbiesBeforeCreate()
	{
		std::string myId, myType, tok;
		{
			std::lock_guard<std::mutex> lock(g_mu);
			myId = g_entityId;
			myType = g_entityType;
			tok = g_entityToken;
		}
		if (myId.empty() || tok.empty())
			return;

		char buf[64];
		sprintf_s(buf, "string_key1 eq 'MC_WIN64' and number_key1 eq %d", (int)MINECRAFT_NET_VERSION);

		nlohmann::json pag;
		pag["PageSizeRequested"] = 50;
		nlohmann::json findBody;
		findBody["Filter"] = buf;
		findBody["Pagination"] = pag;

		const std::string hdr = "X-EntityToken: " + tok + "\r\n";
		std::string raw, err;
		if (!PostTitle("/Lobby/FindLobbies", hdr, findBody.dump(), raw, err))
		{
			app.DebugPrintf("[PlayFabLobby] pre-create cleanup FindLobbies failed: %s\n", err.c_str());
			return;
		}

		nlohmann::json resp;
		if (!PlayFabOk(raw, resp, err))
		{
			app.DebugPrintf("[PlayFabLobby] pre-create cleanup FindLobbies bad response: %s\n", err.c_str());
			return;
		}
		if (!resp.contains("data") || !resp["data"].contains("Lobbies"))
			return;
		const nlohmann::json &lobbiesArr = resp["data"]["Lobbies"];
		if (!lobbiesArr.is_array())
			return;

		int leftCount = 0;
		for (const auto &lob : lobbiesArr)
		{
			const std::string oid = OwnerEntityIdFromSummary(lob);
			if (oid != myId)
				continue;
			const std::string lid = LobbyIdFromSummary(lob);
			if (lid.empty())
				continue;

			nlohmann::json lbody;
			lbody["LobbyId"] = lid;
			nlohmann::json mem;
			mem["Id"] = myId;
			mem["Type"] = myType;
			lbody["MemberEntity"] = mem;

			std::string lraw, lerr;
			if (PostTitle("/Lobby/LeaveLobby", hdr, lbody.dump(), lraw, lerr))
			{
				++leftCount;
				app.DebugPrintf("[PlayFabLobby] left owned lobby before create: %s\n", lid.c_str());
			}
			else
				app.DebugPrintf("[PlayFabLobby] pre-create LeaveLobby failed %s: %s\n", lid.c_str(), lerr.c_str());
		}

		if (leftCount > 0)
			app.DebugPrintf("[PlayFabLobby] pre-create cleanup removed %d owned lobby/lobbies\n", leftCount);

		{
			std::lock_guard<std::mutex> lock(g_mu);
			g_activeLobbyId.clear();
		}
	}

	void OnHostStartedAdvertising(bool onlineGame, bool isPrivate, unsigned char publicSlots, int gamePort,
		const wchar_t *hostName, unsigned int gameHostSettings)
	{
		if (!IsEnabled() || !onlineGame || isPrivate || gamePort <= 0)
			return;

		OnHostStoppedAdvertising();

		std::string err;
		if (!EnsureAuth(err))
		{
			PlayFabFileDiag("[PlayFabLobby] auth failed: %s\n", err.c_str());
			return;
		}

		LeaveAllOwnedMcWin64LobbiesBeforeCreate();

		char lanIp[256] = {};
		if (!PickLanIPv4(lanIp, sizeof(lanIp)) || lanIp[0] == 0)
		{
			PlayFabFileDiag("[PlayFabLobby] no LAN IPv4 for announce (adapter pick failed)\n");
			return;
		}

		char announceHost[256] = {};
		int lobbyAnnouncePort = gamePort;
#if MINECRAFT_PLAYFAB_LOBBY_UPNP
		const bool upnpOk = TryUpnpAddTcpMapAndGetPublicIp(gamePort, lanIp, announceHost, sizeof(announceHost), &lobbyAnnouncePort);
#else
		const bool upnpOk = false;
#endif
		if (!upnpOk)
		{
			strncpy_s(announceHost, sizeof(announceHost), lanIp, _TRUNCATE);
			lobbyAnnouncePort = gamePort;
			PlayFabFileDiag("[PlayFabLobby] SearchData will use LAN %s:%d (UPnP unavailable, disabled, or failed)\n", announceHost,
				lobbyAnnouncePort);
		}
		else
		{
			PlayFabFileDiag("[PlayFabLobby] SearchData will use WAN %s:%d (UPnP ok)\n", announceHost, lobbyAnnouncePort);
		}

		std::string hostUtf8;
		{
			std::wstring wn;
			Minecraft *mc = Minecraft::GetInstance();
			if (mc != nullptr && mc->user != nullptr && !mc->user->name.empty())
				wn = mc->user->name;
			if (wn.empty())
				wn = ProfileManager.GetDisplayName(ProfileManager.GetPrimaryPad());
			if (wn.empty() && hostName != nullptr && hostName[0] != 0)
				wn.assign(hostName);
			hostUtf8 = wn.empty() ? std::string() : WideToUtf8(wn.c_str());
		}
		if (hostUtf8.empty())
			hostUtf8 = "Host";
		{
			const char *pfPrefix = MINECRAFT_PLAYFAB_LOBBY_DISPLAY_PREFIX;
			if (pfPrefix != nullptr && pfPrefix[0] != '\0')
			{
				const size_t plen = strlen(pfPrefix);
				const bool alreadyPrefixed =
					(hostUtf8.size() >= plen && std::strncmp(hostUtf8.c_str(), pfPrefix, plen) == 0);
				if (!alreadyPrefixed)
					hostUtf8.insert(0, pfPrefix);
			}
		}

		int maxPl = (int)publicSlots;
		if (maxPl < 2)
			maxPl = 2;
		if (maxPl > 128)
			maxPl = 128;

		std::string ownerId, ownerType, entityTok;
		{
			std::lock_guard<std::mutex> lock(g_mu);
			ownerId = g_entityId;
			ownerType = g_entityType;
			entityTok = g_entityToken;
		}

		nlohmann::json owner;
		owner["Id"] = ownerId;
		owner["Type"] = ownerType;

		nlohmann::json member;
		member["MemberEntity"] = owner;
		member["MemberData"] = nlohmann::json::object();

		nlohmann::json search;
		search["string_key1"] = "MC_WIN64";
		search["number_key1"] = (double)MINECRAFT_NET_VERSION;
		search["string_key2"] = announceHost;
		search["number_key2"] = (double)lobbyAnnouncePort;
		search["string_key3"] = hostUtf8;
		search["number_key3"] = (double)gameHostSettings;
		{
			std::string pfHost;
			{
				std::lock_guard<std::mutex> lock(g_mu);
				pfHost = g_myPlayFabId;
			}
			if (!pfHost.empty())
				search["string_key4"] = std::move(pfHost);
		}

		nlohmann::json body;
		body["MaxPlayers"] = maxPl;
		body["Owner"] = owner;
		body["RestrictInvitesToLobbyOwner"] = false;
		body["UseConnections"] = false;
		body["AccessPolicy"] = "Public";
		body["OwnerMigrationPolicy"] = "None";
		body["SearchData"] = std::move(search);
		body["Members"] = nlohmann::json::array({ member });

		std::string hdr = "X-EntityToken: " + entityTok + "\r\n";
		std::string raw;
		if (!PostTitle("/Lobby/CreateLobby", hdr, body.dump(), raw, err))
		{
			PlayFabFileDiag("[PlayFabLobby] CreateLobby failed: %s\n", err.c_str());
			UpnpRemoveMappingIfAny();
			return;
		}

		nlohmann::json resp;
		if (!PlayFabOk(raw, resp, err))
		{
			PlayFabFileDiag("[PlayFabLobby] CreateLobby bad response: %s\n", err.c_str());
			UpnpRemoveMappingIfAny();
			return;
		}
		if (!resp.contains("data") || !resp["data"].contains("LobbyId"))
		{
			PlayFabFileDiag("[PlayFabLobby] CreateLobby missing LobbyId\n");
			UpnpRemoveMappingIfAny();
			return;
		}
		std::string newLobbyId = resp["data"]["LobbyId"].get<std::string>();
		{
			std::lock_guard<std::mutex> lock(g_mu);
			g_activeLobbyId = std::move(newLobbyId);
		}
		PlayFabFileDiag("[PlayFabLobby] created lobby (announce %s:%d)\n", announceHost, lobbyAnnouncePort);
	}

	void OnHostStoppedAdvertising()
	{
		if (!IsEnabled())
		{
			UpnpRemoveMappingIfAny();
			return;
		}

		std::string lobbyId;
		std::string token;
		std::string eid;
		std::string etype;
		{
			std::lock_guard<std::mutex> lock(g_mu);
			if (!g_activeLobbyId.empty())
			{
				lobbyId = g_activeLobbyId;
				g_activeLobbyId.clear();
			}
			token = g_entityToken;
			eid = g_entityId;
			etype = g_entityType;
		}

		if (!lobbyId.empty())
		{
			nlohmann::json body;
			body["LobbyId"] = lobbyId;
			nlohmann::json mem;
			mem["Id"] = eid;
			mem["Type"] = etype;
			body["MemberEntity"] = mem;

			std::string hdr = "X-EntityToken: " + token + "\r\n";
			std::string err, raw;
			if (!PostTitle("/Lobby/LeaveLobby", hdr, body.dump(), raw, err))
				app.DebugPrintf("[PlayFabLobby] LeaveLobby failed: %s\n", err.c_str());
		}

		UpnpRemoveMappingIfAny();
	}

	void FindJoinableLobbies(std::vector<PlayFabListedGame> &out)
	{
		out.clear();
		if (!IsEnabled())
			return;

		const DWORD now = GetTickCount();

		auto logCacheUse = [&](const char *reason)
		{
			if (g_lastFindLobbiesCacheLogTick != 0 && (now - g_lastFindLobbiesCacheLogTick) < kFindLobbiesCacheLogIntervalMs)
				return;
			g_lastFindLobbiesCacheLogTick = now;
			DWORD nextMs = 0;
			if (g_findLobbiesBackoffUntilTick != 0 && now < g_findLobbiesBackoffUntilTick)
				nextMs = g_findLobbiesBackoffUntilTick - now;
			else if (g_lastFindLobbiesHttpTick != 0)
			{
				DWORD elapsed = now - g_lastFindLobbiesHttpTick;
				if (elapsed < kFindLobbiesMinIntervalMs)
					nextMs = kFindLobbiesMinIntervalMs - elapsed;
			}
			app.DebugPrintf(
				"[PlayFabLobby] FindLobbies %s — using cache (%zu games, next HTTP ~%u ms)\n",
				reason, g_findLobbiesCache.size(), (unsigned)nextMs);
		};

		if (g_findLobbiesBackoffUntilTick != 0 && now < g_findLobbiesBackoffUntilTick)
		{
			out = g_findLobbiesCache;
			logCacheUse("rate-limit backoff");
			return;
		}

		if (g_lastFindLobbiesHttpTick != 0 && (now - g_lastFindLobbiesHttpTick) < kFindLobbiesMinIntervalMs)
		{
			out = g_findLobbiesCache;
			logCacheUse("throttled");
			return;
		}

		std::string err;
		if (!EnsureAuth(err))
		{
			app.DebugPrintf("[PlayFabLobby] FindLobbies auth failed: %s\n", err.c_str());
			out = g_findLobbiesCache;
			return;
		}

		char buf[64];
		sprintf_s(buf, "string_key1 eq 'MC_WIN64' and number_key1 eq %d", (int)MINECRAFT_NET_VERSION);

		nlohmann::json pag;
		pag["PageSizeRequested"] = 25;

		nlohmann::json body;
		body["Filter"] = buf;
		body["Pagination"] = pag;

		std::string hdr;
		{
			std::lock_guard<std::mutex> lock(g_mu);
			hdr = "X-EntityToken: " + g_entityToken + "\r\n";
		}

		g_lastFindLobbiesHttpTick = now;

		std::string raw;
		if (!PostTitle("/Lobby/FindLobbies", hdr, body.dump(), raw, err))
		{
			if (err.find("429") != std::string::npos)
			{
				g_findLobbiesBackoffUntilTick = now + kFindLobbies429BackoffMs;
				app.DebugPrintf(
					"[PlayFabLobby] FindLobbies rate limited (HTTP 429), backing off %u s (using cached list)\n",
					(unsigned)(kFindLobbies429BackoffMs / 1000));
			}
			else
				app.DebugPrintf("[PlayFabLobby] FindLobbies failed: %s\n", err.c_str());
			out = g_findLobbiesCache;
			return;
		}

		nlohmann::json resp;
		if (!PlayFabOk(raw, resp, err))
		{
			app.DebugPrintf("[PlayFabLobby] FindLobbies bad JSON/API: %s\n", err.c_str());
			out = g_findLobbiesCache;
			return;
		}
		if (!resp.contains("data") || !resp["data"].contains("Lobbies"))
		{
			app.DebugPrintf("[PlayFabLobby] FindLobbies response missing Lobbies array\n");
			out = g_findLobbiesCache;
			return;
		}

		std::string myId;
		{
			std::lock_guard<std::mutex> lock(g_mu);
			myId = g_entityId;
		}

		const nlohmann::json &lobbiesArr = resp["data"]["Lobbies"];
		const size_t rawLobbyCount = lobbiesArr.is_array() ? lobbiesArr.size() : 0;
		size_t droppedOwnLobby = 0;
#if MINECRAFT_PLAYFAB_LOBBY_MUTUAL_FRIENDS_ONLY
		std::unordered_set<std::string> mutualFriendPfIds;
		bool mutualFilterActive = false;
		{
			std::string st;
			{
				std::lock_guard<std::mutex> lock(g_mu);
				st = g_sessionTicket;
			}
			std::string mfErr;
			if (!st.empty() && TryGetMutualFriendPlayFabIds(st, mutualFriendPfIds, mfErr))
				mutualFilterActive = true;
			else if (!st.empty())
				app.DebugPrintf("[PlayFabLobby] mutual friends filter inactive (LCE_GetFriendsMutualFlags: %s)\n",
					mfErr.c_str());
		}
#endif

		for (const auto &lob : lobbiesArr)
		{
			{
				std::string oid = OwnerEntityIdFromSummary(lob);
#if !MINECRAFT_PLAYFAB_LOBBY_INCLUDE_OWN_LOBBY
				if (!oid.empty() && oid == myId)
				{
					++droppedOwnLobby;
					continue;
				}
#endif
			}

			const nlohmann::json *sdPtr = JsonObjectAlt(lob, "SearchData", "searchData");
			if (!sdPtr)
				continue;
			const nlohmann::json &sd = *sdPtr;

			std::string ip = JsonString(sd, "string_key2");
			int port = (int)JsonNumber(sd, "number_key2", 0);
			if (ip.empty() || port <= 0)
				continue;

			std::string tag = JsonString(sd, "string_key1");
			if (tag != "MC_WIN64")
				continue;

			int ver = (int)JsonNumber(sd, "number_key1", -1);
			if (ver != (int)MINECRAFT_NET_VERSION)
				continue;

#if MINECRAFT_PLAYFAB_LOBBY_MUTUAL_FRIENDS_ONLY
			if (mutualFilterActive)
			{
				const std::string hostPf = JsonString(sd, "string_key4");
				if (hostPf.empty() || mutualFriendPfIds.find(hostPf) == mutualFriendPfIds.end())
					continue;
			}
#endif

			std::string nameU8 = JsonString(sd, "string_key3");
			if (nameU8.empty())
				nameU8 = "Online game";

			PlayFabListedGame g;
			g.displayName = Utf8ToWide(nameU8);
			g.hostIP = ip;
			g.hostPort = port;
			g.netVersion = (unsigned short)MINECRAFT_NET_VERSION;
			g.gameHostSettings = (unsigned int)JsonNumber(sd, "number_key3", 0);
			{
				int cp = IntFromSummary(lob, "CurrentPlayers", "currentPlayers", -1);
				if (cp >= 0)
					g.playerCount = (unsigned char)cp;
				int mp = IntFromSummary(lob, "MaxPlayers", "maxPlayers", -1);
				if (mp >= 0)
					g.maxPlayers = (unsigned char)mp;
			}
			std::string lid = LobbyIdFromSummary(lob);
			if (lid.empty())
				continue;
			g.sessionId = HashLobbySessionId(lid);
			out.push_back(std::move(g));
		}

		g_findLobbiesCache = out;
		g_findLobbiesBackoffUntilTick = 0;
		g_lastFindLobbiesCacheLogTick = now;
		app.DebugPrintf(
			"[PlayFabLobby] FindLobbies ok (%zu from API, %zu after filter, MINECRAFT_NET_VERSION=%d)\n",
			rawLobbyCount, out.size(), (int)MINECRAFT_NET_VERSION);
		if (rawLobbyCount > 0 && out.empty())
		{
			if (droppedOwnLobby > 0)
			{
				app.DebugPrintf(
					"[PlayFabLobby] hint: skipped %zu lobby/lobbies as owned by this player (same PlayFab entity as "
					"host — e.g. same uid.dat). Use another PC/account, or set "
					"MINECRAFT_PLAYFAB_LOBBY_INCLUDE_OWN_LOBBY to 1 in PlayFabConfig.h for local testing only.\n",
					droppedOwnLobby);
			}
			else
			{
				app.DebugPrintf(
					"[PlayFabLobby] hint: lobbies returned but none passed filter (SearchData ip/port/tag/version, "
					"or missing LobbyId)\n");
			}
		}
	}
}

#endif
