#include "stdafx.h"
#include "HandshakeManager.h"
#include "AuthModule.h"

static constexpr auto PROTOCOL_VERSION = L"1.0";

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
		protocolVersion = L"";
		for (const auto &[k, v] : packet->fields)
			if (k == L"version") protocolVersion = v;

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
		wstring variation;
		for (const auto &[k, v] : packet->fields)
			if (k == L"variation") variation = v;

		activeVariation = variation;
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

		state = HandshakeState::AUTH_DATA_EXCHANGED;
		return nullptr;
	}

	case AuthStage::AUTH_DONE:
	{
		wstring uid, username;
		for (const auto &[k, v] : packet->fields)
		{
			if (k == L"uid") uid = v;
			else if (k == L"username") username = v;
		}

		if (!activeModule->validate(uid, username))
			return fail();

		finalUid = uid;
		finalUsername = username;
		state = HandshakeState::IDENTITY_ASSIGNED;
		return makePacket(AuthStage::ASSIGN_IDENTITY, {
			{L"uid", finalUid},
			{L"username", finalUsername}
		});
	}

	case AuthStage::CONFIRM_IDENTITY:
	{
		wstring uid, username;
		for (const auto &[k, v] : packet->fields)
		{
			if (k == L"uid") uid = v;
			else if (k == L"username") username = v;
		}

		if (uid != finalUid || username != finalUsername)
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
		wstring scheme;
		for (const auto &[k, v] : packet->fields)
		{
			if (k == L"version") protocolVersion = v;
			else if (k == L"scheme") scheme = v;
		}

		if (protocolVersion != PROTOCOL_VERSION)
			return fail();

		auto it = modules.find(scheme);
		if (it == modules.end())
			return fail();

		activeModule = it->second;

		auto variations = activeModule->supportedVariations();
		activeVariation = variations.empty() ? L"" : variations[0];

		state = HandshakeState::SCHEME_ACCEPTED;
		return makePacket(AuthStage::ACCEPT_SCHEME, {{L"variation", activeVariation}});
	}

	case AuthStage::SCHEME_SETTINGS:
	{
		state = HandshakeState::AUTH_IN_PROGRESS;
		return makePacket(AuthStage::BEGIN_AUTH);
	}

	case AuthStage::ASSIGN_IDENTITY:
	{
		for (const auto &[k, v] : packet->fields)
		{
			if (k == L"uid") finalUid = v;
			else if (k == L"username") finalUsername = v;
		}

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
