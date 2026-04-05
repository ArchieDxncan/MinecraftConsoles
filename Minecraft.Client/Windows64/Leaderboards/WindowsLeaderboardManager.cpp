#include "stdafx.h"

#ifdef _WINDOWS64

#include "WindowsLeaderboardManager.h"
#include "PlayFabConfig.h"
#include "../Windows64_Xuid.h"

#include "../Minecraft.World/StringHelpers.h"
#include "Common/Consoles_App.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <climits>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <windows.h>
#include <winhttp.h>
#include <cwctype>

#pragma comment(lib, "winhttp.lib")

namespace
{
	std::string WStringToUtf8(const std::wstring &w)
	{
		if (w.empty())
			return std::string();
		int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), nullptr, 0, nullptr, nullptr);
		if (n <= 0)
			return std::string();
		std::string u8((size_t)n, '\0');
		WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), &u8[0], n, nullptr, nullptr);
		return u8;
	}

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

	std::string WideToUtf8(const std::wstring &w)
	{
		if (w.empty())
			return std::string();
		int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), nullptr, 0, nullptr, nullptr);
		if (n <= 0)
			return std::string();
		std::string u8((size_t)n, '\0');
		WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), &u8[0], n, nullptr, nullptr);
		return u8;
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

		std::wstring hdr = Utf8ToWide("Content-Type: application/json\r\n" + extraHeadersUtf8);
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

	PlayerUID HashPlayFabIdToXuid(const std::string &playFabId)
	{
		uint64_t h = 14695981039346656037ULL;
		for (size_t i = 0; i < playFabId.size(); ++i)
		{
			h ^= (uint64_t)(unsigned char)playFabId[i];
			h *= 1099511628211ULL;
		}
		h |= 0x8000000000000000ULL;
		return (PlayerUID)h;
	}

	void AppendStat(nlohmann::json &arr, const char *name, int value)
	{
		nlohmann::json o;
		o["StatisticName"] = name;
		o["Value"] = value;
		arr.push_back(o);
	}

	std::string BoardPrefix(LeaderboardManager::EStatsType type, int difficulty)
	{
		const char *cat = "Misc";
		switch (type)
		{
		case LeaderboardManager::eStatsType_Travelling: cat = "Travel"; break;
		case LeaderboardManager::eStatsType_Mining: cat = "Mine"; break;
		case LeaderboardManager::eStatsType_Farming: cat = "Farm"; break;
		case LeaderboardManager::eStatsType_Kills: cat = "Kills"; break;
		default: break;
		}
		char buf[64];
		sprintf_s(buf, "MC_%s_D%d", cat, difficulty);
		return buf;
	}

	std::string SumStatName(LeaderboardManager::EStatsType type, int difficulty)
	{
		return BoardPrefix(type, difficulty) + "_Sum";
	}

	void BuildUpdateStatistics(const LeaderboardManager::RegisterScore &rs, nlohmann::json &stats)
	{
		const int d = rs.m_difficulty;
		const LeaderboardManager::EStatsType t = rs.m_commentData.m_statsType;
		const std::string p = BoardPrefix(t, d);

		AppendStat(stats, SumStatName(t, d).c_str(), rs.m_score);

		switch (t)
		{
		case LeaderboardManager::eStatsType_Kills:
			{
				const LeaderboardManager::KillsRecord &k = rs.m_commentData.m_kills;
				AppendStat(stats, (p + "_C0").c_str(), (int)k.m_zombie);
				AppendStat(stats, (p + "_C1").c_str(), (int)k.m_skeleton);
				AppendStat(stats, (p + "_C2").c_str(), (int)k.m_creeper);
				AppendStat(stats, (p + "_C3").c_str(), (int)k.m_spider);
				AppendStat(stats, (p + "_C4").c_str(), (int)k.m_spiderJockey);
				AppendStat(stats, (p + "_C5").c_str(), (int)k.m_zombiePigman);
				AppendStat(stats, (p + "_C6").c_str(), (int)k.m_slime);
			}
			break;
		case LeaderboardManager::eStatsType_Mining:
			{
				const LeaderboardManager::MiningRecord &m = rs.m_commentData.m_mining;
				AppendStat(stats, (p + "_C0").c_str(), (int)m.m_dirt);
				AppendStat(stats, (p + "_C1").c_str(), (int)m.m_stone);
				AppendStat(stats, (p + "_C2").c_str(), (int)m.m_sand);
				AppendStat(stats, (p + "_C3").c_str(), (int)m.m_cobblestone);
				AppendStat(stats, (p + "_C4").c_str(), (int)m.m_gravel);
				AppendStat(stats, (p + "_C5").c_str(), (int)m.m_clay);
				AppendStat(stats, (p + "_C6").c_str(), (int)m.m_obsidian);
			}
			break;
		case LeaderboardManager::eStatsType_Farming:
			{
				const LeaderboardManager::FarmingRecord &f = rs.m_commentData.m_farming;
				AppendStat(stats, (p + "_C0").c_str(), (int)f.m_eggs);
				AppendStat(stats, (p + "_C1").c_str(), (int)f.m_wheat);
				AppendStat(stats, (p + "_C2").c_str(), (int)f.m_mushroom);
				AppendStat(stats, (p + "_C3").c_str(), (int)f.m_sugarcane);
				AppendStat(stats, (p + "_C4").c_str(), (int)f.m_milk);
				AppendStat(stats, (p + "_C5").c_str(), (int)f.m_pumpkin);
			}
			break;
		case LeaderboardManager::eStatsType_Travelling:
			{
				const LeaderboardManager::TravellingRecord &tr = rs.m_commentData.m_travelling;
				AppendStat(stats, (p + "_C0").c_str(), (int)tr.m_walked);
				AppendStat(stats, (p + "_C1").c_str(), (int)tr.m_fallen);
				AppendStat(stats, (p + "_C2").c_str(), (int)tr.m_minecart);
				AppendStat(stats, (p + "_C3").c_str(), (int)tr.m_boat);
			}
			break;
		default:
			break;
		}
	}

	// PlayFab stores mining columns in write order (C0=dirt,C1=stone,…); UI/PS3 rows use dirt,cobble,sand,stone,…
	unsigned int StatSuffixIndexForUiColumn(LeaderboardManager::EStatsType stype, unsigned int col)
	{
		if (stype == LeaderboardManager::eStatsType_Mining && col < 7u)
		{
			static const unsigned kOrd[7] = { 0u, 3u, 2u, 1u, 4u, 5u, 6u };
			return kOrd[col];
		}
		return col;
	}

	bool ParsePlayerProfileStatisticsMap(const std::string &rawJson, std::unordered_map<std::string, long long> &out)
	{
		out.clear();
		nlohmann::json resp;
		try
		{
			resp = nlohmann::json::parse(rawJson);
		}
		catch (...)
		{
			return false;
		}
		if (resp.contains("code") && resp["code"].is_number() && resp["code"].get<int>() != 200)
			return false;
		if (!resp.contains("data") || !resp["data"].is_object())
			return false;
		const nlohmann::json &data = resp["data"];
		if (!data.contains("PlayerProfile") || !data["PlayerProfile"].is_object())
			return false;
		const nlohmann::json &prof = data["PlayerProfile"];
		if (!prof.contains("Statistics") || !prof["Statistics"].is_array())
			return true;
		for (const auto &s : prof["Statistics"])
		{
			std::string name = s.value("Name", s.value("StatisticName", std::string()));
			if (name.empty())
				continue;
			long long val = 0;
			if (s.contains("Value") && s["Value"].is_number_integer())
				val = s["Value"].get<long long>();
			else if (s.contains("Value") && s["Value"].is_number_unsigned())
				val = (long long)s["Value"].get<uint64_t>();
			else if (s.contains("Value") && s["Value"].is_number_float())
				val = (long long)s["Value"].get<double>();
			out[std::move(name)] = val;
		}
		return true;
	}

	void MergeUpdateStatsWithServerMax(nlohmann::json &stats,
		const std::unordered_map<std::string, long long> &server)
	{
		for (auto &el : stats)
		{
			if (!el.is_object())
				continue;
			const std::string name = el.value("StatisticName", el.value("Name", std::string()));
			if (name.empty())
				continue;
			const auto sit = server.find(name);
			if (sit == server.end())
				continue;
			long long submitted = 0;
			if (el.contains("Value") && el["Value"].is_number_integer())
				submitted = el["Value"].get<long long>();
			else if (el.contains("Value") && el["Value"].is_number_unsigned())
				submitted = (long long)el["Value"].get<uint64_t>();
			else if (el.contains("Value") && el["Value"].is_number_float())
				submitted = (long long)el["Value"].get<double>();
			long long merged = (std::max)(submitted, sit->second);
			if (merged > INT_MAX)
				merged = INT_MAX;
			if (merged < INT_MIN)
				merged = INT_MIN;
			el["Value"] = (int)merged;
		}
	}

	bool ParseGetPlayerStatisticsMap(const std::string &rawJson, std::unordered_map<std::string, long long> &out)
	{
		out.clear();
		nlohmann::json resp;
		try
		{
			resp = nlohmann::json::parse(rawJson);
		}
		catch (...)
		{
			return false;
		}
		if (resp.contains("code") && resp["code"].is_number() && resp["code"].get<int>() != 200)
			return false;
		if (!resp.contains("data") || !resp["data"].is_object())
			return false;
		const nlohmann::json &data = resp["data"];
		if (!data.contains("Statistics") || !data["Statistics"].is_array())
			return true;
		for (const auto &s : data["Statistics"])
		{
			std::string name = s.value("StatisticName", s.value("Name", std::string()));
			if (name.empty())
				continue;
			long long val = 0;
			if (s.contains("Value") && s["Value"].is_number_integer())
				val = s["Value"].get<long long>();
			else if (s.contains("Value") && s["Value"].is_number_unsigned())
				val = (long long)s["Value"].get<uint64_t>();
			else if (s.contains("Value") && s["Value"].is_number_float())
				val = (long long)s["Value"].get<double>();
			out[std::move(name)] = val;
		}
		return true;
	}

	void FillReadScoreColumnsFromMap(LeaderboardManager::ReadScore &rs, LeaderboardManager::EStatsType stype,
		const std::string &columnPrefix, unsigned int cols, const std::unordered_map<std::string, long long> &smap)
	{
		for (unsigned int c = 0; c < cols && c < LeaderboardManager::ReadScore::STATSDATA_MAX; ++c)
		{
			const unsigned int sfx = StatSuffixIndexForUiColumn(stype, c);
			const std::string key = columnPrefix + "_C" + std::to_string(sfx);
			auto it = smap.find(key);
			long long v = (it != smap.end()) ? it->second : 0LL;
			if (v < 0)
				v = 0;
			rs.m_statsData[c] = (unsigned long)v;
		}
	}

	// ExecuteCloudScript → FunctionResult from scripts/playfab-cloudscript-leaderboard-columns.js
	bool ParseCloudScriptLbColumnsResponse(const std::string &rawJson,
		std::unordered_map<std::string, std::unordered_map<std::string, long long>> &byPlayer)
	{
		byPlayer.clear();
		nlohmann::json resp;
		try
		{
			resp = nlohmann::json::parse(rawJson);
		}
		catch (...)
		{
			return false;
		}
		if (resp.contains("code") && resp["code"].is_number() && resp["code"].get<int>() != 200)
			return false;
		if (!resp.contains("data") || !resp["data"].is_object())
			return false;
		const nlohmann::json &data = resp["data"];
		if (!data.contains("FunctionResult") || !data["FunctionResult"].is_object())
			return false;
		const nlohmann::json &fr = data["FunctionResult"];
		if (!fr.contains("statsByPlayer") || !fr["statsByPlayer"].is_object())
			return false;
		const nlohmann::json &sbp = fr["statsByPlayer"];
		for (const auto &pidEl : sbp.items())
		{
			const std::string pid = pidEl.key();
			const nlohmann::json &vals = pidEl.value();
			if (!vals.is_object())
				continue;
			std::unordered_map<std::string, long long> m;
			for (const auto &stEl : vals.items())
			{
				long long v = 0;
				if (stEl.value().is_number_integer())
					v = stEl.value().get<long long>();
				else if (stEl.value().is_number_unsigned())
					v = (long long)stEl.value().get<uint64_t>();
				else if (stEl.value().is_number_float())
					v = (long long)stEl.value().get<double>();
				m[stEl.key()] = v;
			}
			byPlayer[pid] = std::move(m);
		}
		return true;
	}
}

