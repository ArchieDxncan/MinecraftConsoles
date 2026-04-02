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
// Enable Multiplayer / Lobby on the title in Game Manager. Join still uses TCP to hostIP:hostPort until
// relay is integrated — put your public IPv4 in LocalState\playfab_join_host.txt if LAN IP is not reachable.
#ifndef MINECRAFT_PLAYFAB_LOBBY_ENABLED
#define MINECRAFT_PLAYFAB_LOBBY_ENABLED 1
#endif
