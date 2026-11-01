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

// Number of code points.
inline int utf8Length(std::string_view text) { return static_cast<int>(utf8Split(text).size()); }

// The first code point of `text` (U+FFFD for a broken sequence).
inline char32_t utf8Decode(std::string_view text) {
    if (text.empty()) return 0;
    auto b = [&](std::size_t i) { return static_cast<unsigned char>(text[i]); };
    std::size_t n = utf8SeqLength(b(0));
    if (n > text.size()) return 0xFFFD;
    if (n == 1) return b(0) < 0x80 ? b(0) : 0xFFFD;
    char32_t c = b(0) & (0xFF >> (n + 1));
    for (std::size_t i = 1; i < n; ++i) {
        if ((b(i) & 0xC0) != 0x80) return 0xFFFD;
        c = (c << 6) | (b(i) & 0x3F);
    }
    return c;
}

// Terminal columns of one code point: 0 for marks that join the character
// before them (accents, variation selectors, skin tones, the zero-width
// joiner), 2 for wide characters (CJK, Hangul, fullwidth forms, emoji),
// 1 for the rest. Close to what terminals use (wcwidth, Unicode 15 East
// Asian Width and emoji presentation).
inline int codepointWidth(char32_t c) {
    struct Range {
        char32_t from, to;
    };
    static constexpr Range zero[] = {
        {0x0300, 0x036F}, {0x0483, 0x0489}, {0x0591, 0x05BD}, {0x0610, 0x061A}, {0x064B, 0x065F}, {0x0E31, 0x0E31},
        {0x0E34, 0x0E3A}, {0x0E47, 0x0E4E}, {0x1AB0, 0x1AFF}, {0x1DC0, 0x1DFF}, {0x200B, 0x200F}, {0x2028, 0x202E},
        {0x2060, 0x2064}, {0x20D0, 0x20FF}, {0xFE00, 0xFE0F}, {0xFE20, 0xFE2F}, {0xFEFF, 0xFEFF}, {0x1F3FB, 0x1F3FF},
        {0xE0000, 0xE007F}, {0xE0100, 0xE01EF},
    };
    static constexpr Range wide[] = {
        {0x1100, 0x115F},   {0x231A, 0x231B},   {0x2329, 0x232A},   {0x23E9, 0x23EC},   {0x23F0, 0x23F0},
        {0x23F3, 0x23F3},   {0x25FD, 0x25FE},   {0x2614, 0x2615},   {0x2648, 0x2653},   {0x267F, 0x267F},
        {0x2693, 0x2693},   {0x26A1, 0x26A1},   {0x26AA, 0x26AB},   {0x26BD, 0x26BE},   {0x26C4, 0x26C5},
        {0x26CE, 0x26CE},   {0x26D4, 0x26D4},   {0x26EA, 0x26EA},   {0x26F2, 0x26F3},   {0x26F5, 0x26F5},
        {0x26FA, 0x26FA},   {0x26FD, 0x26FD},   {0x2705, 0x2705},   {0x270A, 0x270B},   {0x2728, 0x2728},
        {0x274C, 0x274C},   {0x274E, 0x274E},   {0x2753, 0x2755},   {0x2757, 0x2757},   {0x2795, 0x2797},
        {0x27B0, 0x27B0},   {0x27BF, 0x27BF},   {0x2B1B, 0x2B1C},   {0x2B50, 0x2B50},   {0x2B55, 0x2B55},
        {0x2E80, 0x303E},   {0x3041, 0x33FF},   {0x3400, 0x4DBF},   {0x4E00, 0x9FFF},   {0xA000, 0xA4CF},
        {0xA960, 0xA97F},   {0xAC00, 0xD7A3},   {0xF900, 0xFAFF},   {0xFE10, 0xFE19},   {0xFE30, 0xFE6F},
        {0xFF00, 0xFF60},   {0xFFE0, 0xFFE6},   {0x16FE0, 0x18AFF}, {0x1B000, 0x1B2FF}, {0x1F004, 0x1F004},
        {0x1F0CF, 0x1F0CF}, {0x1F18E, 0x1F18E}, {0x1F191, 0x1F19A}, {0x1F1E6, 0x1F1FF}, {0x1F200, 0x1F202},
        {0x1F210, 0x1F23B}, {0x1F240, 0x1F248}, {0x1F250, 0x1F251}, {0x1F260, 0x1F265}, {0x1F300, 0x1F320},
        {0x1F32D, 0x1F335}, {0x1F337, 0x1F37C}, {0x1F37E, 0x1F393}, {0x1F3A0, 0x1F3CA}, {0x1F3CF, 0x1F3D3},
        {0x1F3E0, 0x1F3F0}, {0x1F3F4, 0x1F3F4}, {0x1F3F8, 0x1F43E}, {0x1F440, 0x1F440}, {0x1F442, 0x1F4FC},
        {0x1F4FF, 0x1F53D}, {0x1F54B, 0x1F54E}, {0x1F550, 0x1F567}, {0x1F57A, 0x1F57A}, {0x1F595, 0x1F596},
        {0x1F5A4, 0x1F5A4}, {0x1F5FB, 0x1F64F}, {0x1F680, 0x1F6C5}, {0x1F6CC, 0x1F6CC}, {0x1F6D0, 0x1F6D2},
        {0x1F6D5, 0x1F6D7}, {0x1F6DC, 0x1F6DF}, {0x1F6EB, 0x1F6EC}, {0x1F6F4, 0x1F6FC}, {0x1F7E0, 0x1F7EB},
        {0x1F7F0, 0x1F7F0}, {0x1F90C, 0x1F93A}, {0x1F93C, 0x1F945}, {0x1F947, 0x1F9FF}, {0x1FA70, 0x1FAFF},
        {0x20000, 0x2FFFD}, {0x30000, 0x3FFFD},
    };
    auto in = [c](const auto& table) {
        auto it = std::ranges::upper_bound(table, c, {}, &Range::from);
        return it != std::ranges::begin(table) && c <= (it - 1)->to;
    };
    if (c < 0x300) return 1;
    if (in(zero)) return 0;
    return in(wide) ? 2 : 1;
}

