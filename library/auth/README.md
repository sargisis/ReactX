# reactx::auth

Sign-in for a [ReactX](https://github.com/sargisis/ReactX) server: password hashes, sessions in an `HttpOnly` cookie, CSRF tokens, and middleware for [`reactx::api`](../api/README.md). For servers (native builds).

На русском: [README.ru.md](README.ru.md).

```cpp
#include "rx/auth.hpp"
using namespace rx;

auth::Sessions sessions({.secure = true});   // .secure: the site is served over HTTPS

api.post("/login", [&](const net::Request& r) {
    auto body = api::body(r, loginSchema);
    if (!body) return body.error();
    auto user = findUser((*body)["email"].asString());
    if (!user || !auth::verifyPassword((*body)["password"].asString(), user->passwordHash))
        return api::error(401, "wrong email or password");
    auto res = api::ok(json::object({{"name", user->name}}));
    sessions.start(res, user->id);           // Set-Cookie: sid=...; HttpOnly; SameSite=Lax
    return res;
});

api.use(auth::requireLogin(sessions)).use(auth::csrf(sessions));   // the routes below need a session
api.get("/me", [&](const net::Request& r) {
    auto s = sessions.get(r);
    return api::ok(json::object({{"user", s->user}, {"csrf", s->csrf}}));
});
api.post("/logout", [&](const net::Request& r) {
    auto res = api::noContent();
    sessions.end(r, res);
    return res;
});
```

## Add it to a project

It comes with ReactX (since 1.5). Link it to the server's target:

```cmake
target_link_libraries(my_server PRIVATE reactx::auth)
```

## Passwords

```cpp
std::string stored = auth::hashPassword(password);      // "pbkdf2-sha256$600000$<salt>$<hash>": store this, never the password
bool ok = auth::verifyPassword(attempt, stored);          // compared in constant time
if (ok && auth::needsRehash(stored)) save(auth::hashPassword(attempt));   // after raising the rounds
```

PBKDF2-HMAC-SHA256 with a random 16-byte salt and 600,000 rounds (OWASP's advice, 2023): about a third of a second per password, on purpose — guessing is that slow too. A malformed or tampered stored value never matches.

## Sessions

| | |
|---|---|
| `Sessions sessions({...})` | `.cookie = "sid"`, `.idle = 2h` (ends without requests), `.lifetime = 7 days`, `.secure`, `.sameSite = "Lax"`, `.path = "/"` |
| `sessions.start(response, user, data)` | signs in: a new random session id every time (an id planted before sign-in is useless) and its `Set-Cookie` |
| `sessions.get(request)` | the `Session` (`id`, `user`, `data`, `csrf`, `started`) or nothing |
| `sessions.update(request, data)` | replaces the data kept with it |
| `sessions.end(request, response)` | signs out and clears the cookie |
| `sessions.endAll(user)` | "sign out everywhere", e.g. after a password change |

Sessions live in memory (they end when the server stops) and are safe to use from the server's threads.

## Middleware

- `auth::requireLogin(sessions)` — 401 `{"error": "sign in first"}` without a live session.
- `auth::csrf(sessions)` — a request that changes something (not GET/HEAD/OPTIONS) with a session cookie must send the session's token in `X-CSRF-Token`, else 403. Give the page `session.csrf` (e.g. in `GET /api/me`) and send it back: `rx::fetch({.method = "POST", ..., .headers = {{"X-CSRF-Token", csrf}}}, ...)`. `SameSite` cookies and `HttpServer`'s Origin check already stop most cross-site requests; this closes the rest.

## Also

`auth::randomToken(bytes)` (URL-safe random text from the kernel — invite links, API keys), `auth::sha256Hex`, `auth::hmacSha256Hex` (signing webhooks), `auth::constantTimeEquals`.

## License

MIT or Apache-2.0, like ReactX.
