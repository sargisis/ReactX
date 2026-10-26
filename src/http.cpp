// HTTP/1.1 server on POSIX sockets. One thread per connection,
// "Connection: close" after each response.
#include "reactx/http.hpp"

#include "reactx/contract.hpp"
#include "reactx/parse.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstring>
#include <format>
#include <fstream>
#include <iterator>
#include <mutex>
#include <print>
#include <stdexcept>
#include <system_error>
#include <thread>

namespace rx::net {

namespace {

constexpr std::size_t kMaxHead = 64 * 1024;
constexpr std::size_t kMaxBody = 10 * 1024 * 1024;

std::string lower(std::string_view s) {
    std::string out(s);
    for (auto& c : out) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return out;
}

std::string_view trim(std::string_view s) {
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) s.remove_prefix(1);
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t')) s.remove_suffix(1);
    return s;
}

const char* reason(int status) {
    switch (status) {
        case 200: return "OK";
        case 201: return "Created";
        case 204: return "No Content";
        case 301: return "Moved Permanently";
        case 302: return "Found";
        case 304: return "Not Modified";
        case 400: return "Bad Request";
        case 403: return "Forbidden";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 413: return "Payload Too Large";
        case 431: return "Request Header Fields Too Large";
        case 500: return "Internal Server Error";
        default: return "Unknown";
    }
}

bool sendAll(int fd, std::string_view data) {
    while (!data.empty()) {
        ssize_t n = ::send(fd, data.data(), data.size(), MSG_NOSIGNAL);
        if (n < 0) {
            if (errno == EINTR) continue;
            return false;
        }
        data.remove_prefix(static_cast<std::size_t>(n));
    }
    return true;
}

// Is `path` inside `root` (both canonical)?
bool inside(const std::filesystem::path& root, const std::filesystem::path& path) {
    auto [r, p] = std::mismatch(root.begin(), root.end(), path.begin(), path.end());
    return r == root.end();
}

std::mutex logMutex;

// Whole file, or why it can't be read.
std::expected<std::string, std::error_code> readFile(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return std::unexpected(std::make_error_code(std::errc::permission_denied));
    std::string data{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
    if (in.bad()) return std::unexpected(std::make_error_code(std::errc::io_error));
    return data;
}

std::unexpected<HttpError> badRequest(std::string message) { return std::unexpected(HttpError{400, std::move(message)}); }

}  // namespace

// ---- helpers ----

std::string mimeType(std::string_view ext) {
    static const std::flat_map<std::string, std::string, std::less<>> types = {
        {".html", "text/html; charset=utf-8"},  {".htm", "text/html; charset=utf-8"},
        {".js", "text/javascript; charset=utf-8"}, {".mjs", "text/javascript; charset=utf-8"},
        {".css", "text/css; charset=utf-8"},    {".json", "application/json; charset=utf-8"},
        {".wasm", "application/wasm"},          {".map", "application/json; charset=utf-8"},
        {".txt", "text/plain; charset=utf-8"},  {".svg", "image/svg+xml"},
        {".png", "image/png"},                  {".jpg", "image/jpeg"},
        {".jpeg", "image/jpeg"},                {".gif", "image/gif"},
        {".webp", "image/webp"},                {".ico", "image/x-icon"},
        {".woff", "font/woff"},                 {".woff2", "font/woff2"},
    };
    auto it = types.find(lower(ext));
    return it == types.end() ? "application/octet-stream" : it->second;
}


std::string Request::pathParam(const std::string& name) const {
    auto it = pathParams.find(name);
    return it == pathParams.end() ? std::string{} : it->second;
}

std::string Request::header(std::string_view name) const {
    auto it = headers.find(lower(name));
    return it == headers.end() ? std::string{} : it->second;
}

std::optional<std::string> Request::param(const std::string& name) const { return queryParam(query, name); }

Response Response::text(std::string body, int status) {
    return {status, "text/plain; charset=utf-8", std::move(body), {}};
}
Response Response::html(std::string body, int status) {
    return {status, "text/html; charset=utf-8", std::move(body), {}};
}
Response Response::json(std::string body, int status) {
    return {status, "application/json; charset=utf-8", std::move(body), {}};
}

std::expected<Request, HttpError> parseRequestHead(std::string_view head) {
    auto lineEnd = head.find("\r\n");
    if (lineEnd == std::string_view::npos) return badRequest("no request line");
    std::string_view line = head.substr(0, lineEnd);

    auto sp1 = line.find(' ');
    auto sp2 = line.rfind(' ');
    if (sp1 == std::string_view::npos || sp2 == sp1) return badRequest("request line is not \"METHOD target HTTP/1.x\"");
    Request req;
    req.method = std::string(line.substr(0, sp1));
    std::string_view target = line.substr(sp1 + 1, sp2 - sp1 - 1);
    std::string_view version = line.substr(sp2 + 1);
    if (req.method.empty() || !std::ranges::all_of(req.method, [](char c) { return c >= 'A' && c <= 'Z'; }))
        return badRequest("bad method");
    if (!version.starts_with("HTTP/1.")) return badRequest("not HTTP/1.x");
    if (target.empty() || target.front() != '/') return badRequest("target must start with /");

    auto q = target.find('?');
    req.path = urlDecode(target.substr(0, q));
    if (q != std::string_view::npos) req.query = std::string(target.substr(q + 1));
    if (req.path.find('\0') != std::string::npos) return badRequest("NUL in path");

    std::string_view rest = head.substr(lineEnd + 2);
    while (!rest.empty()) {
        auto end = rest.find("\r\n");
        std::string_view h = rest.substr(0, end);
        rest = end == std::string_view::npos ? std::string_view{} : rest.substr(end + 2);
        if (h.empty()) break;
        auto colon = h.find(':');
        if (colon == std::string_view::npos || colon == 0) return badRequest("bad header line");
        req.headers.insert_or_assign(lower(trim(h.substr(0, colon))), std::string(trim(h.substr(colon + 1))));
    }
    return req;
}

std::expected<std::size_t, HttpError> bodyLength(const Request& req, std::size_t limit) {
    auto it = req.headers.find("content-length");
    if (it == req.headers.end()) return 0;
    return parseNumber<std::size_t>(it->second)
        .transform_error([](ParseError e) { return HttpError{400, "Content-Length: " + std::string(describe(e))}; })
        .and_then([limit](std::size_t n) -> std::expected<std::size_t, HttpError> {
            if (n > limit) return std::unexpected(HttpError{413, "body too large"});
            return n;
        });
}

std::string serializeResponse(const Response& r, bool headOnly) {
    std::string out = std::format("HTTP/1.1 {} {}\r\nContent-Type: {}\r\nContent-Length: {}\r\nConnection: close\r\n",
                                  r.status, reason(r.status), r.contentType, r.body.size());
    for (const auto& [name, value] : r.headers) std::format_to(std::back_inserter(out), "{}: {}\r\n", name, value);
    out += "\r\n";
    if (!headOnly) out += r.body;
    return out;
}

// ---- server ----

HttpServer::HttpServer(std::string host, int port) : host_(std::move(host)), port_(port) {}

HttpServer::~HttpServer() {
    stop();
    for (int n = active_.load(); n != 0; n = active_.load()) active_.wait(n);
    if (listenFd_ >= 0) ::close(listenFd_);
}

void HttpServer::route(std::string method, std::string path, Handler handler) {
    REACTX_PRE(!method.empty() && std::ranges::all_of(method, [](char c) { return c >= 'A' && c <= 'Z'; }));
    REACTX_PRE(path.starts_with('/'));
    REACTX_PRE(static_cast<bool>(handler));
    routes_[{std::move(method), std::move(path)}] = std::move(handler);
}

void HttpServer::serveDirectory(std::filesystem::path root, bool spaFallback) {
    root_ = std::filesystem::weakly_canonical(std::move(root));
    spaFallback_ = spaFallback;
}

void HttpServer::setUrlPrefix(std::string prefix) {
    while (!prefix.empty() && prefix.back() == '/') prefix.pop_back();
    prefix_ = std::move(prefix);
}

void HttpServer::setNotFoundPage(std::filesystem::path file) { notFound_ = std::move(file); }

std::expected<void, std::string> HttpServer::tryStart() {
    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return std::unexpected(std::string("socket() failed: ") + std::strerror(errno));
    int yes = 1;
    ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes);

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<std::uint16_t>(port_));
    auto fail = [fd](std::string why) {
        ::close(fd);
        return std::unexpected(std::move(why));
    };
    if (::inet_pton(AF_INET, host_.c_str(), &addr.sin_addr) != 1) return fail("bad IPv4 address " + host_);
    if (::bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof addr) != 0)
        return fail(std::format("can't bind {}:{} ({})", host_, port_, std::strerror(errno)));
    if (::listen(fd, 64) != 0) return fail(std::string("listen() failed: ") + std::strerror(errno));

    socklen_t len = sizeof addr;
    ::getsockname(fd, reinterpret_cast<sockaddr*>(&addr), &len);
    port_ = ntohs(addr.sin_port);
    listenFd_ = fd;
    running_ = true;
    return {};
}

