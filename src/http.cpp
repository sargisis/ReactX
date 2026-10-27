// HTTP/1.1 server on POSIX sockets. One thread per connection,
// "Connection: close" after each response. Also a small client for proxying.
#include "reactx/http.hpp"

#include "reactx/contract.hpp"
#include "reactx/parse.hpp"

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <charconv>
#include <cstring>
#include <format>
#include <fstream>
#include <iterator>
#include <limits>
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
        case 202: return "Accepted";
        case 206: return "Partial Content";
        case 303: return "See Other";
        case 304: return "Not Modified";
        case 307: return "Temporary Redirect";
        case 308: return "Permanent Redirect";
        case 400: return "Bad Request";
        case 401: return "Unauthorized";
        case 403: return "Forbidden";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 409: return "Conflict";
        case 410: return "Gone";
        case 413: return "Payload Too Large";
        case 415: return "Unsupported Media Type";
        case 422: return "Unprocessable Content";
        case 429: return "Too Many Requests";
        case 431: return "Request Header Fields Too Large";
        case 500: return "Internal Server Error";
        case 501: return "Not Implemented";
        case 502: return "Bad Gateway";
        case 503: return "Service Unavailable";
        case 504: return "Gateway Timeout";
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
    req.target = std::string(target);
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

// ---- client (proxy) ----

namespace {

constexpr std::size_t kMaxUpstreamReply = 64 * 1024 * 1024;

// Headers that belong to one connection, not to the message (RFC 9110 7.6.1),
// plus the ones the proxy writes itself.
bool hopByHop(std::string_view lowerName) {
    for (std::string_view h : {"connection", "keep-alive", "proxy-connection", "proxy-authenticate", "proxy-authorization",
                               "te", "trailer", "transfer-encoding", "upgrade", "content-length", "host"})
        if (lowerName == h) return true;
    return false;
}

// A connected TCP socket, closed on scope exit.
struct Socket {
    int fd = -1;
    explicit Socket(int f) : fd(f) {}
    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;
    ~Socket() {
        if (fd >= 0) ::close(fd);
    }
};

std::unexpected<HttpError> gateway(int status, std::string message) {
    return std::unexpected(HttpError{status, std::move(message)});
}

std::expected<in_addr, std::string> resolve(const std::string& host) {
    in_addr addr{};
    if (::inet_pton(AF_INET, host.c_str(), &addr) == 1) return addr;
    if (host == "localhost") {
        ::inet_pton(AF_INET, "127.0.0.1", &addr);
        return addr;
    }
    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    addrinfo* found = nullptr;
    if (int rc = ::getaddrinfo(host.c_str(), nullptr, &hints, &found); rc != 0 || !found)
        return std::unexpected(std::format("can't resolve {} ({})", host, ::gai_strerror(rc)));
    addr = reinterpret_cast<sockaddr_in*>(found->ai_addr)->sin_addr;
    ::freeaddrinfo(found);
    return addr;
}

// Decodes a "Transfer-Encoding: chunked" body; nullopt if it is cut off or malformed.
std::optional<std::string> unchunk(std::string_view data) {
    std::string out;
    while (true) {
        auto lineEnd = data.find("\r\n");
        if (lineEnd == std::string_view::npos) return std::nullopt;
        std::string_view sizeText = data.substr(0, lineEnd);
        sizeText = sizeText.substr(0, sizeText.find(';'));  // chunk extensions
        sizeText = trim(sizeText);
        std::size_t size = 0;
        auto [end, ec] = std::from_chars(sizeText.data(), sizeText.data() + sizeText.size(), size, 16);
        if (sizeText.empty() || ec != std::errc{} || end != sizeText.data() + sizeText.size()) return std::nullopt;
        data.remove_prefix(lineEnd + 2);
        if (size == 0) return out;  // trailers, if any, are dropped
        if (size > data.size() || data.size() - size < 2) return std::nullopt;  // no overflow on huge sizes
        out.append(data.substr(0, size));
        data.remove_prefix(size + 2);
    }
}

}  // namespace

