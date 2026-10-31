#include <yoga/Yoga.h>

#include <algorithm>
#include <ranges>
#include <charconv>
#include <cmath>
#include <deque>
#include <inplace_vector>
#include <mdspan>
#include <span>
#include <string>
#include <unordered_map>
#include <utility>

#include "reactx/terminal.hpp"
#include "utf8.hpp"

namespace rx {

namespace {

using PropMap = std::flat_map<std::string, std::string, std::less<>>;

std::string prop(const PropMap& props, std::string_view name, std::string fallback = {}) {
    auto it = props.find(name);
    return it == props.end() ? std::move(fallback) : it->second;
}

bool flag(const PropMap& props, std::string_view name) { return prop(props, name) == "true"; }

bool isLeafType(std::string_view type) { return type == "text" || type == "button" || type == "input"; }

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

// Word wrap to `width` columns. Lines that fit are kept as they are
// (including leading spaces); words longer than a line are split.
std::vector<std::string> wrapText(const std::string& text, int width) {
    width = std::max(width, 1);
    std::vector<std::string> out;
    for (const std::string& paragraph : splitLines(text)) {
        if (detail::utf8Length(paragraph) <= width) {
            out.push_back(paragraph);
            continue;
        }
        std::string line;
        int lineLen = 0;
        std::size_t start = 0;
        while (start <= paragraph.size()) {
            auto end = paragraph.find(' ', start);
            if (end == std::string::npos) end = paragraph.size();
            std::string word = paragraph.substr(start, end - start);
            start = end + 1;
            if (word.empty()) continue;

            auto chars = detail::utf8Split(word);
            std::size_t pos = 0;
            while (chars.size() - pos > static_cast<std::size_t>(width)) {  // long word
                if (lineLen > 0) {
                    out.push_back(std::move(line));
                    line.clear();
                    lineLen = 0;
                }
                std::string piece;
                for (int i = 0; i < width; ++i) piece += chars[pos++];
                out.push_back(std::move(piece));
            }
            std::string rest;
            for (; pos < chars.size(); ++pos) rest += chars[pos];
            int restLen = detail::utf8Length(rest);
            if (restLen == 0) continue;
            if (lineLen == 0) {
                line = std::move(rest);
                lineLen = restLen;
            } else if (lineLen + 1 + restLen <= width) {
                line += " " + rest;
                lineLen += 1 + restLen;
            } else {
                out.push_back(std::move(line));
                line = std::move(rest);
                lineLen = restLen;
            }
        }
        out.push_back(std::move(line));
    }
    return out;
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

// ---- Yoga style from props ----

struct Value {
    enum Kind { None, Points, Percent, Auto } kind = None;
    float number = 0;
};

Value parseValue(const std::string& s) {
    if (s.empty()) return {};
    if (s == "auto") return {Value::Auto, 0};
    bool percent = s.back() == '%';
    std::string_view digits(s.data(), s.size() - (percent ? 1 : 0));
    float v = 0;
    auto [ptr, ec] = std::from_chars(digits.data(), digits.data() + digits.size(), v);
    if (ec != std::errc{} || ptr != digits.data() + digits.size()) return {};
    return {percent ? Value::Percent : Value::Points, v};
}

using SetPoints = void (*)(YGNodeRef, float);
using SetEdge = void (*)(YGNodeRef, YGEdge, float);

void setSize(YGNodeRef n, const PropMap& props, std::string_view name, SetPoints points,
             SetPoints percent, void (*autoFn)(YGNodeRef) = nullptr) {
    Value v = parseValue(prop(props, name));
    if (v.kind == Value::Points) points(n, v.number);
    else if (v.kind == Value::Percent) percent(n, v.number);
    else if (v.kind == Value::Auto && autoFn) autoFn(n);
}

// padding / margin / position: name, nameX, nameY, nameLeft, ...
void setEdges(YGNodeRef n, const PropMap& props, std::string_view name, SetEdge points, SetEdge percent,
              void (*autoFn)(YGNodeRef, YGEdge) = nullptr, bool withAll = true) {
    static constexpr std::pair<std::string_view, YGEdge> suffixes[] = {
        {"", YGEdgeAll},           {"X", YGEdgeHorizontal}, {"Y", YGEdgeVertical},
        {"Left", YGEdgeLeft},      {"Right", YGEdgeRight},  {"Top", YGEdgeTop},
        {"Bottom", YGEdgeBottom},
    };
    for (auto [suffix, edge] : suffixes) {
        if (suffix.empty() && !withAll) continue;
        Value v = parseValue(prop(props, std::string(name) + std::string(suffix)));
        if (v.kind == Value::Points) points(n, edge, v.number);
        else if (v.kind == Value::Percent) percent(n, edge, v.number);
        else if (v.kind == Value::Auto && autoFn) autoFn(n, edge);
    }
}

YGAlign parseAlign(const std::string& s, YGAlign fallback) {
    if (s == "start" || s == "flex-start") return YGAlignFlexStart;
    if (s == "center") return YGAlignCenter;
    if (s == "end" || s == "flex-end") return YGAlignFlexEnd;
    if (s == "stretch") return YGAlignStretch;
    if (s == "baseline") return YGAlignBaseline;
    if (s == "space-between") return YGAlignSpaceBetween;
    if (s == "space-around") return YGAlignSpaceAround;
    if (s == "auto") return YGAlignAuto;
    return fallback;
}

YGJustify parseJustify(const std::string& s) {
    if (s == "center") return YGJustifyCenter;
    if (s == "end" || s == "flex-end") return YGJustifyFlexEnd;
    if (s == "space-between") return YGJustifySpaceBetween;
    if (s == "space-around") return YGJustifySpaceAround;
    if (s == "space-evenly") return YGJustifySpaceEvenly;
    return YGJustifyFlexStart;
}

bool isRow(const PropMap& props) { return prop(props, "direction").starts_with("row"); }

void applyStyle(YGNodeRef n, const PropMap& props, bool container) {
    if (container) {
        std::string dir = prop(props, "direction");
        YGNodeStyleSetFlexDirection(n, dir == "row"              ? YGFlexDirectionRow
                                       : dir == "row-reverse"    ? YGFlexDirectionRowReverse
                                       : dir == "column-reverse" ? YGFlexDirectionColumnReverse
                                                                 : YGFlexDirectionColumn);
        if (auto s = prop(props, "justifyContent"); !s.empty()) YGNodeStyleSetJustifyContent(n, parseJustify(s));
        if (auto s = prop(props, "alignItems"); !s.empty())
            YGNodeStyleSetAlignItems(n, parseAlign(s, YGAlignStretch));
        if (auto s = prop(props, "alignContent"); !s.empty())
            YGNodeStyleSetAlignContent(n, parseAlign(s, YGAlignFlexStart));
        if (auto s = prop(props, "flexWrap"); !s.empty())
            YGNodeStyleSetFlexWrap(n, s == "wrap" ? YGWrapWrap : s == "wrap-reverse" ? YGWrapWrapReverse : YGWrapNoWrap);
        if (flag(props, "border")) YGNodeStyleSetBorder(n, YGEdgeAll, 1);
        static constexpr std::pair<std::string_view, YGGutter> gaps[] = {
            {"gap", YGGutterAll}, {"rowGap", YGGutterRow}, {"columnGap", YGGutterColumn}};
        for (auto [name, gutter] : gaps)
            if (Value v = parseValue(prop(props, name)); v.kind == Value::Points) YGNodeStyleSetGap(n, gutter, v.number);
    }
    if (auto s = prop(props, "alignSelf"); !s.empty()) YGNodeStyleSetAlignSelf(n, parseAlign(s, YGAlignAuto));
    if (Value v = parseValue(prop(props, "flexGrow")); v.kind == Value::Points) YGNodeStyleSetFlexGrow(n, v.number);
    if (Value v = parseValue(prop(props, "flexShrink")); v.kind == Value::Points)
        YGNodeStyleSetFlexShrink(n, v.number);
    setSize(n, props, "flexBasis", YGNodeStyleSetFlexBasis, YGNodeStyleSetFlexBasisPercent,
            YGNodeStyleSetFlexBasisAuto);
    setSize(n, props, "width", YGNodeStyleSetWidth, YGNodeStyleSetWidthPercent, YGNodeStyleSetWidthAuto);
    setSize(n, props, "height", YGNodeStyleSetHeight, YGNodeStyleSetHeightPercent, YGNodeStyleSetHeightAuto);
    setSize(n, props, "minWidth", YGNodeStyleSetMinWidth, YGNodeStyleSetMinWidthPercent);
    setSize(n, props, "maxWidth", YGNodeStyleSetMaxWidth, YGNodeStyleSetMaxWidthPercent);
    setSize(n, props, "minHeight", YGNodeStyleSetMinHeight, YGNodeStyleSetMinHeightPercent);
    setSize(n, props, "maxHeight", YGNodeStyleSetMaxHeight, YGNodeStyleSetMaxHeightPercent);
    setEdges(n, props, "padding", YGNodeStyleSetPadding, YGNodeStyleSetPaddingPercent);
    setEdges(n, props, "margin", YGNodeStyleSetMargin, YGNodeStyleSetMarginPercent, YGNodeStyleSetMarginAuto);
    // top / left / right / bottom (no "all" form)
    static constexpr std::pair<std::string_view, YGEdge> sides[] = {
        {"top", YGEdgeTop}, {"left", YGEdgeLeft}, {"right", YGEdgeRight}, {"bottom", YGEdgeBottom}};
    for (auto [name, edge] : sides) {
        Value v = parseValue(prop(props, name));
        if (v.kind == Value::Points) YGNodeStyleSetPosition(n, edge, v.number);
        else if (v.kind == Value::Percent) YGNodeStyleSetPositionPercent(n, edge, v.number);
    }
    if (prop(props, "position") == "absolute") YGNodeStyleSetPositionType(n, YGPositionTypeAbsolute);
    if (prop(props, "display") == "none") YGNodeStyleSetDisplay(n, YGDisplayNone);
}

// ---- leaves (measured by us, laid out by Yoga) ----

struct Leaf {
    enum Kind { Text, Button, Input } kind = Text;
    std::string text;  // text, button label, or input value/placeholder
    bool wrap = true;
};

int clampTo(int natural, float available, YGMeasureMode mode) {
    if (mode == YGMeasureModeExactly) return static_cast<int>(available);
    if (mode == YGMeasureModeAtMost) return std::min(natural, static_cast<int>(available));
    return natural;
}

YGSize measureLeaf(YGNodeConstRef node, float width, YGMeasureMode widthMode, float, YGMeasureMode) {
    const Leaf& leaf = *static_cast<const Leaf*>(YGNodeGetContext(node));
    int w = 0, h = 1;
    switch (leaf.kind) {
        case Leaf::Button: w = clampTo(detail::utf8Length(leaf.text) + 4, width, widthMode); break;
        case Leaf::Input: w = clampTo(std::max(detail::utf8Length(leaf.text) + 1, 10), width, widthMode); break;
        case Leaf::Text: {
            int natural = textWidth(leaf.text);
            if (widthMode == YGMeasureModeUndefined || !leaf.wrap) {
                w = clampTo(natural, width, widthMode);
                h = static_cast<int>(splitLines(leaf.text).size());
            } else {
                auto lines = wrapText(leaf.text, static_cast<int>(width));
                for (const auto& l : lines) w = std::max(w, detail::utf8Length(l));
                if (widthMode == YGMeasureModeExactly) w = static_cast<int>(width);
                h = static_cast<int>(lines.size());
            }
            break;
        }
    }
    return YGSize{static_cast<float>(w), static_cast<float>(h)};
}

int cells(float v) { return static_cast<int>(std::floor(v + 0.5f)); }

}  // namespace

// Yoga tree for one repaint.
struct TerminalRenderer::Layout {
    YGConfigRef config = YGConfigNew();
    YGNodeRef root = nullptr;
    std::unordered_map<NodeHandle, YGNodeRef> nodes;
    std::deque<Leaf> leaves;  // stable addresses for YGNodeSetContext

