#pragma once

#ifdef _WINDOWS64

#include <string>

namespace PlayFabPartyTransport
{
	bool IsRuntimeEnabled();
	bool IsAvailable();

	enum class PartyJoinAssignWait
	{
		TimedOut,
		Success,
		Rejected
	};

	// Host: create Party network and produce serialized descriptor + invitation id for PlayFab Lobby SearchData.
	// Call before CreateLobby when Party is enabled. On failure, out strings stay empty (TCP-only lobby).
	bool HostCreateNetworkForLobby(std::string *outSerializedDescriptor, std::string *outInvitationId);

	void StopHostSession();

	// Join: connect + authenticate to Party network using lobby strings; returns false on hard failure.
	bool JoinRunPartyHandshake(const char *serializedNetworkDescriptor, const char *invitationId,
		const char *entityId, const char *entityType, const char *entityToken);

	// After JoinRunPartyHandshake succeeds: wait for host's 1-byte smallId (or Party reject) over game endpoint.
	PartyJoinAssignWait WaitForPartyJoinSmallIdAssignment(unsigned char *outSmallId, unsigned int timeoutMs);

	// True when this process is using Party endpoints for Win64 game packets (not TCP sockets).
	bool IsPartyGameTransportActive();

	bool SendPartyGameDataToHost(const void *data, int dataSize);
	bool SendPartyGameDataToClient(unsigned char targetSmallId, const void *data, int dataSize);

	void ShutdownPartyClientTransport();

	// Pump Party networking (host + connected clients). Safe to call every frame while Party is active.
	void PumpNetworking();

	bool StartHostSession(const char *announceHost, int announcePort);
	bool BeginJoinSession(const char *targetHost, int targetPort);
}

#endif
