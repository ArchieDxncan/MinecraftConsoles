#pragma once

#ifdef _WINDOWS64

#include <cstdint>
#include <string>
#include <vector>

// PlayFab Lobby (internet) discovery for Win64 join list — complements LAN UDP discovery.
// Host publishes when hosting an online, non-private game. Clients merge results into the same
// FriendSessionInfo path as LAN. Connection still uses TCP to hostIP:hostPort until relay is added.
// See PlayFabConfig.h (MINECRAFT_PLAYFAB_LOBBY_*). Hosts try UPnP (Win32: COM; UWP: IGD SOAP) for a public announce address when enabled.

struct PlayFabListedGame
{
	std::wstring displayName;
	std::string hostIP;
	int hostPort = 0;
	unsigned short netVersion = 0;
	unsigned int gameHostSettings = 0;
	std::uint64_t sessionId = 0;
	unsigned char playerCount = 1;
	unsigned char maxPlayers = 8;
};

namespace PlayFabLobbyWin64
{
	bool IsEnabled();

	// Call when Win64 host starts UDP LAN advertising for an online, joinable session.
	void OnHostStartedAdvertising(bool onlineGame, bool isPrivate, unsigned char publicSlots, int gamePort,
		const wchar_t *hostName, unsigned int gameHostSettings);

	void OnHostStoppedAdvertising();

	void FindJoinableLobbies(std::vector<PlayFabListedGame> &out);
}

#endif
