#pragma once

// UWP / Xbox: UPnP IGD (SSDP + SOAP) for PlayFab lobby announce — no IUPnPNAT COM API.

bool UpnpIgdUwp_AddTcpMappingAndGetExternalIPv4(int gamePort, const char *lanIpUtf8, char *outAnnounceHost,
	size_t announceHostSize, int *outAnnouncePort);

void UpnpIgdUwp_RemoveMappingIfAny();
