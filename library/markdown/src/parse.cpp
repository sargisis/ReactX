// Markdown -> Document. A small hand-written parser (no <regex>: it is large in
// a .wasm): blocks line by line, then the inline markup of each paragraph.
#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "rx/markdown.hpp"

namespace rx::markdown {

namespace {

using Lines = std::vector<std::string>;

bool blank(std::string_view s) {
    return std::ranges::all_of(s, [](char c) { return c == ' ' || c == '\t' || c == '\r'; });
}

// Leading spaces (a tab counts as 4).
int indentOf(std::string_view s) {
    int n = 0;
    for (char c : s) {
        if (c == ' ') ++n;
        else if (c == '\t') n += 4 - n % 4;
        else break;
    }
    return n;
}

// `s` without its first `n` columns of indentation.
std::string dedent(std::string_view s, int n) {
    int col = 0;
    std::size_t i = 0;
    while (i < s.size() && col < n && (s[i] == ' ' || s[i] == '\t')) {
        col += s[i] == '\t' ? 4 - col % 4 : 1;
        ++i;
    }
    return std::string(s.substr(i));
}

std::string_view trim(std::string_view s) {
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) s.remove_prefix(1);
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r')) s.remove_suffix(1);
    return s;
}

bool isPunct(char c) { return std::ispunct(static_cast<unsigned char>(c)) != 0; }
bool isAlnum(char c) { return std::isalnum(static_cast<unsigned char>(c)) != 0 || static_cast<unsigned char>(c) >= 0x80; }

// ---- block starts ----

int headingLevel(std::string_view line) {
    if (indentOf(line) > 3) return 0;
    std::string_view t = trim(line);
    int n = 0;
    while (n < static_cast<int>(t.size()) && t[static_cast<std::size_t>(n)] == '#') ++n;
    if (n < 1 || n > 6) return 0;
    if (static_cast<std::size_t>(n) < t.size() && t[static_cast<std::size_t>(n)] != ' ' && t[static_cast<std::size_t>(n)] != '\t')
        return 0;
    return n;
}

bool isRule(std::string_view line) {
    if (indentOf(line) > 3) return false;
    char mark = 0;
    int count = 0;
    for (char c : line) {
        if (c == ' ' || c == '\t' || c == '\r') continue;
        if (c != '-' && c != '*' && c != '_') return false;
        if (mark && c != mark) return false;
        mark = c;
        ++count;
    }
    return count >= 3;
}

// "```lang" / "~~~": the fence (its char and length) and the info string.
bool fenceOpen(std::string_view line, char& ch, std::size_t& len, std::string& info) {
    if (indentOf(line) > 3) return false;
    std::string_view t = trim(line);
    if (t.size() < 3 || (t[0] != '`' && t[0] != '~')) return false;
    ch = t[0];
    len = 0;
    while (len < t.size() && t[len] == ch) ++len;
    if (len < 3) return false;
    info = std::string(trim(t.substr(len)));
    if (ch == '`' && info.find('`') != std::string::npos) return false;
    if (auto sp = info.find(' '); sp != std::string::npos) info.resize(sp);
    return true;
}

bool fenceClose(std::string_view line, char ch, std::size_t len) {
    if (indentOf(line) > 3) return false;
    std::string_view t = trim(line);
    std::size_t n = 0;
    while (n < t.size() && t[n] == ch) ++n;
    return n >= len && n == t.size();
}

struct Marker {
    bool ordered = false;
    char bullet = 0;  // '-', '*', '+'; for ordered: '.' or ')'
    int number = 1;
    int contentIndent = 0;     // columns before the item's text
    std::size_t contentStart = 0;  // where the text starts in the marker's line
};

bool listMarker(std::string_view line, Marker& m) {
    const int ind = indentOf(line);
    if (ind > 3) return false;
    std::size_t i = 0;
    while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) ++i;
    std::string_view t = line.substr(i);
    std::size_t markLen = 0;
    if (!t.empty() && (t[0] == '-' || t[0] == '*' || t[0] == '+')) {
        m.ordered = false;
        m.bullet = t[0];
        markLen = 1;
    } else {
        std::size_t d = 0;
        while (d < t.size() && d < 9 && std::isdigit(static_cast<unsigned char>(t[d]))) ++d;
        if (d == 0 || d >= t.size() || (t[d] != '.' && t[d] != ')')) return false;
        m.ordered = true;
        m.bullet = t[d];
        m.number = 0;
        for (std::size_t k = 0; k < d; ++k) m.number = m.number * 10 + (t[k] - '0');
        markLen = d + 1;
    }
    if (markLen < t.size() && t[markLen] != ' ' && t[markLen] != '\t') return false;
    std::size_t spaces = 0;
    while (markLen + spaces < t.size() && t[markLen + spaces] == ' ') ++spaces;
    if (markLen + spaces >= t.size()) spaces = 1;  // "-" alone: an empty item
    if (spaces > 4) spaces = 1;                     // indented code after the marker
    m.contentIndent = ind + static_cast<int>(markLen + spaces);
    m.contentStart = std::min(line.size(), i + markLen + spaces);
    return true;
}

