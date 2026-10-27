#pragma once

// URL path helpers shared by the router (browser/terminal) and the HTTP
// server, so both understand routes like "/todos/:id" the same way.

#include <map>
#include <optional>
#include <string>
#include <string_view>

namespace rx {

// "%20" -> " ", and "+" -> " " when `plusIsSpace` (query strings).
inline std::string urlDecode(std::string_view text, bool plusIsSpace = false) {
    std::string out;
    out.reserve(text.size());
    auto hex = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    };
    for (std::size_t i = 0; i < text.size(); ++i) {
        char c = text[i];
        if (c == '%' && i + 2 < text.size()) {
            int hi = hex(text[i + 1]), lo = hex(text[i + 2]);
            if (hi >= 0 && lo >= 0) {
                out += static_cast<char>(hi * 16 + lo);
                i += 2;
                continue;
            }
        }
        out += (plusIsSpace && c == '+') ? ' ' : c;
    }
    return out;
}

// Percent-encodes everything but the unreserved characters (A-Z a-z 0-9 - . _ ~),
// for one query value or path segment: "milk & tea/2" -> "milk%20%26%20tea%2F2".
// urlDecode gives the text back.
//   fetch({.url = "/api/logs?q=" + rx::urlEncode(query)}, ...)
inline std::string urlEncode(std::string_view text) {
    static constexpr char hex[] = "0123456789ABCDEF";
    std::string out;
    out.reserve(text.size());
    for (char ch : text) {
        auto c = static_cast<unsigned char>(ch);
        bool unreserved = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' ||
                          c == '.' || c == '_' || c == '~';
        if (unreserved) {
            out += ch;
        } else {
            out += '%';
            out += hex[c >> 4];
            out += hex[c & 0xF];
        }
    }
    return out;
}

// Matches "/todos/:id" against "/todos/7" and fills params ({"id": "7"}).
// "*" as the last segment matches the rest of the path (params["*"]).
// Extra or missing slashes don't matter. Parameter values are URL-decoded.
inline bool matchPath(std::string_view pattern, std::string_view path, std::map<std::string, std::string>& params) {
    auto next = [](std::string_view& s) {  // pops the next segment
        while (!s.empty() && s.front() == '/') s.remove_prefix(1);
        auto slash = s.find('/');
        std::string_view seg = s.substr(0, slash);
        s = slash == std::string_view::npos ? std::string_view{} : s.substr(slash);
        return seg;
    };
    std::map<std::string, std::string> found;
    while (true) {
        std::string_view p = next(pattern);
        if (p == "*") {
            while (!path.empty() && path.front() == '/') path.remove_prefix(1);
            found["*"] = urlDecode(path);
            params = std::move(found);
            return true;
        }
        std::string_view s = next(path);
        if (p.empty() || s.empty()) {
            if (p.empty() && s.empty()) {
                params = std::move(found);
                return true;
            }
            return false;
        }
        if (p.front() == ':') found[std::string(p.substr(1))] = urlDecode(s);
        else if (p != s) return false;
    }
}

// Decoded value of `name` in a query string ("q=milk%20tea&page=2").
inline std::optional<std::string> queryParam(std::string_view query, std::string_view name) {
    if (!query.empty() && query.front() == '?') query.remove_prefix(1);
    while (!query.empty()) {
        auto amp = query.find('&');
        std::string_view pair = query.substr(0, amp);
        query = amp == std::string_view::npos ? std::string_view{} : query.substr(amp + 1);
        auto eq = pair.find('=');
        if (urlDecode(pair.substr(0, eq), true) != name) continue;
        return eq == std::string_view::npos ? std::string{} : urlDecode(pair.substr(eq + 1), true);
    }
    return std::nullopt;
}

}  // namespace rx
