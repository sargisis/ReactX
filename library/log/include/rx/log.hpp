// reactx::log — structured logs: a level, a message and key=value fields, as
// text for people or as JSON lines for log collectors. Safe from any thread.
//
//   rx::log::info("signed in", {{"user", 42}, {"ip", request.remote}});
//   // 2026-11-06T14:05:07Z INFO  signed in user=42 ip=10.0.0.7
//   rx::log::setup({.level = rx::log::Level::Debug, .json = true});
//   // {"time":"2026-11-06T14:05:07Z","level":"info","msg":"signed in","user":42,"ip":"10.0.0.7"}
//
// Comes with ReactX (library/log); link it in CMakeLists.txt:
//   target_link_libraries(my_server PRIVATE reactx::log)
#pragma once

#include <concepts>
#include <functional>
#include <initializer_list>
#include <string>
#include <string_view>

namespace rx::log {

enum class Level { Debug, Info, Warn, Error, Off };

// A key=value pair: text, a number or a bool.
struct Field {
    std::string key;
    std::string text{};
    double number = 0;
    enum class Kind { Text, Number, Bool } kind = Kind::Text;

    Field(std::string_view k, std::string_view v) : key(k), text(v) {}
    Field(std::string_view k, const char* v) : key(k), text(v) {}
    Field(std::string_view k, const std::string& v) : key(k), text(v) {}
    Field(std::string_view k, bool v) : key(k), number(v ? 1 : 0), kind(Kind::Bool) {}
    template <class T>
        requires(std::integral<T> || std::floating_point<T>) && (!std::same_as<T, bool>)
    Field(std::string_view k, T v) : key(k), number(static_cast<double>(v)), kind(Kind::Number) {}
};

struct Options {
    Level level = Level::Info;  // lower levels are dropped
    bool json = false;          // one JSON object per line instead of text
    bool time = true;           // the UTC time first
    // Where lines go; default: stderr. Called with the log's lock held, one line at a time.
    std::function<void(std::string_view line)> sink{};
};

// Call once at start (and again to change it); the default is Info, text, stderr.
void setup(Options options);
// The level the LOG_LEVEL environment variable names (debug / info / warn / error / off), else `fallback`.
Level levelFromEnv(Level fallback = Level::Info);

void write(Level level, std::string_view message, std::initializer_list<Field> fields = {});
inline void debug(std::string_view m, std::initializer_list<Field> f = {}) { write(Level::Debug, m, f); }
inline void info(std::string_view m, std::initializer_list<Field> f = {}) { write(Level::Info, m, f); }
inline void warn(std::string_view m, std::initializer_list<Field> f = {}) { write(Level::Warn, m, f); }
inline void error(std::string_view m, std::initializer_list<Field> f = {}) { write(Level::Error, m, f); }

// Whether a level would be written (skip building costly fields otherwise).
bool enabled(Level level);

}  // namespace rx::log
