// reactx::config — settings for a server or a tool from, in this order:
// command-line arguments, environment variables, a JSON file, your defaults.
//
//   auto config = rx::config::load({.args = {argc, argv}, .envPrefix = "TODO_", .file = "todo.json"});
//   if (!config) { std::println(stderr, "{}", config.error()); return 2; }   // a broken file, a bad argument
//   int port = config->number("port", 8080);         // --port 9000, TODO_PORT=9000, {"port": 9000}
//   std::string db = config->text("db.path", "todo.db");   // --db.path x, TODO_DB_PATH=x, {"db": {"path": "x"}}
//   auto secret = config->require("session.secret");  // std::expected: says which names it looked for
//
// Comes with ReactX (library/config); link it in CMakeLists.txt:
//   target_link_libraries(my_server PRIVATE reactx::config)
#pragma once

#include <concepts>
#include <expected>
#include <filesystem>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "reactx/parse.hpp"

namespace rx::config {

struct Args {
    int argc = 0;
    char** argv = nullptr;
};

struct LoadOptions {
    Args args{};                  // --name value, --name=value, --flag (= true); the rest stays in positional()
    std::string envPrefix{};      // "TODO_": the key "db.path" is read from TODO_DB_PATH; "" = no environment
    std::filesystem::path file{}; // a JSON file; nested objects give dotted keys; "" = none
    bool fileRequired = false;    // a missing file is an error (otherwise it is skipped)
};

class Config {
public:
    // The value as text, from the first source that has it.
    std::optional<std::string> find(std::string_view key) const;
    std::string text(std::string_view key, std::string_view fallback = {}) const;
    // A number, or `fallback` when it is missing or isn't one (requireNumber says why).
    template <class T>
        requires Number<T>
    T number(std::string_view key, T fallback) const {
        auto v = find(key);
        if (!v) return fallback;
        auto n = parseNumber<T>(*v);
        return n ? *n : fallback;
    }
    // "true/yes/on/1" and "false/no/off/0" (any case); a flag given alone (--verbose) is true.
    bool flag(std::string_view key, bool fallback = false) const;
    // The value, or an error naming where it was looked for: "missing setting session.secret
    // (--session.secret, TODO_SESSION_SECRET, todo.json)".
    std::expected<std::string, std::string> require(std::string_view key) const;
    template <class T>
        requires Number<T>
    std::expected<T, std::string> requireNumber(std::string_view key) const {
        auto v = require(key);
        if (!v) return std::unexpected(v.error());
        auto n = parseNumber<T>(*v);
        if (!n) return std::unexpected("setting " + std::string(key) + " = \"" + *v + "\": " + std::string(describe(n.error())));
        return *n;
    }

    // Arguments that aren't --options (file names, a command).
    const std::vector<std::string>& positional() const { return positional_; }
    // Where a key's value came from: "argument", "environment", "file", or "" (not set).
    std::string source(std::string_view key) const;

    // Built by load(); public for tests and tools that fill one by hand.
    std::map<std::string, std::string, std::less<>> args_, file_;
    std::string envPrefix_;
    std::string fileName_;
    std::vector<std::string> positional_;
};

// Reads the sources. Errors: an unreadable or invalid JSON file, a missing
// required file, a bare "--".
std::expected<Config, std::string> load(LoadOptions options);

// "db.path" with prefix "TODO_" -> "TODO_DB_PATH".
std::string envName(std::string_view prefix, std::string_view key);

}  // namespace rx::config
