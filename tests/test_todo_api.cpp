// The todo REST API of examples/api_server.cpp, over real HTTP.
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include "../examples/todo_api.hpp"
#include "reactx/fetch.hpp"
#include "reactx/json.hpp"
#include "reactx/reactx.hpp"
#include "reactx/text_renderer.hpp"

using namespace rx;
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

struct Reply {
    int status = 0;
    std::string body;
    json::Value json() const { return json::parse(body); }
};

Reply call(int port, const std::string& method, const std::string& path, const std::string& body = {}) {
    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<std::uint16_t>(port));
    ::inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
    ::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof addr);
    std::string req = method + " " + path + " HTTP/1.1\r\nHost: x\r\nContent-Length: " + std::to_string(body.size()) +
                      "\r\n\r\n" + body;
    ::send(fd, req.data(), req.size(), MSG_NOSIGNAL);
    std::string raw;
    char buf[4096];
    for (ssize_t n; (n = ::recv(fd, buf, sizeof buf, 0)) > 0;) raw.append(buf, static_cast<std::size_t>(n));
    ::close(fd);
    Reply r;
    if (raw.size() > 12) r.status = std::stoi(raw.substr(9, 3));
    if (auto end = raw.find("\r\n\r\n"); end != std::string::npos) r.body = raw.substr(end + 4);
    return r;
}

struct Api {
    todo_api::Store store;
    net::HttpServer server{"127.0.0.1", 0};
    std::jthread thread;
    explicit Api(fs::path file = {}) : store(std::move(file)) {
        todo_api::addRoutes(server, store);
        server.start();
        thread = std::jthread([this] { server.run(); });
    }
    ~Api() { server.stop(); }
    int port() const { return server.port(); }
};

TEST(lists_the_seed_todos) {
    Api api;
    auto r = call(api.port(), "GET", "/api/todos");
    CHECK(r.status == 200);
    CHECK(r.json().size() == 3);
    CHECK(r.json()[0]["title"].asString() == "Написать reconciler");
}

TEST(create_read_update_delete) {
    Api api;
    auto created = call(api.port(), "POST", "/api/todos", R"({"title": "  Купить молоко  "})");
    CHECK(created.status == 201);
    CHECK(created.json()["title"].asString() == "Купить молоко");  // trimmed
    CHECK(created.json()["done"].asBool() == false);
    std::string id = std::to_string(created.json()["id"].asInt());

    CHECK(call(api.port(), "GET", "/api/todos/" + id).json()["title"].asString() == "Купить молоко");

    auto patched = call(api.port(), "PATCH", "/api/todos/" + id, R"({"done": true})");
    CHECK(patched.status == 200 && patched.json()["done"].asBool());
    CHECK(call(api.port(), "GET", "/api/todos").json().size() == 4);

    CHECK(call(api.port(), "DELETE", "/api/todos/" + id).status == 204);
    CHECK(call(api.port(), "GET", "/api/todos/" + id).status == 404);
    CHECK(call(api.port(), "DELETE", "/api/todos/" + id).status == 404);
}

TEST(validates_input) {
    Api api;
    auto bad = call(api.port(), "POST", "/api/todos", "{oops");
    CHECK(bad.status == 400);
    CHECK(bad.json()["error"].asString().find("json:") == 0);
    CHECK(call(api.port(), "POST", "/api/todos", R"({"title": "   "})").json()["error"].asString() == "title is empty");
    CHECK(call(api.port(), "POST", "/api/todos", R"({"title": 5})").status == 400);
    CHECK(call(api.port(), "POST", "/api/todos", R"({"title": ")" + std::string(201, 'x') + R"("})").status == 400);
    CHECK(call(api.port(), "PATCH", "/api/todos/1", R"({"done": "yes"})").status == 400);
    CHECK(call(api.port(), "GET", "/api/todos/abc").status == 400);
    CHECK(call(api.port(), "GET", "/api/todos/-1").status == 400);
}

TEST(unknown_api_paths_are_json_404) {
    Api api;
    auto r = call(api.port(), "GET", "/api/nope");
    CHECK(r.status == 404);
    CHECK(r.json()["error"].asString() == "no API route /api/nope");
    CHECK(call(api.port(), "PUT", "/api/todos/1").status == 405);
}

