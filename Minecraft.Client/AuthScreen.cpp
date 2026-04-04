#include "stdafx.h"
#include "AuthScreen.h"
#include "Minecraft.h"
#include "User.h"
#include "..\Minecraft.World\AuthModule.h"
#include "..\Minecraft.World\HttpClient.h"
#include "..\Minecraft.World\StringHelpers.h"
#include "Common/vendor/nlohmann/json.hpp"
#include <fstream>
#include <shellapi.h>

using json = nlohmann::json;
static constexpr auto PROFILES_FILE = L"auth_profiles.dat";
static constexpr auto MS_CLIENT_ID = "00000000441cc96b";

vector<AuthProfile> AuthProfileManager::profiles;
int AuthProfileManager::selectedProfile = -1;

void AuthProfileManager::load()
{
	profiles.clear();
	std::ifstream file(PROFILES_FILE, std::ios::binary);
	if (!file) return;

	uint32_t count = 0;
	file.read(reinterpret_cast<char *>(&count), sizeof(count));

	for (uint32_t i = 0; i < count && file.good(); i++)
	{
		AuthProfile p;
		uint8_t type;
		file.read(reinterpret_cast<char *>(&type), sizeof(type));
		p.type = static_cast<AuthProfile::Type>(type);

		auto readWstr = [&file]() -> wstring {
			uint16_t len = 0;
			file.read(reinterpret_cast<char *>(&len), sizeof(len));
			if (!file || len > 4096) return {};
			wstring s(len, L'\0');
			file.read(reinterpret_cast<char *>(s.data()), len * sizeof(wchar_t));
			if (!file) return {};
			return s;
		};

		p.uid = readWstr();
		p.username = readWstr();
		p.token = readWstr();
		profiles.push_back(std::move(p));
	}

	int32_t savedIdx = 0;
	file.read(reinterpret_cast<char *>(&savedIdx), sizeof(savedIdx));
	if (!profiles.empty())
		selectedProfile = (savedIdx >= 0 && savedIdx < static_cast<int>(profiles.size())) ? savedIdx : 0;
}

void AuthProfileManager::save()
{
	std::ofstream file(PROFILES_FILE, std::ios::binary | std::ios::trunc);
	if (!file) return;

	uint32_t count = static_cast<uint32_t>(profiles.size());
	file.write(reinterpret_cast<const char *>(&count), sizeof(count));

	auto writeWstr = [&file](const wstring &s) {
		uint16_t len = static_cast<uint16_t>(s.length());
		file.write(reinterpret_cast<const char *>(&len), sizeof(len));
		file.write(reinterpret_cast<const char *>(s.data()), len * sizeof(wchar_t));
	};

	for (const auto &p : profiles)
	{
		uint8_t type = static_cast<uint8_t>(p.type);
		file.write(reinterpret_cast<const char *>(&type), sizeof(type));
		writeWstr(p.uid);
		writeWstr(p.username);
		writeWstr(p.token);
	}

	int32_t idx = static_cast<int32_t>(selectedProfile);
	file.write(reinterpret_cast<const char *>(&idx), sizeof(idx));
}

void AuthProfileManager::addProfile(AuthProfile::Type type, const wstring &username, const wstring &uid, const wstring &token)
{
	wstring finalUid = uid.empty() ? L"offline_" + username : uid;
	profiles.push_back({type, finalUid, username, token});
	selectedProfile = static_cast<int>(profiles.size()) - 1;
	save();
}

void AuthProfileManager::removeSelectedProfile()
{
	if (selectedProfile < 0 || selectedProfile >= static_cast<int>(profiles.size()))
		return;

	profiles.erase(profiles.begin() + selectedProfile);
	if (selectedProfile >= static_cast<int>(profiles.size()))
		selectedProfile = static_cast<int>(profiles.size()) - 1;
	save();
}

bool AuthProfileManager::applySelectedProfile()
{
	if (selectedProfile < 0 || selectedProfile >= static_cast<int>(profiles.size()))
		return false;

	const auto &p = profiles[selectedProfile];
	auto *mc = Minecraft::GetInstance();

	if (mc->user)
		delete mc->user;

	mc->user = new User(p.username, p.token);

	// push auth name into the platform globals so ProfileManager.GetGamertag() picks it up
	// instead of returning the default "Player"
	extern char g_Win64Username[17];
	extern wchar_t g_Win64UsernameW[17];
	string narrow = narrowStr(p.username);
	strncpy_s(g_Win64Username, sizeof(g_Win64Username), narrow.c_str(), _TRUNCATE);
	wcsncpy_s(g_Win64UsernameW, 17, p.username.c_str(), _TRUNCATE);

	return true;
}