namespace {

// Size of the whole reply (head + body) as its head tells it, or nullopt when
// the body runs until the connection closes (or is chunked).
std::optional<std::size_t> replySize(std::string_view data, bool headRequest) {
    std::size_t headEnd = data.find("\r\n\r\n");
    if (headEnd == std::string_view::npos) return std::nullopt;
    std::string_view head = data.substr(0, headEnd);
    int status = head.size() >= 12 ? parseNumber<int>(head.substr(9, 3)).value_or(0) : 0;
    if (headRequest || status == 204 || status == 304 || (status >= 100 && status < 200)) return headEnd + 4;
    std::string lowerHead = lower(head);
    if (lowerHead.find("\r\ntransfer-encoding:") != std::string::npos) return std::nullopt;
    auto at = lowerHead.find("\r\ncontent-length:");
    if (at == std::string::npos) return std::nullopt;
    auto valueStart = at + 17;
    auto valueEnd = lowerHead.find("\r\n", valueStart);
    auto length = parseNumber<std::size_t>(trim(head.substr(valueStart, valueEnd - valueStart)));
    if (!length) return std::nullopt;
    if (*length > std::numeric_limits<std::size_t>::max() - headEnd - 4) return std::numeric_limits<std::size_t>::max();
    return headEnd + 4 + *length;
}

}  // namespace

std::expected<Response, HttpError> parseResponse(std::string_view data, bool headRequest) {
    auto bad = [](std::string why) { return std::unexpected(HttpError{502, std::move(why)}); };
    std::size_t headEnd = data.find("\r\n\r\n");
    if (headEnd == std::string_view::npos) return bad("closed the connection without an answer");

    std::string_view head = data.substr(0, headEnd + 2);
    auto lineEnd = head.find("\r\n");
    std::string_view statusLine = head.substr(0, lineEnd);
    if (!statusLine.starts_with("HTTP/1.") || statusLine.size() < 12) return bad("didn't answer with HTTP/1.x");
    auto status = parseNumber<int>(statusLine.substr(9, 3));
    if (!status || *status < 100 || *status > 599) return bad("bad status line");

    Response r;
    r.status = *status;
    r.contentType.clear();
    bool chunked = false;
    std::string_view rest = head.substr(lineEnd + 2);
    while (!rest.empty()) {
        auto end = rest.find("\r\n");
        std::string_view line = rest.substr(0, end);
        rest = end == std::string_view::npos ? std::string_view{} : rest.substr(end + 2);
        auto colon = line.find(':');
        if (colon == std::string_view::npos || colon == 0) continue;
        std::string name = lower(trim(line.substr(0, colon)));
        std::string value(trim(line.substr(colon + 1)));
        if (name == "transfer-encoding") chunked = lower(value).find("chunked") != std::string::npos;
        if (name == "content-type") r.contentType = std::move(value);
        else if (!hopByHop(name)) r.headers.emplace_back(std::string(trim(line.substr(0, colon))), std::move(value));
    }
    if (r.contentType.empty()) r.contentType = "application/octet-stream";

    std::optional<std::size_t> size = replySize(data, headRequest);
    std::string_view body = data.substr(headEnd + 4);
    if (size && *size == headEnd + 4) return r;  // HEAD, 204, 304, 1xx: no body
    if (chunked) {
        auto decoded = unchunk(body);
        if (!decoded) return bad("a broken chunked reply");
        r.body = std::move(*decoded);
        return r;
    }
    if (size) {
        if (data.size() < *size) return bad("closed the connection mid-reply");
        body = body.substr(0, *size - headEnd - 4);
    }
    r.body = std::string(body);
    return r;
}

std::expected<Upstream, std::string> parseUpstream(std::string_view url) {
    if (url.starts_with("https://")) return std::unexpected("https isn't supported, use http:// (a local backend)");
    if (!url.starts_with("http://")) return std::unexpected(std::format("\"{}\" isn't a URL like http://127.0.0.1:5000", url));
    url.remove_prefix(7);
    Upstream up;
    auto slash = url.find('/');
    std::string_view authority = url.substr(0, slash);
    if (slash != std::string_view::npos) up.path = std::string(url.substr(slash));
    while (up.path.ends_with('/')) up.path.pop_back();
    if (auto colon = authority.rfind(':'); colon != std::string_view::npos) {
        auto port = parsePort(authority.substr(colon + 1));
        if (!port) return std::unexpected(port.error());
        up.port = *port;
        authority = authority.substr(0, colon);
    }
    if (authority.empty()) return std::unexpected("no host in the URL");
    up.host = std::string(authority);
    return up;
}

