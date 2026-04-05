/**
 * LegacyDxncan Account Manager — shared PlayFab + session (all pages).
 * Matches Minecraft.Client Win64: LoginWithCustomID with 16 hex digit CustomId.
 */
const TITLE_ID = "C7923";

const OAUTH_REDIRECT_URI = new URL("oauth-callback.html", window.location.href).href;

const OAUTH_CLIENT_IDS = {
  google: "784521568846-s4oge3car67hrs4v3jpqem5odb941thd.apps.googleusercontent.com",
  microsoft: "",
  dropbox: "",
};

const SESSION_COOKIE_NAME = "lce_pf_session";
const SESSION_COOKIE_MAX_AGE_SEC = 180 * 24 * 60 * 60;
const SESSION_COOKIE_MAX_CHARS = 3800;

const $ = (id) => document.getElementById(id);

function readSessionCookieRaw() {
  const prefix = `${SESSION_COOKIE_NAME}=`;
  const chunks = document.cookie.split("; ");
  for (const part of chunks) {
    if (part.startsWith(prefix)) {
      try {
        return decodeURIComponent(part.slice(prefix.length));
      } catch {
        return null;
      }
    }
  }
  return null;
}

function writeSessionCookie(jsonString) {
  if (!jsonString || jsonString.length > SESSION_COOKIE_MAX_CHARS) return;
  let c = `${SESSION_COOKIE_NAME}=${encodeURIComponent(jsonString)}; Path=/; Max-Age=${SESSION_COOKIE_MAX_AGE_SEC}; SameSite=Lax`;
  if (location.protocol === "https:") c += "; Secure";
  document.cookie = c;
}

function clearSessionCookie() {
  let c = `${SESSION_COOKIE_NAME}=; Path=/; Max-Age=0; SameSite=Lax`;
  document.cookie = c;
  if (location.protocol === "https:") {
    c = `${SESSION_COOKIE_NAME}=; Path=/; Max-Age=0; SameSite=Lax; Secure`;
    document.cookie = c;
  }
}

function normalizeCustomId(raw) {
  let s = String(raw).trim();
  if (s.startsWith("0x") || s.startsWith("0X")) s = s.slice(2);
  s = s.replace(/\s/g, "").toUpperCase();
  if (!/^[0-9A-F]*$/.test(s)) {
    throw new Error("UID must contain only hexadecimal characters (0–9, A–F).");
  }
  if (s.length === 0) throw new Error("Enter your UID.");
  if (s.length > 16) {
    throw new Error("UID is longer than 16 hex digits. Use the same 16-digit ID as the game.");
  }
  while (s.length < 16) s = "0" + s;
  return s;
}

function playFabUrl(path) {
  return `https://${TITLE_ID}.playfabapi.com${path}`;
}

async function playFabPost(path, body, sessionTicket) {
  const headers = { "Content-Type": "application/json" };
  if (sessionTicket) headers["X-Authorization"] = sessionTicket;

  const res = await fetch(playFabUrl(path), {
    method: "POST",
    headers,
    body: JSON.stringify(body),
  });

  const data = await res.json().catch(() => ({}));
  const apiCode = typeof data.code === "number" ? data.code : res.ok ? 200 : res.status;
  if (apiCode !== 200) {
    throw new Error(data.errorMessage || data.error || `Request failed (${apiCode})`);
  }
  if (!res.ok && !data.data) {
    throw new Error(data.errorMessage || data.error || `HTTP ${res.status}`);
  }
  return data;
}

let sessionTicket = null;
let playFabId = null;
let customIdDisplay = null;
let displayUsername = null;

function showError(el, message) {
  if (!el) return;
  el.textContent = message;
  el.classList.remove("hidden");
}

function clearError(el) {
  if (!el) return;
  el.textContent = "";
  el.classList.add("hidden");
}

/** Apply signed-in state to whichever layout this page uses. */
function setSignedInUi() {
  const login = $("screen-login");
  const dash = $("screen-dashboard");
  const guest = $("screen-guest");
  const app = $("screen-app");
  if (login) login.classList.add("hidden");
  if (dash) dash.classList.remove("hidden");
  if (guest) guest.classList.add("hidden");
  if (app) app.classList.remove("hidden");
  const su = $("signed-username");
  if (su) su.textContent = displayUsername || "—";
}