unsigned int WindowsLeaderboardManager::ColumnCountForType(EStatsType type)
{
	switch (type)
	{
	case eStatsType_Farming: return 6;
	case eStatsType_Mining: return 7;
	case eStatsType_Kills: return 7;
	case eStatsType_Travelling: return 4;
	default: return 1;
	}
}

WindowsLeaderboardManager::WindowsLeaderboardManager()
	: m_openSessions(0)
	, m_workerBusy(false)
	, m_cancelRequested(false)
	, m_hasDelivery(false)
	, m_deliveryReturn(eStatsReturn_Success)
	, m_deliveryScores(nullptr)
	, m_deliveryNumScores(0)
	, m_deliveryListener(nullptr)
{
	m_titleId = MINECRAFT_PLAYFAB_TITLE_ID;
	m_cloudScriptLbColumnsFn = MINECRAFT_PLAYFAB_CLOUDSCRIPT_LB_COLUMNS;
}

WindowsLeaderboardManager::~WindowsLeaderboardManager()
{
	m_cancelRequested = true;
	if (m_workerThread.joinable())
		m_workerThread.join();
	DiscardPendingReadDelivery();
}

void WindowsLeaderboardManager::Tick()
{
	DeliverCompletedWork();
}

bool WindowsLeaderboardManager::OpenSession()
{
	if (m_openSessions == 0)
		app.DebugPrintf("[WindowsLeaderboardManager] OpenSession (PlayFab %s)\n",
			PlayFabEnabled() ? "enabled" : "disabled - set MINECRAFT_PLAYFAB_TITLE_ID");
	++m_openSessions;
	return true;
}

