#pragma once

// REST API for todos. The API itself (Store + handle()) doesn't know about
// sockets, so the same C++ code runs
//   - behind rx::net::HttpServer in examples/api_server.cpp, and
//   - inside the browser in the static site (examples/web_app.cpp with
//     rx::setLocalFetch, data in localStorage) — no server needed.
//
//   GET    /api/todos          -> [{"id":1,"title":"...","done":false}, ...]
//   POST   /api/todos          {"title":"..."}          -> 201 + the new todo
//   GET    /api/todos/:id      -> the todo, or 404
//   PATCH  /api/todos/:id      {"title"?:"...","done"?:true} -> the todo
//   DELETE /api/todos/:id      -> 204
// Errors are {"error": "..."} with status 400 / 404 / 405.

#include <algorithm>
#include <charconv>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "reactx/json.hpp"
#include "reactx/path.hpp"

namespace todo_api {

struct Todo {
    int id = 0;
    std::string title;
    bool done = false;

    rx::json::Value toJson() const { return rx::json::object({{"id", id}, {"title", title}, {"done", done}}); }
};

// Where the todos are kept between runs: load() returns the saved JSON text
// ("" = nothing saved yet), save() stores it. Empty functions = memory only.
struct Storage {
    std::function<std::string()> load{};
    std::function<void(const std::string&)> save{};
};

// A JSON file, written atomically (a temporary file, then rename).
inline Storage fileStorage(std::filesystem::path file) {
    if (file.empty()) return {};
    return {
        .load = [file] {
            std::ifstream in(file);
            return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        },
        .save = [file](const std::string& text) {
            std::filesystem::path tmp = file;
            tmp += ".tmp";
            std::ofstream(tmp) << text << '\n';
            std::filesystem::rename(tmp, file);
        },
    };
}

// Thread-safe list of todos, saved after every change.
class Store {
public:
    explicit Store(Storage storage = {}) : storage_(std::move(storage)) { load(); }
    explicit Store(std::filesystem::path file) : Store(fileStorage(std::move(file))) {}

    std::vector<Todo> all() const {
        std::scoped_lock lock(mutex_);
        return todos_;
    }
    std::optional<Todo> find(int id) const {
        std::scoped_lock lock(mutex_);
        auto it = std::ranges::find(todos_, id, &Todo::id);
        return it == todos_.end() ? std::nullopt : std::optional<Todo>(*it);
    }
    Todo add(std::string title) {
        std::scoped_lock lock(mutex_);
        Todo t{nextId_++, std::move(title), false};
        todos_.push_back(t);
        save();
        return t;
    }
    std::optional<Todo> update(int id, std::optional<std::string> title, std::optional<bool> done) {
        std::scoped_lock lock(mutex_);
        auto it = std::ranges::find(todos_, id, &Todo::id);
        if (it == todos_.end()) return std::nullopt;
        if (title) it->title = *title;
        if (done) it->done = *done;
        save();
        return *it;
    }
    bool remove(int id) {
        std::scoped_lock lock(mutex_);
        auto n = std::erase_if(todos_, [id](const Todo& t) { return t.id == id; });
        if (n) save();
        return n > 0;
    }

private:
    void load() {
        std::string text = storage_.load ? storage_.load() : std::string{};
        if (text.find_first_not_of(" \t\r\n") == std::string::npos) {
            todos_ = {{1, "Написать reconciler", true}, {2, "Запустить в браузере", true}, {3, "Загрузить данные с сервера на C++"}};
            nextId_ = 4;
            return;
        }
        const rx::json::Value saved = rx::json::parse(text);
        for (const auto& v : saved.asArray()) {
            todos_.push_back({static_cast<int>(v.get<int>("id")), v.get<std::string>("title"), v.get<bool>("done")});
            nextId_ = std::max(nextId_, todos_.back().id + 1);
        }
    }
    void save() const {  // called with the lock held
        if (!storage_.save) return;
        rx::json::Value list = rx::json::array();
        for (const auto& t : todos_) list.push(t.toJson());
        storage_.save(list.dump(2));
    }

