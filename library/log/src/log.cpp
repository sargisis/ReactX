// reactx::log: formatting and one lock around the sink. No std::format and no
// floating-point formatting (both are large in a .wasm).
#include "rx/log.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <string>
#include <utility>

#include "reactx/json.hpp"

namespace rx::log {

namespace {

struct State {
    std::mutex mutex;
    Options options;
};
State& state() {
    static State s;
    return s;
}

std::string pad(long long v, int width) {
    std::string s = std::to_string(v);
    return std::string(static_cast<std::size_t>(std::max(0, width - static_cast<int>(s.size()))), '0') + s;
}

// "2026-11-06T14:05:07Z"
std::string utcNow() {
    using namespace std::chrono;
    const auto now = floor<seconds>(system_clock::now());
    const auto day = floor<days>(now);
    const year_month_day ymd(day);
    const hh_mm_ss hms(now - day);
    return pad(static_cast<int>(ymd.year()), 4) + "-" + pad(static_cast<unsigned>(ymd.month()), 2) + "-" +
           pad(static_cast<unsigned>(ymd.day()), 2) + "T" + pad(hms.hours().count(), 2) + ":" + pad(hms.minutes().count(), 2) +
           ":" + pad(hms.seconds().count(), 2) + "Z";
}

// Whole numbers as integers, others with up to 6 decimals.
std::string number(double n) {
    if (std::isnan(n) || std::isinf(n)) return "null";
    if (n == std::floor(n) && std::fabs(n) < 9e15) return std::to_string(static_cast<long long>(n));
    const bool negative = n < 0;
    const auto scaled = static_cast<long long>(std::llround(std::fabs(n) * 1e6));
    std::string frac = pad(scaled % 1000000, 6);
    while (!frac.empty() && frac.back() == '0') frac.pop_back();
    return (negative ? "-" : "") + std::to_string(scaled / 1000000) + "." + frac;
}

const char* name(Level l) {
    switch (l) {
        case Level::Debug: return "debug";
        case Level::Info: return "info";
        case Level::Warn: return "warn";
        case Level::Error: return "error";
        case Level::Off: return "off";
    }
    return "?";
}

// Text values with spaces, quotes or '=' get quotes: msg="a b".
std::string textValue(const std::string& v) {
    const bool plain = !v.empty() && v.find_first_of(" \t\n\"=") == std::string::npos;
    if (plain) return v;
    std::string out = "\"";
    for (char c : v) {
        if (c == '"' || c == '\\') out += '\\';
        if (c == '\n') {
            out += "\\n";
            continue;
        }
        out += c;
    }
    return out + "\"";
}

}  // namespace

void setup(Options options) {
    std::lock_guard lock(state().mutex);
    state().options = std::move(options);
}

Level levelFromEnv(Level fallback) {
    const char* v = std::getenv("LOG_LEVEL");
    if (!v) return fallback;
    const std::string s = v;
    if (s == "debug") return Level::Debug;
    if (s == "info") return Level::Info;
    if (s == "warn" || s == "warning") return Level::Warn;
    if (s == "error") return Level::Error;
    if (s == "off" || s == "none") return Level::Off;
    return fallback;
}

bool enabled(Level level) {
    std::lock_guard lock(state().mutex);
    return level != Level::Off && level >= state().options.level;
}

void write(Level level, std::string_view message, std::initializer_list<Field> fields) {
    std::lock_guard lock(state().mutex);
    const Options& o = state().options;
    if (level == Level::Off || level < o.level) return;
    std::string line;
    if (o.json) {
        json::Value v = json::object();
        if (o.time) v["time"] = utcNow();
        v["level"] = name(level);
        v["msg"] = std::string(message);
        for (const auto& f : fields) {
            if (f.kind == Field::Kind::Text) v[f.key] = f.text;
            else if (f.kind == Field::Kind::Bool) v[f.key] = f.number != 0;
            else v[f.key] = f.number;
        }
        line = v.dump();
    } else {
        if (o.time) line = utcNow() + " ";
        std::string lvl = name(level);
        for (char& c : lvl) c = static_cast<char>(c - 'a' + 'A');
        line += lvl + std::string(6 - lvl.size(), ' ') + std::string(message);
        for (const auto& f : fields) {
            line += " " + f.key + "=";
            if (f.kind == Field::Kind::Text) line += textValue(f.text);
            else if (f.kind == Field::Kind::Bool) line += f.number != 0 ? "true" : "false";
            else line += number(f.number);
        }
    }
    if (o.sink) o.sink(line);
    else std::fprintf(stderr, "%s\n", line.c_str());
}

}  // namespace rx::log