void WindowsLeaderboardManager::CloseSession()
{
	if (m_openSessions > 0)
		--m_openSessions;
}

void WindowsLeaderboardManager::DeleteSession()
{
}

bool WindowsLeaderboardManager::EnsureLoggedIn(std::string &err)
{
	err.clear();
	if (!PlayFabEnabled())
	{
		err = "No PlayFab title id";
		return false;
	}

	{
		std::lock_guard<std::mutex> lock(m_authMutex);
		if (!m_sessionTicket.empty())
			return true;
	}

	PlayerUID uid = Win64Xuid::ResolvePersistentXuid();
	char customId[32];
	sprintf_s(customId, "%016llX", (unsigned long long)uid);

	nlohmann::json body;
	body["TitleId"] = m_titleId;
	body["CustomId"] = customId;
	body["CreateAccount"] = true;

	std::string raw;
	if (!PostPlayFab("/Client/LoginWithCustomID", body.dump(), raw, err))
		return false;

	nlohmann::json resp;
	try
	{
		resp = nlohmann::json::parse(raw);
	}
	catch (...)
	{
		err = "Login response JSON parse failed";
		return false;
	}

	if (!resp.contains("data"))
	{
		err = "Login missing data";
		return false;
	}

	const nlohmann::json &data = resp["data"];
	if (!data.contains("SessionTicket") || !data.contains("PlayFabId"))
	{
		err = "Login missing SessionTicket/PlayFabId";
		return false;
	}

	{
		std::lock_guard<std::mutex> lock(m_authMutex);
		m_sessionTicket = data["SessionTicket"].get<std::string>();
		m_playFabId = data["PlayFabId"].get<std::string>();
	}

	// Outside m_authMutex: PostPlayFab re-locks for the session ticket header.
	TrySyncTitleDisplayName();
	return true;
}

