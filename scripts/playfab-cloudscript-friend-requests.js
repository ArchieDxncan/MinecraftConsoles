// PlayFab Classic CloudScript (JavaScript revisions). Upload via Title → Automation → CloudScript.
// Enables incoming friend requests for the docs web app (ExecuteCloudScript).
//
// Stores a JSON array on each player’s User Read-Only Data key "LCEFriendRequests".
// Client calls (FunctionName):
//   LCE_GetIncomingFriendRequests   FunctionParameter: {}
//   LCE_SendFriendRequest           { FriendUsername?: string, FriendPlayFabId?: string, FriendTitleDisplayName?: string }
//   LCE_AcceptFriendRequest         { FromPlayFabId: string }
//   LCE_DeclineFriendRequest        { FromPlayFabId: string } — clears your queue, then Server.RemoveFriend
//                                   so the sender no longer has you (clears their one-way Pending).
//   LCE_GetFriendsMutualFlags       {} → { MutualById: { [friendPlayFabId]: boolean } }
//   LCE_NotifyFriendRemoved         { OtherPlayFabId: string } — you removed them from your list; if they
//                                   still have you on theirs, queue an incoming request for *you* from *them*
//                                   (so you see their name under Friend requests, not the reverse).
//
// Username → PlayFabId: uses server.GetPlayFabIDsFromGenericIDs with ServiceName "username"
// and "Username" (title-dependent). The web client may pass FriendPlayFabId when it can resolve it.
//
// Accept: removes the pending entry, then server.AddFriend both ways (ignores already-friends).

var LCE_FR_KEY = "LCEFriendRequests";

function lceSamePlayFabId(a, b) {
	return String(a || "") === String(b || "");
}

function lceReadPending(playFabId) {
	var read = server.GetUserReadOnlyData({ PlayFabId: playFabId, Keys: [LCE_FR_KEY] });
	var list = [];
	if (read.Data && read.Data[LCE_FR_KEY] && read.Data[LCE_FR_KEY].Value) {
		try {
			list = JSON.parse(read.Data[LCE_FR_KEY].Value);
		} catch (e) {}
	}
	return Array.isArray(list) ? list : [];
}

function lceWritePending(playFabId, list) {
	server.UpdateUserReadOnlyData({
		PlayFabId: playFabId,
		Data: (function () {
			var o = {};
			o[LCE_FR_KEY] = JSON.stringify(list);
			return o;
		})(),
	});
}

function lceResolveTargetPlayFabId(args) {
	if (args.FriendPlayFabId) {
		return args.FriendPlayFabId;
	}
	var genericTries = [];
	var u = args.FriendUsername;
	if (u && typeof u === "string") {
		genericTries.push({ ServiceName: "username", UserId: u });
		genericTries.push({ ServiceName: "Username", UserId: u });
	}
	var d = args.FriendTitleDisplayName;
	if (d && typeof d === "string") {
		genericTries.push({ ServiceName: "title_display_name", UserId: d });
		genericTries.push({ ServiceName: "TitleDisplayName", UserId: d });
	}
	if (genericTries.length === 0) {
		return null;
	}
	var res = server.GetPlayFabIDsFromGenericIDs({ GenericIDs: genericTries.slice(0, 10) });
	if (res && res.Data) {
		for (var i = 0; i < res.Data.length; i++) {
			if (res.Data[i].PlayFabId) {
				return res.Data[i].PlayFabId;
			}
		}
	}
	return null;
}

handlers.LCE_GetIncomingFriendRequests = function (args, context) {
	var list = lceReadPending(currentPlayerId);
	return { Incoming: list };
};

handlers.LCE_SendFriendRequest = function (args, context) {
	var targetId = lceResolveTargetPlayFabId(args || {});
	if (!targetId) {
		throw new Error("Could not find that player for a friend request.");
	}
	var fromId = currentPlayerId;
	if (targetId === fromId) {
		throw new Error("You cannot send a request to yourself.");
	}
	var acc = server.GetUserAccountInfo({ PlayFabId: fromId });
	var ui = acc.UserInfo;
	var entry = {
		FromPlayFabId: fromId,
		FromUsername: ui.Username || "",
		FromDisplayName: ui.TitleInfo && ui.TitleInfo.DisplayName ? ui.TitleInfo.DisplayName : "",
	};
	var list = lceReadPending(targetId);
	list = list.filter(function (x) {
		return x && !lceSamePlayFabId(x.FromPlayFabId, fromId);
	});
	list.push(entry);
	lceWritePending(targetId, list);
	return { ok: true };
};

