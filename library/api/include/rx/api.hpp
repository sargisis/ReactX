// reactx::api — a JSON API on rx::net::HttpServer: routes under a prefix with
// middleware (CORS, rate limits, logging, errors as JSON), request bodies
// checked with reactx::forms, answers in one shape. Native (server) builds.
//
//   rx::net::HttpServer server("127.0.0.1", 8080);
//   rx::api::Router api(server, "/api");
//   api.use(rx::api::catchErrors()).use(rx::api::cors({.origins = {"https://app.example.com"}}));
//
//   const auto newTodo = rx::forms::JsonSchema().text("title", {rx::forms::required()});
//   api.post("/todos", [](const rx::net::Request& r) {
//       auto body = rx::api::body(r, newTodo);
//       if (!body) return body.error();                 // 400 broken JSON, 422 {"errors": {...}}
//       return rx::api::ok(save((*body)["title"].asString()), 201);
//   });
//
// Comes with ReactX (library/api); link it in CMakeLists.txt:
//   target_link_libraries(my_server PRIVATE reactx::api)
#pragma once

#include <chrono>
#include <expected>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

#include "reactx/http.hpp"
#include "reactx/json.hpp"
#include "rx/forms.hpp"

namespace rx::api {

using net::Handler;
using net::HttpServer;
using net::Request;
using net::Response;

// ---- answers: {"...": ...} on success, {"error": "..."} or {"errors": {...}} on failure ----

Response ok(const json::Value& body, int status = 200);
Response error(int status, std::string_view message);  // {"error": "Not found"}
Response invalid(const forms::Errors& errors);         // 422 {"errors": {"title": "Required"}}
Response noContent();                                  // 204

// ---- reading a request ----

// The JSON body, checked against `schema` (an empty schema checks nothing):
// 400 when it isn't JSON, 422 with the field errors when it doesn't fit.
std::expected<json::Value, Response> body(const Request& request, const forms::JsonSchema& schema = {});
// A path parameter as a whole number ("/todos/:id"): 400 when it isn't one.
std::expected<long long, Response> intParam(const Request& request, std::string_view name);
// The client: X-Forwarded-For's first address when `trustProxy`, else Request::remote.
std::string clientAddress(const Request& request, bool trustProxy = false);

// ---- middleware ----

// The rest of the chain (the next middleware, finally the route's handler).
using Next = std::function<Response(const Request&)>;
using Middleware = std::function<Response(const Request&, const Next& next)>;

// Answers an exception with JSON instead of a closed connection: json::Error
// (a field of the wrong type in a handler) -> 400, anything else -> 500
// {"error": "internal error"} (the message goes to stderr, not to the client).
Middleware catchErrors();

struct CorsOptions {
    std::vector<std::string> origins{};  // "https://app.example.com"; "*" = any (not with credentials)
    std::vector<std::string> methods{"GET", "POST", "PUT", "PATCH", "DELETE"};
    std::vector<std::string> headers{"Content-Type", "Authorization"};
    bool credentials = false;  // cookies / Authorization from the other site
    std::chrono::seconds maxAge{600};  // how long a browser may reuse a preflight answer
};
// Lets pages on other sites call the API: answers preflight OPTIONS requests
// and adds Access-Control-* headers. The server refuses POST/PUT/... from a
// foreign Origin unless its host is in HttpServer::allowHosts too.
Middleware cors(CorsOptions options);

struct RateLimitOptions {
    int requests = 60;                         // at most this many ...
    std::chrono::seconds per{60};              // ... in this window, per client
    std::function<std::string(const Request&)> key{};  // the client; default: clientAddress(request)
};
// 429 {"error": "too many requests"} with Retry-After when a client asks too
// often (a fixed window per client; safe from the server's threads).
Middleware rateLimit(RateLimitOptions options = {});

// One line per request: "POST /api/todos 201 3 ms". Default sink: stderr.
Middleware log(std::function<void(const std::string& line)> sink = {});

// ---- a group of routes ----

class Router {
public:
    // Routes go to `server` under `prefix` ("/api": .get("/todos", ...) answers /api/todos).
    explicit Router(HttpServer& server, std::string prefix = "/api");

    // Middleware for the routes added after it, outermost first.
    Router& use(Middleware middleware);

    Router& get(std::string path, Handler handler);
    Router& post(std::string path, Handler handler);
    Router& put(std::string path, Handler handler);
    Router& patch(std::string path, Handler handler);
    Router& del(std::string path, Handler handler);
    // Any method; an OPTIONS route for the path is added too, so cors() can answer preflights.
    Router& route(std::string method, std::string path, Handler handler);

private:
    HttpServer& server_;
    std::string prefix_;
    std::vector<Middleware> middleware_;
    std::vector<std::string> withOptions_;  // paths that already have an OPTIONS route
};

}  // namespace rx::api
