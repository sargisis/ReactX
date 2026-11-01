#include "reactx/ui.hpp"

#include <algorithm>
#include <utility>

#include "reactx/hooks.hpp"
#include "reactx/store.hpp"

// rx::ui for the terminal backend (box/text/button/input tags, Yoga layout).
// The typed helpers (ui::button(...)) are in ui_helpers.cpp.
namespace rx::ui {

Element themeProvider(Theme theme, std::vector<Element> children) {
    return ThemeContext.provide(std::move(theme), std::move(children));
}

const Theme& useTheme() { return useContext(ThemeContext); }

// ---- components ----

Element Button(const Props& p) {
    const Theme& t = useTheme();
    std::string label = p.get<std::string>("label");
    std::string variant = p.get<std::string>("variant", "default");
    if (p.get<bool>("disabled"))
        return h("text", {{"color", t.muted}, {"dim", true}, {"wrap", false}, {"flexShrink", 0}},
                 {"[ " + label + " ]"});
    std::string color = variant == "primary"   ? t.accent
                        : variant == "danger"  ? t.danger
                        : variant == "success" ? t.success
                                               : t.text;
    return h("button", {{"color", color}, {"bold", variant != "default"}, {"onClick", p.get<Callback>("onClick")}},
             {label});
}

namespace {
// The field and, if there is an error, the message under it in the danger color.
Element withError(Element field, const Props& p, const Theme& t) {
    std::string error = p.get<std::string>("error");
    if (error.empty()) return field;
    return h("box", {{"growInRow", 1}}, {std::move(field), h("text", {{"color", t.danger}}, {"✗ " + error})});
}
}  // namespace

Element Input(const Props& p) {
    const Theme& t = useTheme();
    std::string value = p.get<std::string>("value");
    auto onChange = p.get<InputHandler>("onChange");
    auto onSubmit = p.get<InputHandler>("onSubmit");
    std::string label = p.get<std::string>("label");

    // Disabled: the value as dim text, which Tab skips and keys don't change.
    Element field = p.get<bool>("disabled")
        ? h("text", {{"flexGrow", 1}, {"color", t.muted}, {"dim", true}, {"underline", true}, {"wrap", false}},
            {value.empty() ? p.get<std::string>("placeholder") : value})
        : h("input", {
              {"flexGrow", 1},
              {"value", value},
              {"placeholder", p.get<std::string>("placeholder")},
              {"color", p.get<std::string>("error").empty() ? t.text : t.danger},
              {"onInput", onChange},
              {"onKey", [onChange, onSubmit, value](const KeyEvent& e) {
                   if (e.key == "Enter" && onSubmit) onSubmit(value);
                   if (e.key == "Escape" && onChange && !value.empty()) onChange(std::string{});
               }},
          });
    // growInRow: takes the free room in a row (a search box next to buttons), not in a column.
    if (label.empty()) return withError(h("box", {{"direction", "row"}, {"growInRow", 1}}, {std::move(field)}), p, t);
    if (p.get<bool>("labelAbove"))
        return withError(h("box", {{"growInRow", 1}}, {h("text", {{"color", t.muted}, {"wrap", false}}, {label}), std::move(field)}), p, t);
    return withError(h("box", {{"direction", "row"}, {"gap", 1}, {"growInRow", 1}}, {
        h("text", {{"color", t.muted}, {"wrap", false}}, {label}),
        std::move(field),
    }), p, t);
}

Element Checkbox(const Props& p) {
    const Theme& t = useTheme();
    bool checked = p.get<bool>("checked");
    bool off = p.get<bool>("disabled");
    Element box = h("box", {{"direction", "row"}, {"gap", 1}}, {
        h("text", {{"color", checked && !off ? t.accent : t.muted}, {"dim", off}, {"wrap", false}}, {checked ? "[x]" : "[ ]"}),
        h("text", {{"color", off ? t.muted : t.text}, {"dim", off}}, {p.get<std::string>("label")}),
    });
    if (!off) {
        box.props.set("focusable", true);
        box.props.set("onClick", p.get<Callback>("onToggle"));
    }
    return withError(std::move(box), p, t);
}

Element Tabs(const Props& p) {
    const Theme& t = useTheme();
    const auto& tabs = p.children();
    auto [active, setActive] = useState(p.get<int>("initial", 0));
    if (tabs.empty()) return fragment({});
    int current = std::clamp(active, 0, static_cast<int>(tabs.size()) - 1);

    std::vector<Element> header;
    for (std::size_t i = 0; i < tabs.size(); ++i) {
        int index = static_cast<int>(i);
        bool on = index == current;
        std::string label = tabs[i].props.get<std::string>("label");
        header.push_back(h("box", {{"key", label},
                                   {"focusable", true},
                                   {"paddingX", 1},
                                   {"onClick", [setActive, index] { setActive(index); }}},
                           {h("text", {{"color", on ? t.accent : t.muted}, {"bold", on}, {"underline", on}, {"wrap", false}},
                              {label})}));
    }
    const Element& tab = tabs[static_cast<std::size_t>(current)];
    return h("box", {{"gap", 1}}, {
        h("box", {{"direction", "row"}, {"gap", 1}}, std::move(header)),
        // Keyed by label: switching tabs mounts fresh content.
        fragment(tab.props.children(), tab.props.get<std::string>("label")),
    });
}

Element Modal(const Props& p) {
    if (!p.get<bool>("open")) return fragment({});
    const Theme& t = useTheme();
    auto onClose = p.get<Callback>("onClose");
    int width = p.get<int>("width");
    return h("box", {
        // fixed: the screen, even when the page under it is taller than the terminal
        {"position", "fixed"}, {"top", 0}, {"left", 0}, {"right", 0}, {"bottom", 0},
        {"justifyContent", "center"}, {"alignItems", "center"},
        {"trapFocus", true},
        {"onKey", [onClose](const KeyEvent& e) {
             if (e.key == "Escape" && onClose) onClose();
         }},
    }, {
        h("box", {
            {"border", true}, {"opaque", true},
            {"title", p.get<std::string>("title")},
            {"color", t.accent}, {"bg", t.surface},
            {"paddingX", 2}, {"paddingY", 1}, {"gap", 1},
            {"width", width > 0 ? PropValue(std::int64_t{width}) : PropValue(std::string("60%"))},
            {"maxWidth", "100%"},
        }, p.children()),
    });
}

Element List(const Props& p) {
    const Theme& t = useTheme();
    std::vector<Element> items = p.children();
    if (items.empty()) items.push_back(h("text", {{"color", t.muted}, {"dim", true}}, {p.get<std::string>("emptyText")}));
    std::string title = p.get<std::string>("title");
    return h("box", {{"border", !title.empty()}, {"title", title}, {"color", t.muted}, {"paddingX", title.empty() ? 0 : 1},
                     {"overflow", p.get<bool>("scroll") ? "scroll" : "visible"}},
             std::move(items));
}

Element ListItem(const Props& p) {
    const Theme& t = useTheme();
    bool selected = p.get<bool>("selected");
    std::string detail = p.get<std::string>("detail");
    std::vector<Element> parts;
    parts.push_back(h("text", {{"color", selected ? t.accent : t.text}, {"bold", selected}, {"flexShrink", 1}},
                      {(selected ? "› " : "  ") + p.get<std::string>("label")}));
    if (!detail.empty()) parts.push_back(h("text", {{"color", t.muted}, {"wrap", false}}, {detail}));
    return h("box", {{"direction", "row"},
                     {"justifyContent", "space-between"},
                     {"gap", 1},
                     {"focusable", true},
                     {"scrollIntoView", selected},  // the selected item stays on screen in a scroll box
                     {"onClick", p.get<Callback>("onSelect")},
                     {"onKey", p.get<KeyHandler>("onKey")}},
             std::move(parts));
}

Element Text(const Props& p) {
    const Theme& t = useTheme();
    bool muted = p.get<bool>("muted");
    return h("text", {{"color", muted ? t.muted : t.text}, {"dim", muted}}, {p.get<std::string>("text")});
}

Element Card(const Props& p) {
    const Theme& t = useTheme();
    return h("box", {{"border", true}, {"title", p.get<std::string>("title")}, {"color", t.muted},
                     {"paddingX", 1}, {"gap", 1}},
             p.children());
}

Element Stack(const Props& p) {
    std::string align = p.get<std::string>("align", "stretch");
    std::string justify = p.get<std::string>("justify", "start");
    return h("box", {{"direction", p.get<std::string>("direction", "column")},
                     {"gap", p.get<int>("gap", 1)},
                     {"alignItems", align},
                     {"justifyContent", justify == "between" ? std::string("space-between") : justify},
                     {"flexWrap", p.get<bool>("wrap") ? "wrap" : "nowrap"}},
             p.children());
}

// "‹ Medium ›": Left / Right step through the options, Enter or a click picks the next.
Element Select(const Props& p) {
    const Theme& t = useTheme();
    std::vector<std::string> values, labels;
    for (const Element& o : p.children()) {
        values.push_back(o.props.get<std::string>("value"));
        labels.push_back(o.props.get<std::string>("label"));
    }
    std::string value = p.get<std::string>("value");
    auto it = std::ranges::find(values, value);
    int current = it == values.end() ? -1 : static_cast<int>(it - values.begin());
    int n = static_cast<int>(values.size());
    auto onChange = p.get<InputHandler>("onChange");
    auto pick = [onChange, values, n](int i) {
        if (onChange && n > 0) onChange(values[static_cast<std::size_t>(((i % n) + n) % n)]);
    };
    std::string shown = current >= 0 ? labels[static_cast<std::size_t>(current)] : "—";
    std::string label = p.get<std::string>("label");

    bool off = p.get<bool>("disabled");
    Element field = h("box", {{"direction", "row"}},
                      {h("text", {{"color", off ? t.muted : t.accent}, {"dim", off}, {"wrap", false}}, {"‹ " + shown + " ›"})});
    if (!off) {
        field.props.set("focusable", true);
        field.props.set("onClick", Callback([pick, current] { pick(current + 1); }));
        field.props.set("onKey", KeyHandler([pick, current](const KeyEvent& e) {
                            if (e.key == "Right" || e.key == "Enter") pick(current + 1);
                            else if (e.key == "Left") pick(current - 1);
                        }));
    }
    if (label.empty()) return withError(std::move(field), p, t);
    return withError(h("box", {{"direction", p.get<bool>("labelAbove") ? "column" : "row"}, {"gap", p.get<bool>("labelAbove") ? 0 : 1}}, {
        h("text", {{"color", t.muted}, {"wrap", false}}, {label}),
        std::move(field),
    }), p, t);
}

// Several lines: Enter starts a new one, Up / Down move between them, long
// lines wrap and the field scrolls to keep the cursor in view.
Element Textarea(const Props& p) {
    const Theme& t = useTheme();
    std::string label = p.get<std::string>("label");
    std::string value = p.get<std::string>("value");
    Element field = p.get<bool>("disabled")
        ? h("box", {{"height", p.get<int>("rows", 4)}, {"overflow", "hidden"}},
            {h("text", {{"color", t.muted}, {"dim", true}}, {value.empty() ? p.get<std::string>("placeholder") : value})})
        : h("textarea", {
              {"value", value},
              {"placeholder", p.get<std::string>("placeholder")},
              {"rows", p.get<int>("rows", 4)},
              {"color", p.get<std::string>("error").empty() ? t.text : t.danger},
              {"onInput", p.get<InputHandler>("onChange")},
          });
    std::vector<Element> parts;
    if (!label.empty()) parts.push_back(h("text", {{"color", t.muted}, {"wrap", false}}, {label}));
    parts.push_back(std::move(field));
    return withError(h("box", {{"growInRow", 1}}, std::move(parts)), p, t);
}

Element RadioGroup(const Props& p) {
    const Theme& t = useTheme();
    std::string value = p.get<std::string>("value");
    auto onChange = p.get<InputHandler>("onChange");
    std::vector<std::string> values;
    for (const Element& o : p.children()) values.push_back(o.props.get<std::string>("value"));
    auto pick = [onChange, values](int i) {
        int n = static_cast<int>(values.size());
        if (onChange && n > 0) onChange(values[static_cast<std::size_t>(std::clamp(i, 0, n - 1))]);
    };
    std::vector<Element> items;
    std::string label = p.get<std::string>("label");
    bool off = p.get<bool>("disabled");
    if (!label.empty()) items.push_back(h("text", {{"color", t.muted}, {"wrap", false}}, {label}));
    std::vector<Element> options;
    for (std::size_t i = 0; i < values.size(); ++i) {
        bool on = values[i] == value;
        int index = static_cast<int>(i);
        Element option = h("box", {{"key", values[i]}, {"direction", "row"}, {"gap", 1}},
                           {h("text", {{"color", on && !off ? t.accent : t.muted}, {"dim", off}, {"wrap", false}}, {on ? "(•)" : "( )"}),
                            h("text", {{"color", off ? t.muted : t.text}, {"dim", off}, {"bold", on}},
                              {p.children()[i].props.get<std::string>("label")})});
        if (!off) {
            option.props.set("focusable", true);
            option.props.set("onClick", Callback([pick, index] { pick(index); }));
            option.props.set("onKey", KeyHandler([pick, index](const KeyEvent& e) {
                                 if (e.key == "Down") pick(index + 1);
                                 else if (e.key == "Up") pick(index - 1);
                             }));
        }
        options.push_back(std::move(option));
    }
    items.push_back(h("box", {{"direction", p.get<bool>("row") ? "row" : "column"}, {"gap", p.get<bool>("row") ? 2 : 0}},
                      std::move(options)));
    return withError(h("box", {}, std::move(items)), p, t);
}

Element Slider(const Props& p) {
    const Theme& t = useTheme();
    int value = p.get<int>("value"), lo = p.get<int>("min"), hi = std::max(p.get<int>("max", 100), lo + 1);
    int step = std::max(p.get<int>("step", 1), 1);
    auto onChange = p.get<InputHandler>("onChange");
    auto to = [onChange, lo, hi](int v) {
        if (onChange) onChange(std::to_string(std::clamp(v, lo, hi)));
    };
    constexpr int kWidth = 20;
    int filled = (value - lo) * kWidth / (hi - lo);
    std::string bar;
    for (int i = 0; i <= kWidth; ++i) bar += i < filled ? "━" : i == filled ? "●" : "─";  // kWidth + 1 cells
    std::vector<Element> parts;
    std::string label = p.get<std::string>("label");
    if (!label.empty()) parts.push_back(h("text", {{"color", t.muted}, {"wrap", false}}, {label}));
    bool off = p.get<bool>("disabled");
    Element track = h("box", {}, {h("text", {{"color", off ? t.muted : t.accent}, {"dim", off}, {"wrap", false}}, {bar})});
    if (!off) {
        track.props.set("focusable", true);
        track.props.set("onKey", KeyHandler([to, value, step, lo, hi](const KeyEvent& e) {
                            if (e.key == "Right") to(value + step);
                            else if (e.key == "Left") to(value - step);
                            else if (e.key == "Home") to(lo);
                            else if (e.key == "End") to(hi);
                        }));
        track.props.set("onPointerDown", PointerHandler([to, lo, hi](const PointerEvent& e) {  // a click on the bar jumps there
                            to(lo + static_cast<int>(e.localX) * (hi - lo) / kWidth);
                        }));
    }
    parts.push_back(std::move(track));
    if (p.get<bool>("showValue", true)) parts.push_back(h("text", {{"color", t.text}, {"wrap", false}}, {std::to_string(value)}));
    return h("box", {{"direction", "row"}, {"gap", 1}}, std::move(parts));
}

Element Progress(const Props& p) {
    const Theme& t = useTheme();
    int value = p.get<int>("value"), max = std::max(p.get<int>("max", 100), 1);
    int percent = value * 100 / max;
    std::string variant = p.get<std::string>("variant", "primary");
    const std::string& color = variant == "success" ? t.success : variant == "danger" ? t.danger : t.accent;
    constexpr int kWidth = 24;
    int filled = value * kWidth / max;
    std::string bar;
    for (int i = 0; i < kWidth; ++i) bar += i < filled ? "█" : "░";
    std::vector<Element> parts;
    std::string label = p.get<std::string>("label");
    if (!label.empty()) parts.push_back(h("text", {{"color", t.muted}, {"wrap", false}}, {label}));
    parts.push_back(h("text", {{"color", color}, {"wrap", false}}, {bar}));
    parts.push_back(h("text", {{"color", t.text}, {"wrap", false}}, {std::to_string(percent) + "%"}));
    return h("box", {{"direction", "row"}, {"gap", 1}}, std::move(parts));
}

// The text shows under the children while the mouse is over them.
Element Tooltip(const Props& p) {
    auto [shown, setShown] = useState(false);
    std::vector<Element> parts = p.children();
    if (shown)
        parts.push_back(h("text", {{"position", "absolute"}, {"top", 1}, {"left", 0}, {"wrap", false},
                                   {"color", "black"}, {"bg", "gray"}},
                          {" " + p.get<std::string>("text") + " "}));
    return h("box", {{"onMouseEnter", [setShown] { setShown(true); }},
                     {"onMouseLeave", [setShown] { setShown(false); }}},
             std::move(parts));
}

namespace {

struct ToastViewProps {
    detail::ToastItem item{};
    std::string key{};
    bool operator==(const ToastViewProps&) const = default;
};

Element ToastView(const ToastViewProps& p) {
    const Theme& t = useTheme();
    int id = p.item.id;
    useTimeout([id] { detail::dismissToast(id); },
               p.item.duration.count() > 0 ? std::optional(p.item.duration) : std::nullopt);
    const std::string& v = p.item.variant;
    std::string color = v == "primary" ? t.accent : v == "danger" ? t.danger : v == "success" ? t.success : t.muted;
    return h("box", {{"border", true}, {"opaque", true}, {"color", color}, {"bg", t.surface}, {"paddingX", 1},
                     {"onClick", [id] { detail::dismissToast(id); }}},
             {h("text", {{"color", t.text}}, {p.item.text})});
}

}  // namespace

// Bottom right of the screen, over everything else.
Element Toaster(const Props&) {
    const auto& list = useStore(detail::toasts());
    if (list.empty()) return fragment({});
    std::vector<Element> items;
    for (const auto& t : list) items.push_back(h(ToastView, {.item = t, .key = std::to_string(t.id)}));
    return h("box", {{"position", "fixed"}, {"bottom", 0}, {"right", 1}, {"maxWidth", "60%"}, {"alignItems", "end"}},
             std::move(items));
}

}  // namespace rx::ui

