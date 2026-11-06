// reactx::auth — sign-in for a server made with rx::net::HttpServer:
// password hashes (PBKDF2-HMAC-SHA256), sessions in an HttpOnly cookie, CSRF
// tokens, and middleware for reactx::api. Native (server) builds.
//
//   rx::auth::Sessions sessions({.secure = true});           // behind HTTPS
//
//   api.post("/login", [&](const rx::net::Request& r) {
//       auto body = rx::api::body(r, loginSchema);
//       if (!body) return body.error();
//       auto user = findUser((*body)["email"].asString());
//       if (!user || !rx::auth::verifyPassword((*body)["password"].asString(), user->passwordHash))
//           return rx::api::error(401, "wrong email or password");
//       auto res = rx::api::ok(rx::json::object({{"name", user->name}}));
//       sessions.start(res, user->id);                        // Set-Cookie: sid=...; HttpOnly
//       return res;
//   });
//   api.use(rx::auth::requireLogin(sessions));                // the routes after this need a session
//   api.get("/me", [&](const rx::net::Request& r) { return rx::api::ok(profileOf(sessions.get(r)->user)); });
//
// Comes with ReactX (library/auth); link it in CMakeLists.txt:
//   target_link_libraries(my_server PRIVATE reactx::auth)
#pragma once

#include <chrono>
#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "reactx/http.hpp"
#include "reactx/json.hpp"
#include "rx/api.hpp"

namespace rx::auth {

// ---- passwords ----

// A salted PBKDF2-HMAC-SHA256 hash to store instead of the password:
// "pbkdf2-sha256$600000$<salt>$<hash>". 600,000 rounds is OWASP's advice (2023);
// it takes a moment on purpose, so guessing is slow too.
std::string hashPassword(std::string_view password, int iterations = 600000);
// Whether `password` matches a stored hash (compared in constant time). A
// malformed stored value never matches.
bool verifyPassword(std::string_view password, std::string_view stored);
// The stored hash uses fewer rounds than `iterations` (or another format):
// after a successful sign-in, hash the password again and store that.
bool needsRehash(std::string_view stored, int iterations = 600000);

// ---- tokens and hashes ----

// `bytes` random bytes from the operating system, as URL-safe base64 (32 -> 43 characters).
std::string randomToken(std::size_t bytes = 32);
// SHA-256 and HMAC-SHA256, as 64 lower-case hex digits.
std::string sha256Hex(std::string_view data);
std::string hmacSha256Hex(std::string_view key, std::string_view data);
// Equal strings, in a time that doesn't depend on where they differ.
bool constantTimeEquals(std::string_view a, std::string_view b);

// ---- sessions ----

struct Session {
    std::string id{};
    std::string user{};       // who signed in (your user id)
    json::Value data{};       // anything else you keep with it
    std::string csrf{};       // the CSRF token of this session (see csrf())
    std::chrono::system_clock::time_point started{};
};

struct SessionOptions {
    std::string cookie = "sid";
    std::chrono::seconds idle = std::chrono::hours(2);         // ends after this long without a request
    std::chrono::seconds lifetime = std::chrono::days(7);      // and this long after sign-in in any case
    bool secure = false;           // Secure cookie: true when the site is served over HTTPS
    std::string sameSite = "Lax";  // "Lax", "Strict" or "None" (None needs secure)
    std::string path = "/";
};

// Sessions in memory: they end when the server stops. Safe to use from the
// server's threads.
class Sessions {
public:
    explicit Sessions(SessionOptions options = {});
    ~Sessions();
    Sessions(const Sessions&) = delete("sessions are shared: pass a reference");
    Sessions& operator=(const Sessions&) = delete("sessions are shared: pass a reference");

    // Signs `user` in: a new session (a new id every time, so an id planted
    // before sign-in is useless) and its Set-Cookie header on `response`.
    Session start(net::Response& response, std::string user, json::Value data = {});
    // The request's session, if its cookie names a live one (it counts as activity).
    std::optional<Session> get(const net::Request& request);
    // Replaces the data kept with the request's session; false without one.
    bool update(const net::Request& request, json::Value data);
    // Signs out: ends the request's session and clears its cookie on `response`.
    void end(const net::Request& request, net::Response& response);
    // Ends every session of `user` (after a password change: "sign out everywhere").
    void endAll(std::string_view user);
    std::size_t count();

private:
    struct State;
    std::unique_ptr<State> state_;
};

// ---- middleware for reactx::api ----

// 401 {"error": "sign in first"} without a live session.
api::Middleware requireLogin(Sessions& sessions);
// For a request that changes something (not GET/HEAD/OPTIONS) and carries a
// session cookie: the X-CSRF-Token header must be the session's csrf token,
// else 403. Give the page the token (e.g. in GET /api/me) and send it back in
// that header. SameSite cookies and HttpServer's Origin check already stop most
// cross-site requests; this closes the rest.
api::Middleware csrf(Sessions& sessions);

}  // namespace rx::auth
