/**
 * PlayFab friends mini-app for GitHub Pages.
 * Matches Minecraft.Client Win64: LoginWithCustomID with 16 hex digit CustomId.
 * Set TITLE_ID to your PlayFab title (default matches PlayFabConfig.h).
 */
const TITLE_ID = "C7923";

/** Cross-browser-session sign-in (sessionStorage alone clears when the browser closes). */
const SESSION_COOKIE_NAME = "lce_pf_session";
const SESSION_COOKIE_MAX_AGE_SEC = 180 * 24 * 60 * 60; // ~6 months
/** Browsers enforce ~4KB per cookie; skip cookie if payload would exceed this (sessionStorage still used). */
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
  // PlayFab often returns HTTP 200 with code !== 200 in JSON for API errors.
  const apiCode = typeof data.code === "number" ? data.code : res.ok ? 200 : res.status;
  if (apiCode !== 200) {
    throw new Error(
      data.errorMessage || data.error || `Request failed (${apiCode})`
    );
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
  el.textContent = message;
  el.classList.remove("hidden");
}

function clearError(el) {
  el.textContent = "";
  el.classList.add("hidden");
}

function setSignedInUi() {
  $("screen-login").classList.add("hidden");
  $("screen-app").classList.remove("hidden");
  $("signed-username").textContent = displayUsername || "—";
}

function setSignedOutUi() {
  sessionTicket = null;
  playFabId = null;
  customIdDisplay = null;
  displayUsername = null;
  sessionStorage.removeItem("pf_session");
  clearSessionCookie();
  $("screen-app").classList.add("hidden");
  $("screen-login").classList.remove("hidden");
  $("friend-list").innerHTML = "";
  $("friend-requests-list").innerHTML = "";
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

/** Title display name (leaderboard) or PlayFab username — never shown: PlayFabId */
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
    const raw =
      sessionStorage.getItem("pf_session") || readSessionCookieRaw();
    if (!raw) return;
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
            $("signed-username").textContent = displayUsername;
            persistSession();
          })
          .catch(() => {});
      }
      refreshSocial();
    }
  } catch {
    sessionStorage.removeItem("pf_session");
    clearSessionCookie();
  }
}

async function login() {
  const errEl = $("login-error");
  clearError(errEl);
  const btn = $("btn-login");
  btn.disabled = true;

  try {
    const customId = normalizeCustomId($("uid").value);
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
    await refreshSocial();
  } catch (e) {
    showError(errEl, e.message || String(e));
  } finally {
    btn.disabled = false;
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

/**
 * Leaderboards show Title Display Name (Minecraft name). PlayFab account Username is often different.
 * Try FriendTitleDisplayName first, then FriendUsername.
 */
async function addFriendPlayFabByName(name) {
  const attempts = [
    { FriendTitleDisplayName: name },
    { FriendUsername: name },
  ];
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

/** Match the name the user typed to a friend row (title display name or PlayFab username). */
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

/**
 * After AddFriend, PlayFab returns their FriendPlayFabId on your list — use it to queue LCE friend request on their account.
 */
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
  clearError($("app-error"));
  await loadIncomingFriendRequests();
  await loadFriends();
}

async function loadIncomingFriendRequests() {
  const listEl = $("friend-requests-list");
  listEl.innerHTML = "";

  let incoming = [];
  try {
    const result = await executeCloudScript("LCE_GetIncomingFriendRequests", {});
    if (result && typeof result === "object") {
      incoming = result.Incoming || result.incoming || [];
    }
  } catch {
    /* CloudScript not deployed or error — show empty requests */
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

/** Server checks both friend lists; null = CloudScript unavailable (treat everyone as mutual). */
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
  const username = input.value.trim();
  if (!username) {
    showError(errEl, "Enter a Minecraft username.");
    return;
  }

  const btn = $("btn-add-friend");
  btn.disabled = true;

  try {
    await addFriendPlayFabByName(username);
    try {
      await notifyFriendRequestViaCloudScript(username);
    } catch {
      /* CloudScript not deployed or failed — one-way PlayFab friend still added */
    }
    input.value = "";
    await refreshSocial();
  } catch (e) {
    showError(errEl, e.message || String(e));
  } finally {
    btn.disabled = false;
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
      /* They were not on your PlayFab friends list — only the queued request existed */
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
      /* optional: other player won't get re-request if CloudScript missing */
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
}

$("btn-login").addEventListener("click", login);
$("uid").addEventListener("keydown", (e) => {
  if (e.key === "Enter") login();
});

$("btn-logout").addEventListener("click", logout);
$("btn-add-friend").addEventListener("click", addFriend);
$("friend-username").addEventListener("keydown", (e) => {
  if (e.key === "Enter") addFriend();
});

tryRestoreSession();
