// Optional Cloudflare Worker (or similar) — only if you switch the web app to exchange
// OAuth codes in the browser and the provider's token endpoint blocks CORS.
// The shipped flow uses LCE_CloudSave_OAuthComplete in PlayFab CloudScript instead, so this file is not required.
//
// Example route: POST /token with JSON body forwarded to the real token URL (set via Worker secret).

export default {
  async fetch(request, env) {
    if (request.method !== "POST") return new Response("Method not allowed", { status: 405 });
    const url = env.TOKEN_TARGET_URL;
    if (!url) return new Response("TOKEN_TARGET_URL not set", { status: 500 });
    const body = await request.text();
    const r = await fetch(url, {
      method: "POST",
      headers: { "Content-Type": "application/x-www-form-urlencoded" },
      body,
    });
    const t = await r.text();
    return new Response(t, { status: r.status, headers: { "Content-Type": r.headers.get("content-type") || "application/json" } });
  },
};
