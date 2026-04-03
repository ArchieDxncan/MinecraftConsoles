#include "stdafx.h"
#include "AuthModule.h"
#include "HttpClient.h"
#include "StringHelpers.h"
#include "Common/vendor/nlohmann/json.hpp"
#include <random>

static string narrowStr(const wstring &w)
{
	return string(w.begin(), w.end());
}

static wstring generateServerId()
{
	static constexpr wchar_t hex[] = L"0123456789abcdef";
	static std::mt19937 rng(std::random_device{}());
	wstring id(16, L'0');
	for (auto &c : id) c = hex[rng() & 0xF];
	return id;
}

bool AuthModule::validate(const wstring &uid, const wstring &username)
{
	return !uid.empty() && !username.empty() && username.length() <= 16;
}

bool AuthModule::extractIdentity(const vector<pair<wstring, wstring>> &fields, wstring &outUid, wstring &outUsername)
{
	for (const auto &[key, value] : fields)
	{
		if (key == L"uid") outUid = value;
		else if (key == L"username") outUsername = value;
	}
	return validate(outUid, outUsername);
}

SessionAuthModule::SessionAuthModule()
{
	endpoints[L"mojang"] = {L"https://authserver.mojang.com", L"https://sessionserver.mojang.com"};
	endpoints[L"elyby"] = {L"https://authserver.ely.by", L"https://authserver.ely.by"};
}

const wchar_t *SessionAuthModule::schemeName() { return L"mcconsoles:session"; }

vector<wstring> SessionAuthModule::supportedVariations()
{
	return {L"mojang", L"elyby"};
}

vector<pair<wstring, wstring>> SessionAuthModule::getSettings(const wstring &variation)
{
	auto it = endpoints.find(variation);
	if (it == endpoints.end()) return {};

	activeSessionEndpoint = it->second.sessionEndpoint;
	activeServerId = generateServerId();

	return {
		{L"authEndpoint", it->second.authEndpoint},
		{L"sessionEndpoint", it->second.sessionEndpoint},
		{L"serverId", activeServerId}
	};
}

bool SessionAuthModule::onAuthData(const vector<pair<wstring, wstring>> &fields, wstring &outUid, wstring &outUsername)
{
	wstring username;
	for (const auto &[k, v] : fields)
	{
		if (k == L"username") username = v;
	}

	if (username.empty() || activeServerId.empty() || activeSessionEndpoint.empty())
		return false;

	string url = narrowStr(activeSessionEndpoint)
		+ "/session/minecraft/hasJoined?username=" + narrowStr(username)
		+ "&serverId=" + narrowStr(activeServerId);

	auto response = HttpClient::get(url);
	if (response.statusCode != 200)
		return false;

	auto json = nlohmann::json::parse(response.body, nullptr, false);
	if (json.is_discarded())
		return false;

	string id = json.value("id", "");
	string name = json.value("name", "");

	if (id.empty() || name.empty())
		return false;

	outUid = convStringToWstring(id);
	outUsername = convStringToWstring(name);

	return validate(outUid, outUsername);
}

const wchar_t *KeypairOfflineAuthModule::schemeName() { return L"mcconsoles:keypair_offline"; }

vector<wstring> KeypairOfflineAuthModule::supportedVariations()
{
	return {L"rsa2048", L"ed25519"};
}

vector<pair<wstring, wstring>> KeypairOfflineAuthModule::getSettings(const wstring &variation)
{
	return {{L"key_type", variation}};
}

bool KeypairOfflineAuthModule::onAuthData(const vector<pair<wstring, wstring>> &fields, wstring &outUid, wstring &outUsername)
{
	return extractIdentity(fields, outUid, outUsername);
}

const wchar_t *OfflineAuthModule::schemeName() { return L"mcconsoles:offline"; }

vector<wstring> OfflineAuthModule::supportedVariations()
{
	return {};
}

vector<pair<wstring, wstring>> OfflineAuthModule::getSettings(const wstring &variation)
{
	return {};
}

bool OfflineAuthModule::onAuthData(const vector<pair<wstring, wstring>> &fields, wstring &outUid, wstring &outUsername)
{
	return extractIdentity(fields, outUid, outUsername);
}