handlers.LCE_AcceptFriendRequest = function (args, context) {
	var fromId = args.FromPlayFabId;
	if (!fromId) {
		throw new Error("FromPlayFabId required.");
	}
	var me = currentPlayerId;
	var list = lceReadPending(me);
	list = list.filter(function (x) {
		return !x || !lceSamePlayFabId(x.FromPlayFabId, fromId);
	});
	lceWritePending(me, list);
	try {
		server.AddFriend({ PlayFabId: me, FriendPlayFabId: fromId });
	} catch (e1) {}
	try {
		server.AddFriend({ PlayFabId: fromId, FriendPlayFabId: me });
	} catch (e2) {}
	return { ok: true };
};

handlers.LCE_DeclineFriendRequest = function (args, context) {
	var fromId = args.FromPlayFabId;
	if (!fromId) {
		throw new Error("FromPlayFabId required.");
	}
	var me = currentPlayerId;
	var list = lceReadPending(me);
	var next = list.filter(function (x) {
		return x && !lceSamePlayFabId(x.FromPlayFabId, fromId);
	});
	lceWritePending(me, next);
	// Sender (fromId) may have added us one-way — remove us from their list so their "Pending" clears.
	try {
		server.RemoveFriend({ PlayFabId: fromId, FriendPlayFabId: me });
	} catch (e1) {}
	// Decliner may also have the sender on their list — symmetric cleanup.
	try {
		server.RemoveFriend({ PlayFabId: me, FriendPlayFabId: fromId });
	} catch (e2) {}
	return { ok: true, removed: list.length - next.length };
};

function lceFriendRowsFromServerResult(res) {
	if (!res) {
		return [];
	}
	return res.Friends || res.friends || [];
}

function lceRowId(row) {
	return row.FriendPlayFabId || row.friendPlayFabId;
}

handlers.LCE_GetFriendsMutualFlags = function (args, context) {
	var me = currentPlayerId;
	var mutualById = {};
	var mine;
	try {
		mine = server.GetFriendsList({ PlayFabId: me });
	} catch (e) {
		return { MutualById: mutualById };
	}
	var friends = lceFriendRowsFromServerResult(mine);
	for (var i = 0; i < friends.length; i++) {
		var fid = lceRowId(friends[i]);
		if (!fid) {
			continue;
		}
		var hasMe = false;
		try {
			var theirs = server.GetFriendsList({ PlayFabId: fid });
			var theirFriends = lceFriendRowsFromServerResult(theirs);
			for (var j = 0; j < theirFriends.length; j++) {
				if (lceRowId(theirFriends[j]) === me) {
					hasMe = true;
					break;
				}
			}
		} catch (e2) {
			hasMe = false;
		}
		mutualById[fid] = hasMe;
	}
	return { MutualById: mutualById };
};

handlers.LCE_NotifyFriendRemoved = function (args, context) {
	var otherId = args.OtherPlayFabId;
	if (!otherId) {
		throw new Error("OtherPlayFabId required.");
	}
	var me = currentPlayerId;
	if (otherId === me) {
		throw new Error("Invalid OtherPlayFabId.");
	}
	var theyStillHaveMe = false;
	try {
		var theirs = server.GetFriendsList({ PlayFabId: otherId });
		var theirFriends = lceFriendRowsFromServerResult(theirs);
		for (var j = 0; j < theirFriends.length; j++) {
			if (lceRowId(theirFriends[j]) === me) {
				theyStillHaveMe = true;
				break;
			}
		}
	} catch (e) {
		return { ok: true, queued: false };
	}
	if (!theyStillHaveMe) {
		return { ok: true, queued: false };
	}
	var accOther;
	try {
		accOther = server.GetUserAccountInfo({ PlayFabId: otherId });
	} catch (e3) {
		return { ok: true, queued: false };
	}
	var uiOther = accOther.UserInfo;
	var entry = {
		FromPlayFabId: otherId,
		FromUsername: uiOther.Username || "",
		FromDisplayName:
			uiOther.TitleInfo && uiOther.TitleInfo.DisplayName ? uiOther.TitleInfo.DisplayName : "",
	};
	var list = lceReadPending(me);
	list = list.filter(function (x) {
		return x && !lceSamePlayFabId(x.FromPlayFabId, otherId);
	});
	list.push(entry);
	lceWritePending(me, list);
	return { ok: true, queued: true };
};