std::vector<std::string> splitRow(std::string_view line) {
    std::string_view t = trim(line);
    if (!t.empty() && t.front() == '|') t.remove_prefix(1);
    if (!t.empty() && t.back() == '|' && (t.size() < 2 || t[t.size() - 2] != '\\')) t.remove_suffix(1);
    std::vector<std::string> cells;
    std::string cell;
    bool code = false;
    for (std::size_t i = 0; i < t.size(); ++i) {
        const char c = t[i];
        if (c == '\\' && i + 1 < t.size() && t[i + 1] == '|') {
            cell += '|';
            ++i;
        } else if (c == '`') {
            code = !code;
            cell += c;
        } else if (c == '|' && !code) {
            cells.emplace_back(trim(cell));
            cell.clear();
        } else {
            cell += c;
        }
    }
    cells.emplace_back(trim(cell));
    return cells;
}

bool delimiterRow(std::string_view line, std::vector<char>& align) {
    if (line.find('-') == std::string_view::npos) return false;
    align.clear();
    for (auto& cell : splitRow(line)) {
        std::string_view c = trim(cell);
        if (c.empty()) return false;
        const bool left = c.front() == ':', right = c.back() == ':';
        std::string_view dashes = c.substr(left ? 1 : 0, c.size() - (left ? 1 : 0) - (right && c.size() > 1 ? 1 : 0));
        if (dashes.empty() || !std::ranges::all_of(dashes, [](char x) { return x == '-'; })) return false;
        align.push_back(left && right ? 'c' : right ? 'r' : left ? 'l' : 0);
    }
    return true;
}

bool startsBlock(std::string_view line) {
    char ch;
    std::size_t len;
    std::string info;
    Marker m;
    return headingLevel(line) || isRule(line) || fenceOpen(line, ch, len, info) || trim(line).starts_with('>') ||
           (listMarker(line, m) && (!m.ordered || m.number == 1));
}

// ---- inlines ----

std::string slug(const std::vector<Inline>& inlines);