// One character as the user sees it: a code point with the marks after it
// (é as e + U+0301, 👍🏽, ❤️), emoji joined by U+200D (👨‍👩‍👧) and the
// two regional indicators of a flag (🇷🇺). `width` is in terminal columns.
struct Glyph {
    std::string_view text;
    int width = 1;
};

inline std::vector<Glyph> utf8Glyphs(std::string_view text) {
    std::vector<Glyph> out;
    auto isFlagHalf = [](char32_t c) { return c >= 0x1F1E6 && c <= 0x1F1FF; };
    for (std::size_t i = 0; i < text.size();) {
        auto next = [&](std::size_t at) { return std::min(utf8SeqLength(static_cast<unsigned char>(text[at])), text.size() - at); };
        std::size_t start = i;
        char32_t base = utf8Decode(text.substr(i));
        i += next(i);
        int width = codepointWidth(base);
        if (isFlagHalf(base) && i < text.size() && isFlagHalf(utf8Decode(text.substr(i)))) i += next(i);
        while (i < text.size()) {
            char32_t c = utf8Decode(text.substr(i));
            if (c == 0xFE0F && width == 1 && base >= 0x2000) width = 2;  // ❤️: the emoji form of a symbol is wide
            if (c == 0x200D) {  // a joiner: the next code point belongs here too
                i += next(i);
                if (i < text.size()) i += next(i);
                continue;
            }
            if (codepointWidth(c) != 0) break;
            i += next(i);
        }
        out.push_back({text.substr(start, i - start), base < 0x20 || base == 0x7F ? 0 : width});
    }
    return out;
}

// Terminal columns of a line of text.
inline int textCells(std::string_view text) {
    int w = 0;
    for (const Glyph& g : utf8Glyphs(text)) w += g.width;
    return w;
}

// Removes the last code point.
inline void utf8PopBack(std::string& text) {
    while (!text.empty()) {
        unsigned char c = static_cast<unsigned char>(text.back());
        text.pop_back();
        if ((c & 0xC0) != 0x80) break;  // stop after the lead byte
    }
}

// A key that types text: one code point that isn't a control character, or
// several that aren't plain ASCII (👍🏽, ❤️, a family emoji; key names like
// "Left" are ASCII words).
inline bool isPrintableKey(std::string_view key) {
    if (key.empty()) return false;
    auto lead = static_cast<unsigned char>(key[0]);
    if (utf8SeqLength(lead) == key.size()) return lead >= 0x20 && lead != 0x7F;
    bool wide = false;
    for (unsigned char c : key) {
        if (c < 0x20 || c == 0x7F) return false;
        if (c >= 0x80) wide = true;
    }
    return wide;
}

}  // namespace rx::detail
