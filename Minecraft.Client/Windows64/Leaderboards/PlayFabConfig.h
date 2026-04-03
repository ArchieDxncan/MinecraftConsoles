#pragma once

// PlayFab (Windows64 + UWP: both define _WINDOWS64 and use WindowsLeaderboardManager)
//
// 1. Create a title in PlayFab Game Manager and copy the Title ID (hex).
// 2. Define statistics (Client API) with aggregation appropriate for your design.
//    The game sends lifetime totals from the local profile (not per-session deltas).
//    Use aggregation "Last" so each upload sets the stat to that total. Do NOT use "Sum"
//    with these totals — Sum adds each submitted value and will double-count every flush.
//    If MINECRAFT_PLAYFAB_MERGE_STATS_WITH_SERVER is 1 (default), before each upload the
//    client merges with GetPlayerStatistics using max(local, server) per stat so cloud
//    values never drop below a previous high (reinstall, older save, second device).
// 3. For each board, create the primary "Sum" statistic used for ranking (see
//    WindowsLeaderboardManager.cpp — names like MC_Kills_D3_Sum). Detail stats
//    (MC_Kills_D3_C0, …) are uploaded for every player.
//
//    Other players' column stats: either enable ShowStatistics on player profiles
//    (Title settings → Client Profile Options), OR deploy the optional CloudScript in
//    scripts/playfab-cloudscript-leaderboard-columns.js and set the macro below to the
//    handler name (e.g. GetLeaderboardColumnStats). The client then batches column
//    reads via ExecuteCloudScript without exposing the title secret key in the game.
//
// You can pass the title id on the compiler command line, e.g.:
//   /D MINECRAFT_PLAYFAB_TITLE_ID=\"A1B2C\"
// or edit the default below.

#ifndef MINECRAFT_PLAYFAB_TITLE_ID
#define MINECRAFT_PLAYFAB_TITLE_ID "C7923"
#endif

// Optional: CloudScript handler name for batch-reading MC_*_C* stats for other players
// (scripts/playfab-cloudscript-leaderboard-columns.js). Default matches that script.
// Define as "" (or override to "") to skip CloudScript and use GetPlayerProfile only.
#ifndef MINECRAFT_PLAYFAB_CLOUDSCRIPT_LB_COLUMNS
#define MINECRAFT_PLAYFAB_CLOUDSCRIPT_LB_COLUMNS "GetLeaderboardColumnStats"
#endif

#ifndef MINECRAFT_PLAYFAB_MERGE_STATS_WITH_SERVER
#define MINECRAFT_PLAYFAB_MERGE_STATS_WITH_SERVER 1
#endif

// Internet join list via PlayFab Lobby (Win64): hosts publish when running an online, non-private game;
// the join menu merges FindLobbies results with LAN (same row style, host display name as title).
#ifndef MINECRAFT_PLAYFAB_LOBBY_ENABLED
#define MINECRAFT_PLAYFAB_LOBBY_ENABLED 1
#endif

// Win32: Windows NAT UPnP (IUPnPNAT / natupnp) maps TCP gamePort and announces the router's public IPv4 in PlayFab SearchData.
// UWP / Xbox dev mode: UPnP IGD via SSDP + SOAP (UpnpIgdUwp.cpp); same SearchData behavior when the LAN router exposes IGD.
#ifndef MINECRAFT_PLAYFAB_LOBBY_UPNP
#define MINECRAFT_PLAYFAB_LOBBY_UPNP 1
#endif

// If 1, FindLobbies can list lobbies you own (same PlayFab entity as host). Use only for local dev when
// host and browser share the same Custom ID / uid.dat; keep 0 in production.
#ifndef MINECRAFT_PLAYFAB_LOBBY_INCLUDE_OWN_LOBBY
#define MINECRAFT_PLAYFAB_LOBBY_INCLUDE_OWN_LOBBY 0
#endif

// Prepended to the host display name in PlayFab lobby SearchData (string_key3) only — not LAN discovery.
// Use "" for retail builds.
#ifndef MINECRAFT_PLAYFAB_LOBBY_DISPLAY_PREFIX
#define MINECRAFT_PLAYFAB_LOBBY_DISPLAY_PREFIX ""
#endif

// If 1, internet join list (FindLobbies) only lists games whose host PlayFabId is a mutual friend (you have them
// and they have you). Requires host lobbies to publish string_key4 (PlayFabId) and CloudScript
// LCE_GetFriendsMutualFlags (scripts/playfab-cloudscript-friend-requests.js).
#ifndef MINECRAFT_PLAYFAB_LOBBY_MUTUAL_FRIENDS_ONLY
#define MINECRAFT_PLAYFAB_LOBBY_MUTUAL_FRIENDS_ONLY 1
#endif
