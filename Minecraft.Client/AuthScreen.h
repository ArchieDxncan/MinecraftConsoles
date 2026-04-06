#pragma once
using namespace std;
#include <atomic>
#include <thread>

struct AuthProfile
{
	enum Type : uint8_t { MICROSOFT, ELYBY, OFFLINE };

	Type type;
	wstring uid;
	wstring username;
	wstring token;
	wstring clientToken;
};

class AuthProfileManager
{
private:
	static vector<AuthProfile> profiles;
	static int selectedProfile;

public:
	static void load();
	static void save();
	static void addProfile(AuthProfile::Type type, const wstring &username, const wstring &uid = L"", const wstring &token = L"", const wstring &clientToken = L"");
	static void removeSelectedProfile();
	static bool applySelectedProfile();

	static const vector<AuthProfile> &getProfiles() { return profiles; }
	static int getSelectedIndex() { return selectedProfile; }
	static void setSelectedIndex(int idx) { selectedProfile = idx; }
};
struct AuthResult
{
	bool success;
	wstring username;
	wstring uuid;
	wstring accessToken;
	wstring clientToken;
	wstring error;
};

enum class AuthFlowState : uint8_t
{
	IDLE,
	WAITING_FOR_USER,
	POLLING,
	EXCHANGING,
	COMPLETE,
	FAILED
};

class AuthFlow
{
private:
	static std::thread workerThread;
	static std::atomic<AuthFlowState> state;
	static std::atomic<bool> cancelRequested;
	static AuthResult result;
	static wstring userCode;
	static wstring verificationUri;

	static void microsoftFlowThread();
	static void elybyFlowThread(const string &username, const string &password);

public:
	static void startMicrosoft();
	static void startElyBy(const wstring &username, const wstring &password);

	static AuthFlowState getState() { return state.load(); }
	static const AuthResult &getResult() { return result; }
	static const wstring &getUserCode() { return userCode; }
	static const wstring &getVerificationUri() { return verificationUri; }
	static void reset();
};