void WindowsLeaderboardManager::TrySyncTitleDisplayName()
{
	std::wstring w = ProfileManager.GetDisplayName(ProfileManager.GetPrimaryPad());
	while (!w.empty() && std::iswspace(static_cast<wint_t>(w[0])))
		w.erase(0, 1);
	while (!w.empty() && std::iswspace(static_cast<wint_t>(w[w.size() - 1])))
		w.erase(w.size() - 1, 1);
	if (w.empty())
		w = L"Player";
	while (w.size() < 3)
		w += L'_';
	if (w.size() > 25)
		w.resize(25);

	std::string u8 = WStringToUtf8(w);

	auto postDisplayName = [&](const std::string &disp, std::string &outErr) -> bool
	{
		nlohmann::json b;
		b["DisplayName"] = disp;
		std::string raw;
		return PostPlayFab("/Client/UpdateUserTitleDisplayName", b.dump(), raw, outErr);
	};

	std::string err;
	if (postDisplayName(u8, err))
		return;

	// NameNotAvailable (1058) or collision: append PlayFabId suffix, stay within 25 chars.
	std::string suffix = m_playFabId;
	if (suffix.size() > 6)
		suffix = suffix.substr(suffix.size() - 6);
	std::string alt = u8;
	const size_t maxLen = 25;
	const size_t need = suffix.size() + 1;
	if (alt.size() + need > maxLen)
	{
		if (maxLen > need)
			alt.resize(maxLen - need);
		else
			alt.clear();
	}
	alt += '_';
	alt += suffix;
	if (alt.size() > maxLen)
		alt.resize(maxLen);

	if (!postDisplayName(alt, err))
		app.DebugPrintf("[WindowsLeaderboardManager] UpdateUserTitleDisplayName failed: %s\n", err.c_str());
}

bool WindowsLeaderboardManager::PostPlayFab(const char *path, const std::string &jsonBody, std::string &outResponseUtf8, std::string &err)
{
	outResponseUtf8.clear();
	std::string pathStr(path);
	std::wstring wpath = Utf8ToWide(pathStr);
	std::wstring whost = Utf8ToWide(m_titleId + ".playfabapi.com");

	std::string headers;
	{
		std::lock_guard<std::mutex> lock(m_authMutex);
		if (!m_sessionTicket.empty())
			headers += "X-Authorization: " + m_sessionTicket + "\r\n";
	}

	long httpStatus = 0;
	std::string raw;
	if (!HttpsPost(whost, wpath, headers, jsonBody, httpStatus, raw, err))
		return false;

	nlohmann::json outResponse;
	try
	{
		outResponse = nlohmann::json::parse(raw);
	}
	catch (...)
	{
		err = "Invalid JSON from PlayFab";
		return false;
	}

	int code = outResponse.value("code", 200);
	if (code != 200)
	{
		err = outResponse.value("errorMessage", outResponse.value("error", std::string("PlayFab error")));
		return false;
	}

	if (httpStatus < 200 || httpStatus >= 300)
	{
		err = "HTTP " + std::to_string(httpStatus);
		return false;
	}

	outResponseUtf8 = std::move(raw);
	return true;
}

bool WindowsLeaderboardManager::ExecuteCloudScript(const char *functionName, const std::string &functionParameterJson,
	std::string &outFunctionResultJson, std::string &err)
{
	outFunctionResultJson.clear();
	err.clear();
	if (!PlayFabEnabled() || !functionName || !functionName[0])
	{
		err = "PlayFab or function name not set";
		return false;
	}
	if (!EnsureLoggedIn(err))
		return false;

	nlohmann::json body;
	body["FunctionName"] = functionName;
	try
	{
		if (functionParameterJson.empty())
			body["FunctionParameter"] = nlohmann::json::object();
		else
			body["FunctionParameter"] = nlohmann::json::parse(functionParameterJson);
	}
	catch (...)
	{
		err = "Invalid FunctionParameter JSON";
		return false;
	}

	std::string raw;
	if (!PostPlayFab("/Client/ExecuteCloudScript", body.dump(), raw, err))
		return false;

	nlohmann::json root;
	try
	{
		root = nlohmann::json::parse(raw);
	}
	catch (...)
	{
		err = "ExecuteCloudScript response parse failed";
		return false;
	}

	if (!root.contains("data") || !root["data"].is_object())
	{
		err = "ExecuteCloudScript missing data";
		return false;
	}

	const nlohmann::json &d = root["data"];
	if (d.contains("Error"))
	{
		const nlohmann::json &er = d["Error"];
		if (er.is_object())
			err = er.value("Message", er.value("message", std::string("CloudScript error")));
		else
			err = "CloudScript execution error";
		return false;
	}

	if (d.contains("FunctionResult"))
		outFunctionResultJson = d["FunctionResult"].dump();
	else if (d.contains("functionResult"))
		outFunctionResultJson = d["functionResult"].dump();
	else
	{
		err = "FunctionResult missing";
		return false;
	}

	return true;
}

