// Tests for rx::net::HttpServer: parsing helpers and real requests over TCP.
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include "reactx/http.hpp"

using namespace rx::net;
namespace fs = std::filesystem;

namespace {

int failures = 0;
std::vector<std::pair<std::string, std::function<void()>>> tests;

#define CHECK(cond)                                                                   \
    do {                                                                              \
        if (!(cond)) {                                                                \
            ++failures;                                                               \
            std::cerr << "  FAIL " << __FILE__ << ":" << __LINE__ << ": " #cond "\n"; \
        }                                                                             \
    } while (0)

#define TEST(name)                                                            \
    void name();                                                              \
    const bool name##_registered = (tests.emplace_back(#name, name), true);   \
    void name()

// Sends raw bytes to 127.0.0.1:port and returns the whole reply.
std::string rawRequest(int port, const std::string& data) {
    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<std::uint16_t>(port));
    ::inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
    if (::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof addr) != 0) {
        ::close(fd);
        return "connect failed";
    }
    ::send(fd, data.data(), data.size(), MSG_NOSIGNAL);
    std::string reply;
    char buf[4096];
    for (ssize_t n; (n = ::recv(fd, buf, sizeof buf, 0)) > 0;) reply.append(buf, static_cast<std::size_t>(n));
    ::close(fd);
    return reply;
}

struct Reply {
    int status = 0;
    std::string head, body;
    bool hasHeader(const std::string& h) const { return head.find(h) != std::string::npos; }
};

Reply request(int port, const std::string& method, const std::string& target, const std::string& body = {}) {
    std::string data = method + " " + target + " HTTP/1.1\r\nHost: localhost\r\n";
    if (!body.empty()) data += "Content-Length: " + std::to_string(body.size()) + "\r\n";
    data += "\r\n" + body;
    std::string raw = rawRequest(port, data);
    Reply r;
    auto end = raw.find("\r\n\r\n");
    r.head = raw.substr(0, end);
    if (end != std::string::npos) r.body = raw.substr(end + 4);
    if (raw.size() > 12) r.status = std::stoi(raw.substr(9, 3));
    return r;
}

// A temporary site directory with a few files, removed at the end.
struct Site {
    fs::path root = fs::temp_directory_path() / ("reactx_http_test_" + std::to_string(::getpid()));
    Site() {
        fs::create_directories(root / "sub");
        write("index.html", "<h1>home</h1>");
        write("app.wasm", std::string("\0asm\1\0\0\0", 8));
        write("app.js", "console.log(1)");
        write("sub/page.html", "<p>sub</p>");
        write("../reactx_http_secret.txt", "secret");
    }
    ~Site() {
        std::error_code ec;
        fs::remove_all(root, ec);
        fs::remove(root.parent_path() / "reactx_http_secret.txt", ec);
    }
    void write(const std::string& name, const std::string& data) const {
        std::ofstream(root / name, std::ios::binary) << data;
    }
};

// Runs a server on a free port for the duration of a test.
struct Running {
    HttpServer server{"127.0.0.1", 0};
    std::jthread thread;
    explicit Running(const std::function<void(HttpServer&)>& setup) {
        setup(server);
        server.start();
        thread = std::jthread([this] { server.run(); });
    }
    ~Running() { server.stop(); }
    int port() const { return server.port(); }
};

// ---- helpers ----

TEST(parses_request_head) {
    auto req = parseRequestHead("GET /a%20b/c?x=1&y=two+words HTTP/1.1\r\nHost: x\r\nX-Thing:  v \r\n\r\n");
    CHECK(req.has_value());
    CHECK(req->method == "GET");
    CHECK(req->path == "/a b/c");
    CHECK(req->query == "x=1&y=two+words");
    CHECK(req->header("x-thing") == "v");
    CHECK(req->header("HOST") == "x");
    CHECK(req->param("y") == "two words");
    CHECK(req->param("x") == "1");
    CHECK(!req->param("z").has_value());
}

TEST(rejects_malformed_heads) {
    CHECK(!parseRequestHead("garbage\r\n\r\n"));
    CHECK(!parseRequestHead("get / HTTP/1.1\r\n\r\n"));        // lower-case method
    CHECK(!parseRequestHead("GET nopath HTTP/1.1\r\n\r\n"));
    CHECK(!parseRequestHead("GET / SPDY/3\r\n\r\n"));
    CHECK(!parseRequestHead("GET / HTTP/1.1\r\nNoColon\r\n\r\n"));
}

TEST(malformed_heads_say_why) {  // std::expected carries the reason
    auto bad = parseRequestHead("GET / SPDY/3\r\n\r\n");
    CHECK(!bad && bad.error().status == 400 && bad.error().message == "not HTTP/1.x");
    CHECK(parseRequestHead("GET / HTTP/1.1\r\nNoColon\r\n\r\n").error().message == "bad header line");
}

