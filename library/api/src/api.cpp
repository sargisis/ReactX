// reactx::api: answers, request bodies, middleware and the router. Handlers
// run on the server's connection threads, so shared state (rate limits) is
// locked.
#include "rx/api.hpp"

#include <algorithm>
#include <cstdio>
#include <exception>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

#include "reactx/parse.hpp"

namespace rx::api {

namespace {

std::string join(const std::vector<std::string>& items) {
    std::string out;
    for (const auto& i : items) out += (out.empty() ? "" : ", ") + i;
    return out;
}

std::string_view trim(std::string_view s) {
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) s.remove_prefix(1);
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t')) s.remove_suffix(1);
    return s;
}

Handler chain(const std::vector<Middleware>& middleware, Handler handler) {
    Next next = std::move(handler);
    for (auto it = middleware.rbegin(); it != middleware.rend(); ++it)
        next = [m = *it, next](const Request& r) { return m(r, next); };
    return next;
}

}  // namespace

// ---- answers ----

Response ok(const json::Value& body, int status) { return Response::json(body.dump(), status); }

Response error(int status, std::string_view message) {
    return Response::json(json::object({{"error", std::string(message)}}).dump(), status);
}

Response invalid(const forms::Errors& errors) { return Response::json(forms::JsonSchema::toJson(errors).dump(), 422); }

Response noContent() {
    Response r;
    r.status = 204;
    r.contentType.clear();
    return r;
}

// ---- reading a request ----

std::expected<json::Value, Response> body(const Request& request, const forms::JsonSchema& schema) {
    if (std::string type = request.header("content-type"); !type.empty() && type.find("json") == std::string::npos)
        return std::unexpected(error(415, "expected JSON (Content-Type: application/json)"));
    if (trim(request.body).empty()) return std::unexpected(error(400, "the body is empty: expected JSON"));
    auto parsed = json::tryParse(request.body);
    if (!parsed) return std::unexpected(error(400, "invalid JSON: " + std::string(parsed.error().what())));
    if (!schema.empty())
        if (auto errors = schema.validate(*parsed); !errors.empty()) return std::unexpected(invalid(errors));
    return std::move(*parsed);
}

std::expected<long long, Response> intParam(const Request& request, std::string_view name) {
    const std::string text = request.pathParam(std::string(name));
    auto n = parseNumber<long long>(text);
    if (!n) return std::unexpected(error(400, "\"" + std::string(name) + "\" must be a whole number"));
    return *n;
}

std::string clientAddress(const Request& request, bool trustProxy) {
    if (trustProxy)
        if (std::string forwarded = request.header("x-forwarded-for"); !forwarded.empty()) {
            std::string_view first = trim(std::string_view(forwarded).substr(0, forwarded.find(',')));
            if (!first.empty()) return std::string(first);
        }
    return request.remote.empty() ? std::string("unknown") : request.remote;
}

// ---- middleware ----

Middleware catchErrors() {
    return [](const Request& r, const Next& next) -> Response {
        try {
            return next(r);
        } catch (const json::Error& e) {
            return error(400, e.what());
        } catch (const std::exception& e) {
            std::fprintf(stderr, "api: %s %s: %s\n", r.method.c_str(), r.path.c_str(), e.what());
            return error(500, "internal error");
        }
    };
}

