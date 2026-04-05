#pragma once
using namespace std;

#include "AuthModule.h"

class KeypairOfflineAuthModule : public AuthModule
{
public:
	const wchar_t *schemeName() override;
	vector<wstring> supportedVariations() override;
	vector<pair<wstring, wstring>> getSettings(const wstring &variation) override;
	bool onAuthData(const vector<pair<wstring, wstring>> &fields, wstring &outUid, wstring &outUsername) override;
};

class OfflineAuthModule : public AuthModule
{
public:
	const wchar_t *schemeName() override;
	vector<wstring> supportedVariations() override;
	vector<pair<wstring, wstring>> getSettings(const wstring &variation) override;
	bool onAuthData(const vector<pair<wstring, wstring>> &fields, wstring &outUid, wstring &outUsername) override;
};
