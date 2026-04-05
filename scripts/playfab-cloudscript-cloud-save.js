// PlayFab Classic CloudScript — third-party cloud save control plane (no PlayFab Cloud Save / Files).
// Merge into your main CloudScript revision (Title → Automation → CloudScript).
//
// Title INTERNAL data only — NOT public "Title Data".
// Game Manager: Title settings → Internal title data (or Data → Title internal data), key "LCE_CloudSave_Secrets"
//   JSON: {
//     "google_client_id":"", "google_client_secret":"",
//     "microsoft_client_id":"", "microsoft_client_secret":"",
//     "microsoft_tenant":"common",
//     "dropbox_app_key":"", "dropbox_app_secret":""
//   }
//
// Outbound HTTP: enable googleapis.com, login.microsoftonline.com, graph.microsoft.com,
// api.dropboxapi.com, content.dropboxapi.com in PlayFab (Title settings → API Features → External HTTP).
//
// Handlers:
//   LCE_CloudSave_OAuthComplete  { provider:"google"|"microsoft"|"dropbox", code, redirectUri, codeVerifier }
//   LCE_CloudSave_Unlink         {}
//   LCE_CloudSave_GetConfig      {}
//   LCE_CloudSave_GetAccessToken {}
//   LCE_CloudSave_SetLastSync    { isoUtc: "..." }

var LCE_CS_USER_KEY = "LCE_CloudSave";
var LCE_CS_INTERNAL_RT = "LCE_CS_RT";
var LCE_CS_INTERNAL_PROV = "LCE_CS_PROV";

function lceSafeJsonParse(s) {
	try {
		return JSON.parse(s);
	} catch (e) {
		return null;
	}
}

function lceGetInternalDataEntry(dataObj, exactKey) {
	if (!dataObj) return null;
	var entry = dataObj[exactKey];
	if (entry) return entry;
	var want = String(exactKey).toLowerCase();
	for (var k in dataObj) {
		if (!Object.prototype.hasOwnProperty.call(dataObj, k)) continue;
		if (String(k).toLowerCase() === want) return dataObj[k];
	}
	return null;
}

function lceEntryValue(entry) {
	if (!entry) return null;
	var v = entry.Value != null ? entry.Value : entry.value;
	return v;
}

function lceGetSecrets() {
	var res = server.GetTitleInternalData({ Keys: ["LCE_CloudSave_Secrets"] });
	var dataObj = res && (res.Data || res.data);
	var entry = lceGetInternalDataEntry(dataObj, "LCE_CloudSave_Secrets");
	if (!entry) {
		return null;
	}
	var raw = lceEntryValue(entry);
	if (raw == null || String(raw).trim() === "") {
		return null;
	}
	var parsed = lceSafeJsonParse(raw);
	if (!parsed) {
		throw new Error(
			"LCE_CloudSave_Secrets value is not valid JSON. Fix the Value in Internal title data (matching quotes and braces)."
		);
	}
	return parsed;
}

function lceFormEncode(obj) {
	var parts = [];
	for (var k in obj) {
		if (!Object.prototype.hasOwnProperty.call(obj, k) || obj[k] == null) continue;
		parts.push(encodeURIComponent(k) + "=" + encodeURIComponent(String(obj[k])));
	}
	return parts.join("&");
}

function lceHttpPostForm(url, formBody) {
	var h = null;
	var hdr = { "Content-Type": "application/x-www-form-urlencoded" };
	try {
		h = http.request(url, "post", formBody, "application/x-www-form-urlencoded", hdr);
	} catch (e) {
		var em = e && (e.message || e.Message) ? String(e.message || e.Message) : String(e);
		return {
			ok: false,
			body: "",
			err:
				em +
				" — If this is an allowlist error, add this host in Title settings → API Features → External HTTP.",
		};
	}
	return { ok: true, body: h, err: "" };
}

function lceHttpPostJson(url, jsonStr, authBearer) {
	var headers = { "Content-Type": "application/json" };
	if (authBearer) {
		headers.Authorization = "Bearer " + authBearer;
	}
	try {
		var h = http.request(url, "post", jsonStr, "application/json", headers);
		return { ok: true, body: h, err: "" };
	} catch (e2) {
		var em2 = e2 && (e2.message || e2.Message) ? String(e2.message || e2.Message) : String(e2);
		return {
			ok: false,
			body: "",
			err:
				em2 +
				" — If this is an allowlist error, add this host in Title settings → API Features → External HTTP.",
		};
	}
}

