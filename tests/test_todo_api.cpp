// The todo REST API of examples/api_server.cpp, over real HTTP.
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include "../examples/todo_api.hpp"
#include "reactx/json.hpp"

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
    CHECK(r.json()[0]["title"].asString() == "Write the reconciler");
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
