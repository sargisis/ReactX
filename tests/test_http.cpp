// Tests for rx::net::HttpServer: parsing helpers and real requests over TCP.
#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <chrono>
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
        std::filesystem::create_directories((root / name).parent_path());
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
    // urlEncode: one query value or path segment; urlDecode gives it back.
    CHECK(rx::urlEncode("milk & tea/2") == "milk%20%26%20tea%2F2");
    CHECK(rx::urlEncode("a-b.c_d~E9") == "a-b.c_d~E9");
    CHECK(rx::urlEncode("при") == "%D0%BF%D1%80%D0%B8");
    CHECK(rx::urlEncode("") == "");
    for (const std::string& s : std::vector<std::string>{"q=1&x=%zz", "a+b c", std::string("\0\xff/?#", 6), "при вет"})
        CHECK(urlDecode(rx::urlEncode(s), true) == s);
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
    site.write("docs/faq.html", "<p>faq</p>");                          // a prerendered route
    CHECK(request(r.port(), "GET", "/docs/faq").body == "<p>faq</p>");
    site.write("200.html", "<p>app</p>");                               // a prerendered site's empty page
    CHECK(request(r.port(), "GET", "/todos/42").body == "<p>app</p>");
    CHECK(request(r.port(), "GET", "/").body == "<h1>home</h1>");
}

TEST(on_file_changes_served_files) {
    Site site;
    site.write("404.html", "<p>gone</p>");
    Running r([&](HttpServer& s) {
        s.serveDirectory(site.root, true);
        s.setNotFoundPage("404.html");
        s.onFile([](const std::filesystem::path& file, Response& res) {
            if (res.contentType.starts_with("text/html")) res.body += "<!--dev-->";
            if (file.extension() == ".wasm") res.headers.emplace_back("X-Kind", "wasm");
        });
    });
    auto home = request(r.port(), "GET", "/");
    CHECK(home.body == "<h1>home</h1><!--dev-->" && home.hasHeader("Content-Length: 23"));  // the length follows the body
    CHECK(request(r.port(), "GET", "/todos/1").body == "<h1>home</h1><!--dev-->");       // SPA fallback too
    CHECK(request(r.port(), "GET", "/app.wasm").hasHeader("X-Kind: wasm"));
    CHECK(request(r.port(), "GET", "/missing.js").body == "<p>gone</p><!--dev-->");      // the not-found page too
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

// ---- proxy ----

// Accepts one connection on a free port, reads the request head and answers
// with `reply` (or nothing, if `reply` is empty, until the test ends).
struct OneShotServer {
    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    int port = 0;
    std::jthread thread;
    explicit OneShotServer(std::string reply) {
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        ::inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
        ::bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof addr);
        ::listen(fd, 1);
        socklen_t len = sizeof addr;
        ::getsockname(fd, reinterpret_cast<sockaddr*>(&addr), &len);
        port = ntohs(addr.sin_port);
        thread = std::jthread([this, reply = std::move(reply)](std::stop_token stop) {
            int c = ::accept(fd, nullptr, nullptr);
            if (c < 0) return;
            std::string head;
            char buf[1024];
            while (head.find("\r\n\r\n") == std::string::npos) {
                ssize_t n = ::recv(c, buf, sizeof buf, 0);
                if (n <= 0) break;
                head.append(buf, static_cast<std::size_t>(n));
            }
            if (!reply.empty()) ::send(c, reply.data(), reply.size(), MSG_NOSIGNAL);
            else
                while (!stop.stop_requested()) std::this_thread::sleep_for(std::chrono::milliseconds(20));
            ::close(c);
        });
    }
    ~OneShotServer() {
        thread.request_stop();
        ::shutdown(fd, SHUT_RDWR);
        ::close(fd);
    }
};

