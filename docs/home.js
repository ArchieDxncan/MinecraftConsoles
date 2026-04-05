/**
 * Home page only — sign in and dashboard links.
 */
function initHomePage() {
  const btnLogin = $("btn-login");
  const uid = $("uid");
  if (btnLogin) btnLogin.addEventListener("click", () => loginFromHomePage());
  if (uid) {
    uid.addEventListener("keydown", (e) => {
      if (e.key === "Enter") loginFromHomePage();
    });
  }

  const btnLogout = $("btn-logout");
  if (btnLogout) {
    btnLogout.addEventListener("click", () => {
      logout();
    });
  }

  if (tryRestoreSession()) {
    /* already signed in — dashboard visible */
  }
}

initHomePage();
