#pragma once
using namespace std;

#include "AuthModule.h"
#include <unordered_map>

class SessionAuthModule : public AuthModule
{
public:
	struct EndpointPair {
		wstring authEndpoint;
		wstring sessionEndpoint;
	};

private:
	unordered_map<wstring, EndpointPair> endpoints;
	wstring activeSessionEndpoint;
	wstring activeServerId;

public:
	SessionAuthModule();

	const wchar_t *schemeName() override;
	vector<wstring> supportedVariations() override;
	vector<pair<wstring, wstring>> getSettings(const wstring &variation) override;
	bool onAuthData(const vector<pair<wstring, wstring>> &fields, wstring &outUid, wstring &outUsername) override;
};