// --- AuthFlow ---

std::thread AuthFlow::workerThread;
std::atomic<AuthFlowState> AuthFlow::state{AuthFlowState::IDLE};
std::atomic<bool> AuthFlow::cancelRequested{false};
AuthResult AuthFlow::result;
wstring AuthFlow::userCode;
wstring AuthFlow::verificationUri;

void AuthFlow::reset()
{
	cancelRequested = true;
	if (workerThread.joinable())
		workerThread.detach();
	state = AuthFlowState::IDLE;
	result = {};
	userCode.clear();
	verificationUri.clear();
	cancelRequested = false;
}

void AuthFlow::startMicrosoft()
{
	reset();
	state = AuthFlowState::WAITING_FOR_USER;
	workerThread = std::thread(microsoftFlowThread);
}

void AuthFlow::startElyBy(const wstring &username, const wstring &password)
{
	reset();
	state = AuthFlowState::EXCHANGING;
	workerThread = std::thread(elybyFlowThread, narrowStr(username), narrowStr(password));
}

static void authFail(AuthResult &result, std::atomic<AuthFlowState> &state, const wchar_t *msg)
{
	result = {false, {}, {}, {}, msg};
	state = AuthFlowState::FAILED;
}

// parse json response body, return discarded json on bad status
static json parseResponse(const HttpResponse &resp, int expectedStatus = 200)
{
	if (resp.statusCode != expectedStatus) return json::value_t::discarded;
	return json::parse(resp.body, nullptr, false);
}

