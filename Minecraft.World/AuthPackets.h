#pragma once
using namespace std;

#include "Packet.h"

enum class AuthStage : uint8_t
{
	ANNOUNCE_VERSION,
	DECLARE_SCHEME,
	ACCEPT_SCHEME,
	SCHEME_SETTINGS,
	BEGIN_AUTH,
	AUTH_DATA,
	AUTH_DONE,
	ASSIGN_IDENTITY,
	CONFIRM_IDENTITY,
	AUTH_SUCCESS,
	AUTH_FAILURE
};

class AuthPacket : public Packet, public enable_shared_from_this<AuthPacket>
{
public:
	AuthStage stage;
	vector<pair<wstring, wstring>> fields;

	AuthPacket(AuthStage stage = AuthStage::ANNOUNCE_VERSION, vector<pair<wstring, wstring>> fields = {});

	virtual void read(DataInputStream *dis);
	virtual void write(DataOutputStream *dos);
	virtual void handle(PacketListener *listener);
	virtual int getEstimatedSize();

	static shared_ptr<Packet> create() { return std::make_shared<AuthPacket>(); }
	virtual int getId() { return 72; }
};