void HttpServer::start() {
    if (auto started = tryStart(); !started) throw std::runtime_error("ReactX http: " + started.error());
}

void HttpServer::stop() { running_ = false; }

void HttpServer::run() {
    if (listenFd_ < 0) start();
    while (running_) {
        pollfd p{.fd = listenFd_, .events = POLLIN, .revents = 0};
        if (::poll(&p, 1, 200) <= 0) continue;  // wake up regularly to check stop()
        int fd = ::accept(listenFd_, nullptr, nullptr);
        if (fd < 0) continue;
        timeval timeout{.tv_sec = 5, .tv_usec = 0};
        ::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof timeout);
        ++active_;
        std::thread([this, fd] {
            serveConnection(fd);
            --active_;
            active_.notify_all();
        }).detach();
    }
    for (int n = active_.load(); n != 0; n = active_.load()) active_.wait(n);  // C++20 atomic wait
}

void HttpServer::serveConnection(int fd) {
    std::string data;
    char buf[8192];
    std::size_t headEnd = std::string::npos;
    Response response;
    bool headOnly = false;
    std::string method = "-", path = "-";

    while ((headEnd = data.find("\r\n\r\n")) == std::string::npos && data.size() <= kMaxHead) {
        ssize_t n = ::recv(fd, buf, sizeof buf, 0);
        if (n <= 0) break;
        data.append(buf, static_cast<std::size_t>(n));
    }

    if (data.empty()) {  // connected and left without a request (port checks)
        ::close(fd);
        return;
    }
    if (headEnd == std::string::npos) {
        response = data.size() > kMaxHead ? Response::text("request head too large\n", 431)
                                          : Response::text("bad request\n", 400);
    } else if (auto req = parseRequestHead(std::string_view(data).substr(0, headEnd + 4)); !req) {
        response = Response::text(req.error().message + "\n", req.error().status);
    } else {
        method = req->method;
        path = req->path;
        headOnly = req->method == "HEAD";
        if (auto length = bodyLength(*req, kMaxBody); !length) {
            response = Response::text(length.error().message + "\n", length.error().status);
        } else {
            req->body = data.substr(headEnd + 4);
            while (req->body.size() < *length) {
                ssize_t n = ::recv(fd, buf, sizeof buf, 0);
                if (n <= 0) break;
                req->body.append(buf, static_cast<std::size_t>(n));
            }
            req->body.resize(std::min(req->body.size(), *length));
            try {
                response = handle(*req);
            } catch (const std::exception& e) {
                response = Response::text(std::string("server error: ") + e.what() + "\n", 500);
            }
        }
    }

    sendAll(fd, serializeResponse(response, headOnly));
    ::shutdown(fd, SHUT_WR);
    ::close(fd);

    if (logging_) {
        std::scoped_lock lock(logMutex);
        std::println("{} {} {} {}", method, path, response.status, response.body.size());
        std::fflush(stdout);
    }
}

