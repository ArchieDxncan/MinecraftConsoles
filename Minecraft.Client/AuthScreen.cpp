#include "stdafx.h"
#include "AuthScreen.h"
#include "Minecraft.h"
#include "User.h"
#include <fstream>

static constexpr auto PROFILES_FILE = L"auth_profiles.dat";

vector<AuthProfile> AuthProfileManager::profiles;
int AuthProfileManager::selectedProfile = -1;

void AuthProfileManager::load()
{
	profiles.clear();
	std::ifstream file(PROFILES_FILE, std::ios::binary);
	if (!file) return;

	uint32_t count = 0;
	file.read(reinterpret_cast<char *>(&count), sizeof(count));

	for (uint32_t i = 0; i < count && file.good(); i++)
	{
		AuthProfile p;
		uint8_t type;
		file.read(reinterpret_cast<char *>(&type), sizeof(type));
		p.type = static_cast<AuthProfile::Type>(type);

		auto readWstr = [&file]() -> wstring {
			uint16_t len = 0;
			file.read(reinterpret_cast<char *>(&len), sizeof(len));
			if (!file || len > 4096) return {};
			wstring s(len, L'\0');
			file.read(reinterpret_cast<char *>(s.data()), len * sizeof(wchar_t));
			if (!file) return {};
			return s;
		};

		p.uid = readWstr();
		p.username = readWstr();
		p.token = readWstr();
		profiles.push_back(std::move(p));
	}

	if (!profiles.empty())
		selectedProfile = 0;
}

void AuthProfileManager::save()
{
	std::ofstream file(PROFILES_FILE, std::ios::binary | std::ios::trunc);
	if (!file) return;

	uint32_t count = static_cast<uint32_t>(profiles.size());
	file.write(reinterpret_cast<const char *>(&count), sizeof(count));

	auto writeWstr = [&file](const wstring &s) {
		uint16_t len = static_cast<uint16_t>(s.length());
		file.write(reinterpret_cast<const char *>(&len), sizeof(len));
		file.write(reinterpret_cast<const char *>(s.data()), len * sizeof(wchar_t));
	};

	for (const auto &p : profiles)
	{
		uint8_t type = static_cast<uint8_t>(p.type);
		file.write(reinterpret_cast<const char *>(&type), sizeof(type));
		writeWstr(p.uid);
		writeWstr(p.username);
		writeWstr(p.token);
	}
}

void AuthProfileManager::addProfile(AuthProfile::Type type, const wstring &username)
{
	profiles.push_back({type, L"offline_" + username, username, {}});
	selectedProfile = static_cast<int>(profiles.size()) - 1;
	save();
}

void AuthProfileManager::removeSelectedProfile()
{
	if (selectedProfile < 0 || selectedProfile >= static_cast<int>(profiles.size()))
		return;

	profiles.erase(profiles.begin() + selectedProfile);
	if (selectedProfile >= static_cast<int>(profiles.size()))
		selectedProfile = static_cast<int>(profiles.size()) - 1;
	save();
}

bool AuthProfileManager::applySelectedProfile()
{
	if (selectedProfile < 0 || selectedProfile >= static_cast<int>(profiles.size()))
		return false;

	const auto &p = profiles[selectedProfile];
	auto *mc = Minecraft::GetInstance();

	if (mc->user)
		delete mc->user;

	mc->user = new User(p.username, p.token);
	return true;
}
