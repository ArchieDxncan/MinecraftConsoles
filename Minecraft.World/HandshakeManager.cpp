#include "stdafx.h"
#include "HandshakeManager.h"
#include "AuthModule.h"
#include "HttpClient.h"
#include "StringHelpers.h"
#include "Common/vendor/nlohmann/json.hpp"

static constexpr auto PROTOCOL_VERSION = L"1.0";

static wstring getField(const vector<pair<wstring, wstring>> &fields, const wchar_t *key)
{
	for (const auto &[k, v] : fields)
		if (k == key) return v;
	return {};
}

HandshakeManager::HandshakeManager(bool isServer)
	: isServer(isServer), state(HandshakeState::IDLE), activeModule(nullptr)
{
}

HandshakeManager::~HandshakeManager()
{
	for (auto &[name, module] : modules)
		delete module;
}

void HandshakeManager::registerModule(AuthModule *module)
{
	modules[module->schemeName()] = module;
}

void HandshakeManager::setCredentials(const wstring &token, const wstring &uid, const wstring &username, const wstring &variation)
{
	accessToken = token;
	clientUid = uid;
	clientUsername = username;
	preferredVariation = variation;
}

vector<shared_ptr<AuthPacket>> HandshakeManager::drainPendingPackets()
{
	vector<shared_ptr<AuthPacket>> out;
	out.swap(pendingPackets);
	return out;
}

shared_ptr<AuthPacket> HandshakeManager::handlePacket(const shared_ptr<AuthPacket> &packet)
{
	return isServer ? handleServer(packet) : handleClient(packet);
}

shared_ptr<AuthPacket> HandshakeManager::createInitialPacket()
{
	state = HandshakeState::VERSION_SENT;
	return makePacket(AuthStage::ANNOUNCE_VERSION, {{L"version", PROTOCOL_VERSION}});
}

shared_ptr<AuthPacket> HandshakeManager::handleServer(const shared_ptr<AuthPacket> &packet)
{
	switch (packet->stage)
	{
	case AuthStage::ANNOUNCE_VERSION:
	{
		protocolVersion = getField(packet->fields, L"version");
		if (protocolVersion != PROTOCOL_VERSION)
			return fail();

		// Pick first registered module as the scheme
		if (modules.empty())
			return fail();

		activeModule = modules.begin()->second;
		state = HandshakeState::SCHEME_DECLARED;
		return makePacket(AuthStage::DECLARE_SCHEME, {
			{L"version", PROTOCOL_VERSION},
			{L"scheme", activeModule->schemeName()}
		});
	}

	case AuthStage::ACCEPT_SCHEME:
	{
		activeVariation = getField(packet->fields, L"variation");
		state = HandshakeState::SETTINGS_SENT;
		auto settings = activeModule->getSettings(activeVariation);
		return makePacket(AuthStage::SCHEME_SETTINGS, std::move(settings));
	}

	case AuthStage::BEGIN_AUTH:
	{
		state = HandshakeState::AUTH_IN_PROGRESS;
		return nullptr;
	}

	case AuthStage::AUTH_DATA:
	{
		wstring uid, username;
		if (!activeModule->onAuthData(packet->fields, uid, username))
			return fail();
		finalUid = uid;
		finalUsername = username;
		state = HandshakeState::AUTH_DATA_EXCHANGED;
		return nullptr;
	}

	case AuthStage::AUTH_DONE:
	{
		if (getField(packet->fields, L"uid") != finalUid || getField(packet->fields, L"username") != finalUsername)
			return fail();

		state = HandshakeState::IDENTITY_ASSIGNED;
		return makePacket(AuthStage::ASSIGN_IDENTITY, {
			{L"uid", finalUid},
			{L"username", finalUsername}
		});
	}

	case AuthStage::CONFIRM_IDENTITY:
	{
		if (getField(packet->fields, L"uid") != finalUid || getField(packet->fields, L"username") != finalUsername)
			return fail();

		state = HandshakeState::COMPLETE;
		return makePacket(AuthStage::AUTH_SUCCESS);
	}

	default:
		return fail();
	}
}

shared_ptr<AuthPacket> HandshakeManager::handleClient(const shared_ptr<AuthPacket> &packet)
{
	switch (packet->stage)
	{
	case AuthStage::DECLARE_SCHEME:
	{
		protocolVersion = getField(packet->fields, L"version");
		wstring scheme = getField(packet->fields, L"scheme");

		if (protocolVersion != PROTOCOL_VERSION)
			return fail();

		auto it = modules.find(scheme);
		if (it == modules.end())
			return fail();

		activeModule = it->second;

		auto variations = activeModule->supportedVariations();
		if (!preferredVariation.empty() &&
			std::find(variations.begin(), variations.end(), preferredVariation) != variations.end())
			activeVariation = preferredVariation;
		else
			activeVariation = variations.empty() ? L"" : variations[0];

		state = HandshakeState::SCHEME_ACCEPTED;
		return makePacket(AuthStage::ACCEPT_SCHEME, {{L"variation", activeVariation}});
	}

	case AuthStage::SCHEME_SETTINGS:
	{
		wstring serverId = getField(packet->fields, L"serverId");
		wstring sessionEndpoint = getField(packet->fields, L"sessionEndpoint");
		wstring scheme(activeModule->schemeName());
		if (scheme == L"mcconsoles:session" && !accessToken.empty())
		{
			nlohmann::json body = {
				{"accessToken", narrowStr(accessToken)},
				{"selectedProfile", narrowStr(clientUid)},
				{"serverId", narrowStr(serverId)}
			};
			auto resp = HttpClient::post(narrowStr(sessionEndpoint) + "/session/minecraft/join", body.dump());
			if (resp.statusCode != 204)
				return fail();
		}

		state = HandshakeState::AUTH_IN_PROGRESS;
		pendingPackets.push_back(makePacket(AuthStage::BEGIN_AUTH));
		pendingPackets.push_back(makePacket(AuthStage::AUTH_DATA, {
			{L"uid", clientUid},
			{L"username", clientUsername}
		}));
		pendingPackets.push_back(makePacket(AuthStage::AUTH_DONE, {
			{L"uid", clientUid},
			{L"username", clientUsername}
		}));
		return nullptr;
	}

	case AuthStage::ASSIGN_IDENTITY:
	{
		finalUid = getField(packet->fields, L"uid");
		finalUsername = getField(packet->fields, L"username");

		state = HandshakeState::IDENTITY_CONFIRMED;
		return makePacket(AuthStage::CONFIRM_IDENTITY, {
			{L"uid", finalUid},
			{L"username", finalUsername}
		});
	}

	case AuthStage::AUTH_SUCCESS:
	{
		state = HandshakeState::COMPLETE;
		return nullptr;
	}

	case AuthStage::AUTH_FAILURE:
	{
		state = HandshakeState::FAILED;
		return nullptr;
	}

	default:
		return fail();
	}
}

shared_ptr<AuthPacket> HandshakeManager::makePacket(AuthStage stage, vector<pair<wstring, wstring>> fields)
{
	return std::make_shared<AuthPacket>(stage, std::move(fields));
}

shared_ptr<AuthPacket> HandshakeManager::fail()
{
	state = HandshakeState::FAILED;
	return makePacket(AuthStage::AUTH_FAILURE);
}