Response HttpServer::handle(const Request& req) const {
    std::string method = req.method == "HEAD" ? "GET" : req.method;
    if (auto it = routes_.find({method, req.path}); it != routes_.end()) return it->second(req);

    // Patterns: first those with ":param" only, then those ending in "*"
    // (catch-alls like "/api/*" must not hide "/api/todos/:id").
    bool otherMethod = false;
    for (bool wildcards : {false, true}) {
        for (const auto& [key, handler] : routes_) {
            const auto& [m, pattern] = key;
            if (pattern.find_first_of(":*") == std::string::npos) {
                if (!wildcards && pattern == req.path) otherMethod = true;
                continue;
            }
            if ((pattern.find('*') != std::string::npos) != wildcards) continue;
            std::map<std::string, std::string> params;
            if (!matchPath(pattern, req.path, params)) continue;
            if (m != method) {
                otherMethod = true;
                continue;
            }
            Request withParams = req;
            withParams.pathParams = std::move(params);
            return handler(withParams);
        }
        if (otherMethod) break;  // a more specific pattern exists for another method
    }

    if (method == "GET")
        if (auto file = serveFile(req)) return *file;

    if (method == "GET" && !notFound_.empty() && !root_.empty()) {
        if (auto page = readFile(root_ / notFound_)) {
            Response r = Response::html(std::move(*page), 404);
            r.contentType = mimeType(notFound_.extension().string());
            return r;
        }
    }

    if (otherMethod || (!root_.empty() && method != "GET")) return Response::text("method not allowed\n", 405);
    return Response::text("not found\n", 404);
}

std::optional<Response> HttpServer::serveFile(const Request& req) const {
    namespace fs = std::filesystem;
    if (root_.empty()) return std::nullopt;

    std::string urlPath = req.path;
    if (!prefix_.empty()) {  // "/ReactX/x" -> "/x"; anything outside the prefix isn't ours
        if (urlPath != prefix_ && !urlPath.starts_with(prefix_ + "/")) return std::nullopt;
        urlPath = urlPath.substr(prefix_.size());
        if (urlPath.empty()) urlPath = "/";
    }
    std::error_code ec;
    fs::path target = fs::weakly_canonical(root_ / fs::path(urlPath).relative_path(), ec);
    if (ec || !inside(root_, target)) return Response::text("forbidden\n", 403);  // "../" tricks
    if (fs::is_directory(target, ec)) target /= "index.html";

    if (!fs::is_regular_file(target, ec)) {
        bool looksLikeRoute = fs::path(urlPath).extension().empty();
        if (!spaFallback_ || !looksLikeRoute) return std::nullopt;
        target = root_ / "index.html";
        if (!fs::is_regular_file(target, ec)) return std::nullopt;
    }

    auto body = readFile(target);
    if (!body) return Response::text("forbidden\n", 403);
    Response r;
    r.body = std::move(*body);
    r.contentType = mimeType(target.extension().string());
    r.headers.emplace_back("Cache-Control", "no-cache");
    return r;
}

}  // namespace rx::net