TEST(parses_upstream_urls) {
    CHECK((parseUpstream("http://127.0.0.1:5000") == Upstream{"127.0.0.1", 5000, ""}));
    CHECK((parseUpstream("http://api/v1/") == Upstream{"api", 80, "/v1"}));
    CHECK((parseUpstream("http://localhost:8000/") == Upstream{"localhost", 8000, ""}));
    CHECK(!parseUpstream("https://example.com"));
    CHECK(!parseUpstream("127.0.0.1:5000"));
    CHECK(!parseUpstream("http://:5000"));
    CHECK(!parseUpstream("http://host:99999"));
}

TEST(proxy_forwards_requests_and_replies) {
    Running backend([](HttpServer& s) {
        s.get("/api/echo/*", [](const Request& q) {
            return Response::json("{\"target\": \"" + q.target + "\", \"token\": \"" + q.header("x-token") + "\"}");
        });
        s.post("/api/todos", [](const Request& q) {
            Response r = Response::text("got " + q.body, 201);
            r.headers.emplace_back("Location", "/api/todos/7");
            r.headers.emplace_back("Set-Cookie", "a=1");
            r.headers.emplace_back("Set-Cookie", "b=2");
            return r;
        });
        s.get("/v1/api/x", [](const Request&) { return Response::text("versioned"); });
    });
    Running front([&](HttpServer& s) {
        s.proxy("/api", {"127.0.0.1", backend.port(), ""});
        s.get("/local", [](const Request&) { return Response::text("mine"); });
    });

    auto got = rawRequest(front.port(), "GET /api/echo/a%2Fb?x=1 HTTP/1.1\r\nHost: localhost\r\nX-Token: t1\r\n\r\n");
    CHECK(got.starts_with("HTTP/1.1 200 OK"));
    CHECK(got.find("application/json") != std::string::npos);
    CHECK(got.find(R"("target": "/api/echo/a%2Fb?x=1")") != std::string::npos);  // not decoded on the way
    CHECK(got.find(R"("token": "t1")") != std::string::npos);

    auto created = request(front.port(), "POST", "/api/todos", "milk");
    CHECK(created.status == 201);
    CHECK(created.body == "got milk");
    CHECK(created.hasHeader("Location: /api/todos/7"));
    CHECK(created.hasHeader("Set-Cookie: a=1") && created.hasHeader("Set-Cookie: b=2"));

    CHECK(request(front.port(), "GET", "/local").body == "mine");  // outside the prefix: not forwarded
    CHECK(request(front.port(), "GET", "/apis").status == 404);
    CHECK(request(front.port(), "HEAD", "/api/echo/x").status == 200);

    Running versioned([&](HttpServer& s) { s.proxy("/api", {"127.0.0.1", backend.port(), "/v1"}); });
    CHECK(request(versioned.port(), "GET", "/api/x").body == "versioned");
}

TEST(proxy_reports_a_missing_backend) {
    int closedPort = 0;
    {
        Running gone([](HttpServer&) {});
        closedPort = gone.port();
    }
    Running front([&](HttpServer& s) { s.proxy("/api", {"127.0.0.1", closedPort, ""}); });
    auto r = request(front.port(), "GET", "/api/x");
    CHECK(r.status == 502);
    CHECK(r.hasHeader("Bad Gateway"));
    CHECK(r.body.starts_with("proxy: can't connect to 127.0.0.1:"));
}

TEST(forward_decodes_chunked_replies) {
    OneShotServer up("HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nTransfer-Encoding: chunked\r\n\r\n"
                     "5\r\nhello\r\n7;ext=1\r\n, world\r\n0\r\n\r\n");
    auto r = forward({.method = "GET", .path = "/"}, {"127.0.0.1", up.port, ""});
    CHECK(r && r->status == 200 && r->body == "hello, world" && r->contentType == "text/plain");
}

TEST(forward_without_content_length_reads_to_the_end) {
    OneShotServer up("HTTP/1.0 404 Not Found\r\nX-A: b\r\n\r\nno such thing");
    auto r = forward({.method = "GET", .path = "/"}, {"127.0.0.1", up.port, ""});
    CHECK(r && r->status == 404 && r->body == "no such thing");
    CHECK(r && r->headers.size() == 1 && r->headers[0].second == "b");
}