TEST(saves_to_a_file) {
    fs::path file = fs::temp_directory_path() / ("reactx_todos_" + std::to_string(::getpid()) + ".json");
    fs::remove(file);
    {
        Api api(file);
        call(api.port(), "POST", "/api/todos", R"({"title": "survives restart"})");
    }
    CHECK(fs::exists(file));
    {
        Api api(file);  // a new server reads the file
        auto list = call(api.port(), "GET", "/api/todos").json();
        CHECK(list.size() == 4);
        CHECK(list[3]["title"].asString() == "survives restart");
        auto next = call(api.port(), "POST", "/api/todos", R"({"title": "next id"})");
        CHECK(next.json()["id"].asInt() == 5);
    }
    fs::remove(file);
}

// The same API without any network: rx::setLocalFetch + todo_api::handle,
// which is how the static site (GitHub Pages) works in the browser.
Element LocalList(const Props&) {
    auto todos = useFetch("/api/todos");
    return h("p", {}, {todos.ok() ? std::to_string(todos.json().size()) + " todos" : "loading"});
}

TEST(local_fetch_runs_the_api_in_process) {
    std::string saved;
    todo_api::Store store({.load = [&saved] { return saved; }, .save = [&saved](const std::string& t) { saved = t; }});
    setLocalFetch([&store](const FetchRequest& r) -> std::optional<FetchResponse> {
        if (!r.url.starts_with("/api/")) return std::nullopt;
        auto reply = todo_api::handle(store, r.method, r.url, r.body);
        return FetchResponse{.status = reply.status, .body = reply.body, .error = {}};
    });

    TextRenderer screen;
    Root root(screen);
    root.render(h(LocalList));
    CHECK(screen.toString().find("\"3 todos\"") != std::string::npos);

    FetchResponse created;
    fetch({.method = "POST", .url = "/api/todos", .body = R"({"title": "offline"})"},
          [&created](const FetchResponse& r) { created = r; });
    CHECK(created.status == 201);
    CHECK(saved.find("offline") != std::string::npos);  // persisted through Storage

    FetchResponse other;
    fetch({.method = "GET", .url = "https://example.com/"}, [&other](const FetchResponse& r) { other = r; });
    CHECK(other.status == 0 && !other.error.empty());  // not handled locally, no network here
    setLocalFetch({});
}

// The terminal build talks HTTP itself: the answer arrives on Root::flush().
std::string apiBase;
Element RemoteList(const Props&) {
    auto todos = useFetch(apiBase + "/api/todos");
    if (todos.loading) return h("p", {}, {"loading"});
    if (!todos.ok()) return h("p", {}, {"error: " + todos.errorText()});
    return h("p", {}, {std::to_string(todos.json().size()) + " todos"});
}

// Flushes until `ready` (the answers come from other threads), at most ~5 s.
template <class F>
bool flushUntil(Root& root, F ready) {
    for (int i = 0; i < 500 && !ready(); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        root.flush();
    }
    return ready();
}

TEST(fetch_in_the_terminal_build_uses_the_network) {
    Api api;
    apiBase = "http://127.0.0.1:" + std::to_string(api.port());
    TextRenderer screen;
    Root root(screen);
    root.render(h(RemoteList));
    CHECK(screen.toString().find("loading") != std::string::npos);  // asynchronous, like in the browser
    CHECK(flushUntil(root, [&] { return screen.toString().find("3 todos") != std::string::npos; }));

    FetchResponse created;
    fetch({.method = "POST", .url = apiBase + "/api/todos", .body = R"({"title": "over http"})"},
          [&created](const FetchResponse& r) { created = r; });
    CHECK(flushUntil(root, [&] { return created.status != 0; }));
    CHECK(created.status == 201 && created.json()["title"].asString() == "over http");

    FetchResponse missing;
    fetch({.method = "GET", .url = apiBase + "/api/todos/999?x=1#frag"}, [&missing](const FetchResponse& r) { missing = r; });
    CHECK(flushUntil(root, [&] { return missing.status != 0; }));
    CHECK(missing.status == 404 && !missing.ok() && missing.error.empty());
}