function setSignedOutUi() {
  sessionTicket = null;
  playFabId = null;
  customIdDisplay = null;
  displayUsername = null;
  sessionStorage.removeItem("pf_session");
  clearSessionCookie();

  const login = $("screen-login");
  const dash = $("screen-dashboard");
  const guest = $("screen-guest");
  const app = $("screen-app");
  if (login) login.classList.remove("hidden");
  if (dash) dash.classList.add("hidden");
  if (guest) guest.classList.remove("hidden");
  if (app) app.classList.add("hidden");

  const fl = $("friend-list");
  if (fl) fl.innerHTML = "";
  const fr = $("friend-requests-list");
  if (fr) fr.innerHTML = "";
}

function persistSession() {
  const json = JSON.stringify({
    sessionTicket,
    playFabId,
    customIdDisplay,
    displayUsername,
  });
  sessionStorage.setItem("pf_session", json);
  writeSessionCookie(json);
}

function bufferToBase64Url(buf) {
  const bytes = new Uint8Array(buf);
  let s = "";
  for (let i = 0; i < bytes.length; i++) s += String.fromCharCode(bytes[i]);
  return btoa(s).replace(/\+/g, "-").replace(/\//g, "_").replace(/=+$/u, "");
}

async function sha256Base64Url(plain) {
  const data = new TextEncoder().encode(plain);
  const hash = await crypto.subtle.digest("SHA-256", data);
  return bufferToBase64Url(hash);
}

function randomPkceVerifier() {
  const a = new Uint8Array(32);
  crypto.getRandomValues(a);
  return bufferToBase64Url(a);
}

async function startCloudOAuth(provider) {
  const clientId = OAUTH_CLIENT_IDS[provider];
  if (!clientId || !String(clientId).trim()) {
    throw new Error(`Set OAUTH_CLIENT_IDS.${provider} in docs/shared.js for this provider.`);
  }
  const codeVerifier = randomPkceVerifier();
  const codeChallenge = await sha256Base64Url(codeVerifier);
  sessionStorage.setItem("lce_pkce_" + provider, codeVerifier);

  const redir = encodeURIComponent(OAUTH_REDIRECT_URI);
  const st = encodeURIComponent(provider);
  const ch = encodeURIComponent(codeChallenge);

  let url;
  if (provider === "google") {
    const scope = encodeURIComponent("https://www.googleapis.com/auth/drive.file");
    url = `https://accounts.google.com/o/oauth2/v2/auth?client_id=${encodeURIComponent(
      clientId
    )}&redirect_uri=${redir}&response_type=code&scope=${scope}&state=${st}&code_challenge=${ch}&code_challenge_method=S256&access_type=offline&prompt=consent`;
  } else if (provider === "microsoft") {
    const scope = encodeURIComponent("offline_access Files.ReadWrite User.Read");
    url = `https://login.microsoftonline.com/common/oauth2/v2.0/authorize?client_id=${encodeURIComponent(
      clientId
    )}&redirect_uri=${redir}&response_type=code&scope=${scope}&state=${st}&code_challenge=${ch}&code_challenge_method=S256&prompt=consent`;
  } else if (provider === "dropbox") {
    url = `https://www.dropbox.com/oauth2/authorize?client_id=${encodeURIComponent(
      clientId
    )}&redirect_uri=${redir}&response_type=code&state=${st}&code_challenge=${ch}&code_challenge_method=S256&token_access_type=offline`;
  } else {
    throw new Error("Unknown provider");
  }

  const w = window.open(url, "lce_oauth", "width=520,height=720,noopener,noreferrer");
  if (!w) throw new Error("Popup blocked — allow popups for this site.");
}

async function onOAuthMessage(ev) {
  if (ev.origin !== location.origin) return;
  const d = ev.data;
  if (!d || d.type !== "lce-oauth") return;
  const errEl = $("cloud-error") || $("app-error");
  clearError(errEl);
  if (d.error) {
    showError(errEl, d.error_description || d.error || "OAuth error");
    return;
  }
  const provider = d.state;
  if (!provider || !["google", "microsoft", "dropbox"].includes(provider)) {
    showError(errEl, "Invalid OAuth state.");
    return;
  }
  const codeVerifier = sessionStorage.getItem("lce_pkce_" + provider);
  if (!codeVerifier) {
    showError(errEl, "PKCE verifier missing — try Connect again.");
    return;
  }
  if (!d.code) {
    showError(errEl, "No authorization code returned.");
    return;
  }
  try {
    await executeCloudScript("LCE_CloudSave_OAuthComplete", {
      provider,
      code: d.code,
      redirectUri: OAUTH_REDIRECT_URI,
      codeVerifier,
    });
    sessionStorage.removeItem("lce_pkce_" + provider);
    await refreshCloudSavePanel();
  } catch (e) {
    showError(errEl, e.message || String(e));
  }
}

async function refreshCloudSavePanel() {
  const statusEl = $("cloud-save-status");
  const unlinkBtn = $("btn-cloud-unlink");
  if (!statusEl) return;
  if (!sessionTicket) {
    statusEl.textContent = "";
    if (unlinkBtn) unlinkBtn.classList.add("hidden");
    return;
  }
  statusEl.textContent = "Loading cloud save status…";
  try {
    const cfg = await executeCloudScript("LCE_CloudSave_GetConfig", {});
    const en = cfg && (cfg.enabled === true || cfg.enabled === "true");
    const prov = cfg && (cfg.provider || "");
    const last = cfg && (cfg.lastSyncUtc || "");
    if (en && prov) {
      statusEl.textContent = `Linked: ${prov}` + (last ? `. Last sync: ${last}` : ".");
      if (unlinkBtn) unlinkBtn.classList.remove("hidden");
    } else {
      statusEl.textContent = "Not linked. Choose a provider below.";
      if (unlinkBtn) unlinkBtn.classList.add("hidden");
    }
  } catch {
    statusEl.textContent = "Could not load cloud status (deploy CloudScript?).";
    if (unlinkBtn) unlinkBtn.classList.add("hidden");
  }
}

async function unlinkCloudSave() {
  const errEl = $("cloud-error") || $("app-error");
  clearError(errEl);
  try {
    await executeCloudScript("LCE_CloudSave_Unlink", {});
    await refreshCloudSavePanel();
  } catch (e) {
    showError(errEl, e.message || String(e));
  }
}

async function fetchDisplayUsername() {
  const data = await playFabPost("/Client/GetAccountInfo", {}, sessionTicket);
  const acc = (data.data && (data.data.AccountInfo || data.data.accountInfo)) || {};
  const titleInfo = acc.TitleInfo || acc.titleInfo || {};
  const titleName = titleInfo.DisplayName || titleInfo.displayName;
  const userName = acc.Username || acc.username;
  return (titleName && String(titleName).trim()) || (userName && String(userName).trim()) || "Player";
}

function tryRestoreSession() {
  try {
    const raw = sessionStorage.getItem("pf_session") || readSessionCookieRaw();
    if (!raw) return false;
    const o = JSON.parse(raw);
    if (o.sessionTicket && o.playFabId) {
      sessionTicket = o.sessionTicket;
      playFabId = o.playFabId;
      customIdDisplay = o.customIdDisplay || "";
      displayUsername = o.displayUsername || null;
      setSignedInUi();
      persistSession();
      if (!displayUsername) {
        fetchDisplayUsername()
          .then((name) => {
            displayUsername = name;
            const su = $("signed-username");
            if (su) su.textContent = displayUsername;
            persistSession();
          })
          .catch(() => {});
      }
      return true;
    }
  } catch {
    sessionStorage.removeItem("pf_session");
    clearSessionCookie();
  }
  return false;
}

async function loginFromHomePage() {
  const errEl = $("login-error");
  clearError(errEl);
  const btn = $("btn-login");
  if (btn) btn.disabled = true;

  try {
    const uidInput = $("uid");
    if (!uidInput) throw new Error("Missing UID field.");
    const customId = normalizeCustomId(uidInput.value);
    const data = await playFabPost("/Client/LoginWithCustomID", {
      TitleId: TITLE_ID,
      CustomId: customId,
      CreateAccount: true,
    });

    const d = data.data;
    const ticket = d && (d.SessionTicket || d.sessionTicket);
    const pfid = d && (d.PlayFabId || d.playFabId);
    if (!d || !ticket || !pfid) {
      throw new Error("Unexpected login response.");
    }

    sessionTicket = ticket;
    playFabId = pfid;
    customIdDisplay = customId;
    try {
      displayUsername = await fetchDisplayUsername();
    } catch {
      displayUsername = "Player";
    }
    persistSession();
    setSignedInUi();
  } catch (e) {
    showError(errEl, e.message || String(e));
  } finally {
    if (btn) btn.disabled = false;
  }
}

async function executeCloudScript(functionName, functionParameter) {
  const data = await playFabPost(
    "/Client/ExecuteCloudScript",
    {
      FunctionName: functionName,
      FunctionParameter: functionParameter || {},
    },
    sessionTicket
  );
  const d = data.data || {};
  const err = d.Error || d.error;
  if (err && (err.Message || err.message)) {
    throw new Error(err.Message || err.message);
  }
  return d.FunctionResult ?? d.functionResult;
}

async function addFriendPlayFabByName(name) {
  const attempts = [{ FriendTitleDisplayName: name }, { FriendUsername: name }];
  let lastErr = null;
  for (const body of attempts) {
    try {
      await playFabPost("/Client/AddFriend", body, sessionTicket);
      return;
    } catch (e) {
      lastErr = e;
    }
  }
  throw lastErr || new Error("Could not add friend.");
}

async function getFriendsListArray() {
  const data = await playFabPost(
    "/Client/GetFriendsList",
    {
      IncludeFacebookFriends: false,
      IncludeSteamFriends: false,
    },
    sessionTicket
  );
  const d = data.data || {};
  return d.Friends || d.friends || [];
}

function findFriendPlayFabIdByTypedName(friends, typed) {
  const q = typed.trim().toLowerCase();
  if (!q) return null;
  let byUsername = null;
  for (const f of friends) {
    const pfid = f.FriendPlayFabId || f.friendPlayFabId;
    if (!pfid) continue;
    const td = String(f.TitleDisplayName || f.titleDisplayName || "")
      .trim()
      .toLowerCase();
    if (td && td === q) return pfid;
    const un = String(f.Username || f.username || "")
      .trim()
      .toLowerCase();
    if (un && un === q) byUsername = byUsername || pfid;
  }
  return byUsername;
}

async function notifyFriendRequestViaCloudScript(typedName) {
  let friends = await getFriendsListArray();
  let targetId = findFriendPlayFabIdByTypedName(friends, typedName);
  if (!targetId) {
    await new Promise((r) => setTimeout(r, 450));
    friends = await getFriendsListArray();
    targetId = findFriendPlayFabIdByTypedName(friends, typedName);
  }
  if (!targetId || targetId === playFabId) return;
  await executeCloudScript("LCE_SendFriendRequest", {
    FriendPlayFabId: targetId,
    FriendUsername: typedName,
  });
}

async function refreshSocial() {
  const ae = $("app-error");
  clearError(ae);
  if ($("friend-requests-list")) await loadIncomingFriendRequests();
  if ($("friend-list")) await loadFriends();
}

async function loadIncomingFriendRequests() {
  const listEl = $("friend-requests-list");
  if (!listEl) return;
  listEl.innerHTML = "";

  let incoming = [];
  try {
    const result = await executeCloudScript("LCE_GetIncomingFriendRequests", {});
    if (result && typeof result === "object") {
      incoming = result.Incoming || result.incoming || [];
    }
  } catch {
    /* CloudScript not deployed */
  }

  if (!Array.isArray(incoming) || incoming.length === 0) {
    listEl.innerHTML = '<p class="empty">No incoming requests  :(</p>';
    return;
  }

  const ul = document.createElement("ul");
  ul.className = "friend-request-list";

  for (const r of incoming) {
    const fromId = r.FromPlayFabId || r.fromPlayFabId;
    if (!fromId) continue;

    const li = document.createElement("li");
    const meta = document.createElement("div");
    meta.className = "friend-meta";
    const nameSpan = document.createElement("span");
    nameSpan.className = "name";
    nameSpan.textContent =
      r.FromDisplayName ||
      r.fromDisplayName ||
      r.FromUsername ||
      r.fromUsername ||
      fromId;
    meta.append(nameSpan);

    const actions = document.createElement("div");
    actions.className = "friend-request-actions";

    const acceptBtn = document.createElement("button");
    acceptBtn.type = "button";
    acceptBtn.className = "btn-icon btn-accept";
    acceptBtn.setAttribute("aria-label", "Accept friend request");
    acceptBtn.textContent = "\u2713";
    acceptBtn.addEventListener("click", () => acceptFriendRequest(fromId));

    const declineBtn = document.createElement("button");
    declineBtn.type = "button";
    declineBtn.className = "btn-icon btn-decline";
    declineBtn.setAttribute("aria-label", "Decline friend request");
    declineBtn.textContent = "\u2717";
    declineBtn.addEventListener("click", () => declineFriendRequest(fromId));

    actions.append(acceptBtn, declineBtn);
    li.append(meta, actions);
    ul.appendChild(li);
  }

  listEl.appendChild(ul);
}

async function fetchMutualByFriendId() {
  try {
    const r = await executeCloudScript("LCE_GetFriendsMutualFlags", {});
    if (r && typeof r === "object") {
      return r.MutualById || r.mutualById || {};
    }
    return {};
  } catch {
    return null;
  }
}

function isMutualFriend(friendPlayFabId, mutualById) {
  if (mutualById == null) return true;
  if (Object.prototype.hasOwnProperty.call(mutualById, friendPlayFabId)) {
    return mutualById[friendPlayFabId] === true;
  }
  return true;
}

async function loadFriends() {
  const errEl = $("app-error");
  const listEl = $("friend-list");
  if (!listEl) return;
  listEl.innerHTML = "";

  try {
    const [data, mutualById] = await Promise.all([
      playFabPost(
        "/Client/GetFriendsList",
        {
          IncludeFacebookFriends: false,
          IncludeSteamFriends: false,
        },
        sessionTicket
      ),
      fetchMutualByFriendId(),
    ]);

    const d = data.data || {};
    const friends = d.Friends || d.friends || [];
    if (friends.length === 0) {
      listEl.innerHTML = '<p class="empty">No friends yet  :(</p>';
      return;
    }

    const ul = document.createElement("ul");
    ul.className = "friend-list";

    for (const f of friends) {
      const pfid = f.FriendPlayFabId || f.friendPlayFabId;
      const li = document.createElement("li");
      const meta = document.createElement("div");
      meta.className = "friend-meta";
      const nameSpan = document.createElement("span");
      nameSpan.className = "name";
      nameSpan.textContent =
        f.TitleDisplayName ||
        f.titleDisplayName ||
        f.Username ||
        f.username ||
        "Friend";

      meta.append(nameSpan);
      if (!isMutualFriend(pfid, mutualById)) {
        const pending = document.createElement("span");
        pending.className = "friend-status pending";
        pending.textContent = "Pending";
        meta.append(pending);
      }

      const removeBtn = document.createElement("button");
      removeBtn.type = "button";
      removeBtn.className = "btn-danger";
      removeBtn.textContent = "Remove";
      removeBtn.addEventListener("click", () => removeFriend(pfid));

      li.append(meta, removeBtn);
      ul.appendChild(li);
    }

    listEl.appendChild(ul);
  } catch (e) {
    showError(errEl, e.message || String(e));
  }
}

async function addFriend() {
  const errEl = $("app-error");
  const input = $("friend-username");
  clearError(errEl);
  if (!input) return;
  const username = input.value.trim();
  if (!username) {
    showError(errEl, "Enter a Minecraft username.");
    return;
  }

  const btn = $("btn-add-friend");
  if (btn) btn.disabled = true;

  try {
    await addFriendPlayFabByName(username);
    try {
      await notifyFriendRequestViaCloudScript(username);
    } catch {
      /* optional CloudScript */
    }
    input.value = "";
    await refreshSocial();
  } catch (e) {
    showError(errEl, e.message || String(e));
  } finally {
    if (btn) btn.disabled = false;
  }
}

async function acceptFriendRequest(fromPlayFabId) {
  const errEl = $("app-error");
  clearError(errEl);
  try {
    await executeCloudScript("LCE_AcceptFriendRequest", {
      FromPlayFabId: fromPlayFabId,
    });
    await refreshSocial();
  } catch (e) {
    showError(errEl, e.message || String(e));
  }
}

async function declineFriendRequest(fromPlayFabId) {
  const errEl = $("app-error");
  clearError(errEl);
  try {
    await executeCloudScript("LCE_DeclineFriendRequest", {
      FromPlayFabId: fromPlayFabId,
    });
    try {
      await playFabPost(
        "/Client/RemoveFriend",
        { FriendPlayFabId: fromPlayFabId },
        sessionTicket
      );
    } catch {
      /* */
    }
    await refreshSocial();
  } catch (e) {
    showError(errEl, e.message || String(e));
  }
}

async function removeFriend(friendPlayFabId) {
  const errEl = $("app-error");
  clearError(errEl);

  try {
    await playFabPost(
      "/Client/RemoveFriend",
      { FriendPlayFabId: friendPlayFabId },
      sessionTicket
    );
    try {
      await executeCloudScript("LCE_NotifyFriendRemoved", {
        OtherPlayFabId: friendPlayFabId,
      });
    } catch {
      /* */
    }
    await refreshSocial();
  } catch (e) {
    showError(errEl, e.message || String(e));
  }
}

function logout() {
  setSignedOutUi();
  clearError($("login-error"));
  clearError($("app-error"));
  clearError($("cloud-error"));
}
