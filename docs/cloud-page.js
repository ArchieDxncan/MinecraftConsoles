/**
 * Cloud saves page — OAuth + PlayFab CloudScript.
 */
function initCloudPage() {
  const btnLogout = $("btn-logout");
  if (btnLogout) {
    btnLogout.addEventListener("click", () => {
      logout();
      refreshCloudSavePanel();
    });
  }

  window.addEventListener("message", onOAuthMessage);

  for (const [id, prov] of [
    ["btn-oauth-google", "google"],
    ["btn-oauth-microsoft", "microsoft"],
    ["btn-oauth-dropbox", "dropbox"],
  ]) {
    const b = $(id);
    if (b) {
      b.addEventListener("click", async () => {
        const errEl = $("cloud-error");
        clearError(errEl);
        if (!sessionTicket) {
          showError(errEl, "Sign in on the Home page first.");
          return;
        }
        b.disabled = true;
        try {
          await startCloudOAuth(prov);
        } catch (e) {
          showError(errEl, e.message || String(e));
        } finally {
          b.disabled = false;
        }
      });
    }
  }

  $("btn-cloud-unlink")?.addEventListener("click", unlinkCloudSave);

  if (tryRestoreSession()) {
    refreshCloudSavePanel();
  }
}

initCloudPage();
