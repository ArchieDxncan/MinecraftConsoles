// PlayFab Classic CloudScript (JavaScript revisions). Do NOT use "Register Cloud Script Function"
// (HTTP/Queue/EventHub) — that is Azure Functions; this file is for the revision editor only.
// Title → Automation → CloudScript → edit / "Upload new revision" → Deploy revision to live.
// Client calls ExecuteCloudScript with FunctionName matching MINECRAFT_PLAYFAB_CLOUDSCRIPT_LB_COLUMNS
// (e.g. GetLeaderboardColumnStats) and FunctionParameter:
//   { playFabIds: string[], statisticNames: string[] }
// Returns: { statsByPlayer: { [playFabId]: { [StatisticName]: number } } }
//
// Requires no Client Profile "ShowStatistics" flag for other players; uses server.GetPlayerStatistics.

handlers.GetLeaderboardColumnStats = function (args, context) {
	var playFabIds = args && args.playFabIds ? args.playFabIds : [];
	var statisticNames = args && args.statisticNames ? args.statisticNames : [];
	var statsByPlayer = {};

	if (!playFabIds.length || !statisticNames.length) {
		return { statsByPlayer: statsByPlayer };
	}

	for (var i = 0; i < playFabIds.length; i++) {
		var id = playFabIds[i];
		if (!id) {
			continue;
		}
		var m = {};
		try {
			var req = { PlayFabId: id, StatisticNames: statisticNames };
			var res = server.GetPlayerStatistics(req);
			if (res && res.Statistics) {
				for (var j = 0; j < res.Statistics.length; j++) {
					var s = res.Statistics[j];
					if (s && s.StatisticName != null) {
						m[s.StatisticName] = s.Value != null ? s.Value : 0;
					}
				}
			}
		} catch (e) {
			// Leave m empty; client may fall back to GetPlayerProfile.
		}
		statsByPlayer[id] = m;
	}

	return { statsByPlayer: statsByPlayer };
};
