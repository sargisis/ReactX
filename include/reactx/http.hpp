#pragma once

// Small HTTP/1.1 server (POSIX sockets, C++26) for developing ReactX sites:
// serves the files of a web build and lets you add your own routes (an API
// for the page to fetch from).
//
//   rx::net::HttpServer server("127.0.0.1", 8090);
//   server.get("/api/hello", [](const rx::net::Request&) {
//       return rx::net::Response::json(R"({"text": "hi"})");
//   });
//   server.serveDirectory("build/web");
//   server.start();   // bind and listen
//   server.run();     // until stop() or Ctrl+C
//
// Each connection gets its own thread and is closed after one response.
// Meant for local development, not for the open internet.

#include <atomic>
#include <cstddef>
#include <expected>
#include <filesystem>
#include <flat_map>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "reactx/path.hpp"

namespace rx::net {

using rx::matchPath;  // shared with the router (reactx/path.hpp)
using rx::urlDecode;

struct Request {
    std::string method{};  // "GET", "POST", ...
    std::string path{};    // decoded, without the query: "/api/items"
    std::string query{};   // raw, after '?': "page=2&q=milk"
    std::flat_map<std::string, std::string, std::less<>> headers{};  // names in lower case
    std::string body{};
    std::map<std::string, std::string> pathParams{};  // from a route like "/api/todos/:id"
    std::string target{};  // as sent, not decoded: "/api/a%2Fb?page=2" (what a proxy forwards)

    std::string header(std::string_view name) const;
    // Decoded value of a query parameter: query "q=milk%20tea" -> param("q") == "milk tea".
    std::optional<std::string> param(const std::string& name) const;
    // Value of a path parameter: route "/api/todos/:id", path "/api/todos/7" -> pathParam("id") == "7".
    std::string pathParam(const std::string& name) const;
};

struct Response {
    int status = 200;
    std::string contentType = "text/plain; charset=utf-8";
    std::string body{};
    std::vector<std::pair<std::string, std::string>> headers{};  // extra headers

    static Response text(std::string body, int status = 200);
    static Response html(std::string body, int status = 200);
    static Response json(std::string body, int status = 200);
};

// Why a request can't be handled: the status to answer with and a message.
struct HttpError {
    int status = 400;
    std::string message{};
    bool operator==(const HttpError&) const = default;
};

using Handler = std::function<Response(const Request&)>;

// Another HTTP server to forward requests to: "http://127.0.0.1:5000" or
// "http://api:8000/v1" (the path is put in front of the forwarded one).
struct Upstream {
    std::string host{};
    int port = 80;
    std::string path{};  // "" or "/v1", never ending in '/'
    bool operator==(const Upstream&) const = default;
};

// Parses "http://host[:port][/path]". Plain HTTP only: a proxy for local
// development talks to a backend on the same machine or network.
std::expected<Upstream, std::string> parseUpstream(std::string_view url);

// Sends `request` to `upstream` (HTTP/1.1, one connection per request) and
// returns its answer. 502 if the upstream can't be reached or answers
// nonsense, 504 if it doesn't answer in `timeoutSeconds`.
std::expected<Response, HttpError> forward(const Request& request, const Upstream& upstream, int timeoutSeconds = 30);

class HttpServer {
public:
    // port 0 picks a free port (see port() after start()).
    explicit HttpServer(std::string host = "127.0.0.1", int port = 8080);
    ~HttpServer();
    HttpServer(const HttpServer&) = delete("a server owns its socket and threads");
    HttpServer& operator=(const HttpServer&) = delete("a server owns its socket and threads");

    // Routes are tried before the files. A path can have parameters
    // ("/api/todos/:id") and end with "*" (the rest of the path); exact
    // routes win over patterns.
    void route(std::string method, std::string path, Handler handler);
    void get(std::string path, Handler handler) { route("GET", std::move(path), std::move(handler)); }
    void post(std::string path, Handler handler) { route("POST", std::move(path), std::move(handler)); }
    void put(std::string path, Handler handler) { route("PUT", std::move(path), std::move(handler)); }
    void patch(std::string path, Handler handler) { route("PATCH", std::move(path), std::move(handler)); }
    void del(std::string path, Handler handler) { route("DELETE", std::move(path), std::move(handler)); }

    // Forward every request under `prefix` ("/api": "/api" and "/api/...")
    // to another server, like Vite's server.proxy:
    //   server.proxy("/api", *parseUpstream("http://127.0.0.1:5000"));
    void proxy(std::string prefix, Upstream upstream);

    // Serve files from `root` for GET and HEAD ("/" -> index.html).
    // spaFallback: unknown paths without a file extension get index.html,
    // so a client-side router can handle them.
    void serveDirectory(std::filesystem::path root, bool spaFallback = false);
    // Serve the files under a URL prefix: "/ReactX" -> /ReactX/index.html is
    // root/index.html (like a GitHub Pages project site).
    void setUrlPrefix(std::string prefix);
    // For GET requests that match nothing: this file (relative to the root)
    // with status 404, like GitHub Pages' 404.html.
    void setNotFoundPage(std::filesystem::path file);

    // Print one line per request ("GET /app.wasm 200 145765").
    void setLogging(bool on) { logging_ = on; }

    // Bind + listen. tryStart() returns the reason on failure ("port in use");
    // start() throws std::runtime_error with it.
    std::expected<void, std::string> tryStart();
    void start();
    void run();    // accept connections until stop()
    void stop();   // safe to call from another thread or a signal handler

    int port() const { return port_; }
    const std::string& host() const { return host_; }

    // Builds the response for one request (used by run(); handy for tests).
    Response handle(const Request& request) const;

private:
    void serveConnection(int fd);
    std::optional<Response> serveFile(const Request& request) const;

    std::string host_;
    int port_;
    int listenFd_ = -1;
    std::atomic<bool> running_{false};
    std::atomic<int> active_{0};  // connections being served
    bool logging_ = false;
    std::map<std::pair<std::string, std::string>, Handler> routes_;
    std::filesystem::path root_;
    bool spaFallback_ = false;
    std::string prefix_;
    std::filesystem::path notFound_;
};

// Content type for a file extension (".wasm" -> "application/wasm").
std::string mimeType(std::string_view extension);
// Parses the head of an HTTP request ("GET /x?a=1 HTTP/1.1\r\nHost: ...\r\n\r\n").
// A malformed request gives HttpError{400, what is wrong}.
std::expected<Request, HttpError> parseRequestHead(std::string_view head);
// Length of the body from Content-Length (0 without it): 400 if it isn't a
// number, 413 if it is over `limit` bytes.
std::expected<std::size_t, HttpError> bodyLength(const Request& request, std::size_t limit);
// Parses a whole HTTP/1.x response as a server sent it (status, headers,
// Content-Length, chunked or read-to-close body). `headRequest`: the answer
// to a HEAD request, which has no body. A broken one gives HttpError{502, why}.
std::expected<Response, HttpError> parseResponse(std::string_view raw, bool headRequest = false);
// Serializes a response (headers + body; no body for HEAD requests).
std::string serializeResponse(const Response& response, bool headOnly = false);

}  // namespace rx::net