bool safeUrl(std::string_view url) {
    std::string lower;
    for (char c : url) {
        if (c == ' ' || c == '\t' || c == '\n') continue;  // "java script:" tricks
        lower += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    auto colon = lower.find(':');
    if (colon == std::string::npos) return true;  // relative: "/docs", "#top", "a.png"
    const auto slash = lower.find_first_of("/?#");
    if (slash != std::string::npos && slash < colon) return true;  // "/a:b"
    const std::string scheme = lower.substr(0, colon);
    return scheme == "http" || scheme == "https" || scheme == "mailto" || scheme == "tel";
}

class InlineParser {
public:
    explicit InlineParser(std::string_view s) : s_(s) {}

    std::vector<Inline> parse() { return run(0, s_.size()); }

private:
    std::string_view s_;

    static void addText(std::vector<Inline>& out, std::string_view text) {
        if (text.empty()) return;
        if (!out.empty() && out.back().kind == Inline::Kind::Text) out.back().text += text;
        else out.push_back({.kind = Inline::Kind::Text, .text = std::string(text)});
    }

    // The closing run of `delim` for an opening one at `from`, before `end`.
    std::size_t closer(std::string_view delim, std::size_t from, std::size_t end) const {
        std::size_t i = from;
        while (i + delim.size() <= end) {
            if (s_[i] == '\\') {
                i += 2;
                continue;
            }
            if (s_[i] == '`') {  // skip code spans
                std::size_t n = 0;
                while (i + n < end && s_[i + n] == '`') ++n;
                auto close = s_.find(std::string(n, '`'), i + n);
                if (close != std::string_view::npos && close < end) {
                    i = close + n;
                    continue;
                }
            }
            if (s_.substr(i, delim.size()) == delim && i > from && s_[i - 1] != ' ' && s_[i - 1] != '\t') {
                // "_" doesn't close inside a word ("snake_case_name").
                if (delim[0] == '_' && i + delim.size() < end && isAlnum(s_[i + delim.size()])) {
                    ++i;
                    continue;
                }
                // "**a**" must not end "*a" at its first "*".
                if (delim.size() == 1 && i + 1 < end && s_[i + 1] == delim[0] && (i + 2 >= end || s_[i + 2] != delim[0])) {
                    i += 2;
                    continue;
                }
                return i;
            }
            ++i;
        }
        return std::string_view::npos;
    }

    // "[label](url)" at `i` (s_[i] == '['): the end of the label and of the whole.
    bool link(std::size_t i, std::size_t end, std::size_t& labelEnd, std::size_t& after, std::string& url) const {
        int depth = 0;
        std::size_t j = i;
        for (; j < end; ++j) {
            if (s_[j] == '\\') {
                ++j;
                continue;
            }
            if (s_[j] == '[') ++depth;
            else if (s_[j] == ']' && --depth == 0) break;
        }
        if (j >= end || j + 1 >= end || s_[j + 1] != '(') return false;
        labelEnd = j;
        std::size_t k = j + 2;
        while (k < end && s_[k] == ' ') ++k;
        std::size_t urlStart = k, urlEnd;
        if (k < end && s_[k] == '<') {
            auto close = s_.find('>', k);
            if (close == std::string_view::npos || close >= end) return false;
            urlStart = k + 1;
            urlEnd = close;
            k = close + 1;
        } else {
            int parens = 0;
            while (k < end && s_[k] != ' ' && !(s_[k] == ')' && parens == 0)) {
                if (s_[k] == '(') ++parens;
                if (s_[k] == ')') --parens;
                ++k;
            }
            urlEnd = k;
        }
        while (k < end && s_[k] == ' ') ++k;
        if (k < end && (s_[k] == '"' || s_[k] == '\'')) {  // a title: skipped
            auto close = s_.find(s_[k], k + 1);
            if (close == std::string_view::npos || close >= end) return false;
            k = close + 1;
            while (k < end && s_[k] == ' ') ++k;
        }
        if (k >= end || s_[k] != ')') return false;
        url = std::string(s_.substr(urlStart, urlEnd - urlStart));
        after = k + 1;
        return true;
    }

    std::vector<Inline> run(std::size_t from, std::size_t end) {
        std::vector<Inline> out;
        std::size_t textStart = from;
        auto flush = [&](std::size_t upto) {
            addText(out, s_.substr(textStart, upto - textStart));
        };
        std::size_t i = from;
        while (i < end) {
            const char c = s_[i];
            // Escapes and hard line breaks.
            if (c == '\\' && i + 1 < end) {
                flush(i);
                if (s_[i + 1] == '\n') {
                    out.push_back({.kind = Inline::Kind::LineBreak});
                } else if (isPunct(s_[i + 1])) {
                    addText(out, s_.substr(i + 1, 1));
                } else {
                    addText(out, s_.substr(i, 2));
                }
                i += 2;
                textStart = i;
                continue;
            }
            if (c == '\n') {
                std::size_t spaces = 0;
                while (spaces < i - textStart && s_[i - 1 - spaces] == ' ') ++spaces;
                flush(i - spaces);
                if (spaces >= 2) out.push_back({.kind = Inline::Kind::LineBreak});
                else addText(out, " ");
                ++i;
                while (i < end && s_[i] == ' ') ++i;
                textStart = i;
                continue;
            }
            if (c == '`') {
                std::size_t n = 0;
                while (i + n < end && s_[i + n] == '`') ++n;
                auto close = s_.find(std::string(n, '`'), i + n);
                while (close != std::string_view::npos && close + n < end && s_[close + n] == '`')
                    close = s_.find(std::string(n, '`'), close + n + 1);
                if (close != std::string_view::npos && close < end) {
                    flush(i);
                    std::string code(s_.substr(i + n, close - i - n));
                    std::ranges::replace(code, '\n', ' ');
                    if (code.size() >= 2 && code.front() == ' ' && code.back() == ' ' && !blank(code))
                        code = code.substr(1, code.size() - 2);
                    out.push_back({.kind = Inline::Kind::Code, .text = std::move(code)});
                    i = close + n;
                    textStart = i;
                    continue;
                }
                i += n;
                continue;
            }
            if (c == '!' && i + 1 < end && s_[i + 1] == '[') {
                std::size_t labelEnd, after;
                std::string url;
                if (link(i + 1, end, labelEnd, after, url)) {
                    flush(i);
                    out.push_back({.kind = Inline::Kind::Image,
                                   .text = plainOf(run(i + 2, labelEnd)),
                                   .url = safeUrl(url) ? url : ""});
                    i = after;
                    textStart = i;
                    continue;
                }
            }
            if (c == '[') {
                std::size_t labelEnd, after;
                std::string url;
                if (link(i, end, labelEnd, after, url)) {
                    flush(i);
                    out.push_back({.kind = Inline::Kind::Link,
                                   .url = safeUrl(url) ? url : "",
                                   .children = run(i + 1, labelEnd)});
                    i = after;
                    textStart = i;
                    continue;
                }
            }
            if (c == '<') {  // <https://...>, <mail@x.y>
                auto close = s_.find('>', i);
                if (close != std::string_view::npos && close < end) {
                    std::string_view inner = s_.substr(i + 1, close - i - 1);
                    const bool url = inner.starts_with("http://") || inner.starts_with("https://");
                    const bool mail = !url && inner.find('@') != std::string_view::npos && inner.find(' ') == std::string_view::npos;
                    if ((url || mail) && inner.find(' ') == std::string_view::npos) {
                        flush(i);
                        out.push_back({.kind = Inline::Kind::Link,
                                       .url = (mail ? "mailto:" : "") + std::string(inner),
                                       .children = {{.kind = Inline::Kind::Text, .text = std::string(inner)}}});
                        i = close + 1;
                        textStart = i;
                        continue;
                    }
                }
            }
            if ((c == 'h') && (i == from || !isAlnum(s_[i - 1])) &&
                (s_.substr(i, 7) == "http://" || s_.substr(i, 8) == "https://")) {  // a bare link
                std::size_t j = i;
                while (j < end && s_[j] != ' ' && s_[j] != '\n' && s_[j] != '<') ++j;
                while (j > i && std::string_view(".,:;!?)'\"").find(s_[j - 1]) != std::string_view::npos) --j;
                flush(i);
                std::string url(s_.substr(i, j - i));
                out.push_back({.kind = Inline::Kind::Link,
                               .url = url,
                               .children = {{.kind = Inline::Kind::Text, .text = url}}});
                i = j;
                textStart = i;
                continue;
            }
            if (c == '*' || c == '_' || c == '~') {
                std::size_t n = 0;
                while (i + n < end && s_[i + n] == c) ++n;
                const bool canOpen = i + n < end && s_[i + n] != ' ' && s_[i + n] != '\n' &&
                                     !(c == '_' && i > from && isAlnum(s_[i - 1]));
                if (canOpen && (c != '~' || n == 2)) {
                    // *** is strong + emphasis; try the longest delimiter first.
                    const std::size_t len = c == '~' ? 2 : std::min<std::size_t>(n, 3);
                    for (std::size_t take = len; take >= 1; --take) {
                        if (take == 3) {
                            std::string d(3, c);
                            auto close = closer(d, i + 3, end);
                            if (close == std::string_view::npos) continue;
                            flush(i);
                            out.push_back({.kind = Inline::Kind::Strong,
                                           .children = {{.kind = Inline::Kind::Emphasis, .children = run(i + 3, close)}}});
                            i = close + 3;
                            textStart = i;
                            goto next;
                        }
                        std::string d(take, c);
                        auto close = closer(d, i + take, end);
                        if (close == std::string_view::npos) continue;
                        flush(i);
                        out.push_back({.kind = c == '~' ? Inline::Kind::Strike
                                               : take == 2 ? Inline::Kind::Strong
                                                           : Inline::Kind::Emphasis,
                                       .children = run(i + take, close)});
                        i = close + take;
                        textStart = i;
                        goto next;
                    }
                }
                i += n;
                continue;
            }
            ++i;
        next:;
        }
        flush(end);
        return out;
    }

public:
    static std::string plainOf(const std::vector<Inline>& inlines) {
        std::string out;
        for (const auto& in : inlines) {
            if (in.kind == Inline::Kind::Text || in.kind == Inline::Kind::Code || in.kind == Inline::Kind::Image)
                out += in.text;
            else if (in.kind == Inline::Kind::LineBreak)
                out += ' ';
            else
                out += plainOf(in.children);
        }
        return out;
    }
};

std::vector<Inline> inlines(std::string_view text) { return InlineParser(trim(text)).parse(); }

std::string slug(const std::vector<Inline>& in) {
    std::string out;
    bool dash = false;
    for (char c : InlineParser::plainOf(in)) {
        const auto u = static_cast<unsigned char>(c);
        if (std::isalnum(u) || u >= 0x80) {
            if (dash && !out.empty()) out += '-';
            dash = false;
            out += static_cast<char>(std::tolower(u));
        } else if (c == ' ' || c == '-' || c == '_') {
            dash = true;
        }
    }
    return out;
}

// ---- blocks ----

class BlockParser {
public:
    explicit BlockParser(Lines lines) : lines_(std::move(lines)) {}

    Document parse() {
        Document doc;
        while (i_ < lines_.size()) {
            if (blank(lines_[i_])) {
                ++i_;
                continue;
            }
            doc.push_back(block());
        }
        return doc;
    }

private:
    Lines lines_;
    std::size_t i_ = 0;

    Block block() {
        const std::string& line = lines_[i_];
        char ch;
        std::size_t len;
        std::string info;
        if (fenceOpen(line, ch, len, info)) {
            const int ind = indentOf(line);
            ++i_;
            std::string code;
            while (i_ < lines_.size() && !fenceClose(lines_[i_], ch, len)) {
                code += dedent(lines_[i_], ind);
                code += '\n';
                ++i_;
            }
            if (i_ < lines_.size()) ++i_;  // the closing fence
            if (!code.empty()) code.pop_back();
            return {.kind = Block::Kind::Code, .code = std::move(code), .language = std::move(info)};
        }
        if (int level = headingLevel(line)) {
            ++i_;
            std::string_view t = trim(line).substr(static_cast<std::size_t>(level));
            t = trim(t);
            while (!t.empty() && t.back() == '#') t.remove_suffix(1);  // "## Title ##"
            Block b{.kind = Block::Kind::Heading, .level = level, .inlines = inlines(t)};
            b.id = slug(b.inlines);
            return b;
        }
        if (isRule(line)) {
            ++i_;
            return {.kind = Block::Kind::Rule};
        }
        if (indentOf(line) >= 4) {  // indented code
            std::string code;
            while (i_ < lines_.size() && (indentOf(lines_[i_]) >= 4 || blank(lines_[i_]))) {
                code += dedent(lines_[i_], 4);
                code += '\n';
                ++i_;
            }
            while (!code.empty() && (code.back() == '\n' || code.back() == ' ')) code.pop_back();
            return {.kind = Block::Kind::Code, .code = std::move(code)};
        }
        if (trim(line).starts_with('>')) return quote();
        Marker m;
        if (listMarker(line, m)) return list(m);
        std::vector<char> align;
        if (line.find('|') != std::string::npos && i_ + 1 < lines_.size() && delimiterRow(lines_[i_ + 1], align)) {
            auto header = splitRow(line);
            if (header.size() == align.size()) return table(std::move(header), std::move(align));
        }
        return paragraph();
    }

    Block paragraph() {
        std::string text = lines_[i_++];
        while (i_ < lines_.size() && !blank(lines_[i_])) {
            std::string_view next = lines_[i_];
            // Setext headings: "Title\n=====" / "Title\n-----".
            std::string_view t = trim(next);
            if (!t.empty() && (std::ranges::all_of(t, [](char c) { return c == '='; }) ||
                               std::ranges::all_of(t, [](char c) { return c == '-'; }))) {
                ++i_;
                Block b{.kind = Block::Kind::Heading, .level = t[0] == '=' ? 1 : 2, .inlines = inlines(text)};
                b.id = slug(b.inlines);
                return b;
            }
            if (startsBlock(next)) break;
            text += '\n';
            text += next;
            ++i_;
        }
        return {.kind = Block::Kind::Paragraph, .inlines = inlines(text)};
    }

    Block quote() {
        Lines inner;
        while (i_ < lines_.size() && !blank(lines_[i_])) {
            std::string_view l = lines_[i_];
            std::string_view t = trim(l);
            if (t.starts_with('>')) {
                t.remove_prefix(1);
                if (!t.empty() && t.front() == ' ') t.remove_prefix(1);
                inner.emplace_back(t);
            } else if (startsBlock(l)) {
                break;
            } else {
                inner.emplace_back(l);  // a lazy continuation of the quoted paragraph
            }
            ++i_;
        }
        return {.kind = Block::Kind::Quote, .children = BlockParser(std::move(inner)).parse()};
    }

    Block list(Marker first) {
        Block b{.kind = Block::Kind::List, .ordered = first.ordered, .start = first.number};
        Marker m = first;
        while (true) {
            Lines item;
            item.push_back(lines_[i_].substr(m.contentStart));
            ++i_;
            while (i_ < lines_.size()) {
                const std::string& l = lines_[i_];
                if (blank(l)) {
                    // A blank line keeps the item only if indented text follows.
                    std::size_t k = i_ + 1;
                    while (k < lines_.size() && blank(lines_[k])) ++k;
                    if (k < lines_.size() && indentOf(lines_[k]) >= m.contentIndent) {
                        for (; i_ < k; ++i_) item.emplace_back();
                        continue;
                    }
                    break;
                }
                if (indentOf(l) >= m.contentIndent) {
                    item.push_back(dedent(l, m.contentIndent));
                    ++i_;
                    continue;
                }
                Marker next;
                if (listMarker(l, next) || startsBlock(l)) break;
                item.push_back(l);  // lazy continuation of the item's paragraph
                ++i_;
            }
            // "[ ] " / "[x] " at the start: a task.
            std::optional<bool> task;
            std::string& head = item.front();
            if (head.size() >= 3 && head[0] == '[' && head[2] == ']' && (head.size() == 3 || head[3] == ' ') &&
                (head[1] == ' ' || head[1] == 'x' || head[1] == 'X')) {
                task = head[1] != ' ';
                head = head.size() > 4 ? head.substr(4) : "";
            }
            b.items.push_back(BlockParser(std::move(item)).parse());
            b.checked.push_back(task);

            // The next item of the same list (blank lines between are allowed).
            std::size_t k = i_;
            while (k < lines_.size() && blank(lines_[k])) ++k;
            Marker next;
            if (k < lines_.size() && listMarker(lines_[k], next) && next.ordered == first.ordered &&
                next.bullet == first.bullet && !isRule(lines_[k])) {
                i_ = k;
                m = next;
                continue;
            }
            break;
        }
        return b;
    }

    Block table(std::vector<std::string> header, std::vector<char> align) {
        Block b{.kind = Block::Kind::Table, .align = std::move(align)};
        const std::size_t columns = b.align.size();
        auto row = [&](std::vector<std::string> cells) {
            cells.resize(columns);
            std::vector<std::vector<Inline>> out;
            for (auto& c : cells) out.push_back(inlines(c));
            b.rows.push_back(std::move(out));
        };
        row(std::move(header));
        i_ += 2;
        while (i_ < lines_.size() && !blank(lines_[i_]) && lines_[i_].find('|') != std::string::npos &&
               !startsBlock(lines_[i_]))
            row(splitRow(lines_[i_++]));
        return b;
    }
};

Lines splitLines(std::string_view text) {
    Lines lines;
    std::size_t start = 0;
    while (start <= text.size()) {
        auto nl = text.find('\n', start);
        if (nl == std::string_view::npos) nl = text.size();
        std::string_view line = text.substr(start, nl - start);
        if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
        lines.emplace_back(line);
        start = nl + 1;
    }
    return lines;
}

void plainBlocks(const Document& doc, std::string& out) {
    auto sep = [&] {
        if (!out.empty() && out.back() != '\n') out += '\n';
    };
    for (const auto& b : doc) {
        switch (b.kind) {
            case Block::Kind::Paragraph:
            case Block::Kind::Heading:
                sep();
                out += InlineParser::plainOf(b.inlines);
                break;
            case Block::Kind::Code:
                sep();
                out += b.code;
                break;
            case Block::Kind::Quote:
                plainBlocks(b.children, out);
                break;
            case Block::Kind::List:
                for (const auto& item : b.items) plainBlocks(item, out);
                break;
            case Block::Kind::Table:
                for (const auto& row : b.rows) {
                    sep();
                    for (std::size_t c = 0; c < row.size(); ++c) out += (c ? " " : "") + InlineParser::plainOf(row[c]);
                }
                break;
            case Block::Kind::Rule:
                break;
        }
    }
}

}  // namespace

Document parse(std::string_view markdown) { return BlockParser(splitLines(markdown)).parse(); }

std::string plainText(std::string_view markdown) {
    std::string out;
    plainBlocks(parse(markdown), out);
    return out;
}

}  // namespace rx::markdown
