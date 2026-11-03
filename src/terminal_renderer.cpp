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
#include <unordered_set>
#include <utility>

#include "reactx/terminal.hpp"
#include "reactx/parse.hpp"
#include "utf8.hpp"

namespace rx {

namespace {

using PropMap = std::flat_map<std::string, std::string, std::less<>>;

std::string prop(const PropMap& props, std::string_view name, std::string fallback = {}) {
    auto it = props.find(name);
    return it == props.end() ? std::move(fallback) : it->second;
}

bool flag(const PropMap& props, std::string_view name) { return prop(props, name) == "true"; }

bool isLeafType(std::string_view type) { return type == "text" || type == "button" || type == "input" || type == "textarea"; }

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
    for (const auto& line : splitLines(text)) w = std::max(w, detail::textCells(line));
    return w;
}

// Word wrap to `width` columns (wide characters take two). Lines that fit
// are kept as they are (including leading spaces); words longer than a line
// are split between characters.
std::vector<std::string> wrapText(const std::string& text, int width) {
    width = std::max(width, 1);
    std::vector<std::string> out;
    for (const std::string& paragraph : splitLines(text)) {
        if (detail::textCells(paragraph) <= width) {
            out.push_back(paragraph);
            continue;
        }
        std::string line;
        int lineLen = 0;
        std::size_t start = 0;
        while (start <= paragraph.size()) {
            auto end = paragraph.find(' ', start);
            if (end == std::string::npos) end = paragraph.size();
            std::string_view word(paragraph.data() + start, end - start);
            start = end + 1;
            if (word.empty()) continue;

            auto glyphs = detail::utf8Glyphs(word);
            std::size_t pos = 0;
            int left = detail::textCells(word);
            while (left > width) {  // a word longer than a line: cut it into pieces
                if (lineLen > 0) {
                    out.push_back(std::move(line));
                    line.clear();
                    lineLen = 0;
                }
                std::string piece;
                int used = 0;
                while (pos < glyphs.size() && (used == 0 || used + glyphs[pos].width <= width)) {
                    piece += glyphs[pos].text;
                    used += glyphs[pos].width;
                    left -= glyphs[pos].width;
                    ++pos;
                }
                out.push_back(std::move(piece));
            }
            std::string rest;
            for (; pos < glyphs.size(); ++pos) rest += glyphs[pos].text;
            int restLen = left;
            if (rest.empty()) continue;
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
    int rows = 1;      // a textarea's height
    bool operator==(const Leaf&) const = default;
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
        case Leaf::Button: w = clampTo(detail::textCells(leaf.text) + 4, width, widthMode); break;
        case Leaf::Input:
            w = clampTo(std::max(textWidth(leaf.text) + 1, 10), width, widthMode);
            h = leaf.rows;
            break;
        case Leaf::Text: {
            int natural = textWidth(leaf.text);
            if (widthMode == YGMeasureModeUndefined || !leaf.wrap) {
                w = clampTo(natural, width, widthMode);
                h = static_cast<int>(splitLines(leaf.text).size());
            } else {
                auto lines = wrapText(leaf.text, static_cast<int>(width));
                for (const auto& l : lines) w = std::max(w, detail::textCells(l));
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

// The Yoga tree, kept between repaints. Changes to the node tree mark what
// they touched (restyle_, relist_) and the path to it from the root (path_);
// sync() updates only those Yoga nodes, so Yoga's cached layout of everything
// else stays valid and a long list costs little when a few rows change.
struct TerminalRenderer::Layout {
    struct Entry {
        YGNodeRef y = nullptr;
        std::unique_ptr<Leaf> leaf;  // text, button, input: measured by us
        bool parentRow = false;      // flexShrink depends on the parent's direction
        bool holdsScroll = false;    // an overflow="scroll" box, or one is inside (not in a fixed node)
        bool fresh = true;           // not synced yet
    };

    YGConfigRef config = YGConfigNew();
    YGNodeRef scratch = nullptr;  // styles are built here, then copied if they changed
    YGNodeRef blank = nullptr;    // the default style
    std::unordered_map<NodeHandle, Entry> nodes;
    // position="fixed" nodes: each is laid out in its own screen-sized wrapper
    // instead of its parent. `fixed` is them in tree order, for this repaint.
    std::unordered_map<NodeHandle, YGNodeRef> wraps;
    std::vector<std::pair<NodeHandle, YGNodeRef>> fixed;

    std::unordered_set<NodeHandle> restyle;  // props or text changed
    std::unordered_set<NodeHandle> relist;   // children changed
    std::unordered_set<NodeHandle> path;     // on the way from the root to a change

    // No rounding inside Yoga: it rounds text and boxes differently, which can
    // make neighbours overlap by a cell. draw() rounds every edge the same way.
    Layout() {
        YGConfigSetPointScaleFactor(config, 0.0f);
        scratch = YGNodeNewWithConfig(config);
        blank = YGNodeNewWithConfig(config);
    }
    ~Layout() {
        for (auto& [h, e] : nodes) YGNodeFree(e.y);
        for (auto& [h, w] : wraps) YGNodeFree(w);
        YGNodeFree(scratch);
        YGNodeFree(blank);
        YGConfigFree(config);
    }
    Layout(const Layout&) = delete;
    Layout& operator=(const Layout&) = delete;

    YGNodeRef yoga(NodeHandle h) const {
        auto it = nodes.find(h);
        return it == nodes.end() ? nullptr : it->second.y;
    }

    void markPath(const TerminalRenderer& r, NodeHandle h) {
        for (const Node* n; h && path.insert(h).second && (n = r.findNode(h));) h = n->parent;
    }
    void changed(const TerminalRenderer& r, NodeHandle h) {
        restyle.insert(h);
        markPath(r, h);
    }
    void childrenChanged(const TerminalRenderer& r, NodeHandle h) {
        relist.insert(h);
        markPath(r, h);
    }

    void destroy(NodeHandle h) {
        if (auto it = nodes.find(h); it != nodes.end()) {
            // Through the owner, so it is marked for layout (YGNodeFree alone doesn't).
            if (YGNodeRef owner = YGNodeGetOwner(it->second.y)) YGNodeRemoveChild(owner, it->second.y);
            YGNodeFree(it->second.y);
            nodes.erase(it);
        }
        if (auto w = wraps.find(h); w != wraps.end()) {
            YGNodeFree(w->second);
            wraps.erase(w);
        }
        restyle.erase(h);
        relist.erase(h);
        path.erase(h);
    }

    static bool isFixed(const TerminalRenderer& r, NodeHandle h) {
        const Node* n = r.findNode(h);
        return n && !n->isText && prop(n->props, "position") == "fixed";
    }

    static void detach(YGNodeRef y) {
        if (YGNodeRef owner = YGNodeGetOwner(y)) YGNodeRemoveChild(owner, y);
    }

    Entry& entry(NodeHandle h) {
        Entry& e = nodes[h];
        if (!e.y) e.y = YGNodeNewWithConfig(config);
        return e;
    }

    // Style of `n` from its props, built on `scratch` and copied to `y` only if
    // it changed (copying marks the node for layout).
    void applyStyleTo(YGNodeRef y, const Node& n, bool parentRow, bool fixedNode, bool holdsScroll) {
        YGNodeCopyStyle(scratch, blank);
        // Rows squeeze their children (text gets narrower); columns cut off.
        // Buttons never shrink: a half button is useless.
        bool button = !n.isText && n.type == "button";
        YGNodeStyleSetFlexShrink(scratch, parentRow && !button ? 1.0f : 0.0f);
        if (!n.isText) {
            bool leaf = isLeafType(n.type);
            // A scroll box in a column is as tall as its rows, or the room that is
            // left if they don't fit (CSS `flex: 1 1 auto; min-height: 0`); the boxes
            // around it give way too, so a short list shows all of its rows even in a
            // box that isn't stretched, and a long one scrolls.
            if (!parentRow && holdsScroll) YGNodeStyleSetFlexShrink(scratch, 1.0f);
            if (!leaf && prop(n.props, "overflow") == "scroll") {
                YGNodeStyleSetOverflow(scratch, YGOverflowScroll);  // children keep their size
                if (!parentRow && prop(n.props, "height").empty() && prop(n.props, "flexBasis").empty() &&
                    prop(n.props, "flexGrow").empty())
                    YGNodeStyleSetFlexGrow(scratch, 1.0f);
            }
            applyStyle(scratch, n.props, !leaf);
        }
        if (parentRow && !n.isText)  // growInRow=N: flexGrow, but only along a row
            if (Value v = parseValue(prop(n.props, "growInRow")); v.kind == Value::Points)
                YGNodeStyleSetFlexGrow(scratch, v.number);
        if (fixedNode) YGNodeStyleSetPositionType(scratch, YGPositionTypeAbsolute);
        YGNodeCopyStyle(y, scratch);
    }

    void refreshLeaf(const TerminalRenderer& r, Entry& e, const Node& n) {
        if (!e.leaf) {
            e.leaf = std::make_unique<Leaf>();
            YGNodeSetContext(e.y, e.leaf.get());
            YGNodeSetMeasureFunc(e.y, measureLeaf);
        }
        Leaf next;
        if (n.isText || n.type == "text") {
            next.text = r.textOf(n);
            next.wrap = n.isText || prop(n.props, "wrap") != "false";
        } else if (n.type == "button") {
            next.kind = Leaf::Button;
            next.text = r.textOf(n);
        } else {
            next.kind = Leaf::Input;
            next.text = prop(n.props, "value");
            if (next.text.empty()) next.text = prop(n.props, "placeholder");
            if (n.type == "textarea") next.rows = std::clamp(rx::parseNumber<int>(prop(n.props, "rows")).value_or(4), 1, 100);
        }
        if (e.fresh || next != *e.leaf) {
            *e.leaf = std::move(next);
            YGNodeMarkDirty(e.y);
        }
        for (NodeHandle c : n.children) {  // the text nodes inside are part of this leaf
            restyle.erase(c);
            path.erase(c);
        }
    }

    // Puts the Yoga children of `y` in the order of the node's children,
    // touching only those that came, went or moved.
    void relistChildren(const TerminalRenderer& r, YGNodeRef y, const Node& n) {
        std::vector<YGNodeRef> want;
        want.reserve(n.children.size());
        for (NodeHandle c : n.children) {
            Entry& ce = entry(c);
            if (isFixed(r, c)) {
                YGNodeRef& w = wraps[c];
                if (!w) w = YGNodeNewWithConfig(config);
                if (YGNodeGetOwner(ce.y) != w) {
                    detach(ce.y);
                    YGNodeInsertChild(w, ce.y, 0);
                }
                continue;
            }
            if (auto w = wraps.find(c); w != wraps.end()) {  // no longer fixed
                detach(ce.y);
                YGNodeFree(w->second);
                wraps.erase(w);
            }
            want.push_back(ce.y);
        }
        std::size_t count = YGNodeGetChildCount(y);
        bool prefix = count <= want.size();
        for (std::size_t i = 0; prefix && i < count; ++i) prefix = YGNodeGetChild(y, i) == want[i];
        if (!prefix) {
            std::unordered_set<YGNodeRef> wanted(want.begin(), want.end());
            for (std::size_t i = count; i-- > 0;)
                if (YGNodeRef c = YGNodeGetChild(y, i); !wanted.contains(c)) YGNodeRemoveChild(y, c);
        }
        for (std::size_t i = 0; i < want.size(); ++i) {
            if (i < YGNodeGetChildCount(y) && YGNodeGetChild(y, i) == want[i]) continue;
            detach(want[i]);
            YGNodeInsertChild(y, want[i], i);
        }
    }

    void sync(const TerminalRenderer& r, NodeHandle h, bool parentRow) {
        const Node* n = r.findNode(h);
        if (!n) return;
        Entry& e = entry(h);
        bool fresh = e.fresh;
        bool restyled = restyle.erase(h) > 0;
        bool relisted = relist.erase(h) > 0;
        bool onPath = path.erase(h) > 0;
        bool fixedNode = isFixed(r, h);
        if (fresh || restyled || e.parentRow != parentRow) applyStyleTo(e.y, *n, parentRow, fixedNode, e.holdsScroll);
        e.parentRow = parentRow;

        if (n->isText || isLeafType(n->type)) {
            if (fresh || restyled || onPath) refreshLeaf(r, e, *n);
            e.fresh = false;
            return;
        }
        e.fresh = false;
        if (fresh || relisted) relistChildren(r, e.y, *n);
        if (!(fresh || restyled || relisted || onPath)) return;
        bool row = isRow(n->props);
        for (NodeHandle c : n->children) {
            auto it = nodes.find(c);
            bool childFixed = wraps.contains(c);
            bool childRow = childFixed ? false : row;  // a fixed node is laid out on the screen
            if (it == nodes.end() || it->second.fresh || it->second.parentRow != childRow || restyle.contains(c) ||
                relist.contains(c) || path.contains(c))
                sync(r, c, childRow);
        }
        bool holds = prop(n->props, "overflow") == "scroll";
        for (NodeHandle c : n->children)
            if (auto it = nodes.find(c); !holds && it != nodes.end() && it->second.holdsScroll && !wraps.contains(c))
                holds = true;
        if (holds != e.holdsScroll) {
            e.holdsScroll = holds;
            applyStyleTo(e.y, *n, parentRow, fixedNode, holds);
        }
    }

    // Fixed nodes that are on the page, in tree order (outer ones first).
    void collectFixed(const TerminalRenderer& r) {
        fixed.clear();
        std::vector<std::pair<std::vector<std::size_t>, NodeHandle>> order;
        for (auto& [h, w] : wraps) {
            std::vector<std::size_t> where;  // child indexes from the root, reversed
            NodeHandle at = h;
            const Node* n = r.findNode(at);
            while (n && n->parent) {
                const Node* p = r.findNode(n->parent);
                if (!p) break;
                where.push_back(static_cast<std::size_t>(std::ranges::find(p->children, at) - p->children.begin()));
                at = n->parent;
                n = p;
            }
            if (at != kRoot) continue;  // not on the page
            std::ranges::reverse(where);
            order.emplace_back(std::move(where), h);
        }
        std::ranges::sort(order);
        for (auto& [where, h] : order) fixed.emplace_back(h, wraps.at(h));
    }
};

// The cells of one repaint.
struct TerminalRenderer::Frame {
    struct Cell {
        // One character (a glyph: a code point and its marks), no heap (C++26).
        // Empty: the right half of the wide character in the cell before.
        std::inplace_vector<char, 16> ch{' '};
        bool continuation() const { return ch.empty(); }
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

TerminalRenderer::TerminalRenderer(Output output) : output_(std::move(output)), layout_(std::make_unique<Layout>()) {}
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

void TerminalRenderer::setProp(NodeHandle h, std::string_view name, const PropValue& value) {
    TextRenderer::setProp(h, name, value);
    propChanged(h, name);
    if (name == "scrollIntoView" && std::holds_alternative<bool>(value) && std::get<bool>(value)) scrollRequest_ = h;
}

void TerminalRenderer::propChanged(NodeHandle h, std::string_view name) {
    // Props that only change how a node is drawn don't touch the layout.
    static constexpr std::string_view drawOnly[] = {"color", "bg", "bold", "dim", "underline", "title",
                                                    "focusable", "arrowKeys", "trapFocus", "opaque", "scrollIntoView", "followEnd", "key"};
    if (std::ranges::contains(drawOnly, name) || name.starts_with("on")) return;
    layout_->changed(*this, h);
    if (name == "position")  // fixed nodes leave their parent's layout
        if (const Node* n = findNode(h); n && n->parent) layout_->childrenChanged(*this, n->parent);
}

void TerminalRenderer::removeProp(NodeHandle h, std::string_view name) {
    TextRenderer::removeProp(h, name);
    propChanged(h, name);
}

void TerminalRenderer::setText(NodeHandle h, std::string_view text) {
    TextRenderer::setText(h, text);
    layout_->changed(*this, h);
}

void TerminalRenderer::insertChild(NodeHandle parent, NodeHandle child, std::size_t index) {
    TextRenderer::insertChild(parent, child, index);
    layout_->childrenChanged(*this, parent);
}

void TerminalRenderer::removeChild(NodeHandle parent, NodeHandle child) {
    TextRenderer::removeChild(parent, child);
    layout_->childrenChanged(*this, parent);
}

void TerminalRenderer::destroyNode(NodeHandle h) {
    layout_->destroy(h);
    if (h == focused_) focused_ = 0;  // fixFocus picks a neighbour on commit
    if (h == scrolledTo_) scrolledTo_ = 0;
    if (h == scrollRequest_) scrollRequest_ = 0;
    scrollTop_.erase(h);
    scrollBoxes_.erase(h);
    edits_.erase(h);
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
    int left = std::max(clip.left, 0);
    auto at = [&](int col) -> Frame::Cell& { return screen[static_cast<std::size_t>(y), static_cast<std::size_t>(col)]; };
    // Writing over half of a wide character blanks its other half.
    auto claim = [&](int col) {
        if (at(col).continuation() && col > 0) at(col - 1).ch = {' '};
        if (col + 1 < static_cast<int>(screen.extent(1)) && at(col + 1).continuation()) at(col + 1).ch = {' '};
    };
    auto blank = [&](int col) {
        if (col < left || col >= right) return;
        claim(col);
        at(col).ch = {' '};
        at(col).style = style;
    };
    for (const detail::Glyph& g : detail::utf8Glyphs(text)) {
        if (x >= right) break;
        if (g.width == 0) continue;  // a control character or a mark with nothing before it
        if (x >= left) {
            if (g.width == 2 && x + 1 >= right) {
                blank(x);  // half of it would be cut off
            } else {
                claim(x);
                Frame::Cell& cell = at(x);
                if (g.text.size() <= cell.ch.capacity()) cell.ch.assign(g.text.begin(), g.text.end());
                else cell.ch.assign(g.text.begin(), g.text.begin() + static_cast<std::ptrdiff_t>(detail::utf8SeqLength(static_cast<unsigned char>(g.text[0]))));
                cell.style = style;
                if (g.width == 2) {
                    claim(x + 1);
                    at(x + 1).ch.clear();
                    at(x + 1).style = style;
                }
            }
        } else if (g.width == 2 && x + 1 >= left) {
            blank(x + 1);  // the left half is cut off
        }
        x += g.width;
    }
}

void TerminalRenderer::draw(const Layout& layout, NodeHandle h, float originX, float originY, Style inherited,
                            Rect clip, Frame& frame) {
    Frame::Screen screen = frame.screen();
    const Node* n = findNode(h);
    YGNodeRef y = layout.yoga(h);
    if (!n || !y) return;
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
    if (n->type == "input" || n->type == "textarea") {
        drawField(h, content, clip, st, n->type == "textarea", frame);
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
    std::string overflow = prop(n->props, "overflow");
    bool scroll = overflow == "scroll";
    Rect inner = clip;
    if (b || overflow == "hidden" || scroll)
        inner = Rect{x0 + b, y0 + b, x0 + w - b, y0 + hgt - b}.intersect(clip);
    float childY = fy;
    int thumbTop = 0, thumbLen = 0;  // the scroll position on the right border
    if (scroll) {
        float bottom = 0;
        for (NodeHandle c : n->children) {
            YGNodeRef cy = layout.yoga(c);
            if (!cy || layout.wraps.contains(c)) continue;
            bottom = std::max(bottom, YGNodeLayoutGetTop(cy) + YGNodeLayoutGetHeight(cy) +
                                          YGNodeLayoutGetMargin(cy, YGEdgeBottom));
        }
        int content = cells(bottom + YGNodeLayoutGetPadding(y, YGEdgeBottom)) + b;  // from the box's top
        ScrollBox box{.max = std::max(content - hgt, 0), .content = std::max(content - 2 * b, 0),
                      .viewport = std::max(hgt - 2 * b, 0), .width = std::max(w - 2 * b, 0)};
        int& top = scrollTop_[h];
        int was = top;
        auto before = lastScrollBoxes_.find(h);
        bool atEnd = before == lastScrollBoxes_.end() || was >= before->second.max;  // at the end until now
        top = std::clamp(top, 0, box.max);  // the content may have shrunk
        // followEnd: rows added at the end of a box that was at its end keep it
        // there in this very repaint (no frame at the old place); scrolling up stops it.
        if (atEnd && flag(n->props, "followEnd")) top = box.max;
        if (top != was && std::ranges::find(scrolled_, h) == scrolled_.end()) scrolled_.push_back(h);
        scrollBoxes_[h] = box;
        childY = fy - static_cast<float>(top);
        if (b && box.max > 0 && hgt > 2) {
            int track = hgt - 2;
            thumbLen = std::clamp(track * box.viewport / std::max(box.content, 1), 1, track);
            thumbTop = y0 + 1 + (track - thumbLen) * top / box.max;
        }
    }
    for (NodeHandle c : n->children) {
        if (layout.wraps.contains(c)) continue;  // fixed: drawn last
        if (scroll) {  // rows scrolled out of view: skip them (and their subtrees) at once
            YGNodeRef cy = layout.yoga(c);
            if (!cy) continue;
            float top = childY + YGNodeLayoutGetTop(cy);
            if (cells(top + YGNodeLayoutGetHeight(cy)) <= inner.top || cells(top) >= inner.bottom) continue;
        }
        draw(layout, c, fx, childY, st, inner, frame);
    }

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
            int room = w - 6, cells = 0;
            std::string t;
            for (const detail::Glyph& g : detail::utf8Glyphs(title)) {
                if (cells + g.width > room) break;
                t += g.text;
                cells += g.width;
            }
            topLine += std::string(hz) + " " + t + " ";
            used += 3 + cells;
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
        for (int yy = thumbTop; yy < thumbTop + thumbLen; ++yy) put(frame, right - 1, yy, clip, unicode_ ? "┃" : "#", bs);
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
        if (c.continuation()) continue;  // the right half of a wide character: the terminal moved two columns
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
    Layout& layout = *layout_;
    layout.sync(*this, kRoot, false);
    layout.collectFixed(*this);
    YGNodeRef root = layout.yoga(kRoot);
    YGNodeStyleSetWidth(root, static_cast<float>(width_));
    YGNodeStyleSetHeight(root, static_cast<float>(height_));
    YGNodeCalculateLayout(root, static_cast<float>(width_), static_cast<float>(height_), YGDirectionLTR);
    for (auto& [h, wrap] : layout.fixed) {
        if (!wrap) continue;
        YGNodeStyleSetWidth(wrap, static_cast<float>(width_));
        YGNodeStyleSetHeight(wrap, static_cast<float>(height_));
        YGNodeCalculateLayout(wrap, static_cast<float>(width_), static_cast<float>(height_), YGDirectionLTR);
    }

    // Keep a newly focused node (or a scrollIntoView target) visible.
    NodeHandle target = std::exchange(scrollRequest_, 0);
    if (focused_ != scrolledTo_) {
        scrolledTo_ = focused_;
        if (!target) target = focused_;
    }
    if (target) bringIntoView(layout, target);

    frame_ = std::make_unique<Frame>(static_cast<std::size_t>(height_), static_cast<std::size_t>(width_));
    hits_.clear();
    caret_.reset();
    origins_.clear();
    lastScrollBoxes_ = std::exchange(scrollBoxes_, {});
    Rect screenRect{0, 0, width_, height_};
    draw(layout, kRoot, 0, 0, Style{}, screenRect, *frame_);
    for (const auto& [h, wrap] : layout.fixed) {  // over everything, in tree order
        if (!wrap) continue;
        std::vector<const Node*> above;  // colors are inherited from the tree, not the screen
        for (const Node* n = findNode(h); n && n->parent && (n = findNode(n->parent));) above.push_back(n);
        Style inherited;
        for (const Node* a : above | std::views::reverse) inherited = styleOf(*a, inherited);
        draw(layout, h, 0, 0, inherited, screenRect, *frame_);
    }
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
    return !n.isText && (n.type == "input" || n.type == "textarea" || n.type == "button" || flag(n.props, "focusable"));
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
    // Nothing focused inside an open modal (only text in it): keys still reach the modal,
    // so its Escape closes it.
    NodeHandle from = focused_ ? focused_ : trap_;
    if (!from) return false;
    // Collect the path first: handlers may change the tree.
    std::vector<NodeHandle> path;
    for (NodeHandle h = from; h && h != kRoot;) {
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
    return n && !n->isText && (n->type == "input" || n->type == "textarea");
}

bool TerminalRenderer::handleKey(Root& root, const KeyEvent& key) {
    NodeHandle before = focused_;
    bool handled = handleKeyNow(root, key);
    if (focused_ != before) {  // Tab / arrows / a handler moved the focus
        NodeHandle now = focused_;
        if (before && findNode(before)) root.dispatch(before, "onBlur");
        if (now && findNode(now)) root.dispatch(now, "onFocus");
    }
    sendScrollEvents(root);
    return handled;
}

bool TerminalRenderer::handleKeyNow(Root& root, const KeyEvent& key) {
    // A text field moves its cursor: Ctrl+Left / Right by words, and in a
    // textarea Up / Down between lines (past the first or last line they move
    // the focus, as in any other place).
    const Node* f = findNode(focused_);
    bool field = f && !f->isText && (f->type == "input" || f->type == "textarea");
    bool caretKey = key.key == "Left" || key.key == "Right" || key.key == "Home" || key.key == "End";
    if (field && key.ctrl && !key.alt && caretKey) {
        NodeHandle target = focused_;
        bool handled = bubbleKey(root, key);
        return (findNode(target) && editKey(root, target, key, f->type == "textarea")) || handled;
    }
    if (field && f->type == "textarea" && !key.ctrl && !key.alt && (key.key == "Up" || key.key == "Down")) {
        NodeHandle target = focused_;
        bool handled = bubbleKey(root, key);
        if (findNode(target) && editKey(root, target, key, true)) return true;
        if (handled || focused_ != target) return true;
    }
    if (key.ctrl || key.alt) return bubbleKey(root, key);  // no default action
    // arrowKeys=true: the focused node uses Up / Down itself (a radio group);
    // they reach its onKey instead of moving the focus.
    if ((key.key == "Up" || key.key == "Down") && f && !f->isText && flag(f->props, "arrowKeys"))
        return bubbleKey(root, key);
    if ((key.key == "Tab" && !key.shift) || key.key == "Down") {
        focusNext();
        return true;
    }
    if ((key.key == "Tab" && key.shift) || key.key == "Up") {
        focusPrev();
        return true;
    }
    // PageUp / PageDown scroll even after an onKey ran (dispatch can't tell
    // "ran" from "used"), like a browser without preventDefault.
    if (!focused_) {
        bool handled = bubbleKey(root, key);
        return scrollKey(key) || handled;
    }

    NodeHandle target = focused_;
    bool handled = bubbleKey(root, key);
    const Node* n = findNode(target);
    if (!n) return handled;  // the handler removed the node

    if ((n->type == "input" || n->type == "textarea") && editKey(root, target, key, n->type == "textarea")) return true;
    if (key.key == "Enter" || key.key == " ") return root.dispatch(target, "onClick") || handled;
    return scrollKey(key) || handled;
}

// ---- text fields: cursor and editing ----

namespace {

// Where the cursor may stop: the start of each character, and the end.
std::vector<std::size_t> stops(std::string_view v) {
    std::vector<std::size_t> out;
    for (const detail::Glyph& g : detail::utf8Glyphs(v)) out.push_back(static_cast<std::size_t>(g.text.data() - v.data()));
    out.push_back(v.size());
    return out;
}
std::size_t stepLeft(std::string_view v, std::size_t pos) {
    auto s = stops(v);
    auto it = std::ranges::lower_bound(s, pos);
    return it == s.begin() ? 0 : *(it - 1);
}
std::size_t stepRight(std::string_view v, std::size_t pos) {
    auto s = stops(v);
    auto it = std::ranges::upper_bound(s, pos);
    return it == s.end() ? v.size() : *it;
}
bool isSpace(char c) { return c == ' ' || c == '\n' || c == '\t'; }
// Ctrl+Left / Ctrl+Right: to the start of this or the previous word, past the end of the next one.
std::size_t wordLeft(std::string_view v, std::size_t pos) {
    while (pos > 0 && isSpace(v[pos - 1])) --pos;
    while (pos > 0 && !isSpace(v[pos - 1])) pos = stepLeft(v, pos);
    return pos;
}
std::size_t wordRight(std::string_view v, std::size_t pos) {
    while (pos < v.size() && isSpace(v[pos])) ++pos;
    while (pos < v.size() && !isSpace(v[pos])) pos = stepRight(v, pos);
    return pos;
}

// A textarea's lines on screen: its own lines, wrapped at `width` cells
// between characters (an editor doesn't move words around while you type).
struct VisualLine {
    std::size_t begin = 0, end = 0;  // bytes of the value, without the '\n'
};
std::vector<VisualLine> visualLines(std::string_view v, int width) {
    std::vector<VisualLine> out;
    std::size_t begin = 0;
    int used = 0;
    for (const detail::Glyph& g : detail::utf8Glyphs(v)) {
        auto at = static_cast<std::size_t>(g.text.data() - v.data());
        if (g.text == "\n") {
            out.push_back({begin, at});
            begin = at + 1;
            used = 0;
            continue;
        }
        if (used > 0 && used + g.width > width) {
            out.push_back({begin, at});
            begin = at;
            used = 0;
        }
        used += g.width;
    }
    out.push_back({begin, v.size()});
    return out;
}
// The line and column (cells) of a byte offset.
std::pair<std::size_t, int> lineAndColumn(std::string_view v, const std::vector<VisualLine>& lines, std::size_t pos) {
    std::size_t i = 0;
    while (i + 1 < lines.size() && lines[i + 1].begin <= pos) ++i;
    return {i, detail::textCells(v.substr(lines[i].begin, pos - lines[i].begin))};
}
// The byte offset in `line` closest to column `col` (not past it).
std::size_t offsetAt(std::string_view v, const VisualLine& line, int col) {
    std::size_t pos = line.begin;
    int cells = 0;
    for (const detail::Glyph& g : detail::utf8Glyphs(v.substr(line.begin, line.end - line.begin))) {
        if (cells + g.width > col) break;
        cells += g.width;
        pos += g.text.size();
    }
    return pos;
}

}  // namespace

std::size_t TerminalRenderer::cursorIn(NodeHandle field, const std::string& value) {
    Edit& e = edits_[field];
    if (e.cursor == std::string::npos || value != e.expect) {
        e.expect = value;
        e.cursor = value.size();
    }
    e.cursor = std::min(e.cursor, value.size());
    return e.cursor;
}

// The keys of a focused input or textarea. True if the key was used.
bool TerminalRenderer::editKey(Root& root, NodeHandle field, const KeyEvent& key, bool multiline) {
    const Node* n = findNode(field);
    if (!n) return false;
    std::string value = prop(n->props, "value");
    std::size_t cur = cursorIn(field, value);
    Edit& e = edits_[field];
    // Set the edit state first: dispatching re-renders and repaints.
    auto change = [&](std::string next, std::size_t pos) {
        e.expect = next;
        e.cursor = pos;
        root.dispatch(field, "onInput", next);
        return true;
    };
    auto move = [&](std::size_t pos) {
        e.cursor = pos;
        repaint();
        return true;
    };
    std::vector<VisualLine> lines;
    std::size_t line = 0;
    int col = 0;
    if (multiline) {
        lines = visualLines(value, std::max(e.width, 1));
        std::tie(line, col) = lineAndColumn(value, lines, cur);
    }
    if (key.alt) return false;
    if (key.ctrl) {
        if (key.key == "Left") return move(wordLeft(value, cur));
        if (key.key == "Right") return move(wordRight(value, cur));
        if (key.key == "Home") return move(0);
        if (key.key == "End") return move(value.size());
        return false;
    }
    if (key.key == "Backspace") {
        if (cur == 0) return true;
        std::size_t from = stepLeft(value, cur);
        return change(value.erase(from, cur - from), from);
    }
    if (key.key == "Delete") {
        if (cur == value.size()) return true;
        std::size_t to = stepRight(value, cur);
        return change(value.erase(cur, to - cur), cur);
    }
    if (key.key == "Left") return move(stepLeft(value, cur));
    if (key.key == "Right") return move(stepRight(value, cur));
    if (key.key == "Home") return move(multiline ? lines[line].begin : 0);
    if (key.key == "End") return move(multiline ? lines[line].end : value.size());
    // Past the first or last line: not used here, the focus moves on.
    if (multiline && key.key == "Up") return line > 0 && move(offsetAt(value, lines[line - 1], col));
    if (multiline && key.key == "Down") return line + 1 < lines.size() && move(offsetAt(value, lines[line + 1], col));
    if (multiline && key.key == "Enter") return change(value.insert(cur, "\n"), cur + 1);
    if (detail::isPrintableKey(key.key)) return change(value.insert(cur, key.key), cur + key.key.size());
    return false;
}

// An input (one line, scrolled sideways) or a textarea (lines, scrolled down):
// the cursor stays in view and shows as an inverted cell while focused.
void TerminalRenderer::drawField(NodeHandle h, const Rect& content, const Rect& clip, Style st, bool multiline, Frame& frame) {
    const Node* n = findNode(h);
    std::string value = prop(n->props, "value");
    Rect area = content.intersect(clip);
    int width = std::max(content.right - content.left, 0);
    int height = multiline ? std::max(content.bottom - content.top, 1) : 1;
    bool focused = h == focused_;
    Style field = st;
    field.underline = true;
    for (int row = 0; row < height; ++row)
        put(frame, content.left, content.top + row, area, std::string(static_cast<std::size_t>(width), ' '), field);
    if (value.empty() && !focused) {
        Style hint = field;
        hint.dim = true;
        auto lines = multiline ? wrapText(prop(n->props, "placeholder"), width) : splitLines(prop(n->props, "placeholder"));
        for (int row = 0; row < height && row < static_cast<int>(lines.size()); ++row)
            put(frame, content.left, content.top + row, area, lines[static_cast<std::size_t>(row)], hint);
        return;
    }
    std::size_t cur = cursorIn(h, value);
    if (!multiline && prop(n->props, "type") == "password") {  // drawn as one • per character; edited as typed
        std::size_t before = 0, count = 0;
        for (const detail::Glyph& g : detail::utf8Glyphs(value)) {
            if (static_cast<std::size_t>(g.text.data() - value.data()) < cur) ++before;
            ++count;
        }
        std::string masked;
        for (std::size_t i = 0; i < count; ++i) masked += "•";
        value = std::move(masked);
        cur = before * std::string_view("•").size();
    }
    Edit& e = edits_[h];
    Style caret = field;
    caret.reverse = !field.reverse;
    auto glyphAt = [&](std::size_t pos) -> std::string {  // the character under the cursor (a space at the end)
        if (pos >= value.size() || value[pos] == '\n') return " ";
        return std::string(detail::utf8Glyphs(std::string_view(value).substr(pos)).front().text);
    };

    if (!multiline) {
        int at = detail::textCells(std::string_view(value).substr(0, cur));
        int total = detail::textCells(value);
        if (!focused) {
            e.scrollX = 0;
        } else {
            if (at < e.scrollX) e.scrollX = at;
            if (at >= e.scrollX + width) e.scrollX = at - width + 1;
            e.scrollX = std::clamp(e.scrollX, 0, std::max(0, total + 1 - width));
        }
        std::string shown;
        int pos = 0;
        for (const detail::Glyph& g : detail::utf8Glyphs(value)) {
            if (pos >= e.scrollX) shown += g.text;
            else if (pos + g.width > e.scrollX) shown += ' ';  // a wide character cut in half
            pos += g.width;
        }
        put(frame, content.left, content.top, area, shown, field);
        if (focused) {
            put(frame, content.left + at - e.scrollX, content.top, area, glyphAt(cur), caret);
            caret_ = std::pair{content.left + at - e.scrollX, content.top};
        }
        return;
    }

    e.width = std::max(width, 1);
    auto lines = visualLines(value, e.width);
    auto [line, col] = lineAndColumn(value, lines, cur);
    int row = static_cast<int>(line);
    if (focused) {
        if (row < e.scrollY) e.scrollY = row;
        if (row >= e.scrollY + height) e.scrollY = row - height + 1;
    }
    e.scrollY = std::clamp(e.scrollY, 0, std::max(0, static_cast<int>(lines.size()) - height));
    for (int r = 0; r < height; ++r) {
        auto i = static_cast<std::size_t>(e.scrollY + r);
        if (i >= lines.size()) break;
        put(frame, content.left, content.top + r, area, std::string_view(value).substr(lines[i].begin, lines[i].end - lines[i].begin), field);
    }
    if (focused) {
        put(frame, content.left + col, content.top + row - e.scrollY, area, glyphAt(cur), caret);
        caret_ = std::pair{content.left + col, content.top + row - e.scrollY};
    }
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
    NodeHandle hit = nodeAt(m.x, m.y);
    if (m.action == Action::WheelUp || m.action == Action::WheelDown) {
        int rows = m.action == Action::WheelUp ? -3 : 3;
        for (NodeHandle h : pathOf(hit)) {  // the innermost box that can still move that way
            if (trap_ && !inside(h, trap_)) break;  // not the page under a modal
            if (scrollBy(h, rows)) {
                sendScrollEvents(root);
                return true;
            }
        }
        return false;
    }
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

// ---- scrolling ----

bool TerminalRenderer::inside(NodeHandle h, NodeHandle ancestor) const {
    for (const Node* n = nullptr; h && (n = findNode(h)); h = n->parent)
        if (h == ancestor) return true;
    return false;
}

int TerminalRenderer::scrollTop(NodeHandle box) const {
    auto it = scrollTop_.find(box);
    return it == scrollTop_.end() ? 0 : it->second;
}

bool TerminalRenderer::scrollBy(NodeHandle box, int rows) {
    auto it = scrollBoxes_.find(box);
    if (it == scrollBoxes_.end()) return false;
    int& top = scrollTop_[box];
    int next = std::clamp(top + rows, 0, it->second.max);
    if (next == top) return false;
    top = next;
    if (std::ranges::find(scrolled_, box) == scrolled_.end()) scrolled_.push_back(box);
    repaint();
    return true;
}

std::optional<ScrollEvent> TerminalRenderer::scrollMetrics(NodeHandle box) {
    auto it = scrollBoxes_.find(box);
    if (it == scrollBoxes_.end()) return std::nullopt;
    const ScrollBox& b = it->second;
    return ScrollEvent{.top = scrollTop(box), .left = 0, .scrollHeight = b.content, .scrollWidth = b.width,
                       .clientHeight = b.viewport, .clientWidth = b.width};
}

bool TerminalRenderer::scrollTo(NodeHandle box, int top) {
    if (!scrollBoxes_.contains(box)) return false;
    scrollBy(box, top - scrollTop(box));
    return true;
}

bool TerminalRenderer::scrollIntoView(NodeHandle node) {
    if (!findNode(node)) return false;
    scrollRequest_ = node;
    repaint();
    return true;
}

// Scroll each overflow="scroll" ancestor of `node`, innermost first, the
// least needed to show it. `top` is the node's top within the current
// ancestor, as laid out (before scrolling).
void TerminalRenderer::bringIntoView(const Layout& layout, NodeHandle node) {
    YGNodeRef self = layout.yoga(node);
    if (!self) return;
    float height = YGNodeLayoutGetHeight(self);
    float top = 0;
    for (NodeHandle h = node; h && h != kRoot;) {
        const Node* n = findNode(h);
        YGNodeRef y = layout.yoga(h);
        if (!n || !y) return;
        top += YGNodeLayoutGetTop(y);
        if (layout.wraps.contains(h)) return;  // placed on the screen: nothing above scrolls it
        const Node* parent = findNode(n->parent);
        YGNodeRef p = layout.yoga(n->parent);
        if (!parent || !p) return;
        if (!parent->isText && prop(parent->props, "overflow") == "scroll") {
            float from = YGNodeLayoutGetBorder(p, YGEdgeTop) + YGNodeLayoutGetPadding(p, YGEdgeTop);
            float to = YGNodeLayoutGetHeight(p) - YGNodeLayoutGetBorder(p, YGEdgeBottom) -
                       YGNodeLayoutGetPadding(p, YGEdgeBottom);
            int& scroll = scrollTop_[n->parent];
            int before = scroll;
            if (top + height - static_cast<float>(scroll) > to) scroll = cells(top + height - to);
            if (top - static_cast<float>(scroll) < from) scroll = cells(top - from);  // a tall node: its top wins
            scroll = std::max(scroll, 0);  // draw() clamps the other end
            if (scroll != before && std::ranges::find(scrolled_, n->parent) == scrolled_.end())
                scrolled_.push_back(n->parent);
            top -= static_cast<float>(scroll);
        }
        h = n->parent;
    }
}

// PageUp / PageDown: the scroll box around the focus, or else the biggest
// one on screen (inside an open modal, if there is one).
bool TerminalRenderer::scrollKey(const KeyEvent& key) {
    if (key.key != "PageUp" && key.key != "PageDown") return false;
    NodeHandle box = 0;
    for (NodeHandle h : pathOf(focused_ ? focused_ : trap_)) {
        if (auto it = scrollBoxes_.find(h); it != scrollBoxes_.end() && it->second.max > 0) {
            box = h;
            break;
        }
    }
    if (!box) {
        int best = 0;
        for (const Hit& hit : hits_) {
            auto it = scrollBoxes_.find(hit.node);
            if (it == scrollBoxes_.end() || it->second.max == 0 || (trap_ && !inside(hit.node, trap_))) continue;
            if (it->second.viewport > best) {
                best = it->second.viewport;
                box = hit.node;
            }
        }
    }
    if (!box) return false;
    int page = std::max(scrollBoxes_[box].viewport - 1, 1);
    return scrollBy(box, key.key == "PageUp" ? -page : page);
}

void TerminalRenderer::sendScrollEvents(Root& root) {
    for (NodeHandle h : std::exchange(scrolled_, {})) {
        auto it = scrollBoxes_.find(h);
        if (!findNode(h) || it == scrollBoxes_.end()) continue;
        const ScrollBox& b = it->second;
        root.dispatch(h, "onScroll", ScrollEvent{.top = scrollTop(h), .left = 0, .scrollHeight = b.content,
                                                 .scrollWidth = b.width, .clientHeight = b.viewport,
                                                 .clientWidth = b.width});
    }
}

}  // namespace rx