    // No rounding inside Yoga: it rounds text and boxes differently, which can
    // make neighbours overlap by a cell. draw() rounds every edge the same way.
    Layout() { YGConfigSetPointScaleFactor(config, 0.0f); }
    ~Layout() {
        if (root) YGNodeFreeRecursive(root);
        YGConfigFree(config);
    }
    Layout(const Layout&) = delete;
    Layout& operator=(const Layout&) = delete;

    YGNodeRef build(const TerminalRenderer& r, NodeHandle h, bool parentRow) {
        const Node* n = r.findNode(h);
        if (!n) return nullptr;
        YGNodeRef y = YGNodeNewWithConfig(config);
        nodes[h] = y;
        // Rows squeeze their children (text gets narrower); columns cut off.
        // Buttons never shrink: a half button is useless.
        bool button = !n->isText && n->type == "button";
        YGNodeStyleSetFlexShrink(y, parentRow && !button ? 1.0f : 0.0f);

        if (n->isText || isLeafType(n->type)) {
            Leaf& leaf = leaves.emplace_back();
            if (n->isText || n->type == "text") {
                leaf.text = r.textOf(*n);
                leaf.wrap = n->isText || prop(n->props, "wrap") != "false";
            } else if (n->type == "button") {
                leaf.kind = Leaf::Button;
                leaf.text = r.textOf(*n);
            } else {
                leaf.kind = Leaf::Input;
                leaf.text = prop(n->props, "value");
                if (leaf.text.empty()) leaf.text = prop(n->props, "placeholder");
            }
            if (!n->isText) applyStyle(y, n->props, false);
            YGNodeSetContext(y, &leaf);
            YGNodeSetMeasureFunc(y, measureLeaf);
            return y;
        }

        applyStyle(y, n->props, true);
        bool row = isRow(n->props);
        std::size_t index = 0;
        for (NodeHandle c : n->children)
            if (YGNodeRef child = build(r, c, row)) YGNodeInsertChild(y, child, index++);
        return y;
    }
};

// The cells of one repaint.
struct TerminalRenderer::Frame {
    struct Cell {
        std::inplace_vector<char, 4> ch{' '};  // one UTF-8 code point, no heap (C++26)
        Style style;
        std::string_view text() const { return {ch.data(), ch.size()}; }
    };
    // rows x columns, screen[row, col] (C++23 std::mdspan over one array).
    using Screen = std::mdspan<Cell, std::dextents<std::size_t, 2>>;
    using ConstScreen = std::mdspan<const Cell, std::dextents<std::size_t, 2>>;