function lceReadUserConfig() {
	var read = server.GetUserReadOnlyData({ PlayFabId: currentPlayerId, Keys: [LCE_CS_USER_KEY] });
	if (!read.Data || !read.Data[LCE_CS_USER_KEY]) {
		return { enabled: false, provider: "", googleRootId: "", dropboxRootPath: "", msRootId: "", lastSyncUtc: "" };
	}
	var o = lceSafeJsonParse(read.Data[LCE_CS_USER_KEY].Value);
	return o || { enabled: false };
}

function lceWriteUserConfig(cfg) {
	var data = {};
	data[LCE_CS_USER_KEY] = JSON.stringify(cfg);
	server.UpdateUserReadOnlyData({ PlayFabId: currentPlayerId, Data: data });
}

function lceReadInternalTokens() {
	var read = server.GetUserInternalData({ PlayFabId: currentPlayerId, Keys: [LCE_CS_INTERNAL_RT, LCE_CS_INTERNAL_PROV] });
	var rt = "";
	var prov = "";
	if (read.Data && read.Data[LCE_CS_INTERNAL_RT]) {
		rt = read.Data[LCE_CS_INTERNAL_RT].Value || "";
	}
	if (read.Data && read.Data[LCE_CS_INTERNAL_PROV]) {
		prov = read.Data[LCE_CS_INTERNAL_PROV].Value || "";
	}
	return { refreshToken: rt, provider: prov };
}

function lceWriteInternalTokens(refreshToken, provider) {
	var d = {};
	d[LCE_CS_INTERNAL_RT] = refreshToken;
	d[LCE_CS_INTERNAL_PROV] = provider;
	server.UpdateUserInternalData({ PlayFabId: currentPlayerId, Data: d });
}

function lceClearAllCloudSaveData() {
	server.UpdateUserInternalData({
		PlayFabId: currentPlayerId,
		Data: (function () {
			var o = {};
			o[LCE_CS_INTERNAL_RT] = null;
			o[LCE_CS_INTERNAL_PROV] = null;
			return o;
		})(),
	});
	var data = {};
	data[LCE_CS_USER_KEY] = JSON.stringify({
		enabled: false,
		provider: "",
		googleRootId: "",
		dropboxRootPath: "",
		msRootId: "",
		lastSyncUtc: "",
	});
	server.UpdateUserReadOnlyData({ PlayFabId: currentPlayerId, Data: data });
}

function lceExchangeGoogle(secrets, code, redirectUri, codeVerifier) {
	var form = lceFormEncode({
		code: code,
		client_id: secrets.google_client_id,
		client_secret: secrets.google_client_secret,
		redirect_uri: redirectUri,
		grant_type: "authorization_code",
		code_verifier: codeVerifier,
	});
	var r = lceHttpPostForm("https://oauth2.googleapis.com/token", form);
	if (!r.ok) {
		throw new Error("Google token HTTP: " + r.err);
	}
	var tok = lceSafeJsonParse(r.body);
	if (!tok || !tok.access_token) {
		throw new Error("Google token: " + (r.body || "empty"));
	}
	return tok;
}

function lceCreateGoogleRootFolder(accessToken) {
	var body = JSON.stringify({
		name: "Minecraft LCE Cloud Saves",
		mimeType: "application/vnd.google-apps.folder",
	});
	var r = lceHttpPostJson("https://www.googleapis.com/drive/v3/files?fields=id", body, accessToken);
	if (!r.ok) {
		throw new Error("Drive folder HTTP: " + r.err);
	}
	var j = lceSafeJsonParse(r.body);
	if (!j || !j.id) {
		throw new Error("Drive folder: " + (r.body || ""));
	}
	return j.id;
}

