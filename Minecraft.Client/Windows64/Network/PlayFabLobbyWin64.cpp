#include "stdafx.h"

#ifdef _WINDOWS64

#include "PlayFabLobbyWin64.h"
#include "../Leaderboards/PlayFabConfig.h"
#include "../Windows64_Xuid.h"

#include "Common/Consoles_App.h"
#include "Common/Network/PlatformNetworkManagerInterface.h"

#include <nlohmann/json.hpp>

#include <iphlpapi.h>
#include <ipifcons.h>
#include <cstring>
#include <mutex>
#include <vector>
#include <winhttp.h>

#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "iphlpapi.lib")

extern char g_LocalStatePath[512];

namespace
{
	std::mutex g_mu;
	std::string g_titleId;
	std::string g_sessionTicket;
	std::string g_entityToken;
	std::string g_entityId;
	std::string g_entityType;
	std::string g_activeLobbyId;

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

	bool ReadJoinHostOverride(char *out, size_t outSize)
	{
		out[0] = 0;
		if (g_LocalStatePath[0] == '\0')
			return false;
		char path[MAX_PATH];
		if (strcpy_s(path, g_LocalStatePath) != 0)
			return false;
		size_t n = strlen(path);
		if (n > 0 && path[n - 1] != '\\' && path[n - 1] != '/')
		{
			if (strcat_s(path, "\\") != 0)
				return false;
		}
		if (strcat_s(path, "playfab_join_host.txt") != 0)
			return false;
		FILE *f = nullptr;
		if (fopen_s(&f, path, "r") != 0 || !f)
			return false;
		char line[128] = {};
		if (!fgets(line, sizeof(line), f))
		{
			fclose(f);
			return false;
		}
		fclose(f);
		char *s = line;
		while (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n')
			++s;
		char *e = s + strlen(s);
		while (e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\r' || e[-1] == '\n'))
			--e;
		*e = 0;
		if (s[0] == 0)
			return false;
		strncpy_s(out, outSize, s, _TRUNCATE);
		return out[0] != 0;
	}

	// Uses GetAdaptersInfo (IPv4 string) so we do not need winsock2.h before windows.h (PCH order).
	bool PickLanIPv4(char *out, size_t outSize)
	{
		out[0] = 0;
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

		for (PIP_ADAPTER_INFO a = info; a != nullptr; a = a->Next)
		{
			if (a->Type == MIB_IF_TYPE_LOOPBACK)
				continue;
			const char *ip = a->IpAddressList.IpAddress.String;
			if (ip == nullptr || ip[0] == 0 || strcmp(ip, "0.0.0.0") == 0)
				continue;
			if (strncmp(ip, "127.", 4) == 0)
				continue;
			if (strncmp(ip, "169.254.", 8) == 0)
				continue;
			strncpy_s(out, outSize, ip, _TRUNCATE);
			return out[0] != 0;
		}
		return false;
	}

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
		return def;
	}

	std::string JsonString(const nlohmann::json &o, const char *key)
	{
		if (!o.contains(key) || !o[key].is_string())
			return std::string();
		return o[key].get<std::string>();
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

	void OnHostStartedAdvertising(bool onlineGame, bool isPrivate, unsigned char publicSlots, int gamePort,
		const wchar_t *hostName, unsigned int gameHostSettings)
	{
		if (!IsEnabled() || !onlineGame || isPrivate || gamePort <= 0)
			return;

		OnHostStoppedAdvertising();

		std::string err;
		if (!EnsureAuth(err))
		{
			app.DebugPrintf("[PlayFabLobby] auth failed: %s\n", err.c_str());
			return;
		}

		char announceIp[64] = {};
		if (!ReadJoinHostOverride(announceIp, sizeof(announceIp)))
			PickLanIPv4(announceIp, sizeof(announceIp));
		if (announceIp[0] == 0)
		{
			app.DebugPrintf("[PlayFabLobby] no announce IP (set LocalState\\playfab_join_host.txt for WAN)\n");
			return;
		}

		std::string hostUtf8 = WideToUtf8(hostName);
		if (hostUtf8.empty())
			hostUtf8 = "Host";

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
		search["string_key2"] = announceIp;
		search["number_key2"] = (double)gamePort;
		search["string_key3"] = hostUtf8;
		search["number_key3"] = (double)gameHostSettings;

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
			app.DebugPrintf("[PlayFabLobby] CreateLobby failed: %s\n", err.c_str());
			return;
		}

		nlohmann::json resp;
		if (!PlayFabOk(raw, resp, err))
		{
			app.DebugPrintf("[PlayFabLobby] CreateLobby bad response: %s\n", err.c_str());
			return;
		}
		if (!resp.contains("data") || !resp["data"].contains("LobbyId"))
		{
			app.DebugPrintf("[PlayFabLobby] CreateLobby missing LobbyId\n");
			return;
		}
		std::string newLobbyId = resp["data"]["LobbyId"].get<std::string>();
		{
			std::lock_guard<std::mutex> lock(g_mu);
			g_activeLobbyId = std::move(newLobbyId);
		}
		app.DebugPrintf("[PlayFabLobby] created lobby (announce %s:%d)\n", announceIp, gamePort);
	}

	void OnHostStoppedAdvertising()
	{
		if (!IsEnabled())
			return;

		std::string lobbyId;
		std::string token;
		std::string eid;
		std::string etype;
		{
			std::lock_guard<std::mutex> lock(g_mu);
			if (g_activeLobbyId.empty())
				return;
			lobbyId = g_activeLobbyId;
			g_activeLobbyId.clear();
			token = g_entityToken;
			eid = g_entityId;
			etype = g_entityType;
		}

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

	void FindJoinableLobbies(std::vector<PlayFabListedGame> &out)
	{
		out.clear();
		if (!IsEnabled())
			return;

		std::string err;
		if (!EnsureAuth(err))
			return;

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

		std::string raw;
		if (!PostTitle("/Lobby/FindLobbies", hdr, body.dump(), raw, err))
		{
			app.DebugPrintf("[PlayFabLobby] FindLobbies failed: %s\n", err.c_str());
			return;
		}

		nlohmann::json resp;
		if (!PlayFabOk(raw, resp, err))
			return;
		if (!resp.contains("data") || !resp["data"].contains("Lobbies"))
			return;

		std::string myId;
		{
			std::lock_guard<std::mutex> lock(g_mu);
			myId = g_entityId;
		}

		for (const auto &lob : resp["data"]["Lobbies"])
		{
			if (lob.contains("Owner") && lob["Owner"].is_object())
			{
				std::string oid = JsonString(lob["Owner"], "Id");
				if (!oid.empty() && oid == myId)
					continue;
			}

			if (!lob.contains("SearchData") || !lob["SearchData"].is_object())
				continue;
			const nlohmann::json &sd = lob["SearchData"];

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

			std::string nameU8 = JsonString(sd, "string_key3");
			if (nameU8.empty())
				nameU8 = "Online game";

			PlayFabListedGame g;
			g.displayName = Utf8ToWide(nameU8);
			g.hostIP = ip;
			g.hostPort = port;
			g.netVersion = (unsigned short)MINECRAFT_NET_VERSION;
			g.gameHostSettings = (unsigned int)JsonNumber(sd, "number_key3", 0);
			if (lob.contains("CurrentPlayers") && lob["CurrentPlayers"].is_number_integer())
				g.playerCount = (unsigned char)lob["CurrentPlayers"].get<int>();
			if (lob.contains("MaxPlayers") && lob["MaxPlayers"].is_number_integer())
				g.maxPlayers = (unsigned char)lob["MaxPlayers"].get<int>();
			std::string lid = JsonString(lob, "LobbyId");
			if (lid.empty())
				continue;
			g.sessionId = HashLobbySessionId(lid);
			out.push_back(std::move(g));
		}
	}
}

#endif