void AuthFlow::microsoftFlowThread()
{
	auto dcResp = HttpClient::post(
		"https://login.live.com/oauth20_connect.srf",
		"client_id=" + string(MS_CLIENT_ID) + "&scope=service::user.auth.xboxlive.com::MBI_SSL&response_type=device_code",
		"application/x-www-form-urlencoded"
	);

	auto dcJson = parseResponse(dcResp);
	if (dcJson.is_discarded())
	{
		authFail(result, state, L"Failed to get device code");
		return;
	}

	string deviceCode = dcJson.value("device_code", "");
	string uCode = dcJson.value("user_code", "");
	string vUri = dcJson.value("verification_uri", "");
	int interval = dcJson.value("interval", 5);

	if (deviceCode.empty() || uCode.empty())
	{
		authFail(result, state, L"Missing device code fields");
		return;
	}

	userCode = convStringToWstring(uCode);
	verificationUri = convStringToWstring(vUri);

	// copy code to clipboard so the user can just paste it
	if (OpenClipboard(nullptr))
	{
		EmptyClipboard();
		size_t bytes = (uCode.size() + 1) * sizeof(char);
		HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, bytes);
		if (hMem)
		{
			memcpy(GlobalLock(hMem), uCode.c_str(), bytes);
			GlobalUnlock(hMem);
			SetClipboardData(CF_TEXT, hMem);
		}
		CloseClipboard();
	}

	if (!vUri.empty())
		ShellExecuteW(nullptr, L"open", verificationUri.c_str(), nullptr, nullptr, SW_SHOWNORMAL);

	state = AuthFlowState::POLLING;
	string msAccessToken;

	for (int attempt = 0; attempt < 180; attempt++)
	{
		for (int ms = 0; ms < interval * 1000; ms += 250)
		{
			if (cancelRequested) return;
			Sleep(250);
		}

		auto pollResp = HttpClient::post(
			"https://login.live.com/oauth20_token.srf",
			"client_id=" + string(MS_CLIENT_ID) + "&device_code=" + deviceCode + "&grant_type=urn:ietf:params:oauth:grant-type:device_code",
			"application/x-www-form-urlencoded"
		);

		auto pollJson = json::parse(pollResp.body, nullptr, false);
		if (pollJson.is_discarded()) continue;

		if (pollResp.statusCode == 200)
		{
			msAccessToken = pollJson.value("access_token", "");
			if (!msAccessToken.empty()) break;
		}

		string err = pollJson.value("error", "");
		if (err == "authorization_pending") continue;
		if (err == "slow_down") { interval += 5; continue; }
		if (!err.empty())
		{
			result = {false, {}, {}, {}, convStringToWstring("Auth error: " + err)};
			state = AuthFlowState::FAILED;
			return;
		}
	}

	if (msAccessToken.empty())
	{
		authFail(result, state, L"Timed out waiting for login");
		return;
	}

	state = AuthFlowState::EXCHANGING;
	if (cancelRequested) return;

	// xbox live auth
	auto xblResp = HttpClient::post("https://user.auth.xboxlive.com/user/authenticate", json({
		{"Properties", {{"AuthMethod", "RPS"}, {"SiteName", "user.auth.xboxlive.com"}, {"RpsTicket", msAccessToken}}},
		{"RelyingParty", "http://auth.xboxlive.com"},
		{"TokenType", "JWT"}
	}).dump());

	auto xblJson = parseResponse(xblResp);
	if (xblJson.is_discarded())
	{
		authFail(result, state, L"Xbox Live auth failed");
		return;
	}

	string xblToken = xblJson.value("Token", "");
	string userHash;
	try { userHash = xblJson["DisplayClaims"]["xui"][0].value("uhs", ""); } catch (...) {}

	if (xblToken.empty() || userHash.empty())
	{
		authFail(result, state, L"Bad Xbox Live response");
		return;
	}

	// xsts auth
	auto xstsResp = HttpClient::post("https://xsts.auth.xboxlive.com/xsts/authorize", json({
		{"Properties", {{"SandboxId", "RETAIL"}, {"UserTokens", {xblToken}}}},
		{"RelyingParty", "rp://api.minecraftservices.com/"},
		{"TokenType", "JWT"}
	}).dump());

	auto xstsJson = parseResponse(xstsResp);
	string xstsToken = xstsJson.is_discarded() ? "" : xstsJson.value("Token", "");

	if (xstsToken.empty())
	{
		authFail(result, state, L"XSTS auth failed");
		return;
	}

	// minecraft login
	auto mcResp = HttpClient::post("https://api.minecraftservices.com/authentication/login_with_xbox",
		json({{"identityToken", "XBL3.0 x=" + userHash + ";" + xstsToken}}).dump());

	auto mcJson = parseResponse(mcResp);
	string mcAccessToken = mcJson.is_discarded() ? "" : mcJson.value("access_token", "");

	if (mcAccessToken.empty())
	{
		authFail(result, state, L"Minecraft auth failed");
		return;
	}

	// get profile
	auto profResp = HttpClient::get("https://api.minecraftservices.com/minecraft/profile",
		{"Authorization: Bearer " + mcAccessToken});

	auto profJson = parseResponse(profResp);
	if (profJson.is_discarded())
	{
		authFail(result, state, L"Failed to get Minecraft profile");
		return;
	}

	string profId = profJson.value("id", "");
	string profName = profJson.value("name", "");

	if (profId.empty() || profName.empty())
	{
		authFail(result, state, L"Profile missing id or name");
		return;
	}

	result = {true, convStringToWstring(profName), convStringToWstring(profId), convStringToWstring(mcAccessToken), {}};
	state = AuthFlowState::COMPLETE;
}

void AuthFlow::elybyFlowThread(const string &username, const string &password)
{
	auto resp = HttpClient::post("https://authserver.ely.by/auth/authenticate", json({
		{"username", username},
		{"password", password},
		{"clientToken", "mcconsoles"},
		{"agent", {{"name", "Minecraft"}, {"version", 1}}}
	}).dump());

	auto respJson = json::parse(resp.body, nullptr, false);

	if (resp.statusCode != 200 || respJson.is_discarded())
	{
		string msg = "Ely.by auth failed";
		if (!respJson.is_discarded()) msg = respJson.value("errorMessage", msg);
		result = {false, {}, {}, {}, convStringToWstring(msg)};
		state = AuthFlowState::FAILED;
		return;
	}

	string accessToken = respJson.value("accessToken", "");
	string uuid, name;
	try { uuid = respJson["selectedProfile"].value("id", ""); name = respJson["selectedProfile"].value("name", ""); } catch (...) {}

	if (accessToken.empty() || uuid.empty() || name.empty())
	{
		authFail(result, state, L"Ely.by response missing profile");
		return;
	}

	result = {true, convStringToWstring(name), convStringToWstring(uuid), convStringToWstring(accessToken), {}};
	state = AuthFlowState::COMPLETE;
}
