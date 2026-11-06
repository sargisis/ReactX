// reactx::config: arguments, environment, a JSON file.
#include "rx/config.hpp"

#include <cstdlib>
#include <fstream>
#include <iterator>
#include <string>
#include <utility>

#include "reactx/json.hpp"

namespace rx::config {

namespace {

std::string lower(std::string_view s) {
    std::string out(s);
    for (char& c : out) c = static_cast<char>(c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c);
    return out;
}

void flatten(const json::Value& v, const std::string& prefix, std::map<std::string, std::string, std::less<>>& out) {
    if (v.isObject()) {
        for (const auto& [name, child] : v.asObject()) flatten(child, prefix.empty() ? name : prefix + "." + name, out);
    } else if (v.isString()) {
        out[prefix] = v.asString();
    } else if (v.isBool()) {
        out[prefix] = v.asBool() ? "true" : "false";
    } else if (v.isNumber()) {
        // Numbers as written in JSON (no floating-point formatting of our own).
        out[prefix] = v.dump();
    } else if (!v.isNull()) {
        out[prefix] = v.dump();  // an array: its JSON text
    }
}

}  // namespace

std::string envName(std::string_view prefix, std::string_view key) {
    std::string out(prefix);
    for (char c : key) {
        if (c == '.' || c == '-') out += '_';
        else out += static_cast<char>(c >= 'a' && c <= 'z' ? c - 'a' + 'A' : c);
    }
    return out;
}

std::optional<std::string> Config::find(std::string_view key) const {
    if (auto it = args_.find(key); it != args_.end()) return it->second;
    if (!envPrefix_.empty())
        if (const char* v = std::getenv(envName(envPrefix_, key).c_str())) return std::string(v);
    if (auto it = file_.find(key); it != file_.end()) return it->second;
    return std::nullopt;
}

std::string Config::text(std::string_view key, std::string_view fallback) const {
    auto v = find(key);
    return v ? *v : std::string(fallback);
}

bool Config::flag(std::string_view key, bool fallback) const {
    auto v = find(key);
    if (!v) return fallback;
    const std::string s = lower(*v);
    if (s == "true" || s == "yes" || s == "on" || s == "1") return true;
    if (s == "false" || s == "no" || s == "off" || s == "0") return false;
    return fallback;
}

std::expected<std::string, std::string> Config::require(std::string_view key) const {
    if (auto v = find(key)) return *v;
    std::string where = "--" + std::string(key);
    if (!envPrefix_.empty()) where += ", " + envName(envPrefix_, key);
    if (!fileName_.empty()) where += ", " + fileName_;
    return std::unexpected("missing setting " + std::string(key) + " (" + where + ")");
}

std::string Config::source(std::string_view key) const {
    if (args_.contains(key)) return "argument";
    if (!envPrefix_.empty() && std::getenv(envName(envPrefix_, key).c_str())) return "environment";
    if (file_.contains(key)) return "file";
    return {};
}

std::expected<Config, std::string> load(LoadOptions options) {
    Config c;
    c.envPrefix_ = std::move(options.envPrefix);
    for (int i = 1; i < options.args.argc && options.args.argv; ++i) {
        std::string a = options.args.argv[i];
        if (a == "--") {  // the rest is positional
            for (++i; i < options.args.argc; ++i) c.positional_.emplace_back(options.args.argv[i]);
            break;
        }
        if (!a.starts_with("--") || a.size() == 2) {
            c.positional_.push_back(std::move(a));
            continue;
        }
        a.erase(0, 2);
        if (auto eq = a.find('='); eq != std::string::npos) {
            c.args_[a.substr(0, eq)] = a.substr(eq + 1);
        } else if (i + 1 < options.args.argc && !std::string_view(options.args.argv[i + 1]).starts_with("--")) {
            c.args_[a] = options.args.argv[++i];
        } else {
            c.args_[a] = "true";  // a flag
        }
    }
    if (!options.file.empty()) {
        c.fileName_ = options.file.string();
        std::ifstream in(options.file, std::ios::binary);
        if (!in) {
            if (options.fileRequired) return std::unexpected("can't read " + c.fileName_);
        } else {
            std::string text{std::istreambuf_iterator<char>(in), {}};
            auto parsed = json::tryParse(text);
            if (!parsed) return std::unexpected(c.fileName_ + ": invalid JSON: " + parsed.error().what());
            if (!parsed->isObject()) return std::unexpected(c.fileName_ + ": expected a JSON object");
            flatten(*parsed, "", c.file_);
        }
    }
    return c;
}

}  // namespace rx::config