TEST(parse_response_handles_lengths_chunks_and_hostile_sizes) {
    auto ok = parseResponse("HTTP/1.1 200 OK\r\nContent-Length: 5\r\nX-A: b\r\n\r\nhelloEXTRA");
    CHECK(ok && ok->body == "hello" && ok->contentType == "application/octet-stream" && ok->headers.size() == 1);
    CHECK(parseResponse("HTTP/1.1 204 No Content\r\n\r\nignored")->body.empty());
    CHECK(parseResponse("HTTP/1.1 200 OK\r\nContent-Length: 9\r\n\r\nabc", true)->body.empty());  // HEAD
    auto cut = parseResponse("HTTP/1.1 200 OK\r\nContent-Length: 9\r\n\r\nabc");
    CHECK(!cut && cut.error().status == 502);
    // A Content-Length that overflows size_t is a cut reply, not a short complete one.
    CHECK(!parseResponse("HTTP/1.1 200 OK\r\nContent-Length: 18446744073709551615\r\n\r\nabc"));
    // A chunk size near SIZE_MAX used to make the decoder copy the reply again and again.
    CHECK(!parseResponse("HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\nffffffffffffffff\r\n" + std::string(100000, 'x')));
    CHECK(!parseResponse("HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\nfffffffffffffffe\r\nab"));
    CHECK(!parseResponse("HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n3\r\nab"));  // cut
    CHECK(!parseResponse("HTTP/1.1 999 Odd\r\n\r\n") && !parseResponse("SSH-2.0\r\n\r\n") && !parseResponse("HTTP/1.1 200"));
}

TEST(forward_rejects_garbage_and_times_out) {
    OneShotServer garbage("SSH-2.0-OpenSSH\r\n\r\n");
    auto bad = forward({.method = "GET", .path = "/"}, {"127.0.0.1", garbage.port, ""});
    CHECK(!bad && bad.error().status == 502);

    OneShotServer silent("");
    auto slow = forward({.method = "GET", .path = "/"}, {"127.0.0.1", silent.port, ""}, 1);
    CHECK(!slow && slow.error().status == 504);
}


// ---- security (a dev server still meets hostile pages and clients) ----

// Connects to 127.0.0.1:port; -1 on failure.
int connectTo(int port) {
    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<std::uint16_t>(port));
    ::inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
    if (::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof addr) != 0) {
        ::close(fd);
        return -1;
    }
    return fd;
}

std::string readAll(int fd) {
    std::string reply;
    char buf[4096];
    for (ssize_t n; (n = ::recv(fd, buf, sizeof buf, 0)) > 0;) reply.append(buf, static_cast<std::size_t>(n));
    return reply;
}

Request withHeaders(std::string method, std::string path, std::vector<std::pair<std::string, std::string>> headers) {
    Request req{.method = std::move(method), .path = std::move(path)};
    for (auto& [name, value] : headers) req.headers.insert_or_assign(std::move(name), std::move(value));
    return req;
}