std::expected<Response, HttpError> forward(const Request& req, const Upstream& up, int timeoutSeconds) {
    REACTX_PRE(timeoutSeconds > 0);
    std::string where = std::format("{}:{}", up.host, up.port);
    auto addr = resolve(up.host);
    if (!addr) return gateway(502, addr.error());

    Socket sock(::socket(AF_INET, SOCK_STREAM, 0));
    if (sock.fd < 0) return gateway(502, std::string("socket() failed: ") + std::strerror(errno));
    timeval timeout{.tv_sec = timeoutSeconds, .tv_usec = 0};
    ::setsockopt(sock.fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof timeout);
    ::setsockopt(sock.fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof timeout);  // Linux: connect() too
    sockaddr_in to{};
    to.sin_family = AF_INET;
    to.sin_port = htons(static_cast<std::uint16_t>(up.port));
    to.sin_addr = *addr;
    if (::connect(sock.fd, reinterpret_cast<sockaddr*>(&to), sizeof to) != 0)
        return gateway(errno == EINPROGRESS || errno == ETIMEDOUT ? 504 : 502,
                       std::format("can't connect to {} ({})", where, std::strerror(errno)));

    std::string target = req.target.empty() ? req.path + (req.query.empty() ? "" : "?" + req.query) : req.target;
    std::string out = std::format("{} {}{} HTTP/1.1\r\nHost: {}\r\nConnection: close\r\n", req.method, up.path, target,
                                  up.port == 80 ? up.host : where);
    for (const auto& [name, value] : req.headers)
        if (!hopByHop(name)) std::format_to(std::back_inserter(out), "{}: {}\r\n", name, value);
    if (auto host = req.header("host"); !host.empty()) std::format_to(std::back_inserter(out), "X-Forwarded-Host: {}\r\n", host);
    if (!req.body.empty() || (req.method != "GET" && req.method != "HEAD"))
        std::format_to(std::back_inserter(out), "Content-Length: {}\r\n", req.body.size());
    out += "\r\n";
    out += req.body;
    if (!sendAll(sock.fd, out)) return gateway(502, std::format("can't send the request to {} ({})", where, std::strerror(errno)));

    // Read the reply: until the body is complete (Content-Length) or the upstream closes.
    std::string data;
    char buf[16384];
    std::optional<std::size_t> wanted;  // whole reply size, once the head says it
    bool headKnown = false;
    bool headRequest = req.method == "HEAD";
    while (!wanted || data.size() < *wanted) {
        ssize_t n = ::recv(sock.fd, buf, sizeof buf, 0);
        if (n < 0 && errno == EINTR) continue;
        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
            return gateway(504, std::format("{} didn't answer in {} s", where, timeoutSeconds));
        if (n <= 0) break;
        data.append(buf, static_cast<std::size_t>(n));
        if (data.size() > kMaxUpstreamReply) return gateway(502, std::format("the reply from {} is over 64 MB", where));
        if (!headKnown && data.find("\r\n\r\n") != std::string::npos) {
            headKnown = true;
            wanted = replySize(data, headRequest);
            if (wanted && *wanted > kMaxUpstreamReply) return gateway(502, std::format("the reply from {} is over 64 MB", where));
        }
    }
    auto reply = parseResponse(data, headRequest);
    if (!reply) return gateway(502, std::format("{}: {}", where, reply.error().message));
    return std::move(*reply);
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

void HttpServer::proxy(std::string prefix, Upstream upstream) {
    REACTX_PRE(prefix.starts_with('/'));
    REACTX_PRE(!upstream.host.empty());
    while (prefix.size() > 1 && prefix.back() == '/') prefix.pop_back();
    Handler handler = [upstream = std::move(upstream)](const Request& req) {
        auto reply = forward(req, upstream);
        return reply ? std::move(*reply) : Response::text("proxy: " + reply.error().message + "\n", reply.error().status);
    };
    for (const char* method : {"GET", "POST", "PUT", "PATCH", "DELETE", "OPTIONS"}) {
        route(method, prefix, handler);
        route(method, prefix == "/" ? "/*" : prefix + "/*", handler);
    }
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