Middleware cors(CorsOptions options) {
    auto o = std::make_shared<const CorsOptions>(std::move(options));
    return [o](const Request& r, const Next& next) -> Response {
        const std::string origin = r.header("origin");
        if (origin.empty()) return next(r);  // same site, curl, a server: nothing to do
        const bool any = std::ranges::find(o->origins, std::string_view("*")) != o->origins.end() && !o->credentials;
        const bool allowed = any || std::ranges::find(o->origins, origin) != o->origins.end();
        const bool preflight = r.method == "OPTIONS" && !r.header("access-control-request-method").empty();
        if (!allowed) return preflight ? error(403, "origin not allowed: " + origin) : next(r);

        Response res = preflight ? noContent() : next(r);
        res.headers.emplace_back("Access-Control-Allow-Origin", any ? std::string("*") : origin);
        if (!any) res.headers.emplace_back("Vary", "Origin");
        if (o->credentials) res.headers.emplace_back("Access-Control-Allow-Credentials", "true");
        if (preflight) {
            res.headers.emplace_back("Access-Control-Allow-Methods", join(o->methods));
            res.headers.emplace_back("Access-Control-Allow-Headers", join(o->headers));
            res.headers.emplace_back("Access-Control-Max-Age", std::to_string(o->maxAge.count()));
        }
        return res;
    };
}

Middleware rateLimit(RateLimitOptions options) {
    struct Window {
        std::chrono::steady_clock::time_point start;
        int count = 0;
    };
    struct State {
        std::mutex mutex;
        std::unordered_map<std::string, Window> clients;
    };
    auto state = std::make_shared<State>();
    auto o = std::make_shared<const RateLimitOptions>(std::move(options));
    return [state, o](const Request& r, const Next& next) -> Response {
        const std::string key = o->key ? o->key(r) : clientAddress(r);
        const auto now = std::chrono::steady_clock::now();
        long long wait = 0;
        {
            std::lock_guard lock(state->mutex);
            if (state->clients.size() > 10000)  // forget clients whose window is over
                std::erase_if(state->clients, [&](const auto& c) { return now - c.second.start >= o->per; });
            Window& w = state->clients[key];
            if (w.count == 0 || now - w.start >= o->per) w = {now, 0};
            if (++w.count > o->requests)
                wait = std::chrono::ceil<std::chrono::seconds>(o->per - (now - w.start)).count();
        }
        if (wait == 0) return next(r);
        Response res = error(429, "too many requests");
        res.headers.emplace_back("Retry-After", std::to_string(std::max(1LL, wait)));
        return res;
    };
}

Middleware log(std::function<void(const std::string&)> sink) {
    auto out = std::make_shared<const std::function<void(const std::string&)>>(std::move(sink));
    return [out](const Request& r, const Next& next) -> Response {
        const auto start = std::chrono::steady_clock::now();
        Response res = next(r);
        const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start);
        std::string line = r.method + " " + r.path + " " + std::to_string(res.status) + " " + std::to_string(ms.count()) + " ms";
        if (*out) (*out)(line);
        else std::fprintf(stderr, "%s\n", line.c_str());
        return res;
    };
}

// ---- the router ----

Router::Router(HttpServer& server, std::string prefix) : server_(server), prefix_(std::move(prefix)) {
    while (!prefix_.empty() && prefix_.back() == '/') prefix_.pop_back();
}

Router& Router::use(Middleware middleware) {
    middleware_.push_back(std::move(middleware));
    return *this;
}

Router& Router::route(std::string method, std::string path, Handler handler) {
    std::string full = prefix_ + (path.starts_with('/') ? path : "/" + path);
    if (full.empty()) full = "/";
    if (method != "OPTIONS" && std::ranges::find(withOptions_, full) == withOptions_.end()) {
        withOptions_.push_back(full);
        server_.route("OPTIONS", full, chain(middleware_, [](const Request&) { return noContent(); }));
    }
    server_.route(std::move(method), std::move(full), chain(middleware_, std::move(handler)));
    return *this;
}

Router& Router::get(std::string path, Handler handler) { return route("GET", std::move(path), std::move(handler)); }
Router& Router::post(std::string path, Handler handler) { return route("POST", std::move(path), std::move(handler)); }
Router& Router::put(std::string path, Handler handler) { return route("PUT", std::move(path), std::move(handler)); }
Router& Router::patch(std::string path, Handler handler) { return route("PATCH", std::move(path), std::move(handler)); }
Router& Router::del(std::string path, Handler handler) { return route("DELETE", std::move(path), std::move(handler)); }

}  // namespace rx::api
