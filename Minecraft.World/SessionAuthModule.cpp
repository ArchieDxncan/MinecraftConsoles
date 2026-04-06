#include "stdafx.h"
#include "SessionAuthModule.h"
#include "HttpClient.h"
#include "StringHelpers.h"
#include "Common/vendor/nlohmann/json.hpp"
#include <random>

static wstring generateServerId()
{
	static constexpr wchar_t hex[] = L"0123456789abcdef";
	static std::mt19937 rng(std::random_device{}());
	wstring id(16, L'0');
	for (auto &c : id) c = hex[rng() & 0xF];
	return id;
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