TEST(heads_that_could_smuggle_or_inject_are_refused) {
    auto status = [](std::string_view head) {
        auto req = parseRequestHead(head);
        return req ? 200 : req.error().status;
    };
    CHECK(status("GET / HTTP/1.1\r\nHost: localhost\r\nX: a\tb\r\n\r\n") == 200);
    CHECK(status("GET /a\nX-Evil: 1 HTTP/1.1\r\n\r\n") == 400);           // a bare LF reaching a proxied server
    CHECK(status("GET /a b HTTP/1.1\r\n\r\n") == 400);                     // a space in the target
    CHECK(status("GET / HTTP/1.1\r\nX: a\rb\r\n\r\n") == 400);             // a bare CR in a value
    CHECK(status("GET / HTTP/1.1\r\nX: a\x01" "b\r\n\r\n") == 400);
    CHECK(status("GET / HTTP/1.1\r\nContent-Length : 5\r\n\r\n") == 400);  // a space before the colon
    CHECK(status("GET / HTTP/1.1\r\nX: a\r\n folded\r\n\r\n") == 400);     // obs-fold
    CHECK(status("GET / HTTP/1.1\r\nX(y): a\r\n\r\n") == 400);
    CHECK(status("POST / HTTP/1.1\r\nContent-Length: 1\r\nContent-Length: 2\r\n\r\n") == 400);
    CHECK(status("POST / HTTP/1.1\r\nContent-Length: 2\r\nContent-Length: 2\r\n\r\n") == 200);
    CHECK(status("GET / HTTP/1.1\r\nHost: a\r\nHost: b\r\n\r\n") == 400);
    CHECK(status("POST / HTTP/1.1\r\nTransfer-Encoding: chunked\r\n\r\n") == 501);
    std::string many = "GET / HTTP/1.1\r\n";
    for (int i = 0; i < 101; ++i) many += "X" + std::to_string(i) + ": y\r\n";
    CHECK(status(many + "\r\n") == 431);
}

TEST(response_headers_cant_be_split) {
    Response r = Response::text("x");
    r.headers.emplace_back("Location", "/a\r\nSet-Cookie: session=evil");
    r.contentType = "text/plain\r\nX-Evil: 1";
    std::string out = serializeResponse(r);
    CHECK(!out.contains("\r\nSet-Cookie"));
    CHECK(!out.contains("\r\nX-Evil"));
    CHECK(out.contains("Location: /aSet-Cookie: session=evil\r\n"));
}

TEST(only_local_hosts_are_answered) {  // DNS rebinding
    HttpServer s;
    s.get("/api", [](const Request&) { return Response::text("data"); });
    auto status = [&](std::string host) { return s.handle(withHeaders("GET", "/api", {{"host", host}})).status; };
    CHECK(status("localhost:8090") == 200);
    CHECK(status("LocalHost.") == 200);
    CHECK(status("app.localhost:8090") == 200);
    CHECK(status("127.0.0.1:8090") == 200);
    CHECK(status("192.168.1.20:8090") == 200);  // a phone on the Wi-Fi
    CHECK(status("[::1]:8090") == 200);
    CHECK(status("evil.example:8090") == 403);
    CHECK(status("localhost.evil.example") == 403);
    CHECK(s.handle(withHeaders("GET", "/api", {})).status == 200);  // HTTP/1.0, no Host
    s.allowHosts({"api", ".example.com"});
    CHECK(status("api:8000") == 200);
    CHECK(status("example.com") == 200);
    CHECK(status("app.example.com") == 200);
    CHECK(status("badexample.com") == 403);
    s.allowHosts({"*"});
    CHECK(status("anything.test") == 200);
}

TEST(cross_site_requests_are_refused) {  // CSRF, cross-site WebSocket hijacking
    Running r([](HttpServer& s) {
        s.post("/api", [](const Request&) { return Response::text("done"); });
        s.get("/api", [](const Request&) { return Response::text("read"); });
        s.webSocket("/ws", {});
    });
    auto post = [&](std::string origin) {
        return r.server.handle(withHeaders("POST", "/api", {{"host", "localhost"}, {"origin", origin}})).status;
    };
    CHECK(post("http://localhost:5173") == 200);  // another local port: fine
    CHECK(post("https://evil.example") == 403);
    CHECK(post("null") == 403);
    CHECK(r.server.handle(withHeaders("GET", "/api", {{"origin", "https://evil.example"}})).status == 200);

    auto upgrade = [&](std::string origin) {
        int fd = connectTo(r.port());
        std::string req = "GET /ws HTTP/1.1\r\nHost: localhost\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
                          "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\nSec-WebSocket-Version: 13\r\nOrigin: " +
                          origin + "\r\n\r\n";
        ::send(fd, req.data(), req.size(), MSG_NOSIGNAL);
        char buf[64] = {};
        ::recv(fd, buf, 12, MSG_WAITALL);
        ::close(fd);
        return std::string(buf, 12);
    };
    CHECK(upgrade("http://localhost:3000") == "HTTP/1.1 101");
    CHECK(upgrade("https://evil.example") == "HTTP/1.1 403");
}