bool WindowsLeaderboardManager::WriteStats(unsigned int viewCount, ViewIn views)
{
	if (!PlayFabEnabled() || viewCount == 0 || views == nullptr)
		return true;

	std::vector<RegisterScore> copy;
	copy.assign(views, views + viewCount);
	StartWriteJob(std::move(copy));
	return true;
}

void WindowsLeaderboardManager::DiscardPendingReadDelivery()
{
	std::lock_guard<std::mutex> lock(m_deliveryMutex);
	if (!m_hasDelivery)
		return;
	m_hasDelivery = false;
	delete[] m_deliveryScores;
	m_deliveryScores = nullptr;
	m_deliveryListener = nullptr;
	m_deliveryNumScores = 0;
}

void WindowsLeaderboardManager::FlushStats()
{
	// WriteStats posts to a worker thread; wait so save/quit does not exit before PlayFab POST completes.
	for (;;)
	{
		if (m_workerThread.joinable())
			m_workerThread.join();

		// Read jobs capture LeaderboardReadListener* at start; if the UI scene was destroyed (e.g. user left
		// leaderboards then exited multiplayer), finish() may have queued a delivery for a freed object. Joining
		// alone does not consume it — Tick would still call OnStatsReadComplete. Drop it after the worker stops.
		DiscardPendingReadDelivery();

		std::vector<RegisterScore> pending;
		{
			std::lock_guard<std::mutex> lock(m_queueMutex);
			pending.swap(m_pendingWrites);
		}
		if (pending.empty())
			break;
		StartWriteJob(std::move(pending));
	}
}

void WindowsLeaderboardManager::CancelOperation()
{
	m_cancelRequested = true;
	m_readListener = nullptr;
	if (m_workerThread.joinable())
		m_workerThread.join();
	DiscardPendingReadDelivery();
}

bool WindowsLeaderboardManager::isIdle()
{
	return !m_workerBusy.load();
}

void WindowsLeaderboardManager::DeliverCompletedWork()
{
	LeaderboardReadListener *listener = nullptr;
	ReadScore *scores = nullptr;
	unsigned int n = 0;
	eStatsReturn ret = eStatsReturn_Success;
	{
		std::lock_guard<std::mutex> lock(m_deliveryMutex);
		if (!m_hasDelivery)
			return;
		m_hasDelivery = false;
		listener = m_deliveryListener;
		scores = m_deliveryScores;
		n = m_deliveryNumScores;
		ret = m_deliveryReturn;
		m_deliveryListener = nullptr;
		m_deliveryScores = nullptr;
		m_deliveryNumScores = 0;
	}

	if (!listener)
	{
		delete[] scores;
		return;
	}

	ReadView view;
	view.m_numQueries = n;
	view.m_queries = scores;

	if (app.DebugSettingsOn())
		LeaderboardManager::printStats(view);

	bool unused = listener->OnStatsReadComplete(ret, (int)n, view);
	(void)unused;

	delete[] scores;
	m_readListener = nullptr;
	zeroReadParameters();
}

