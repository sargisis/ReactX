#pragma once

// REST API for todos on rx::net::HttpServer, used by examples/api_server.cpp
// and tests/test_todo_api.cpp.
//
//   GET    /api/todos          -> [{"id":1,"title":"...","done":false}, ...]
//   POST   /api/todos          {"title":"..."}          -> 201 + the new todo
//   GET    /api/todos/:id      -> the todo, or 404
//   PATCH  /api/todos/:id      {"title"?:"...","done"?:true} -> the todo
//   DELETE /api/todos/:id      -> 204
// Errors are {"error": "..."} with status 400 / 404.

#include <algorithm>
#include <charconv>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "reactx/http.hpp"
#include "reactx/json.hpp"

namespace todo_api {

struct Todo {
    int id = 0;
    std::string title;
    bool done = false;

    rx::json::Value toJson() const { return rx::json::object({{"id", id}, {"title", title}, {"done", done}}); }
};

// Thread-safe list of todos; saved to a JSON file after every change when a
// path is given.
class Store {
public:
    explicit Store(std::filesystem::path file = {}) : file_(std::move(file)) { load(); }

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
        if (file_.empty() || !std::filesystem::exists(file_)) {
            todos_ = {{1, "Write the reconciler", true}, {2, "Run it in the browser", true}, {3, "Load data from a C++ server"}};
            nextId_ = 4;
            return;
        }
        std::ifstream in(file_);
        std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        const rx::json::Value saved = rx::json::parse(text);
        for (const auto& v : saved.asArray()) {
            todos_.push_back({static_cast<int>(v.get<int>("id")), v.get<std::string>("title"), v.get<bool>("done")});
            nextId_ = std::max(nextId_, todos_.back().id + 1);
        }
    }
    void save() const {  // called with the lock held
        if (file_.empty()) return;
        rx::json::Value list = rx::json::array();
        for (const auto& t : todos_) list.push(t.toJson());
        std::filesystem::path tmp = file_;
        tmp += ".tmp";
        std::ofstream(tmp) << list.dump(2) << '\n';
        std::filesystem::rename(tmp, file_);  // never leave a half-written file
    }

    std::filesystem::path file_;
    mutable std::mutex mutex_;
    std::vector<Todo> todos_;
    int nextId_ = 1;
};

inline rx::net::Response error(int status, const std::string& message) {
    return rx::net::Response::json(rx::json::object({{"error", message}}).dump(), status);
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

inline void addRoutes(rx::net::HttpServer& server, Store& store) {
    using rx::net::Request;
    using rx::net::Response;

    server.get("/api/todos", [&store](const Request&) {
        rx::json::Value list = rx::json::array();
        for (const auto& t : store.all()) list.push(t.toJson());
        return Response::json(list.dump());
    });

    server.post("/api/todos", [&store](const Request& req) {
        rx::json::Value body;
        try {
            body = rx::json::parse(req.body);
        } catch (const rx::json::Error& e) {
            return error(400, e.what());
        }
        std::string why;
        auto title = validTitle(body, why);
        if (!title) return error(400, why);
        return Response::json(store.add(*title).toJson().dump(), 201);
    });

    server.get("/api/todos/:id", [&store](const Request& req) {
        auto id = parseId(req.pathParam("id"));
        if (!id) return error(400, "bad id");
        auto t = store.find(*id);
        return t ? Response::json(t->toJson().dump()) : error(404, "no todo " + req.pathParam("id"));
    });

    server.patch("/api/todos/:id", [&store](const Request& req) {
        auto id = parseId(req.pathParam("id"));
        if (!id) return error(400, "bad id");
        rx::json::Value body;
        try {
            body = rx::json::parse(req.body);
        } catch (const rx::json::Error& e) {
            return error(400, e.what());
        }
        std::optional<std::string> title;
        std::optional<bool> done;
        if (body.contains("title")) {
            std::string why;
            title = validTitle(body, why);
            if (!title) return error(400, why);
        }
        if (body.contains("done")) {
            if (!body["done"].isBool()) return error(400, "done must be true or false");
            done = body["done"].asBool();
        }
        auto t = store.update(*id, title, done);
        return t ? Response::json(t->toJson().dump()) : error(404, "no todo " + req.pathParam("id"));
    });

    server.del("/api/todos/:id", [&store](const Request& req) {
        auto id = parseId(req.pathParam("id"));
        if (!id) return error(400, "bad id");
        return store.remove(*id) ? Response{.status = 204, .contentType = "text/plain", .body = {}, .headers = {}}
                                 : error(404, "no todo " + req.pathParam("id"));
    });

    // Unknown API paths get a JSON 404, not the site's index.html.
    for (const char* method : {"GET", "POST", "PATCH", "PUT", "DELETE"})
        server.route(method, "/api/*", [](const Request& req) { return error(404, "no API route " + req.path); });
}

}  // namespace todo_api