TEST(body_length_from_content_length) {
    auto with = [](std::string_view value) {
        auto req = parseRequestHead("POST /x HTTP/1.1\r\nContent-Length: " + std::string(value) + "\r\n\r\n");
        return bodyLength(*req, 1000);
    };
    CHECK(bodyLength(*parseRequestHead("GET / HTTP/1.1\r\n\r\n"), 1000) == 0u);
    CHECK(with("12") == 12u);
    CHECK(with("1000") == 1000u);
    CHECK(with("1001").error().status == 413);
    CHECK(with("12abc").error() == (HttpError{400, "Content-Length: unexpected characters after the number"}));
    CHECK(with("-5").error().status == 400);  // std::stoull used to read this as a huge number
    CHECK(with("99999999999999999999999").error().status == 400);
}

TEST(mime_types) {
    CHECK(mimeType(".wasm") == "application/wasm");
    CHECK(mimeType(".HTML") == "text/html; charset=utf-8");
    CHECK(mimeType(".js") == "text/javascript; charset=utf-8");
    CHECK(mimeType(".bin") == "application/octet-stream");
}

TEST(url_decode) {
    CHECK(urlDecode("a%2Fb%zz") == "a/b%zz");
    CHECK(urlDecode("a+b") == "a+b");
    CHECK(urlDecode("a+b", true) == "a b");
    CHECK(urlDecode("%D0%BF%D1%80%D0%B8") == "при");
}

// ---- over TCP ----

TEST(serves_files_with_types) {
    Site site;
    Running r([&](HttpServer& s) { s.serveDirectory(site.root); });
    auto home = request(r.port(), "GET", "/");
    CHECK(home.status == 200);
    CHECK(home.body == "<h1>home</h1>");
    CHECK(home.hasHeader("Content-Type: text/html; charset=utf-8"));
    CHECK(home.hasHeader("Content-Length: 13"));

    auto wasm = request(r.port(), "GET", "/app.wasm");
    CHECK(wasm.status == 200);
    CHECK(wasm.hasHeader("Content-Type: application/wasm"));
    CHECK(wasm.body == std::string("\0asm\1\0\0\0", 8));  // binary-safe

    CHECK(request(r.port(), "GET", "/sub/page.html").body == "<p>sub</p>");
    CHECK(request(r.port(), "GET", "/sub/").status == 404);  // no sub/index.html
    CHECK(request(r.port(), "GET", "/missing.js").status == 404);
}

TEST(head_has_no_body) {
    Site site;
    Running r([&](HttpServer& s) { s.serveDirectory(site.root); });
    auto head = request(r.port(), "HEAD", "/app.js");
    CHECK(head.status == 200);
    CHECK(head.hasHeader("Content-Length: 14"));
    CHECK(head.body.empty());
}

TEST(blocks_path_traversal) {
    Site site;
    Running r([&](HttpServer& s) { s.serveDirectory(site.root); });
    CHECK(request(r.port(), "GET", "/../reactx_http_secret.txt").status == 403);
    CHECK(request(r.port(), "GET", "/sub/../../reactx_http_secret.txt").status == 403);
    CHECK(request(r.port(), "GET", "/%2e%2e/reactx_http_secret.txt").status == 403);
}

TEST(spa_fallback) {
    Site site;
    Running r([&](HttpServer& s) { s.serveDirectory(site.root, true); });
    CHECK(request(r.port(), "GET", "/todos/42").body == "<h1>home</h1>");  // a route: index.html
    CHECK(request(r.port(), "GET", "/missing.js").status == 404);        // a file: still 404
}

TEST(prefix_and_not_found_page_like_github_pages) {
    Site site;
    site.write("404.html", "<p>app</p>");
    Running r([&](HttpServer& s) {
        s.serveDirectory(site.root);
        s.setUrlPrefix("/ReactX/");
        s.setNotFoundPage("404.html");
    });
    CHECK(request(r.port(), "GET", "/ReactX/").body == "<h1>home</h1>");
    CHECK(request(r.port(), "GET", "/ReactX").body == "<h1>home</h1>");
    CHECK(request(r.port(), "GET", "/ReactX/app.js").body == "console.log(1)");
    auto deep = request(r.port(), "GET", "/ReactX/todos/7");
    CHECK(deep.status == 404 && deep.body == "<p>app</p>");  // the app handles it in the browser
    CHECK(deep.hasHeader("Content-Type: text/html"));
    CHECK(request(r.port(), "GET", "/app.js").status == 404);       // outside the prefix
    CHECK(request(r.port(), "GET", "/ReactXtra/app.js").status == 404);
}

TEST(routes_and_methods) {
    Site site;
    Running r([&](HttpServer& s) {
        s.get("/api/hello", [](const Request& q) {
            return Response::json("{\"hello\": \"" + q.param("name").value_or("world") + "\"}");
        });
        s.post("/api/echo", [](const Request& q) { return Response::text(q.body, 201); });
        s.get("/api/fail", [](const Request&) -> Response { throw std::runtime_error("boom"); });
        s.serveDirectory(site.root);
    });
    auto hello = request(r.port(), "GET", "/api/hello?name=ReactX");
    CHECK(hello.status == 200);
    CHECK(hello.body == "{\"hello\": \"ReactX\"}");
    CHECK(hello.hasHeader("Content-Type: application/json"));

    auto echo = request(r.port(), "POST", "/api/echo", "привет, сервер");
    CHECK(echo.status == 201);
    CHECK(echo.body == "привет, сервер");

    CHECK(request(r.port(), "DELETE", "/api/echo").status == 405);
    CHECK(request(r.port(), "POST", "/index.html").status == 405);
    CHECK(request(r.port(), "GET", "/api/fail").status == 500);
}

