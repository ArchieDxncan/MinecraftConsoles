#pragma once
using namespace std;

#include <string>
#include <unordered_map>
#include <memory>
#include "AuthPackets.h"

class AuthModule;

enum class HandshakeState : uint8_t
{
	IDLE,
	VERSION_SENT,
	SCHEME_DECLARED,
	SCHEME_ACCEPTED,
	SETTINGS_SENT,
	AUTH_IN_PROGRESS,
	AUTH_DATA_EXCHANGED,
	IDENTITY_ASSIGNED,
	IDENTITY_CONFIRMED,
	COMPLETE,
	FAILED
};

class HandshakeManager
{
private:
	bool isServer;
	HandshakeState state;
	unordered_map<wstring, unique_ptr<AuthModule>> modules;
	AuthModule *activeModule;
	wstring activeVariation;
	wstring protocolVersion;

	wstring accessToken;
	wstring clientUid;
	wstring clientUsername;
	wstring preferredVariation;

	vector<shared_ptr<AuthPacket>> pendingPackets;

public:
	wstring finalUid;
	wstring finalUsername;

	HandshakeManager(bool isServer);
	~HandshakeManager() = default;

	void registerModule(unique_ptr<AuthModule> module);
	void setCredentials(const wstring &token, const wstring &uid, const wstring &username, const wstring &variation = L"");
	shared_ptr<AuthPacket> handlePacket(const shared_ptr<AuthPacket> &packet);
	shared_ptr<AuthPacket> createInitialPacket();
	vector<shared_ptr<AuthPacket>> drainPendingPackets();

	bool isComplete() const { return state == HandshakeState::COMPLETE; }
	bool isFailed() const { return state == HandshakeState::FAILED; }
	HandshakeState getState() const { return state; }

private:
	shared_ptr<AuthPacket> handleServer(const shared_ptr<AuthPacket> &packet);
	shared_ptr<AuthPacket> handleClient(const shared_ptr<AuthPacket> &packet);

	shared_ptr<AuthPacket> makePacket(AuthStage stage, vector<pair<wstring, wstring>> fields = {});
	shared_ptr<AuthPacket> fail();
};