void WindowsLeaderboardManager::StartReadJob(EFilterMode filter)
{
	if (m_workerBusy.exchange(true))
		return;

	LeaderboardReadListener *cb = m_readListener;
	const int difficulty = m_difficulty;
	const EStatsType stype = m_statsType;
	const PlayerUID myUid = m_myXUID;
	const unsigned int startIndex = m_startIndex;
	const unsigned int readCount = m_readCount;
	const std::string titleId = m_titleId;
	const std::string cloudFn = m_cloudScriptLbColumnsFn;

	if (m_workerThread.joinable())
		m_workerThread.join();

	m_cancelRequested = false;

	m_workerThread = std::thread([this, filter, cb, difficulty, stype, myUid, startIndex, readCount, titleId, cloudFn]()
	{
		std::string err;
		eStatsReturn outRet = eStatsReturn_NetworkError;
		ReadScore *outScores = nullptr;
		unsigned int outN = 0;

		auto finish = [&]()
		{
			std::lock_guard<std::mutex> lock(m_deliveryMutex);
			m_deliveryListener = cb;
			m_deliveryScores = outScores;
			m_deliveryNumScores = outN;
			m_deliveryReturn = outRet;
			m_hasDelivery = true;
			outScores = nullptr;
			m_workerBusy = false;
		};

		if (titleId.empty())
		{
			outRet = eStatsReturn_NoResults;
			finish();
			return;
		}

		if (!EnsureLoggedIn(err))
		{
			app.DebugPrintf("[WindowsLeaderboardManager] Login failed: %s\n", err.c_str());
			outRet = eStatsReturn_NetworkError;
			finish();
			return;
		}

		const std::string sumName = SumStatName(stype, difficulty);
		nlohmann::json body;
		body["StatisticName"] = sumName;
		body["MaxResults"] = (int)readCount;

		const char *apiPath = "/Client/GetLeaderboard";
		if (filter == eFM_MyScore)
		{
			apiPath = "/Client/GetLeaderboardAroundPlayer";
		}
		else if (filter == eFM_Friends)
		{
			apiPath = "/Client/GetFriendLeaderboard";
		}
		else
		{
			// UIScene passes 1-based rank/start (first page = 1). PlayFab StartPosition is 0-based.
			const int startPos = (int)startIndex > 0 ? (int)startIndex - 1 : 0;
			body["StartPosition"] = startPos;
		}

		std::string rawLb;
		if (!PostPlayFab(apiPath, body.dump(), rawLb, err))
		{
			app.DebugPrintf("[WindowsLeaderboardManager] %s failed: %s\n", apiPath, err.c_str());
			outRet = eStatsReturn_NetworkError;
			finish();
			return;
		}

		nlohmann::json resp;
		try
		{
			resp = nlohmann::json::parse(rawLb);
		}
		catch (...)
		{
			outRet = eStatsReturn_NetworkError;
			finish();
			return;
		}

		const nlohmann::json *leaderboard = nullptr;
		if (resp.contains("data") && resp["data"].contains("Leaderboard"))
			leaderboard = &resp["data"]["Leaderboard"];

		if (!leaderboard || !leaderboard->is_array())
		{
			outRet = eStatsReturn_NoResults;
			finish();
			return;
		}

		const unsigned int cols = ColumnCountForType(stype);
		outN = (unsigned int)leaderboard->size();
		if (outN == 0)
		{
			outRet = eStatsReturn_NoResults;
			finish();
			return;
		}

		outScores = new ReadScore[outN];
		ZeroMemory(outScores, sizeof(ReadScore) * outN);

		const std::string columnPrefix = BoardPrefix(stype, difficulty);
		std::vector<std::string> pfIds(outN);
		std::string myPfId;
		{
			std::lock_guard<std::mutex> lock(m_authMutex);
			myPfId = m_playFabId;
		}

		for (unsigned int i = 0; i < outN; ++i)
		{
			const nlohmann::json &row = (*leaderboard)[(size_t)i];
			std::string pfId = row.value("PlayFabId", std::string());
			pfIds[i] = pfId;
			std::string disp = row.value("DisplayName", std::string());
			if (disp.empty())
				disp = pfId;

			long long statVal = 0;
			if (row.contains("StatValue") && row["StatValue"].is_number())
				statVal = row["StatValue"].get<long long>();
			else if (row.contains("Value") && row["Value"].is_number())
				statVal = row["Value"].get<long long>();

			int pos = row.value("Position", (int)i);

			outScores[i].m_uid = HashPlayFabIdToXuid(pfId);
			outScores[i].m_rank = (unsigned long)(pos + 1);
			outScores[i].m_name = convStringToWstring(disp);
			outScores[i].m_totalScore = (unsigned long)(statVal < 0 ? 0 : statVal);
			outScores[i].m_statsSize = (unsigned short)cols;
			for (unsigned int c = 0; c < cols && c < ReadScore::STATSDATA_MAX; ++c)
				outScores[i].m_statsData[c] = 0;
			outScores[i].m_idsErrorMessage = 0;
			outScores[i].m_isLocalPlayer = (!myPfId.empty() && !pfId.empty() && pfId == myPfId);
		}

		// Match later legacy My Score UI: only the local player's row (before column fetches for other players).
		if (filter == eFM_MyScore)
		{
			unsigned int idx = outN;
			for (unsigned int i = 0; i < outN; ++i)
			{
				if (outScores[i].m_isLocalPlayer)
				{
					idx = i;
					break;
				}
			}
			if (idx >= outN)
			{
				delete[] outScores;
				outScores = nullptr;
				outN = 0;
				outRet = eStatsReturn_NoResults;
				finish();
				return;
			}
			const std::string keepPf = pfIds[idx];
			ReadScore keepSc = outScores[idx];
			delete[] outScores;
			outScores = new ReadScore[1];
			outScores[0] = keepSc;
			pfIds.clear();
			pfIds.push_back(keepPf);
			outN = 1;
		}

		// GetLeaderboard only returns _Sum. Column breakdowns come from MC_*_C0.. stats.
		// Local player: Client/GetPlayerStatistics (no profile constraint). Others: Client/GetPlayerProfile
		// needs Title settings → Client Profile Options → allow statistics on profiles (ShowStatistics).
		if (cols > 0)
		{
			bool needLocalColumns = false;
			for (unsigned int i = 0; i < outN; ++i)
			{
				if (!pfIds[i].empty() && pfIds[i] == myPfId)
				{
					needLocalColumns = true;
					break;
				}
			}

			std::unordered_map<std::string, long long> localStatMap;
			std::string localErr;
			if (needLocalColumns && !myPfId.empty())
			{
				nlohmann::json nameArr = nlohmann::json::array();
				for (unsigned int c = 0; c < cols; ++c)
				{
					const unsigned int sfx = StatSuffixIndexForUiColumn(stype, c);
					nameArr.push_back(columnPrefix + "_C" + std::to_string(sfx));
				}
				nlohmann::json stBody;
				stBody["StatisticNames"] = std::move(nameArr);
				std::string rawSt;
				if (PostPlayFab("/Client/GetPlayerStatistics", stBody.dump(), rawSt, localErr))
					ParseGetPlayerStatisticsMap(rawSt, localStatMap);
				else
					app.DebugPrintf("[WindowsLeaderboardManager] GetPlayerStatistics (local columns): %s\n",
						localErr.c_str());
			}

			std::unordered_map<std::string, std::unordered_map<std::string, long long>> cloudByPlayer;
			std::string cloudErr;
			if (!cloudFn.empty())
			{
				std::unordered_set<std::string> seenRemote;
				nlohmann::json idArr = nlohmann::json::array();
				for (unsigned int i = 0; i < outN; ++i)
				{
					if (pfIds[i].empty() || pfIds[i] == myPfId)
						continue;
					if (seenRemote.insert(pfIds[i]).second)
						idArr.push_back(pfIds[i]);
				}
				if (!idArr.empty())
				{
					nlohmann::json nameArr = nlohmann::json::array();
					for (unsigned int c = 0; c < cols; ++c)
					{
						const unsigned int sfx = StatSuffixIndexForUiColumn(stype, c);
						nameArr.push_back(columnPrefix + "_C" + std::to_string(sfx));
					}
					nlohmann::json fparam;
					fparam["playFabIds"] = std::move(idArr);
					fparam["statisticNames"] = std::move(nameArr);
					nlohmann::json cbody;
					cbody["FunctionName"] = cloudFn;
					cbody["FunctionParameter"] = std::move(fparam);
					std::string rawCloud;
					if (PostPlayFab("/Client/ExecuteCloudScript", cbody.dump(), rawCloud, cloudErr))
					{
						if (!ParseCloudScriptLbColumnsResponse(rawCloud, cloudByPlayer))
							app.DebugPrintf(
								"[WindowsLeaderboardManager] ExecuteCloudScript %s: unexpected FunctionResult shape\n",
								cloudFn.c_str());
					}
					else
						app.DebugPrintf("[WindowsLeaderboardManager] ExecuteCloudScript %s: %s\n", cloudFn.c_str(),
							cloudErr.c_str());
				}
			}

			std::string profErr;
			static bool s_loggedProfileConstraintHint;
			for (unsigned int i = 0; i < outN; ++i)
			{
				if (pfIds[i].empty())
					continue;

				if (!localStatMap.empty() && pfIds[i] == myPfId)
				{
					FillReadScoreColumnsFromMap(outScores[i], stype, columnPrefix, cols, localStatMap);
					continue;
				}

				const auto cloudIt = cloudByPlayer.find(pfIds[i]);
				if (cloudIt != cloudByPlayer.end())
				{
					FillReadScoreColumnsFromMap(outScores[i], stype, columnPrefix, cols, cloudIt->second);
					continue;
				}

				nlohmann::json profBody;
				profBody["PlayFabId"] = pfIds[i];
				nlohmann::json constraints = nlohmann::json::object();
				constraints["ShowStatistics"] = true;
				profBody["ProfileConstraints"] = std::move(constraints);
				std::string rawProf;
				if (!PostPlayFab("/Client/GetPlayerProfile", profBody.dump(), rawProf, profErr))
				{
					if (profErr.find("constraint") != std::string::npos)
					{
						if (!s_loggedProfileConstraintHint)
						{
							s_loggedProfileConstraintHint = true;
							app.DebugPrintf(
								"[WindowsLeaderboardManager] Other players' column stats need ShowStatistics on profiles "
								"(Title settings → Client Profile Options) or deploy CloudScript "
								"(scripts/playfab-cloudscript-leaderboard-columns.js) and set "
								"MINECRAFT_PLAYFAB_CLOUDSCRIPT_LB_COLUMNS to the function name. "
								"Local player uses GetPlayerStatistics.\n");
						}
					}
					else
						app.DebugPrintf("[WindowsLeaderboardManager] GetPlayerProfile %s: %s\n", pfIds[i].c_str(),
							profErr.c_str());
					continue;
				}
				std::unordered_map<std::string, long long> smap;
				if (!ParsePlayerProfileStatisticsMap(rawProf, smap))
					continue;
				FillReadScoreColumnsFromMap(outScores[i], stype, columnPrefix, cols, smap);
			}
		}

		// Leaderboard StatValue is MC_*_Sum. If only _Sum was set in Game Manager (no C0–C3), columns stay 0 — mirror total into walked.
		if (stype == eStatsType_Travelling && cols >= 1u)
		{
			for (unsigned int i = 0; i < outN; ++i)
			{
				unsigned long colSum = 0;
				for (unsigned int c = 0; c < cols && c < ReadScore::STATSDATA_MAX; ++c)
					colSum += outScores[i].m_statsData[c];
				if (colSum == 0 && outScores[i].m_totalScore > 0)
					outScores[i].m_statsData[0] = outScores[i].m_totalScore;
			}
		}

		outRet = eStatsReturn_Success;
		finish();
	});
}