TEST(no_dot_files_and_no_symlinks_out_of_the_root) {
    Site site;
    site.write(".env", "SECRET=1");
    site.write(".git/config", "[core]");
    site.write(".well-known/security.txt", "Contact: x");
    fs::create_symlink(site.root.parent_path() / "reactx_http_secret.txt", site.root / "sub" / "leak.html");
    fs::create_symlink(site.root / "sub" / "page.html", site.root / "inside.html");
    Running r([&](HttpServer& s) { s.serveDirectory(site.root, /*spaFallback=*/true); });
    CHECK(request(r.port(), "GET", "/.env").status == 404);
    CHECK(request(r.port(), "GET", "/.git/config").status == 404);
    CHECK(request(r.port(), "GET", "/sub/%2eenv").status == 404);
    CHECK(request(r.port(), "GET", "/.well-known/security.txt").status == 200);
    CHECK(request(r.port(), "GET", "/sub/leak").status == 403);  // "/x" -> x.html, a symlink out
    CHECK(request(r.port(), "GET", "/sub/leak.html").status == 403);
    CHECK(request(r.port(), "GET", "/inside.html").body == "<p>sub</p>");  // symlinks inside are fine
    CHECK(!request(r.port(), "GET", "//etc/passwd").body.contains("root:"));  // index.html at most
    CHECK(request(r.port(), "GET", "/app.js").hasHeader("X-Content-Type-Options: nosniff"));
}

TEST(slow_and_cut_off_requests) {
    bool called = false;
    Running r([&](HttpServer& s) {
        s.setRequestTimeout(std::chrono::milliseconds(400));
        s.post("/api", [&](const Request&) {
            called = true;
            return Response::text("ok");
        });
    });
    // Slowloris: a byte at a time, never finishing the head.
    int fd = connectTo(r.port());
    auto start = std::chrono::steady_clock::now();
    std::string reply;
    for (char c : std::string("GET / HTTP/1.1\r\nHost: localhost\r\nX: aaaaaaaaaaaaaaaaaaaaaaaa")) {
        if (::send(fd, &c, 1, MSG_NOSIGNAL) <= 0) break;
        pollfd p{.fd = fd, .events = POLLIN, .revents = 0};
        if (::poll(&p, 1, 100) > 0) break;  // the server answered
    }
    reply = readAll(fd);
    ::close(fd);
    CHECK(reply.starts_with("HTTP/1.1 408"));
    CHECK(std::chrono::steady_clock::now() - start < std::chrono::seconds(3));

    // A body shorter than its Content-Length never reaches the handler.
    fd = connectTo(r.port());
    std::string cut = "POST /api HTTP/1.1\r\nHost: localhost\r\nContent-Length: 10\r\n\r\nabc";
    ::send(fd, cut.data(), cut.size(), MSG_NOSIGNAL);
    ::shutdown(fd, SHUT_WR);
    reply = readAll(fd);
    ::close(fd);
    CHECK(reply.starts_with("HTTP/1.1 400"));
    CHECK(!called);
}

TEST(connections_are_limited) {
    Running r([](HttpServer& s) { s.setMaxConnections(2); });
    int a = connectTo(r.port()), b = connectTo(r.port());
    std::this_thread::sleep_for(std::chrono::milliseconds(300));  // both accepted, waiting for a head
    CHECK(rawRequest(r.port(), "GET / HTTP/1.1\r\nHost: localhost\r\n\r\n").starts_with("HTTP/1.1 503"));
    ::close(a);
    ::close(b);
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    CHECK(request(r.port(), "GET", "/").status == 404);  // served again
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