function lceExchangeMicrosoft(secrets, code, redirectUri, codeVerifier) {
	var tenant = secrets.microsoft_tenant || "common";
	var url = "https://login.microsoftonline.com/" + tenant + "/oauth2/v2.0/token";
	var form = lceFormEncode({
		client_id: secrets.microsoft_client_id,
		client_secret: secrets.microsoft_client_secret,
		code: code,
		redirect_uri: redirectUri,
		grant_type: "authorization_code",
		code_verifier: codeVerifier,
		scope:
			"offline_access Files.ReadWrite User.Read",
	});
	var r = lceHttpPostForm(url, form);
	if (!r.ok) {
		throw new Error("Microsoft token HTTP: " + r.err);
	}
	var tok = lceSafeJsonParse(r.body);
	if (!tok || !tok.access_token) {
		throw new Error("Microsoft token: " + (r.body || ""));
	}
	return tok;
}

function lceCreateMicrosoftRootFolder(accessToken) {
	var body = JSON.stringify({
		name: "Minecraft LCE Cloud Saves",
		folder: {},
		"@microsoft.graph.conflictBehavior": "rename",
	});
	var r = lceHttpPostJson("https://graph.microsoft.com/v1.0/me/drive/root/children", body, accessToken);
	if (!r.ok) {
		throw new Error("Graph folder HTTP: " + r.err);
	}
	var j = lceSafeJsonParse(r.body);
	if (!j || !j.id) {
		throw new Error("Graph folder: " + (r.body || ""));
	}
	return j.id;
}

function lceExchangeDropbox(secrets, code, redirectUri, codeVerifier) {
	var form = lceFormEncode({
		code: code,
		grant_type: "authorization_code",
		client_id: secrets.dropbox_app_key,
		client_secret: secrets.dropbox_app_secret,
		redirect_uri: redirectUri,
		code_verifier: codeVerifier,
	});
	var r = lceHttpPostForm("https://api.dropboxapi.com/oauth2/token", form);
	if (!r.ok) {
		throw new Error("Dropbox token HTTP: " + r.err);
	}
	var tok = lceSafeJsonParse(r.body);
	if (!tok || !tok.access_token) {
		throw new Error("Dropbox token: " + (r.body || ""));
	}
	return tok;
}

function lceCreateDropboxRoot(accessToken) {
	var r = lceHttpPostJson(
		"https://api.dropboxapi.com/2/files/create_folder_v2",
		JSON.stringify({ path: "/Minecraft LCE Cloud Saves", autorename: false }),
		accessToken
	);
	if (!r.ok) {
		throw new Error("Dropbox mkdir HTTP: " + r.err);
	}
	// path is fixed for client sync
	return "/Minecraft LCE Cloud Saves";
}

function lceRefreshGoogle(secrets, refreshToken) {
	var form = lceFormEncode({
		client_id: secrets.google_client_id,
		client_secret: secrets.google_client_secret,
		refresh_token: refreshToken,
		grant_type: "refresh_token",
	});
	var r = lceHttpPostForm("https://oauth2.googleapis.com/token", form);
	if (!r.ok) {
		throw new Error("Google refresh HTTP: " + r.err);
	}
	var tok = lceSafeJsonParse(r.body);
	if (!tok || !tok.access_token) {
		throw new Error("Google refresh: " + (r.body || ""));
	}
	return { accessToken: tok.access_token, expiresIn: tok.expires_in || 3600 };
}

function lceRefreshMicrosoft(secrets, refreshToken) {
	var tenant = secrets.microsoft_tenant || "common";
	var url = "https://login.microsoftonline.com/" + tenant + "/oauth2/v2.0/token";
	var form = lceFormEncode({
		client_id: secrets.microsoft_client_id,
		client_secret: secrets.microsoft_client_secret,
		refresh_token: refreshToken,
		grant_type: "refresh_token",
		scope: "offline_access Files.ReadWrite User.Read",
	});
	var r = lceHttpPostForm(url, form);
	if (!r.ok) {
		throw new Error("Microsoft refresh HTTP: " + r.err);
	}
	var tok = lceSafeJsonParse(r.body);
	if (!tok || !tok.access_token) {
		throw new Error("Microsoft refresh: " + (r.body || ""));
	}
	return { accessToken: tok.access_token, expiresIn: tok.expires_in || 3600 };
}