TEST(fetch_in_the_terminal_build_reports_what_is_wrong) {
    FetchResponse relative, secure;
    fetch({.method = "GET", .url = "/api/todos"}, [&](const FetchResponse& r) { relative = r; });
    fetch({.method = "GET", .url = "https://example.com/"}, [&](const FetchResponse& r) { secure = r; });
    CHECK(relative.status == 0 && relative.error.find("full URL") != std::string::npos);  // right away
    CHECK(secure.status == 0 && secure.error.find("https") != std::string::npos);

    int closedPort = 0;
    {
        Api gone;
        closedPort = gone.port();
    }
    TextRenderer screen;
    Root root(screen);
    FetchResponse refused;
    bool answered = false;
    fetch({.method = "GET", .url = "http://127.0.0.1:" + std::to_string(closedPort) + "/"},
          [&](const FetchResponse& r) { refused = r, answered = true; });
    CHECK(flushUntil(root, [&] { return answered; }));
    CHECK(refused.status == 0 && refused.error.find("can't connect") != std::string::npos);
}

TEST(fetch_sends_headers_times_out_and_cancels) {
    net::HttpServer server{"127.0.0.1", 0};
    server.get("/echo", [](const net::Request& r) {
        auto reply = net::Response::text(r.header("authorization"));
        reply.headers.emplace_back("X-Answer", "42");
        return reply;
    });
    server.get("/slow", [](const net::Request&) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1500));
        return net::Response::text("late");
    });
    server.start();
    std::jthread thread([&server] { server.run(); });
    std::string base = "http://127.0.0.1:" + std::to_string(server.port());
    TextRenderer screen;
    Root root(screen);

    FetchResponse echo;
    fetch({.url = base + "/echo", .headers = {{"Authorization", "Bearer abc"}}}, [&](const FetchResponse& r) { echo = r; });
    CHECK(flushUntil(root, [&] { return echo.status != 0; }));
    CHECK(echo.body == "Bearer abc");
    CHECK(echo.header("x-answer") == "42" && echo.header("X-ANSWER") == "42");
    CHECK(echo.header("content-type").starts_with("text/plain"));
    CHECK(echo.header("missing").empty());

    FetchResponse slow;
    auto started = std::chrono::steady_clock::now();
    fetch({.url = base + "/slow", .timeout = std::chrono::milliseconds(200)}, [&](const FetchResponse& r) { slow = r; });
    CHECK(flushUntil(root, [&] { return !slow.error.empty(); }));
    CHECK(slow.status == 0 && slow.error == "timeout after 200 ms");
    CHECK(std::chrono::steady_clock::now() - started < std::chrono::milliseconds(1000));

    bool called = false;
    FetchHandle cancelled = fetch({.url = base + "/echo"}, [&](const FetchResponse&) { called = true; });
    CHECK(!cancelled.cancelled());
    cancelled.cancel();
    CHECK(cancelled.cancelled());
    for (int i = 0; i < 40; ++i) {  // 400 ms: the answer comes and is dropped
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        root.flush();
    }
    CHECK(!called);
    server.stop();
}

std::string echoBase;
Element WhoAmI(const Props&) {
    auto me = useFetch({.url = echoBase + "/echo", .headers = {{"Authorization", "Bearer xyz"}}});
    return h("p", {}, {me.loading ? "loading" : me.response.body});
}

TEST(use_fetch_takes_a_whole_request) {
    net::HttpServer server{"127.0.0.1", 0};
    server.get("/echo", [](const net::Request& r) { return net::Response::text("me=" + r.header("authorization")); });
    server.start();
    std::jthread thread([&server] { server.run(); });
    echoBase = "http://127.0.0.1:" + std::to_string(server.port());
    TextRenderer screen;
    Root root(screen);
    root.render(h(WhoAmI));
    CHECK(flushUntil(root, [&] { return screen.toString().find("me=Bearer xyz") != std::string::npos; }));
    server.stop();
}

TEST(parallel_posts_get_unique_ids) {
    Api api;
    std::vector<std::jthread> clients;
    for (int i = 0; i < 20; ++i)
        clients.emplace_back([&, i] { call(api.port(), "POST", "/api/todos", R"({"title": "t)" + std::to_string(i) + R"("})"); });
    clients.clear();
    auto list = call(api.port(), "GET", "/api/todos").json();
    CHECK(list.size() == 23);
    std::vector<std::int64_t> ids;
    for (const auto& t : list.asArray()) ids.push_back(t["id"].asInt());
    std::ranges::sort(ids);
    CHECK(std::ranges::adjacent_find(ids) == ids.end());
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
