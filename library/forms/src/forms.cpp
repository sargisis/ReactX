// reactx::forms: the built-in rules and the JSON schema. Numbers become text
// with integers and std::to_string (no floating-point formatting: it is large
// in a .wasm).
#include "rx/forms.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

namespace rx::forms {

namespace {

std::string orDefault(const std::string& message, const std::string& fallback) { return message.empty() ? fallback : message; }

std::string withN(std::string message, std::size_t n) {
    if (auto at = message.find("{n}"); at != std::string::npos) message.replace(at, 3, std::to_string(n));
    return message;
}

bool blank(std::string_view s) {
    return std::ranges::all_of(s, [](char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r'; });
}

// Characters (UTF-8 code points), not bytes: "Ann" and "Аня" are both 3.
std::size_t length(std::string_view s) {
    std::size_t n = 0;
    for (char c : s)
        if ((static_cast<unsigned char>(c) & 0xC0) != 0x80) ++n;
    return n;
}

bool hasSpace(std::string_view s) {
    return std::ranges::any_of(s, [](char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r'; });
}

}  // namespace

Messages& messages() {
    static Messages m;
    return m;
}

std::string formatLimit(double n) {
    if (std::isnan(n)) return "NaN";
    const bool negative = n < 0;
    const double a = std::fabs(n);
    if (a >= 9e15) return negative ? "-9e15" : "9e15";  // limits that big are not real form limits
    const auto hundredths = static_cast<std::int64_t>(std::llround(a * 100));
    std::string out = std::to_string(hundredths / 100);
    if (const auto frac = hundredths % 100; frac != 0) {
        out += '.';
        out += static_cast<char>('0' + frac / 10);
        if (frac % 10) out += static_cast<char>('0' + frac % 10);
    }
    return negative ? "-" + out : out;
}

Rule<std::string> required(std::string message) {
    return [message = std::move(message)](const std::string& v) -> std::string {
        return blank(v) ? orDefault(message, messages().required) : std::string();
    };
}

Rule<std::string> minLength(std::size_t n, std::string message) {
    return [n, message = std::move(message)](const std::string& v) -> std::string {
        if (v.empty() || length(v) >= n) return {};
        return withN(orDefault(message, messages().minLength), n);
    };
}

Rule<std::string> maxLength(std::size_t n, std::string message) {
    return [n, message = std::move(message)](const std::string& v) -> std::string {
        if (length(v) <= n) return {};
        return withN(orDefault(message, messages().maxLength), n);
    };
}

Rule<std::string> email(std::string message) {
    return [message = std::move(message)](const std::string& v) -> std::string {
        if (v.empty()) return {};
        const auto at = v.find('@');
        const bool ok = at != std::string::npos && at > 0 && v.find('@', at + 1) == std::string::npos && !hasSpace(v) &&
                        [&] {
                            const std::string_view host = std::string_view(v).substr(at + 1);
                            const auto dot = host.rfind('.');
                            return dot != std::string_view::npos && dot > 0 && dot + 1 < host.size() &&
                                   !host.starts_with('.') && host.find("..") == std::string_view::npos;
                        }();
        return ok ? std::string() : orDefault(message, messages().email);
    };
}

Rule<std::string> url(std::string message) {
    return [message = std::move(message)](const std::string& v) -> std::string {
        if (v.empty()) return {};
        std::string_view rest = v;
        if (rest.starts_with("https://")) rest.remove_prefix(8);
        else if (rest.starts_with("http://")) rest.remove_prefix(7);
        else return orDefault(message, messages().url);
        const std::string_view host = rest.substr(0, rest.find_first_of("/?#"));
        const bool ok = !host.empty() && !hasSpace(v) && host.front() != '.' && host.front() != ':';
        return ok ? std::string() : orDefault(message, messages().url);
    };
}

Rule<std::string> digits(std::string message) {
    return [message = std::move(message)](const std::string& v) -> std::string {
        const bool ok = std::ranges::all_of(v, [](char c) { return c >= '0' && c <= '9'; });
        return ok ? std::string() : orDefault(message, messages().digits);
    };
}

Rule<std::string> oneOf(std::vector<std::string> choices, std::string message) {
    return [choices = std::move(choices), message = std::move(message)](const std::string& v) -> std::string {
        if (v.empty() || std::ranges::find(choices, v) != choices.end()) return {};
        return orDefault(message, messages().oneOf);
    };
}

Rule<bool> accepted(std::string message) {
    return [message = std::move(message)](const bool& v) -> std::string {
        return v ? std::string() : orDefault(message, messages().accepted);
    };
}

// ---- JSON ----

JsonSchema& JsonSchema::text(std::string name, std::initializer_list<Rule<std::string>> rules, bool optional) {
    std::vector<Rule<std::string>> list(rules);
    std::string key = name;
    checks_.push_back({std::move(name), [key, list = std::move(list), optional](const json::Value& v) -> std::string {
                           const json::Value& f = v[key];
                           if (f.isNull()) {
                               if (optional) return {};
                               // A missing text field is an empty one: required() says so.
                               static const std::string empty;
                               for (const auto& r : list)
                                   if (std::string m = r(empty); !m.empty()) return m;
                               return messages().required;
                           }
                           if (!f.isString()) return messages().notText;
                           for (const auto& r : list)
                               if (std::string m = r(f.asString()); !m.empty()) return m;
                           return {};
                       }});
    return *this;
}

JsonSchema& JsonSchema::number(std::string name, std::initializer_list<Rule<double>> rules, bool optional, bool whole) {
    std::vector<Rule<double>> list(rules);
    std::string key = name;
    checks_.push_back({std::move(name), [key, list = std::move(list), optional, whole](const json::Value& v) -> std::string {
                           const json::Value& f = v[key];
                           if (f.isNull()) return optional ? std::string() : messages().required;
                           if (!f.isNumber()) return messages().notNumber;
                           const double n = f.asNumber();
                           if (whole && n != std::floor(n)) return messages().notWhole;
                           for (const auto& r : list)
                               if (std::string m = r(n); !m.empty()) return m;
                           return {};
                       }});
    return *this;
}

JsonSchema& JsonSchema::boolean(std::string name, std::initializer_list<Rule<bool>> rules, bool optional) {
    std::vector<Rule<bool>> list(rules);
    std::string key = name;
    checks_.push_back({std::move(name), [key, list = std::move(list), optional](const json::Value& v) -> std::string {
                           const json::Value& f = v[key];
                           if (f.isNull()) return optional ? std::string() : messages().required;
                           if (!f.isBool()) return messages().notBool;
                           for (const auto& r : list)
                               if (std::string m = r(f.asBool()); !m.empty()) return m;
                           return {};
                       }});
    return *this;
}

JsonSchema& JsonSchema::check(std::string name, std::function<std::string(const json::Value&)> rule) {
    checks_.push_back({std::move(name), std::move(rule)});
    return *this;
}

Errors JsonSchema::validate(const json::Value& value) const {
    if (!value.isObject()) return {{"", messages().notObject}};
    Errors errors;
    for (const auto& c : checks_) {
        if (errors.contains(c.name)) continue;
        if (std::string m = c.rule(value); !m.empty()) errors.emplace(c.name, std::move(m));
    }
    return errors;
}

json::Value JsonSchema::toJson(const Errors& errors) {
    json::Value fields = json::object();
    for (const auto& [name, message] : errors) fields[name] = message;
    return json::object({{"errors", fields}});
}

}  // namespace rx::forms
