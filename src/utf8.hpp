#pragma once

#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

namespace rx::detail {

// Byte length of the UTF-8 sequence that starts with `lead` (1 for invalid bytes).
inline std::size_t utf8SeqLength(unsigned char lead) {
    if (lead >= 0xF0 && lead < 0xF8) return 4;
    if (lead >= 0xE0) return lead < 0xF0 ? 3 : 1;
    if (lead >= 0xC0) return 2;
    return 1;
}

// Splits text into code points (each as a view into `text`).
inline std::vector<std::string_view> utf8Split(std::string_view text) {
    std::vector<std::string_view> out;
    for (std::size_t i = 0; i < text.size();) {
        std::size_t n = std::min(utf8SeqLength(static_cast<unsigned char>(text[i])), text.size() - i);
        out.push_back(text.substr(i, n));
        i += n;
    }
    return out;
}

// Number of code points (one terminal column each).
inline int utf8Length(std::string_view text) { return static_cast<int>(utf8Split(text).size()); }

// Removes the last code point.
inline void utf8PopBack(std::string& text) {
    while (!text.empty()) {
        unsigned char c = static_cast<unsigned char>(text.back());
        text.pop_back();
        if ((c & 0xC0) != 0x80) break;  // stop after the lead byte
    }
}

// A key that types a character: one code point, not a control character.
inline bool isPrintableKey(std::string_view key) {
    if (key.empty()) return false;
    auto lead = static_cast<unsigned char>(key[0]);
    return lead >= 0x20 && lead != 0x7F && utf8SeqLength(lead) == key.size();
}

}  // namespace rx::detail