function lceRefreshDropbox(secrets, refreshToken) {
	var form = lceFormEncode({
		grant_type: "refresh_token",
		refresh_token: refreshToken,
		client_id: secrets.dropbox_app_key,
		client_secret: secrets.dropbox_app_secret,
	});
	var r = lceHttpPostForm("https://api.dropboxapi.com/oauth2/token", form);
	if (!r.ok) {
		throw new Error("Dropbox refresh HTTP: " + r.err);
	}
	var tok = lceSafeJsonParse(r.body);
	if (!tok || !tok.access_token) {
		throw new Error("Dropbox refresh: " + (r.body || ""));
	}
	return { accessToken: tok.access_token, expiresIn: tok.expires_in || 14400 };
}

handlers.LCE_CloudSave_OAuthComplete = function (args, context) {
	var secrets = lceGetSecrets();
	if (!secrets) {
		throw new Error(
			"Missing Internal title data key LCE_CloudSave_Secrets. In PlayFab Game Manager open the same title your game uses, go to Internal title data (not public Title Data), add Key LCE_CloudSave_Secrets and paste your JSON Value, then Save."
		);
	}
	var provider = (args && args.provider) || "";
	var code = (args && args.code) || "";
	var redirectUri = (args && args.redirectUri) || "";
	var codeVerifier = (args && args.codeVerifier) || "";
	if (!provider || !code || !redirectUri || !codeVerifier) {
		throw new Error("provider, code, redirectUri, and codeVerifier are required.");
	}

	var tok;
	var cfg = {
		enabled: true,
		provider: provider,
		googleRootId: "",
		dropboxRootPath: "",
		msRootId: "",
		lastSyncUtc: "",
	};
	var refresh = "";

	if (provider === "google") {
		tok = lceExchangeGoogle(secrets, code, redirectUri, codeVerifier);
		refresh = tok.refresh_token || "";
		cfg.googleRootId = lceCreateGoogleRootFolder(tok.access_token);
	} else if (provider === "microsoft") {
		tok = lceExchangeMicrosoft(secrets, code, redirectUri, codeVerifier);
		refresh = tok.refresh_token || "";
		cfg.msRootId = lceCreateMicrosoftRootFolder(tok.access_token);
	} else if (provider === "dropbox") {
		tok = lceExchangeDropbox(secrets, code, redirectUri, codeVerifier);
		refresh = tok.refresh_token || "";
		try {
			cfg.dropboxRootPath = lceCreateDropboxRoot(tok.access_token);
		} catch (eMk) {
			// Folder may already exist
			cfg.dropboxRootPath = "/Minecraft LCE Cloud Saves";
		}
	} else {
		throw new Error("Unknown provider.");
	}

	if (!refresh) {
		throw new Error("No refresh token returned; ensure offline access / prompt=consent where required.");
	}

	lceWriteInternalTokens(refresh, provider);
	lceWriteUserConfig(cfg);
	return { ok: true, provider: provider };
};

handlers.LCE_CloudSave_Unlink = function (args, context) {
	lceClearAllCloudSaveData();
	return { ok: true };
};

handlers.LCE_CloudSave_GetConfig = function (args, context) {
	return lceReadUserConfig();
};

handlers.LCE_CloudSave_GetAccessToken = function (args, context) {
	var secrets = lceGetSecrets();
	if (!secrets) {
		throw new Error(
			"Missing Internal title data key LCE_CloudSave_Secrets (not public Title Data). Add it under Title settings → Internal title data."
		);
	}
	var t = lceReadInternalTokens();
	if (!t.refreshToken || !t.provider) {
		throw new Error("Cloud save not linked.");
	}
	var prov = t.provider;
	if (prov === "google") {
		return lceRefreshGoogle(secrets, t.refreshToken);
	}
	if (prov === "microsoft") {
		return lceRefreshMicrosoft(secrets, t.refreshToken);
	}
	if (prov === "dropbox") {
		return lceRefreshDropbox(secrets, t.refreshToken);
	}
	throw new Error("Unknown linked provider.");
};

handlers.LCE_CloudSave_SetLastSync = function (args, context) {
	var iso = (args && args.isoUtc) || new Date().toISOString();
	var cfg = lceReadUserConfig();
	cfg.lastSyncUtc = iso;
	lceWriteUserConfig(cfg);
	return { ok: true, lastSyncUtc: iso };
};
