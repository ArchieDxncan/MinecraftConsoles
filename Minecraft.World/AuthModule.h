#pragma once
using namespace std;

#include <string>
#include <vector>
#include <utility>

inline string narrowStr(const wstring &w)
{
	return string(w.begin(), w.end());
}

class AuthModule
{
public:
	virtual ~AuthModule() = default;

	virtual const wchar_t *schemeName() = 0;
	virtual vector<wstring> supportedVariations() = 0;
	virtual vector<pair<wstring, wstring>> getSettings(const wstring &variation) = 0;
	virtual bool onAuthData(const vector<pair<wstring, wstring>> &fields, wstring &outUid, wstring &outUsername) = 0;

	bool validate(const wstring &uid, const wstring &username);

protected:
	bool extractIdentity(const vector<pair<wstring, wstring>> &fields, wstring &outUid, wstring &outUsername);
};