void WindowsLeaderboardManager::StartWriteJob(std::vector<RegisterScore> scores)
{
	if (!PlayFabEnabled() || scores.empty())
		return;

	if (m_workerBusy.exchange(true))
	{
		std::lock_guard<std::mutex> lock(m_queueMutex);
		m_pendingWrites.insert(m_pendingWrites.end(), scores.begin(), scores.end());
		return;
	}

	if (m_workerThread.joinable())
		m_workerThread.join();

	m_cancelRequested = false;

	m_workerThread = std::thread([this, scores = std::move(scores), titleId = m_titleId]() mutable
	{
		std::string err;
		auto done = [&]()
		{
			m_workerBusy = false;
		};

		auto sendBatch = [&](std::vector<RegisterScore> &batch) -> bool
		{
			if (titleId.empty())
				return false;
			if (!EnsureLoggedIn(err))
			{
				app.DebugPrintf("[WindowsLeaderboardManager] Write login failed: %s\n", err.c_str());
				return false;
			}
			nlohmann::json stats = nlohmann::json::array();
			for (const RegisterScore &rs : batch)
				BuildUpdateStatistics(rs, stats);

#if MINECRAFT_PLAYFAB_MERGE_STATS_WITH_SERVER
			{
				nlohmann::json nameArr = nlohmann::json::array();
				std::unordered_set<std::string> distinct;
				for (const auto &el : stats)
				{
					if (!el.is_object())
						continue;
					const std::string n = el.value("StatisticName", std::string());
					if (n.empty())
						continue;
					if (distinct.insert(n).second)
						nameArr.push_back(n);
				}
				if (!nameArr.empty())
				{
					nlohmann::json getBody;
					getBody["StatisticNames"] = std::move(nameArr);
					std::string rawGet;
					std::unordered_map<std::string, long long> serverMap;
					if (PostPlayFab("/Client/GetPlayerStatistics", getBody.dump(), rawGet, err)
						&& ParseGetPlayerStatisticsMap(rawGet, serverMap))
						MergeUpdateStatsWithServerMax(stats, serverMap);
				}
			}
#endif

			nlohmann::json body;
			body["Statistics"] = stats;
			std::string rawUp;
			if (!PostPlayFab("/Client/UpdatePlayerStatistics", body.dump(), rawUp, err))
			{
				app.DebugPrintf("[WindowsLeaderboardManager] UpdatePlayerStatistics failed: %s\n", err.c_str());
				return false;
			}
			app.DebugPrintf("[WindowsLeaderboardManager] UpdatePlayerStatistics ok (%zu stats)\n", stats.size());
			return true;
		};

		sendBatch(scores);

		for (;;)
		{
			std::vector<RegisterScore> more;
			{
				std::lock_guard<std::mutex> lock(m_queueMutex);
				more.swap(m_pendingWrites);
			}
			if (more.empty())
				break;
			sendBatch(more);
		}

		done();
	});
}