    std::vector<Cell> cells;
    std::size_t rows = 0, cols = 0;

    Frame(std::size_t r, std::size_t c) : cells(r * c), rows(r), cols(c) {}
    Screen screen() { return Screen(cells.data(), rows, cols); }
    ConstScreen screen() const { return ConstScreen(cells.data(), rows, cols); }
    std::span<const Cell> row(std::size_t y) const {
        ConstScreen s = screen();
        return std::span(&s[y, 0], cols);  // rows are contiguous (layout_right)
    }

    static std::string lineToAnsi(std::span<const Cell> line);
};

TerminalRenderer::Rect TerminalRenderer::Rect::intersect(const Rect& o) const {
    return {std::max(left, o.left), std::max(top, o.top), std::min(right, o.right), std::min(bottom, o.bottom)};
}

TerminalRenderer::TerminalRenderer(Output output) : output_(std::move(output)) {}
TerminalRenderer::~TerminalRenderer() = default;

void TerminalRenderer::setSize(int width, int height) {
    width = std::max(width, 1);
    height = std::max(height, 1);
    if (width == width_ && height == height_) return;
    width_ = width;
    height_ = height;
    invalidate();
}

void TerminalRenderer::invalidate() { fullRedraw_ = true; }

void TerminalRenderer::setUnicode(bool on) {
    if (on == unicode_) return;
    unicode_ = on;
    invalidate();
}

void TerminalRenderer::commit() {
    fixFocus();
    repaint();
}

void TerminalRenderer::destroyNode(NodeHandle h) {
    if (h == focused_) focused_ = 0;  // fixFocus picks a neighbour on commit
    TextRenderer::destroyNode(h);
}

// ---- drawing ----

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

void TerminalRenderer::put(Frame& frame, int x, int y, const Rect& clip, std::string_view text,
                           const Style& style) {
    Frame::Screen screen = frame.screen();
    if (y < 0 || y < clip.top || y >= clip.bottom || y >= static_cast<int>(screen.extent(0))) return;
    int right = std::min(clip.right, static_cast<int>(screen.extent(1)));
    for (std::string_view ch : detail::utf8Split(text)) {
        if (x >= right) break;
        if (x >= clip.left && x >= 0 && static_cast<unsigned char>(ch[0]) >= 0x20) {
            Frame::Cell& cell = screen[static_cast<std::size_t>(y), static_cast<std::size_t>(x)];
            cell.ch.assign(ch.begin(), ch.end());  // utf8Split: at most 4 bytes
            cell.style = style;
        }
        ++x;
    }
}

void TerminalRenderer::draw(const Layout& layout, NodeHandle h, float originX, float originY, Style inherited,
                            Rect clip, Frame& frame) {
    Frame::Screen screen = frame.screen();
    const Node* n = findNode(h);
    auto it = layout.nodes.find(h);
    if (!n || it == layout.nodes.end()) return;
    YGNodeRef y = it->second;
    if (!n->isText && prop(n->props, "display") == "none") return;

    // Absolute position in fractional cells; each edge is rounded on its own,
    // so boxes that touch in Yoga also touch on screen.
    float fx = originX + YGNodeLayoutGetLeft(y);
    float fy = originY + YGNodeLayoutGetTop(y);
    int x0 = cells(fx);
    int y0 = cells(fy);
    int w = cells(fx + YGNodeLayoutGetWidth(y)) - x0;
    int hgt = cells(fy + YGNodeLayoutGetHeight(y)) - y0;
    Rect rect{x0, y0, x0 + w, y0 + hgt};
    Rect visible = rect.intersect(clip);
    if (!n->isText && h != kRoot) {  // for the mouse: what is where
        hits_.push_back(Hit{.visible = visible, .node = h});
        origins_[h] = {x0, y0};
    }

    // Content box: inside border and padding.
    auto inset = [&](YGEdge e) {
        return cells(YGNodeLayoutGetBorder(y, e)) + cells(YGNodeLayoutGetPadding(y, e));
    };
    Rect content{x0 + inset(YGEdgeLeft), y0 + inset(YGEdgeTop), x0 + w - inset(YGEdgeRight),
                 y0 + hgt - inset(YGEdgeBottom)};
    int contentW = std::max(content.right - content.left, 0);

    Style st = n->isText ? inherited : styleOf(*n, inherited);
    if (h == focused_) st.reverse = true;

    if (n->isText || n->type == "text") {
        bool wrap = n->isText || prop(n->props, "wrap") != "false";
        auto text = textOf(*n);
        auto lines = wrap ? wrapText(text, contentW) : splitLines(text);
        Rect area = content.intersect(clip);
        for (std::size_t i = 0; i < lines.size(); ++i)
            put(frame, content.left, content.top + static_cast<int>(i), area, lines[i], st);
        return;
    }
    if (n->type == "button") {
        put(frame, content.left, content.top, content.intersect(clip), "[ " + textOf(*n) + " ]", st);
        return;
    }
    if (n->type == "input") {
        Rect area = content.intersect(clip);
        std::string value = prop(n->props, "value");
        Style field = st;
        field.underline = true;
        put(frame, content.left, content.top, area, std::string(static_cast<std::size_t>(contentW), ' '), field);
        if (value.empty() && h != focused_) {
            Style hint = field;
            hint.dim = true;
            put(frame, content.left, content.top, area, prop(n->props, "placeholder"), hint);
        } else {
            // Keep the end of the text (and the cursor) visible.
            std::string shown = value + (h == focused_ ? "_" : "");
            auto chars = detail::utf8Split(shown);
            auto room = static_cast<std::size_t>(contentW);
            std::size_t skip = chars.size() > room ? chars.size() - room : 0;
            std::string tail;
            for (std::size_t i = skip; i < chars.size(); ++i) tail += chars[i];
            put(frame, content.left, content.top, area, tail, field);
        }
        return;
    }

    // Box. opaque=true clears what was drawn under it (for modals).
    if (flag(n->props, "opaque")) {
        for (int yy = visible.top; yy < visible.bottom; ++yy) {
            Style blank;
            blank.bg = st.bg;
            put(frame, visible.left, yy, visible,
                std::string(static_cast<std::size_t>(std::max(visible.right - visible.left, 0)), ' '), blank);
        }
    }
    if (!st.bg.empty() || st.reverse) {
        for (int yy = visible.top; yy < visible.bottom; ++yy) {
            if (yy < 0 || yy >= static_cast<int>(screen.extent(0))) continue;
            for (int xx = std::max(visible.left, 0); xx < std::min(visible.right, static_cast<int>(screen.extent(1))); ++xx) {
                auto& cell = screen[static_cast<std::size_t>(yy), static_cast<std::size_t>(xx)].style;
                if (!st.bg.empty()) cell.bg = st.bg;
                if (st.reverse) cell.reverse = true;
                if (!st.fg.empty() && cell.fg.empty()) cell.fg = st.fg;
            }
        }
    }

    // Like CSS overflow: visible, except that children never draw over a
    // border; overflow="hidden" clips them to the box.
    int b = flag(n->props, "border") ? 1 : 0;
    Rect inner = clip;
    if (b || prop(n->props, "overflow") == "hidden")
        inner = Rect{x0 + b, y0 + b, x0 + w - b, y0 + hgt - b}.intersect(clip);
    for (NodeHandle c : n->children) draw(layout, c, fx, fy, st, inner, frame);

    if (b && w >= 2 && hgt >= 2) {
        Style bs = st;
        bs.bold = false;
        int right = x0 + w;
        int bottom = y0 + hgt - 1;
        std::string title = prop(n->props, "title");
        // Box drawing (UTF-8) or ASCII for other locales.
        std::string_view tl = unicode_ ? "┌" : "+", tr = unicode_ ? "┐" : "+", bl = unicode_ ? "└" : "+",
                         br = unicode_ ? "┘" : "+", hz = unicode_ ? "─" : "-", vt = unicode_ ? "│" : "|";
        std::string topLine(tl);
        int used = 1;
        if (!title.empty() && w > 6) {
            auto chars = detail::utf8Split(title);
            std::size_t room = static_cast<std::size_t>(w - 6);
            if (chars.size() > room) chars.resize(room);
            std::string t;
            for (auto c : chars) t += c;
            topLine += std::string(hz) + " " + t + " ";
            used += 3 + static_cast<int>(chars.size());
        }
        for (; used < w - 1; ++used) topLine += hz;
        topLine += tr;
        put(frame, x0, y0, clip, topLine, bs);
        for (int yy = y0 + 1; yy < bottom; ++yy) {
            put(frame, x0, yy, clip, vt, bs);
            put(frame, right - 1, yy, clip, vt, bs);
        }
        std::string bottomLine(bl);
        for (int i = 1; i < w - 1; ++i) bottomLine += hz;
        bottomLine += br;
        put(frame, x0, bottom, clip, bottomLine, bs);
    }
}

// ---- painting ----

std::string TerminalRenderer::Frame::lineToAnsi(std::span<const Cell> line) {
    std::size_t end = line.size();
    while (end > 0 && line[end - 1].text() == " " && line[end - 1].style == Style{}) --end;

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
        out += c.text();
    }
    if (current != Style{}) out += "\x1b[0m";
    return out;
}

void TerminalRenderer::repaint() {
    Layout layout;
    layout.root = layout.build(*this, kRoot, false);
    YGNodeStyleSetWidth(layout.root, static_cast<float>(width_));
    YGNodeStyleSetHeight(layout.root, static_cast<float>(height_));
    YGNodeCalculateLayout(layout.root, static_cast<float>(width_), static_cast<float>(height_), YGDirectionLTR);

    frame_ = std::make_unique<Frame>(static_cast<std::size_t>(height_), static_cast<std::size_t>(width_));
    hits_.clear();
    origins_.clear();
    draw(layout, kRoot, 0, 0, Style{}, Rect{0, 0, width_, height_}, *frame_);
    if (!output_) return;

    std::string out;
    if (fullRedraw_) {
        out += "\x1b[0m\x1b[2J";
        shown_.assign(static_cast<std::size_t>(height_), std::string{});
        fullRedraw_ = false;
    }
    shown_.resize(static_cast<std::size_t>(height_));
    for (std::size_t i = 0; i < shown_.size(); ++i) {
        std::string line = i < frame_->rows ? Frame::lineToAnsi(frame_->row(i)) : std::string{};
        if (line == shown_[i]) continue;
        out += "\x1b[" + std::to_string(i + 1) + ";1H" + line + "\x1b[K";
        shown_[i] = std::move(line);
    }
    if (!out.empty()) output_(out);
}

std::vector<std::string> TerminalRenderer::frameLines() const {
    std::vector<std::string> lines;
    if (!frame_) return lines;
    for (std::size_t y = 0; y < frame_->rows; ++y) {
        std::string line;
        for (const Frame::Cell& cell : frame_->row(y)) line += cell.text();
        while (line.ends_with(' ')) line.pop_back();
        lines.push_back(std::move(line));
    }
    while (!lines.empty() && lines.back().empty()) lines.pop_back();  // screen is mostly empty
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

// Last node (in tree order) with trapFocus=true, or 0.
NodeHandle TerminalRenderer::focusTrap(NodeHandle h) const {
    const Node* n = findNode(h);
    if (!n || n->isText) return 0;
    NodeHandle found = flag(n->props, "trapFocus") ? h : 0;
    for (NodeHandle c : n->children)
        if (NodeHandle inner = focusTrap(c)) found = inner;
    return found;
}

std::vector<NodeHandle> TerminalRenderer::focusable() const {
    std::vector<NodeHandle> out;
    NodeHandle trap = focusTrap(kRoot);
    collectFocusable(trap ? trap : kRoot, out);
    return out;
}

void TerminalRenderer::fixFocus() {
    // A focus trap (e.g. a modal) appeared or went away.
    if (NodeHandle trap = focusTrap(kRoot); trap != trap_) {
        if (trap && !trap_) savedFocus_ = focused_;  // to restore when it closes
        focused_ = 0;
        focusIndex_ = 0;
        if (!trap && savedFocus_ && findNode(savedFocus_)) focused_ = savedFocus_;
        if (!trap) savedFocus_ = 0;
        trap_ = trap;
    }
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

bool TerminalRenderer::bubbleKey(Root& root, const KeyEvent& key) {
    if (!focused_) return false;
    // Collect the path first: handlers may change the tree.
    std::vector<NodeHandle> path;
    for (NodeHandle h = focused_; h && h != kRoot;) {
        const Node* n = findNode(h);
        if (!n) break;
        path.push_back(h);
        h = n->parent;
    }
    bool handled = false;
    for (NodeHandle h : path)
        if (findNode(h)) handled = root.dispatch(h, "onKey", key) || handled;
    return handled;
}

bool TerminalRenderer::editingText() const {
    const Node* n = findNode(focused_);
    return n && !n->isText && n->type == "input";
}

bool TerminalRenderer::handleKey(Root& root, const KeyEvent& key) {
    NodeHandle before = focused_;
    bool handled = handleKeyNow(root, key);
    if (focused_ != before) {  // Tab / arrows / a handler moved the focus
        NodeHandle now = focused_;
        if (before && findNode(before)) root.dispatch(before, "onBlur");
        if (now && findNode(now)) root.dispatch(now, "onFocus");
    }
    return handled;
}

bool TerminalRenderer::handleKeyNow(Root& root, const KeyEvent& key) {
    if (key.ctrl || key.alt) return bubbleKey(root, key);  // no default action
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
    bool handled = bubbleKey(root, key);
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

// ---- mouse ----

NodeHandle TerminalRenderer::nodeAt(int x, int y) const {
    for (const Hit& hit : hits_ | std::views::reverse) {  // drawn last = on top
        const Rect& r = hit.visible;
        if (x >= r.left && x < r.right && y >= r.top && y < r.bottom) return hit.node;
    }
    return 0;
}

std::vector<NodeHandle> TerminalRenderer::pathOf(NodeHandle h) const {
    std::vector<NodeHandle> path;  // the node, then its ancestors
    for (const Node* n = nullptr; h && h != kRoot && (n = findNode(h)); h = n->parent) path.push_back(h);
    return path;
}

// Bubbles like the DOM; localX / localY are relative to each node in turn.
bool TerminalRenderer::sendPointer(Root& root, const std::vector<NodeHandle>& path, const char* event,
                                   PointerEvent e) {
    bool handled = false;
    for (NodeHandle h : path) {
        if (!findNode(h)) continue;  // removed by an earlier handler
        auto it = origins_.find(h);
        e.localX = it == origins_.end() ? 0 : e.x - it->second.first;
        e.localY = it == origins_.end() ? 0 : e.y - it->second.second;
        handled = root.dispatch(h, event, e) || handled;
    }
    return handled;
}

// onMouseLeave for the nodes the pointer left (inner first), then onMouseEnter
// for those it entered (outer first); neither bubbles, like React.
void TerminalRenderer::hover(Root& root, NodeHandle node) {
    std::vector<NodeHandle> now = pathOf(node);
    if (now == hovered_) return;
    std::vector<NodeHandle> before = std::exchange(hovered_, now);
    for (NodeHandle h : before)
        if (std::ranges::find(now, h) == now.end() && findNode(h)) root.dispatch(h, "onMouseLeave");
    for (NodeHandle h : now | std::views::reverse)
        if (std::ranges::find(before, h) == before.end() && findNode(h)) root.dispatch(h, "onMouseEnter");
}

bool TerminalRenderer::handleMouse(Root& root, const MouseInput& m) {
    using Action = MouseInput::Action;
    if (m.action == Action::WheelUp || m.action == Action::WheelDown) return false;
    NodeHandle hit = nodeAt(m.x, m.y);
    PointerEvent e{.x = static_cast<double>(m.x), .y = static_cast<double>(m.y), .button = m.button,
                   .buttons = 0, .type = "mouse", .ctrl = m.ctrl, .alt = m.alt, .shift = m.shift};
    int bit = m.button == 1 ? 4 : m.button == 2 ? 2 : 1;  // the DOM's `buttons` bits

    switch (m.action) {
        case Action::Down: {
            hover(root, hit);
            e.buttons = bit;
            pressed_ = hit;
            auto path = pathOf(hit);
            captured_ = 0;
            for (NodeHandle h : path)
                if (const Node* n = findNode(h); n && flag(n->props, "pointerCapture")) {
                    captured_ = h;
                    break;
                }
            sendPointer(root, path, "onPointerDown", e);
            // A press focuses the nearest node that can take focus, like a browser.
            for (NodeHandle h : path) {
                const Node* n = findNode(h);
                if (!n || !isFocusable(*n)) continue;
                NodeHandle before = focused_;
                if (h != before && focus(h)) {
                    if (before && findNode(before)) root.dispatch(before, "onBlur");
                    if (findNode(h)) root.dispatch(h, "onFocus");
                }
                break;
            }
            return true;
        }
        case Action::Move: {
            hover(root, hit);
            e.buttons = m.held ? bit : 0;
            return sendPointer(root, pathOf(captured_ && findNode(captured_) ? captured_ : hit), "onPointerMove", e);
        }
        case Action::Up: {
            NodeHandle target = captured_ && findNode(captured_) ? captured_ : hit;
            bool handled = sendPointer(root, pathOf(target), "onPointerUp", e);
            // A click is a press and a release over the same node (or inside
            // it): it goes to their nearest common ancestor and bubbles.
            auto down = pathOf(pressed_);
            for (NodeHandle h : pathOf(hit)) {
                if (std::ranges::find(down, h) == down.end()) continue;
                bool clicked = false;
                for (NodeHandle c : pathOf(h)) clicked = (findNode(c) && root.dispatch(c, "onClick")) || clicked;
                handled = handled || clicked;
                break;
            }
            pressed_ = captured_ = 0;
            hover(root, nodeAt(m.x, m.y));  // the tree may have changed under the pointer
            return handled;
        }
        default: return false;
    }
}

}  // namespace rx
