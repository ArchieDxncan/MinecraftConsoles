#include "stdafx.h"
#include "OfflineAuthModule.h"

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
