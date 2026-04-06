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