bool WindowsLeaderboardManager::ReadStats_Friends(LeaderboardReadListener *callback, int difficulty, EStatsType type, PlayerUID myUID, unsigned int startIndex, unsigned int readCount)
{
	if (!isIdle())
		return false;
	if (!LeaderboardManager::ReadStats_Friends(callback, difficulty, type, myUID, startIndex, readCount))
		return false;
	StartReadJob(eFM_Friends);
	return true;
}

bool WindowsLeaderboardManager::ReadStats_MyScore(LeaderboardReadListener *callback, int difficulty, EStatsType type, PlayerUID myUID, unsigned int readCount)
{
	if (!isIdle())
		return false;
	if (!LeaderboardManager::ReadStats_MyScore(callback, difficulty, type, myUID, readCount))
		return false;
	StartReadJob(eFM_MyScore);
	return true;
}

bool WindowsLeaderboardManager::ReadStats_TopRank(LeaderboardReadListener *callback, int difficulty, EStatsType type, unsigned int startIndex, unsigned int readCount)
{
	if (!isIdle())
		return false;
	if (!LeaderboardManager::ReadStats_TopRank(callback, difficulty, type, startIndex, readCount))
		return false;
	StartReadJob(eFM_TopRank);
	return true;
}

LeaderboardManager *LeaderboardManager::m_instance = new WindowsLeaderboardManager();

#endif // _WINDOWS64
