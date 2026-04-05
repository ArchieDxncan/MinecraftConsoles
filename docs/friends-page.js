/**
 * Friends page — requires session from home.
 */
function initFriendsPage() {
  const btnLogout = $("btn-logout");
  if (btnLogout) {
    btnLogout.addEventListener("click", () => {
      logout();
    });
  }

  $("btn-add-friend")?.addEventListener("click", addFriend);
  $("friend-username")?.addEventListener("keydown", (e) => {
    if (e.key === "Enter") addFriend();
  });

  if (tryRestoreSession()) {
    refreshSocial();
  }
}

initFriendsPage();
