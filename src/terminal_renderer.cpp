#include <algorithm>
#include <charconv>
#include <string>
#include <utility>

#include "reactx/terminal.hpp"
#include "utf8.hpp"

namespace rx {

namespace {

using PropMap = std::map<std::string, std::string, std::less<>>;

std::string prop(const PropMap& props, std::string_view name, std::string fallback = {}) {
    auto it = props.find(name);
    return it == props.end() ? std::move(fallback) : it->second;
}

bool flag(const PropMap& props, std::string_view name) { return prop(props, name) == "true"; }

int intProp(const PropMap& props, std::string_view name, int fallback = 0) {
    std::string s = prop(props, name);
    int value = fallback;
    if (!s.empty()) std::from_chars(s.data(), s.data() + s.size(), value);
    return std::max(value, 0);
}

std::vector<std::string> splitLines(const std::string& text) {
    std::vector<std::string> lines;
    std::size_t start = 0;
    while (true) {
        auto end = text.find('\n', start);
        lines.push_back(text.substr(start, end - start));
        if (end == std::string::npos) break;
        start = end + 1;
    }
    return lines;
}

int textWidth(const std::string& text) {
    int w = 0;
    for (const auto& line : splitLines(text)) w = std::max(w, detail::utf8Length(line));
    return w;
}

// SGR color code for a color name, or 0 if unknown.
int colorCode(std::string_view name) {
    static constexpr std::pair<std::string_view, int> base[] = {
        {"black", 0}, {"red", 1},     {"green", 2}, {"yellow", 3},
        {"blue", 4},  {"magenta", 5}, {"cyan", 6},  {"white", 7},
    };
    if (name == "gray" || name == "grey") return 90;
    bool bright = name.starts_with("bright-");
    if (bright) name.remove_prefix(7);
    for (auto [n, c] : base)
        if (n == name) return (bright ? 90 : 30) + c;
    return 0;
}

}  // namespace

TerminalRenderer::TerminalRenderer(Output output) : output_(std::move(output)) {}

void TerminalRenderer::setSize(int width, int height) {
    width = std::max(width, 1);
    height = std::max(height, 1);
    if (width == width_ && height == height_) return;
    width_ = width;
    height_ = height;
    invalidate();
}

void TerminalRenderer::invalidate() { fullRedraw_ = true; }

void TerminalRenderer::commit() {
    fixFocus();
    repaint();
}

void TerminalRenderer::destroyNode(NodeHandle h) {
    if (h == focused_) focused_ = 0;  // fixFocus picks a neighbour on commit
    TextRenderer::destroyNode(h);
}

// ---- layout ----

TerminalRenderer::Style TerminalRenderer::styleOf(const Node& n, Style s) const {
    if (auto c = prop(n.props, "color"); !c.empty()) s.fg = c;
    if (auto c = prop(n.props, "bg"); !c.empty()) s.bg = c;
    if (flag(n.props, "bold")) s.bold = true;
    if (flag(n.props, "dim")) s.dim = true;
    if (flag(n.props, "underline")) s.underline = true;
    return s;
}

std::string TerminalRenderer::textOf(const Node& n) const {
    if (n.isText) return n.text;
    std::string out;
    for (NodeHandle c : n.children)
        if (const Node* child = findNode(c)) out += textOf(*child);
    return out;
}

int TerminalRenderer::naturalWidth(NodeHandle h) const {
    const Node* n = findNode(h);
    if (!n) return 0;
    if (n->isText || n->type == "text") return textWidth(textOf(*n));
    if (int w = intProp(n->props, "width"); w > 0) return w;
    if (n->type == "button") return detail::utf8Length(textOf(*n)) + 4;
    if (n->type == "input") {
        auto shown = prop(n->props, "value");
        if (shown.empty()) shown = prop(n->props, "placeholder");
        return std::max(detail::utf8Length(shown) + 1, 10);
    }

    bool row = prop(n->props, "direction") == "row";
    int gap = intProp(n->props, "gap", row ? 1 : 0);
    int content = 0;
    for (std::size_t i = 0; i < n->children.size(); ++i) {
        int w = naturalWidth(n->children[i]);
        if (row) content += w + (i ? gap : 0);
        else content = std::max(content, w);
    }
    int frame = 2 * intProp(n->props, "padding");
    if (flag(n->props, "border")) {
        frame += 2;
        content = std::max(content, detail::utf8Length(prop(n->props, "title")) + 4);
    }
    return content + frame;
}

void TerminalRenderer::put(Canvas& canvas, int x, int y, int maxX, std::string_view text,
                           const Style& style) {
    if (y < 0 || x < 0) return;
    maxX = std::min(maxX, width_);
    if (static_cast<int>(canvas.size()) <= y)
        canvas.resize(static_cast<std::size_t>(y) + 1, std::vector<Cell>(static_cast<std::size_t>(width_)));
    auto& line = canvas[static_cast<std::size_t>(y)];
    for (std::string_view ch : detail::utf8Split(text)) {
        if (x >= maxX) break;
        if (static_cast<unsigned char>(ch[0]) < 0x20) continue;  // control characters
        line[static_cast<std::size_t>(x)] = Cell{std::string(ch), style};
        ++x;
    }
}

int TerminalRenderer::draw(NodeHandle h, int x, int y, int width, Style inherited, Canvas& canvas) {
    const Node* n = findNode(h);
    if (!n || width <= 0) return 0;
    Style st = n->isText ? inherited : styleOf(*n, inherited);
    if (h == focused_) st.reverse = true;
    int right = x + width;

    if (n->isText || n->type == "text") {
        auto lines = splitLines(textOf(*n));
        for (std::size_t i = 0; i < lines.size(); ++i)
            put(canvas, x, y + static_cast<int>(i), right, lines[i], st);
        return static_cast<int>(lines.size());
    }
    if (n->type == "button") {
        put(canvas, x, y, right, "[ " + textOf(*n) + " ]", st);
        return 1;
    }
    if (n->type == "input") {
        int w = intProp(n->props, "width");
        if (w > 0) right = std::min(right, x + w);
        std::string value = prop(n->props, "value");
        Style field = st;
        field.underline = true;
        put(canvas, x, y, right, std::string(static_cast<std::size_t>(right - x), ' '), field);
        if (value.empty() && h != focused_) {
            Style hint = field;
            hint.dim = true;
            put(canvas, x, y, right, prop(n->props, "placeholder"), hint);
        } else {
            // Keep the end of the text (and the cursor) visible.
            std::string shown = value + (h == focused_ ? "_" : "");
            auto chars = detail::utf8Split(shown);
            std::size_t room = static_cast<std::size_t>(right - x);
            std::size_t skip = chars.size() > room ? chars.size() - room : 0;
            std::string tail;
            for (std::size_t i = skip; i < chars.size(); ++i) tail += chars[i];
            put(canvas, x, y, right, tail, field);
        }
        return 1;
    }

    // Box: any other tag.
    if (int w = intProp(n->props, "width"); w > 0) {
        width = std::min(width, w);
        right = x + width;
    }
    bool border = flag(n->props, "border");
    bool row = prop(n->props, "direction") == "row";
    int pad = intProp(n->props, "padding");
    int gap = intProp(n->props, "gap", row ? 1 : 0);
    int inset = (border ? 1 : 0) + pad;
    int innerX = x + inset;
    int innerW = width - 2 * inset;
    int top = y + inset;
    int content = 0;

    if (innerW > 0) {
        if (row) {
            int cx = innerX;
            for (std::size_t i = 0; i < n->children.size(); ++i) {
                int room = innerX + innerW - cx;
                if (room <= 0) break;
                bool last = i + 1 == n->children.size();
                int cw = last ? room : std::min(naturalWidth(n->children[i]), room);
                content = std::max(content, draw(n->children[i], cx, top, cw, st, canvas));
                cx += cw + gap;
            }
        } else {
            int cy = top;
            for (std::size_t i = 0; i < n->children.size(); ++i) {
                if (i) cy += gap;
                cy += draw(n->children[i], innerX, cy, innerW, st, canvas);
            }
            content = cy - top;
        }
    }
    int height = content + 2 * inset;
    if (height > 0) put(canvas, x, y + height - 1, x, "", st);  // keep trailing padding rows

    if (!st.bg.empty() || st.reverse) {
        // Fill the box background where children did not set their own.
        for (int yy = y; yy < y + height; ++yy) {
            put(canvas, x, yy, x, "", st);  // make sure the row exists
            auto& line = canvas[static_cast<std::size_t>(yy)];
            for (int xx = x; xx < std::min(right, width_); ++xx) {
                auto& cell = line[static_cast<std::size_t>(xx)].style;
                if (cell.bg.empty() && !st.bg.empty()) cell.bg = st.bg;
                if (st.reverse) cell.reverse = true;
            }
        }
    }

    if (border && width >= 2) {
        Style bs = st;
        bs.bold = false;
        int bottom = y + height - 1;
        std::string title = prop(n->props, "title");
        std::string topLine = "┌";
        int used = 1;
        if (!title.empty() && width > 6) {
            topLine += "─ " + title + " ";
            used += 3 + detail::utf8Length(title);
        }
        for (; used < width - 1; ++used) topLine += "─";
        put(canvas, x, y, right - 1, topLine, bs);
        put(canvas, right - 1, y, right, "┐", bs);
        for (int yy = y + 1; yy < bottom; ++yy) {
            put(canvas, x, yy, right, "│", bs);
            put(canvas, right - 1, yy, right, "│", bs);
        }
        std::string bottomLine = "└";
        for (int i = 1; i < width - 1; ++i) bottomLine += "─";
        bottomLine += "┘";
        put(canvas, x, bottom, right, bottomLine, bs);
    }
    return height;
}

// ---- painting ----

std::string TerminalRenderer::lineToAnsi(const std::vector<Cell>& line) {
    std::size_t end = line.size();
    while (end > 0 && line[end - 1].ch == " " && line[end - 1].style == Style{}) --end;

    std::string out;
    Style current;
    for (std::size_t i = 0; i < end; ++i) {
        const Cell& c = line[i];
        if (c.style != current) {
            out += "\x1b[0";
            if (int fg = colorCode(c.style.fg)) out += ";" + std::to_string(fg);
            if (int bg = colorCode(c.style.bg)) out += ";" + std::to_string(bg + 10);
            if (c.style.bold) out += ";1";
            if (c.style.dim) out += ";2";
            if (c.style.underline) out += ";4";
            if (c.style.reverse) out += ";7";
            out += "m";
            current = c.style;
        }
        out += c.ch;
    }
    if (current != Style{}) out += "\x1b[0m";
    return out;
}

void TerminalRenderer::repaint() {
    Canvas canvas;
    draw(kRoot, 0, 0, width_, Style{}, canvas);
    frame_ = std::move(canvas);
    if (!output_) return;

    std::string out;
    if (fullRedraw_) {
        out += "\x1b[0m\x1b[2J";
        shown_.assign(static_cast<std::size_t>(height_), std::string{});
        fullRedraw_ = false;
    }
    shown_.resize(static_cast<std::size_t>(height_));
    for (std::size_t i = 0; i < shown_.size(); ++i) {
        std::string line = i < frame_.size() ? lineToAnsi(frame_[i]) : std::string{};
        if (line == shown_[i]) continue;
        out += "\x1b[" + std::to_string(i + 1) + ";1H" + line + "\x1b[K";
        shown_[i] = std::move(line);
    }
    if (!out.empty()) output_(out);
}

std::vector<std::string> TerminalRenderer::frameLines() const {
    std::vector<std::string> lines;
    for (const auto& row : frame_) {
        std::string line;
        for (const auto& cell : row) line += cell.ch;
        while (line.ends_with(' ')) line.pop_back();
        lines.push_back(std::move(line));
    }
    return lines;
}

std::string TerminalRenderer::frameText() const {
    std::string out;
    for (const auto& line : frameLines()) out += line + '\n';
    return out;
}

// ---- focus ----

bool TerminalRenderer::isFocusable(const Node& n) const {
    return !n.isText && (n.type == "input" || n.type == "button" || flag(n.props, "focusable"));
}

void TerminalRenderer::collectFocusable(NodeHandle h, std::vector<NodeHandle>& out) const {
    const Node* n = findNode(h);
    if (!n) return;
    if (isFocusable(*n)) out.push_back(h);
    for (NodeHandle c : n->children) collectFocusable(c, out);
}

std::vector<NodeHandle> TerminalRenderer::focusable() const {
    std::vector<NodeHandle> out;
    collectFocusable(kRoot, out);
    return out;
}

void TerminalRenderer::fixFocus() {
    auto list = focusable();
    if (auto it = std::ranges::find(list, focused_); focused_ && it != list.end()) {
        focusIndex_ = static_cast<std::size_t>(it - list.begin());
        return;
    }
    if (list.empty()) {
        focused_ = 0;
        return;
    }
    focusIndex_ = std::min(focusIndex_, list.size() - 1);
    focused_ = list[focusIndex_];
}

bool TerminalRenderer::focus(NodeHandle node) {
    auto list = focusable();
    auto it = std::ranges::find(list, node);
    if (it == list.end()) return false;
    focused_ = node;
    focusIndex_ = static_cast<std::size_t>(it - list.begin());
    repaint();
    return true;
}

void TerminalRenderer::focusNext() {
    auto list = focusable();
    if (list.empty()) return;
    auto it = std::ranges::find(list, focused_);
    std::size_t i = it == list.end() ? 0 : (static_cast<std::size_t>(it - list.begin()) + 1) % list.size();
    focus(list[i]);
}

void TerminalRenderer::focusPrev() {
    auto list = focusable();
    if (list.empty()) return;
    auto it = std::ranges::find(list, focused_);
    std::size_t i = it == list.end() || it == list.begin()
                        ? list.size() - 1
                        : static_cast<std::size_t>(it - list.begin()) - 1;
    focus(list[i]);
}

bool TerminalRenderer::handleKey(Root& root, const KeyEvent& key) {
    if (key.ctrl || key.alt) {
        // No default action; still let the app see it.
        return focused_ && root.dispatch(focused_, "onKey", key);
    }
    if ((key.key == "Tab" && !key.shift) || key.key == "Down") {
        focusNext();
        return true;
    }
    if ((key.key == "Tab" && key.shift) || key.key == "Up") {
        focusPrev();
        return true;
    }
    if (!focused_) return false;

    NodeHandle target = focused_;
    bool handled = root.dispatch(target, "onKey", key);
    const Node* n = findNode(target);
    if (!n) return handled;  // the handler removed the node

    if (n->type == "input") {
        std::string value = prop(n->props, "value");
        if (key.key == "Backspace") {
            if (value.empty()) return handled;
            detail::utf8PopBack(value);
            return root.dispatch(target, "onInput", value) || handled;
        }
        if (detail::isPrintableKey(key.key))
            return root.dispatch(target, "onInput", value + key.key) || handled;
    }
    if (key.key == "Enter" || key.key == " ") return root.dispatch(target, "onClick") || handled;
    return handled;
}

}  // namespace rx
