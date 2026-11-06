// Sessions in memory behind a lock (the server's handlers run on several
// threads), the cookie, and the middleware.
#include <algorithm>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

#include "rx/auth.hpp"

namespace rx::auth {

namespace {

using Clock = std::chrono::system_clock;

std::string_view trim(std::string_view s) {
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) s.remove_prefix(1);
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t')) s.remove_suffix(1);
    return s;
}

// The value of cookie `name` in a Cookie header ("a=1; sid=xyz"), "" if none.
std::string cookieValue(std::string_view header, std::string_view name) {
    while (!header.empty()) {
        const auto semi = header.find(';');
        std::string_view part = trim(header.substr(0, semi));
        header = semi == std::string_view::npos ? std::string_view{} : header.substr(semi + 1);
        const auto eq = part.find('=');
        if (eq != std::string_view::npos && trim(part.substr(0, eq)) == name) return std::string(trim(part.substr(eq + 1)));
    }
    return {};
}

bool unsafeMethod(std::string_view m) { return m != "GET" && m != "HEAD" && m != "OPTIONS"; }

}  // namespace

struct Sessions::State {
    struct Entry {
        Session session;
        Clock::time_point lastSeen;
    };
    std::mutex mutex;
    SessionOptions options;
    std::unordered_map<std::string, Entry> entries;

    bool expired(const Entry& e, Clock::time_point now) const {
        return now - e.lastSeen >= options.idle || now - e.session.started >= options.lifetime;
    }
    std::string cookie(std::string_view value, std::chrono::seconds maxAge) const {
        std::string c = options.cookie + "=" + std::string(value) + "; Path=" + options.path +
                        "; Max-Age=" + std::to_string(maxAge.count()) + "; HttpOnly; SameSite=" + options.sameSite;
        if (options.secure || options.sameSite == "None") c += "; Secure";
        return c;
    }
};

Sessions::Sessions(SessionOptions options) : state_(std::make_unique<State>()) { state_->options = std::move(options); }
Sessions::~Sessions() = default;

Session Sessions::start(net::Response& response, std::string user, json::Value data) {
    Session s{.id = randomToken(32), .user = std::move(user), .data = std::move(data), .csrf = randomToken(32),
              .started = Clock::now()};
    std::lock_guard lock(state_->mutex);
    const auto now = s.started;
    std::erase_if(state_->entries, [&](const auto& e) { return state_->expired(e.second, now); });
    state_->entries.insert_or_assign(s.id, State::Entry{s, now});
    response.headers.emplace_back("Set-Cookie", state_->cookie(s.id, state_->options.lifetime));
    response.headers.emplace_back("Cache-Control", "no-store");
    return s;
}

std::optional<Session> Sessions::get(const net::Request& request) {
    const std::string id = cookieValue(request.header("cookie"), state_->options.cookie);
    if (id.empty()) return std::nullopt;
    std::lock_guard lock(state_->mutex);
    auto it = state_->entries.find(id);
    if (it == state_->entries.end()) return std::nullopt;
    const auto now = Clock::now();
    if (state_->expired(it->second, now)) {
        state_->entries.erase(it);
        return std::nullopt;
    }
    it->second.lastSeen = now;
    return it->second.session;
}

bool Sessions::update(const net::Request& request, json::Value data) {
    const std::string id = cookieValue(request.header("cookie"), state_->options.cookie);
    std::lock_guard lock(state_->mutex);
    auto it = state_->entries.find(id);
    if (id.empty() || it == state_->entries.end()) return false;
    it->second.session.data = std::move(data);
    return true;
}

void Sessions::end(const net::Request& request, net::Response& response) {
    const std::string id = cookieValue(request.header("cookie"), state_->options.cookie);
    std::lock_guard lock(state_->mutex);
    if (!id.empty()) state_->entries.erase(id);
    response.headers.emplace_back("Set-Cookie", state_->cookie("", std::chrono::seconds(0)));
}

void Sessions::endAll(std::string_view user) {
    std::lock_guard lock(state_->mutex);
    std::erase_if(state_->entries, [&](const auto& e) { return e.second.session.user == user; });
}

std::size_t Sessions::count() {
    std::lock_guard lock(state_->mutex);
    const auto now = Clock::now();
    std::erase_if(state_->entries, [&](const auto& e) { return state_->expired(e.second, now); });
    return state_->entries.size();
}

api::Middleware requireLogin(Sessions& sessions) {
    return [&sessions](const net::Request& r, const api::Next& next) -> net::Response {
        if (!sessions.get(r)) return api::error(401, "sign in first");
        return next(r);
    };
}

api::Middleware csrf(Sessions& sessions) {
    return [&sessions](const net::Request& r, const api::Next& next) -> net::Response {
        if (unsafeMethod(r.method))
            if (auto s = sessions.get(r); s && !constantTimeEquals(r.header("x-csrf-token"), s->csrf))
                return api::error(403, "missing or wrong CSRF token (send it in X-CSRF-Token)");
        return next(r);
    };
}

}  // namespace rx::auth