namespace rx::ui::detail {

Element TableView(const TableProps& p) {
    const Theme& t = useTheme();
    // A cell: `width` cells wide, or a share of the room left; text is cut, not wrapped.
    auto cell = [](const TableColumn& c, Element content, const std::string& color, bool bold) {
        if (content.kind == ElementKind::Text)
            content = h("text", {{"color", color}, {"bold", bold}, {"wrap", false}}, {std::move(content)});
        Element box = h("box", {{"direction", "row"}, {"overflow", "hidden"},
                                {"justifyContent", c.align == Align::End      ? "end"
                                                   : c.align == Align::Center ? "center"
                                                                              : "start"}},
                        {std::move(content)});
        if (c.width > 0) {
            box.props.set("width", std::int64_t{c.width});
            box.props.set("flexShrink", std::int64_t{0});
        } else {
            box.props.set("flexGrow", std::int64_t{1});
            box.props.set("flexBasis", std::int64_t{0});
            box.props.set("minWidth", std::int64_t{0});
        }
        return box;
    };
    std::vector<Element> head;
    for (std::size_t i = 0; i < p.columns.size(); ++i) {
        const auto& c = p.columns[i];
        bool sorted = static_cast<int>(i) == p.sortColumn;
        std::string label = c.title + (sorted ? (p.sortDescending ? " ▼" : " ▲") : "");
        Element content = label;
        if (c.sortable && p.onSort)
            content = h("box", {{"focusable", true}, {"onClick", [onSort = p.onSort, i] { onSort(static_cast<int>(i)); }}},
                        {h("text", {{"color", sorted ? t.accent : t.muted}, {"bold", true}, {"underline", true}, {"wrap", false}},
                           {label})});
        head.push_back(cell(c, std::move(content), t.muted, true));
    }
    std::vector<Element> rows;
    rows.reserve(p.rows.size());
    for (std::size_t r = 0; r < p.rows.size(); ++r) {
        const auto& row = p.rows[r];
        const std::string& color = row.selected ? t.accent : t.text;
        std::vector<Element> cells;
        for (std::size_t i = 0; i < p.columns.size(); ++i)
            cells.push_back(cell(p.columns[i], i < row.cells.size() ? row.cells[i] : Element(""), color, row.selected));
        Element e = h("box", {{"direction", "row"}, {"gap", 1}}, std::move(cells));
        if (row.onSelect) {
            e.props.set("focusable", true);
            e.props.set("scrollIntoView", row.selected);
            e.props.set("onClick", row.onSelect);
        }
        e.key = row.key.empty() ? std::to_string(r) : row.key;
        rows.push_back(std::move(e));
    }
    std::vector<Element> parts;
    parts.push_back(h("box", {{"direction", "row"}, {"gap", 1}, {"flexShrink", 0}}, std::move(head)));
    if (p.rows.empty() && !p.emptyText.empty())
        parts.push_back(h("text", {{"color", t.muted}, {"dim", true}}, {p.emptyText}));
    else if (p.list)
        parts.push_back(p.list->view(std::move(rows)));
    else
        parts.push_back(h("box", {}, std::move(rows)));
    // With a virtual list the table takes the room left in its column, like a scroll box.
    return h("box", {{"border", true}, {"color", t.muted}, {"paddingX", 1}, {"flexShrink", 1}, {"minHeight", 0},
                     {"flexGrow", std::int64_t{p.list ? 1 : 0}}},
             std::move(parts));
}

}  // namespace rx::ui::detail
