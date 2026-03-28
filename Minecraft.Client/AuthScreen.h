#pragma once
using namespace std;

struct AuthProfile
{
	enum Type : uint8_t { MICROSOFT, ELYBY, OFFLINE };

	Type type;
	wstring uid;
	wstring username;
	wstring token;
};

class AuthProfileManager
{
private:
	static vector<AuthProfile> profiles;
	static int selectedProfile;

public:
	static void load();
	static void save();
	static void addProfile(AuthProfile::Type type, const wstring &username);
	static void removeSelectedProfile();
	static bool applySelectedProfile();

	static const vector<AuthProfile> &getProfiles() { return profiles; }
	static int getSelectedIndex() { return selectedProfile; }
	static void setSelectedIndex(int idx) { selectedProfile = idx; }
};