    Storage storage_;
    mutable std::mutex mutex_;
    std::vector<Todo> todos_;
    int nextId_ = 1;
};

// ---- the API, without any networking ----

struct Reply {
    int status = 200;
    std::string body{};  // JSON (empty for 204)
};

inline Reply error(int status, const std::string& message) {
    return {status, rx::json::object({{"error", message}}).dump()};
}

inline std::optional<int> parseId(const std::string& s) {
    int id = 0;
    auto [p, ec] = std::from_chars(s.data(), s.data() + s.size(), id);
    if (ec != std::errc{} || p != s.data() + s.size() || id <= 0) return std::nullopt;
    return id;
}

// Validates {"title": "..."}: a non-empty string of at most 200 bytes.
inline std::optional<std::string> validTitle(const rx::json::Value& v, std::string& why) {
    if (!v["title"].isString()) {
        why = "title must be a string";
        return std::nullopt;
    }
    std::string title = v["title"].asString();
    title.erase(0, title.find_first_not_of(" \t\n\r"));
    title.erase(title.find_last_not_of(" \t\n\r") + 1);
    if (title.empty()) why = "title is empty";
    else if (title.size() > 200) why = "title is longer than 200 bytes";
    else return title;
    return std::nullopt;
}

// Answers one request. `path` has no query string.
inline Reply handle(Store& store, std::string_view method, std::string_view path, std::string_view body) {
    auto parseBody = [&](rx::json::Value& out) -> std::optional<Reply> {
        auto parsed = rx::json::tryParse(body);
        if (!parsed) return error(400, parsed.error().what());
        out = std::move(*parsed);
        return std::nullopt;
    };

    std::map<std::string, std::string> params;
    if (rx::matchPath("/api/todos", path, params)) {
        if (method == "GET") {
            rx::json::Value list = rx::json::array();
            for (const auto& t : store.all()) list.push(t.toJson());
            return {200, list.dump()};
        }
        if (method == "POST") {
            rx::json::Value v;
            if (auto bad = parseBody(v)) return *bad;
            std::string why;
            auto title = validTitle(v, why);
            if (!title) return error(400, why);
            return {201, store.add(*title).toJson().dump()};
        }
        return error(405, "method not allowed");
    }

    if (rx::matchPath("/api/todos/:id", path, params)) {
        auto id = parseId(params["id"]);
        if (!id) return error(400, "bad id");
        if (method == "GET") {
            auto t = store.find(*id);
            return t ? Reply{200, t->toJson().dump()} : error(404, "no todo " + params["id"]);
        }
        if (method == "PATCH") {
            rx::json::Value v;
            if (auto bad = parseBody(v)) return *bad;
            std::optional<std::string> title;
            std::optional<bool> done;
            if (v.contains("title")) {
                std::string why;
                title = validTitle(v, why);
                if (!title) return error(400, why);
            }
            if (v.contains("done")) {
                if (!v["done"].isBool()) return error(400, "done must be true or false");
                done = v["done"].asBool();
            }
            auto t = store.update(*id, title, done);
            return t ? Reply{200, t->toJson().dump()} : error(404, "no todo " + params["id"]);
        }
        if (method == "DELETE") return store.remove(*id) ? Reply{204, {}} : error(404, "no todo " + params["id"]);
        return error(405, "method not allowed");
    }

    return error(404, "no API route " + std::string(path));
}

}  // namespace todo_api

// ---- on the HTTP server (native builds) ----

#ifndef __EMSCRIPTEN__
#include "reactx/http.hpp"

namespace todo_api {

// Routes every /api/... request to handle().
inline void addRoutes(rx::net::HttpServer& server, Store& store) {
    for (const char* method : {"GET", "POST", "PATCH", "PUT", "DELETE"}) {
        server.route(method, "/api/*", [&store](const rx::net::Request& req) {
            Reply r = handle(store, req.method, req.path, req.body);
            return rx::net::Response{.status = r.status,
                                     .contentType = r.body.empty() ? "text/plain" : "application/json; charset=utf-8",
                                     .body = std::move(r.body),
                                     .headers = {}};
        });
    }
}

}  // namespace todo_api
#endif
