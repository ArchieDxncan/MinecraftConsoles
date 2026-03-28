#include "stdafx.h"
#include "AuthModule.h"

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

ElyByAuthModule::ElyByAuthModule(const wstring &endpoint)
	: endpoint(endpoint)
{
}

const wchar_t *ElyByAuthModule::schemeName() { return L"mcconsoles:elyby"; }

vector<wstring> ElyByAuthModule::supportedVariations()
{
	return {L"java"};
}

vector<pair<wstring, wstring>> ElyByAuthModule::getSettings(const wstring &variation)
{
	return {{L"endpoint", endpoint}};
}

bool ElyByAuthModule::onAuthData(const vector<pair<wstring, wstring>> &fields, wstring &outUid, wstring &outUsername)
{
	return extractIdentity(fields, outUid, outUsername);
}

MojangAuthModule::MojangAuthModule()
	: ElyByAuthModule(L"https://sessionserver.mojang.com")
{
}

const wchar_t *MojangAuthModule::schemeName() { return L"mcconsoles:mojang"; }

vector<wstring> MojangAuthModule::supportedVariations()
{
	return {L"java", L"bedrock"};
}

// --- KeypairOfflineAuthModule ---

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