TEST(try_start_reports_a_busy_port) {
    HttpServer first("127.0.0.1", 0);
    CHECK(first.tryStart().has_value());
    HttpServer second("127.0.0.1", first.port());
    auto again = second.tryStart();
    CHECK(!again.has_value());
    CHECK(again.error().find("can't bind") != std::string::npos);
    HttpServer bad("not-an-ip", 0);
    CHECK(bad.tryStart().error() == "bad IPv4 address not-an-ip");
    bool threw = false;
    try {
        bad.start();
    } catch (const std::runtime_error& e) {
        threw = std::string(e.what()).find("bad IPv4") != std::string::npos;
    }
    CHECK(threw);
}

TEST(path_patterns) {
    std::map<std::string, std::string> p;
    CHECK(matchPath("/api/todos/:id", "/api/todos/7", p) && p["id"] == "7");
    CHECK(matchPath("/u/:name/posts/:post", "/u/ann%20b/posts/3/", p) && p["name"] == "ann b" && p["post"] == "3");
    CHECK(!matchPath("/api/todos/:id", "/api/todos", p));
    CHECK(!matchPath("/api/todos/:id", "/api/todos/7/x", p));
    CHECK(matchPath("/files/*", "/files/a/b.txt", p) && p["*"] == "a/b.txt");
    CHECK(matchPath("/", "/", p));
}

TEST(routes_with_params) {
    Running r([](HttpServer& s) {
        s.get("/api/todos/:id", [](const Request& q) { return Response::text("get " + q.pathParam("id")); });
        s.del("/api/todos/:id", [](const Request& q) { return Response::text("del " + q.pathParam("id")); });
        s.patch("/api/todos/:id", [](const Request& q) { return Response::text("patch " + q.pathParam("id") + " " + q.body); });
        s.get("/api/todos/special", [](const Request&) { return Response::text("exact"); });
    });
    CHECK(request(r.port(), "GET", "/api/todos/7").body == "get 7");
    CHECK(request(r.port(), "DELETE", "/api/todos/7").body == "del 7");
    CHECK(request(r.port(), "PATCH", "/api/todos/3", "{}").body == "patch 3 {}");
    CHECK(request(r.port(), "GET", "/api/todos/special").body == "exact");  // exact wins
    CHECK(request(r.port(), "POST", "/api/todos/7").status == 405);
    CHECK(request(r.port(), "GET", "/api/todos").status == 404);
}

TEST(wildcards_come_last) {
    Running r([](HttpServer& s) {
        s.get("/api/*", [](const Request& q) { return Response::text("any " + q.pathParam("*"), 404); });
        s.get("/api/items/:id", [](const Request& q) { return Response::text("item " + q.pathParam("id")); });
    });
    CHECK(request(r.port(), "GET", "/api/items/5").body == "item 5");
    auto other = request(r.port(), "GET", "/api/whatever/x");
    CHECK(other.status == 404 && other.body == "any whatever/x");
}

TEST(bad_requests) {
    Running r([](HttpServer&) {});
    CHECK(rawRequest(r.port(), "nonsense\r\n\r\n").starts_with("HTTP/1.1 400"));
    CHECK(rawRequest(r.port(), "POST /x HTTP/1.1\r\nContent-Length: 999999999999\r\n\r\n").starts_with("HTTP/1.1 413"));
    CHECK(rawRequest(r.port(), "POST /x HTTP/1.1\r\nContent-Length: ten\r\n\r\n").starts_with("HTTP/1.1 400"));
    CHECK(request(r.port(), "GET", "/").status == 404);  // nothing to serve
}

TEST(many_parallel_requests) {
    Running r([](HttpServer& s) { s.get("/n", [](const Request& q) { return Response::text(q.param("i").value_or("")); }); });
    std::vector<std::jthread> clients;
    std::vector<int> ok(32, 0);
    for (int i = 0; i < 32; ++i)
        clients.emplace_back([&, i] { ok[static_cast<std::size_t>(i)] = request(r.port(), "GET", "/n?i=" + std::to_string(i)).body == std::to_string(i); });
    clients.clear();  // join
    CHECK(std::ranges::count(ok, 1) == 32);
}

}  // namespace

int main() {
    for (auto& [name, fn] : tests) {
        std::cout << name << '\n';
        try {
            fn();
        } catch (const std::exception& e) {
            ++failures;
            std::cerr << "  EXCEPTION: " << e.what() << '\n';
        }
    }
    std::cout << (failures ? "FAILED: " : "OK, all tests passed") << (failures ? std::to_string(failures) : "")
              << '\n';
    return failures ? 1 : 0;
}
