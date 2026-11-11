// rx::ui for the browser: the same components as ui_terminal.cpp, rendered as
// HTML with rx-* classes. The stylesheet is added to the page on first use.
#include <emscripten.h>

#include <algorithm>
#include <cstdint>
#include <string>
#include <utility>

#include "reactx/dom.hpp"
#include "reactx/hooks.hpp"
#include "reactx/store.hpp"
#include "reactx/ui.hpp"

namespace {

// ---- stylesheet ----

// Text colors keep 4.5:1 (WCAG AA) on white and on dark surfaces, and so does
// the --rx-on-accent text of a button. The terminal color names a Theme uses
// ("cyan", "bright-red") are --rx-c-* variables with a light and a dark value.
constexpr const char* kCss = R"css(
:root {
  --rx-c-black: #111827; --rx-c-red: #b91c1c; --rx-c-green: #15803d; --rx-c-yellow: #a16207; --rx-c-blue: #2563eb;
  --rx-c-magenta: #a21caf; --rx-c-cyan: #0e7490; --rx-c-gray: #6b7280; --rx-c-bright-red: #dc2626;
  --rx-c-bright-green: #166534; --rx-c-bright-yellow: #854d0e; --rx-c-bright-blue: #1d4ed8;
  --rx-c-bright-magenta: #86198f; --rx-c-bright-cyan: #155e75;
  --rx-accent: #2563eb; --rx-danger: #dc2626; --rx-success: #15803d; --rx-warning: #ca8a04;
  --rx-muted: #6b7280; --rx-border: #d1d5db; --rx-surface: #ffffff; --rx-hover: rgb(0 0 0 / 0.04);
  --rx-on-accent: #ffffff; --rx-radius: 10px;
}
@media (prefers-color-scheme: dark) {
  :root { --rx-c-black: #e5e7eb; --rx-c-red: #f87171; --rx-c-green: #4ade80; --rx-c-yellow: #facc15; --rx-c-blue: #60a5fa;
          --rx-c-magenta: #e879f9; --rx-c-cyan: #22d3ee; --rx-c-gray: #9ca3af; --rx-c-bright-red: #fca5a5;
          --rx-c-bright-green: #86efac; --rx-c-bright-yellow: #fde047; --rx-c-bright-blue: #93c5fd;
          --rx-c-bright-magenta: #f0abfc; --rx-c-bright-cyan: #67e8f9;
          --rx-accent: #60a5fa; --rx-danger: #f87171; --rx-success: #4ade80; --rx-on-accent: #0b1220;
          --rx-border: #374151; --rx-surface: #1f2430; --rx-muted: #9ca3af; --rx-hover: rgb(255 255 255 / 0.06); }
}
.rx-theme { display: contents; }

.rx-btn {
  font: inherit; padding: 8px 14px; border-radius: var(--rx-radius); cursor: pointer;
  border: 1px solid var(--rx-border); background: transparent; color: inherit;
  transition: background-color .15s, border-color .15s;
}
.rx-btn:hover:not(:disabled) { border-color: var(--rx-accent); }
.rx-btn:focus-visible { outline: 2px solid var(--rx-accent); outline-offset: 2px; }
.rx-btn--primary { background: var(--rx-accent); border-color: var(--rx-accent); color: var(--rx-on-accent); }
.rx-btn--success { background: var(--rx-success); border-color: var(--rx-success); color: var(--rx-on-accent); }
.rx-btn--danger { color: var(--rx-danger); }
.rx-btn--danger:hover:not(:disabled) { border-color: var(--rx-danger); }
.rx-btn:disabled { opacity: .45; cursor: not-allowed; }

.rx-field { display: flex; flex-wrap: wrap; align-items: center; gap: 8px; }
.rx-field__error { flex-basis: 100%; margin: 0; color: var(--rx-danger); font-size: .85em; }
.rx-input[aria-invalid="true"] { border-color: var(--rx-danger); }
.rx-input:disabled, .rx-check--disabled, .rx-radios:disabled, .rx-slider input:disabled { opacity: .5; cursor: not-allowed; }
.rx-textarea { resize: vertical; line-height: 1.45; }
.rx-radios { border: 0; margin: 0; padding: 0; min-width: 0; display: flex; flex-direction: column; gap: 6px; }
.rx-radios--row { flex-direction: row; flex-wrap: wrap; gap: 6px 18px; }
.rx-radios > legend { padding: 0; margin-bottom: 4px; color: var(--rx-muted); }
.rx-radios--row > legend { flex-basis: 100%; }
.rx-radio { display: inline-flex; align-items: center; gap: 8px; cursor: pointer; }
.rx-radio input { width: 18px; height: 18px; margin: 0; accent-color: var(--rx-accent); }
.rx-slider input { flex: 1; min-width: 8em; accent-color: var(--rx-accent); }
.rx-slider__value { min-width: 3ch; text-align: right; font-variant-numeric: tabular-nums; }
.rx-progress { display: flex; flex-direction: column; gap: 6px; }
.rx-progress__head { display: flex; justify-content: space-between; gap: 12px; color: var(--rx-muted); font-size: .9em; }
.rx-progress__track { height: 8px; border-radius: 999px; background: var(--rx-hover); overflow: hidden; }
.rx-progress__bar { height: 100%; border-radius: inherit; background: var(--rx-accent); transition: width .25s ease; }
.rx-progress--success .rx-progress__bar { background: var(--rx-success); }
.rx-progress--danger .rx-progress__bar { background: var(--rx-danger); }
.rx-field__label { color: var(--rx-muted); white-space: nowrap; }
.rx-stack--row > .rx-field--input { flex: 1 1 10em; min-width: 0; }
.rx-field--above { flex-direction: column; flex-wrap: nowrap; align-items: stretch; gap: 4px; }
.rx-field--above > .rx-input { flex: none; }  /* full width, its own height */
.rx-vlist { overflow-y: auto; overscroll-behavior: contain; }
.rx-vlist--border { border: 1px solid var(--rx-border); border-radius: var(--rx-radius); }
.rx-vlist__row { overflow: hidden; }
.rx-input {
  flex: 1; min-width: 0; font: inherit; line-height: 1.5; padding: 8px 12px; border-radius: var(--rx-radius);
  border: 1px solid var(--rx-border); background: transparent; color: inherit;
}
.rx-input:focus { outline: 2px solid var(--rx-accent); outline-offset: 1px; }

.rx-check { display: inline-flex; align-items: center; gap: 8px; cursor: pointer; user-select: none; }
.rx-check input { width: 18px; height: 18px; margin: 0; accent-color: var(--rx-accent); }

.rx-tabs__list { display: flex; flex-wrap: wrap; gap: 4px; border-bottom: 1px solid var(--rx-border); margin-bottom: 16px; }
.rx-tab {
  font: inherit; background: none; border: none; border-bottom: 2px solid transparent; margin-bottom: -1px;
  padding: 8px 12px; color: var(--rx-muted); cursor: pointer; white-space: nowrap;
}
.rx-tab:hover { color: inherit; }
.rx-tab--active { color: var(--rx-accent); border-bottom-color: var(--rx-accent); font-weight: 600; }
.rx-tab:focus-visible { outline: 2px solid var(--rx-accent); outline-offset: -2px; border-radius: 6px; }
.rx-tabs__panel { display: flex; flex-direction: column; gap: 12px; }

.rx-modal {
  position: fixed; inset: 0; z-index: 1000; display: grid; place-items: center; padding: 16px;
  background: rgb(0 0 0 / 0.45); animation: rx-fade .15s ease-out;
}
.rx-modal__dialog {
  width: min(100%, var(--rx-modal-width, 420px)); background: var(--rx-surface); color: inherit;
  border-radius: 14px; padding: 20px 24px; box-shadow: 0 20px 50px rgb(0 0 0 / 0.3);
  display: flex; flex-direction: column; gap: 14px;
}
.rx-modal__title { margin: 0; font-size: 1.15rem; }
@keyframes rx-fade { from { opacity: 0; } }

.rx-list { border: 1px solid var(--rx-border); border-radius: var(--rx-radius); overflow: hidden; }
.rx-list__title { padding: 8px 12px; font-size: .85rem; color: var(--rx-muted); border-bottom: 1px solid var(--rx-border); }
.rx-list__items { list-style: none; margin: 0; padding: 0; }
.rx-list__items > li + li { border-top: 1px solid var(--rx-border); }
.rx-list--scroll { display: flex; flex-direction: column; min-height: 0; max-height: 100%; }
.rx-list--scroll > .rx-list__items { overflow-y: auto; min-height: 0; }
.rx-list__empty { margin: 0; padding: 14px 12px; color: var(--rx-muted); text-align: center; }
.rx-item {
  width: 100%; display: flex; justify-content: space-between; align-items: center; gap: 12px;
  font: inherit; text-align: left; padding: 10px 12px; background: none; border: none; color: inherit; cursor: pointer;
}
.rx-item:hover { background: var(--rx-hover); }
.rx-item:focus-visible { outline: 2px solid var(--rx-accent); outline-offset: -2px; }
.rx-item--selected { color: var(--rx-accent); font-weight: 600; }
.rx-item__detail { color: var(--rx-muted); font-weight: normal; white-space: nowrap; }

.rx-text { margin: 0; white-space: pre-line; }
.rx-text--muted { color: var(--rx-muted); }

.rx-card { border: 1px solid var(--rx-border); border-radius: 14px; padding: 16px 18px; background: var(--rx-surface);
           display: flex; flex-direction: column; gap: 12px; }
.rx-card__title { margin: 0; font-size: 1rem; }

.rx-select { cursor: pointer; box-sizing: border-box; height: calc(1.5em + 18px); }  /* as tall as .rx-input */

.rx-tip { position: relative; display: inline-flex; }
.rx-tip__bubble {
  position: absolute; bottom: calc(100% + 6px); left: 50%; transform: translateX(-50%); z-index: 1001;
  background: #111827; color: #f9fafb; font-size: .82rem; line-height: 1.35; padding: 5px 9px; border-radius: 7px;
  white-space: nowrap; pointer-events: none; box-shadow: 0 4px 14px rgb(0 0 0 / 0.25); animation: rx-fade .12s ease-out;
}

.rx-toaster { position: fixed; right: 16px; bottom: 16px; z-index: 1002; display: flex; flex-direction: column;
              align-items: flex-end; gap: 8px; max-width: min(360px, calc(100vw - 32px)); }
.rx-toast {
  font: inherit; text-align: left; cursor: pointer; padding: 10px 14px; border-radius: var(--rx-radius);
  border: 1px solid var(--rx-border); border-left: 4px solid var(--rx-muted); background: var(--rx-surface); color: inherit;
  box-shadow: 0 8px 24px rgb(0 0 0 / 0.18); animation: rx-toast-in .18s ease-out;
}
.rx-toast--primary { border-left-color: var(--rx-accent); }
.rx-toast--success { border-left-color: var(--rx-success); }
.rx-toast--danger { border-left-color: var(--rx-danger); }
.rx-toast:focus-visible { outline: 2px solid var(--rx-accent); outline-offset: 2px; }
.rx-table { display: flex; flex-direction: column; min-height: 0; width: 100%; max-width: 100%; box-sizing: border-box;
            border: 1px solid var(--rx-border); border-radius: var(--rx-radius); overflow-x: auto; overflow-y: hidden; }
.rx-table > * { min-width: var(--rx-table-min); }
.rx-table__head, .rx-table__row { display: grid; grid-template-columns: var(--rx-table-cols); align-items: center; }
.rx-table__head { color: var(--rx-muted); border-bottom: 1px solid var(--rx-border); overflow-y: hidden; scrollbar-gutter: stable; }
.rx-table__head > * { font-size: .85rem; font-weight: 600; }  /* on the cells: the columns are in ch of the table's font */
.rx-table__body, .rx-table > .rx-vlist { overflow-y: auto; min-height: 0; scrollbar-gutter: stable; }
.rx-table__row { box-sizing: border-box; height: 100%; border-bottom: 1px solid var(--rx-hover); }
.rx-table__row--click { cursor: pointer; }
.rx-table__row--click:hover { background: var(--rx-hover); }
.rx-table__row:focus-visible { outline: 2px solid var(--rx-accent); outline-offset: -2px; }
.rx-table__row--selected { color: var(--rx-accent); font-weight: 600; }
.rx-table__cell { padding: 7px 12px; min-width: 0; overflow: hidden; text-overflow: ellipsis; white-space: nowrap; }
.rx-table__cell--end { text-align: right; }
.rx-table__cell--center { text-align: center; }
.rx-table__sort { font: inherit; color: inherit; background: none; border: none; padding: 0; cursor: pointer; }
.rx-table__sort:focus-visible { outline: 2px solid var(--rx-accent); outline-offset: 2px; }
.rx-table__empty { margin: 0; padding: 14px 12px; color: var(--rx-muted); text-align: center; }
.rx-table > .rx-vlist { border: none; }
@keyframes rx-toast-in { from { opacity: 0; transform: translateY(8px); } }
)css";

// Returns 0 without a page (prerendering under Node): the CSS then goes into the prerendered <head>.
EM_JS(int, rx_ui_install_styles, (const char* css), {
    if (typeof document === "undefined") return 0;
    if (document.getElementById("rx-ui-styles")) return 1;
    const style = document.createElement("style");
    style.id = "rx-ui-styles";
    style.textContent = UTF8ToString(css);
    document.head.appendChild(style);
    return 1;
});

void ensureStyles() {
    static bool done = false;
    if (!done) {
        if (!rx_ui_install_styles(kCss)) rx::detail::addPrerenderHead(std::string("<style id=\"rx-ui-styles\">") + kCss + "</style>");
        done = true;
    }
}

// A field with an error is marked for screen readers and drawn red; the message goes under it.
// Also passes `disabled` on (a native attribute: not editable, not focusable).
rx::Element invalid(rx::Element field, const rx::Props& p) {
    if (!p.get<std::string>("error").empty()) field.props.set("aria-invalid", "true");
    if (p.get<bool>("disabled")) field.props.set("disabled", true);
    return field;
}
void addError(std::vector<rx::Element>& parts, const rx::Props& p) {
    std::string error = p.get<std::string>("error");
    if (!error.empty()) parts.push_back(rx::h("span", {{"className", "rx-field__error"}, {"role", "alert"}}, {error}));
}

// Terminal color names (used by ui::Theme) as CSS colors. Hex and other CSS
// values pass through; "white" and "" mean "keep the page default".
// A Theme color for CSS: a terminal color name becomes its --rx-c-* variable
// (light and dark values in the stylesheet), anything else ("#ff8800") is used as is.
std::string cssColor(const std::string& name) {
    static constexpr std::string_view palette[] = {
        "black", "red", "green", "yellow", "blue", "magenta", "cyan", "gray",
        "bright-red", "bright-green", "bright-yellow", "bright-blue", "bright-magenta", "bright-cyan",
    };
    if (name.empty() || name == "white" || name == "bright-white") return {};
    if (name == "grey") return "var(--rx-c-gray)";
    for (std::string_view n : palette)
        if (name == n) return "var(--rx-c-" + name + ")";
    return name;
}



}  // namespace

namespace rx::ui {

Element themeProvider(Theme theme, std::vector<Element> children) {
    ensureStyles();
    std::string vars;
    // Only the colors the app changed: the defaults stay in the stylesheet,
    // which has a light and a dark version of each.
    const Theme defaults;
    auto add = [&](const char* var, const std::string& value, const std::string& standard) {
        if (value == standard) return;
        if (auto css = cssColor(value); !css.empty()) vars += std::string(var) + ": " + css + "; ";
    };
    add("--rx-accent", theme.accent, {});  // always: Theme's accent ("cyan") differs from the stylesheet's
    add("--rx-danger", theme.danger, defaults.danger);
    add("--rx-success", theme.success, defaults.success);
    add("--rx-warning", theme.warning, defaults.warning);
    add("--rx-muted", theme.muted, defaults.muted);
    add("--rx-surface", theme.surface, defaults.surface);
    if (!vars.empty()) vars.pop_back();
    return ThemeContext.provide(std::move(theme), {h("div", {{"className", "rx-theme"}, {"style", vars}}, std::move(children))});
}

const Theme& useTheme() { return useContext(ThemeContext); }

// ---- components ----

Element Button(const Props& p) {
    ensureStyles();
    std::string variant = p.get<std::string>("variant", "default");
    bool disabled = p.get<bool>("disabled");
    return h("button", {{"type", "button"},
                        {"className", "rx-btn rx-btn--" + variant},
                        {"disabled", disabled},
                        {"onClick", disabled ? Callback{} : p.get<Callback>("onClick")}},
             {p.get<std::string>("label")});
}

Element Input(const Props& p) {
    ensureStyles();
    std::string value = p.get<std::string>("value");
    auto onChange = p.get<InputHandler>("onChange");
    auto onSubmit = p.get<InputHandler>("onSubmit");
    std::string label = p.get<std::string>("label");

    std::vector<Element> parts;
    if (!label.empty()) parts.push_back(h("span", {{"className", "rx-field__label"}}, {label}));
    parts.push_back(invalid(h("input", {
        {"className", "rx-input"},
        {"type", p.get<std::string>("type", "text")},
        {"value", value},
        {"placeholder", p.get<std::string>("placeholder")},
        {"onInput", onChange},
        {"onKey", [onChange, onSubmit, value](const KeyEvent& e) {
             if (e.key == "Enter" && onSubmit) onSubmit(value);
             if (e.key == "Escape" && onChange && !value.empty()) onChange(std::string{});
         }},
    }), p));
    addError(parts, p);
    return h("label", {{"className", p.get<bool>("labelAbove") ? "rx-field rx-field--input rx-field--above" : "rx-field rx-field--input"}},
             std::move(parts));
}

Element Checkbox(const Props& p) {
    ensureStyles();
    Element box = h("label", {{"className", p.get<bool>("disabled") ? "rx-check rx-check--disabled" : "rx-check"}}, {
        invalid(h("input", {{"type", "checkbox"}, {"checked", p.get<bool>("checked")}, {"onChange", p.get<Callback>("onToggle")}}), p),
        h("span", {}, {p.get<std::string>("label")}),
    });
    if (p.get<std::string>("error").empty()) return box;
    std::vector<Element> parts{std::move(box)};
    addError(parts, p);
    return h("div", {{"className", "rx-field rx-field--above"}}, std::move(parts));
}

Element Tabs(const Props& p) {
    ensureStyles();
    const auto& tabs = p.children();
    auto [own, setOwn] = useState(p.get<int>("initial", 0));
    if (tabs.empty()) return fragment({});
    const int controlled = p.get<int>("active", -1);  // >= 0: the app keeps the open tab
    int current = std::clamp(controlled >= 0 ? controlled : own, 0, static_cast<int>(tabs.size()) - 1);
    auto onChange = p.get<InputHandler>("onChange");
    auto pick = [setOwn, onChange](int index) {
        setOwn(index);
        if (onChange) onChange(std::to_string(index));
    };

    std::vector<Element> header;
    for (std::size_t i = 0; i < tabs.size(); ++i) {
        int index = static_cast<int>(i);
        bool on = index == current;
        std::string label = tabs[i].props.get<std::string>("label");
        header.push_back(h("button", {{"key", label},
                                      {"type", "button"},
                                      {"role", "tab"},
                                      {"aria-selected", on ? "true" : "false"},
                                      {"className", on ? "rx-tab rx-tab--active" : "rx-tab"},
                                      {"onClick", [pick, index] { pick(index); }}},
                           {label}));
    }
    const Element& tab = tabs[static_cast<std::size_t>(current)];
    return h("div", {{"className", "rx-tabs"}}, {
        h("div", {{"className", "rx-tabs__list"}, {"role", "tablist"}}, std::move(header)),
        // Keyed by label: switching tabs mounts fresh content.
        h("div", {{"className", "rx-tabs__panel"}, {"role", "tabpanel"}},
          {fragment(tab.props.children(), tab.props.get<std::string>("label"))}),
    });
}

Element Modal(const Props& p) {
    if (!p.get<bool>("open")) return fragment({});
    ensureStyles();
    auto onClose = p.get<Callback>("onClose");
    int width = p.get<int>("width");  // in characters, like the terminal
    std::string title = p.get<std::string>("title");

    std::vector<Element> body;
    if (!title.empty()) body.push_back(h("h2", {{"className", "rx-modal__title"}}, {title}));
    for (const Element& child : p.children()) body.push_back(child);

    return h("div", {
        {"className", "rx-modal"},
        {"data-rx-self", true},  // clicks inside the dialog don't close it
        {"data-rx-trap", true},  // Tab stays inside, focus comes back on close
        {"onClick", onClose},
        {"onKey", [onClose](const KeyEvent& e) {
             if (e.key == "Escape" && onClose) onClose();
         }},
    }, {
        h("div", {{"className", "rx-modal__dialog"},
                  {"role", "dialog"},
                  {"aria-modal", "true"},
                  {"aria-label", title},
                  {"style", width > 0 ? "--rx-modal-width: " + std::to_string(width + 6) + "ch" : std::string{}}},
          std::move(body)),
    });
}

Element List(const Props& p) {
    ensureStyles();
    std::string title = p.get<std::string>("title");
    std::vector<Element> parts;
    if (!title.empty()) parts.push_back(h("div", {{"className", "rx-list__title"}}, {title}));
    if (p.children().empty())
        parts.push_back(h("p", {{"className", "rx-list__empty"}}, {p.get<std::string>("emptyText")}));
    else
        parts.push_back(h("ul", {{"className", "rx-list__items"}}, p.children()));
    return h("div", {{"className", p.get<bool>("scroll") ? "rx-list rx-list--scroll" : "rx-list"}}, std::move(parts));
}

Element ListItem(const Props& p) {
    ensureStyles();
    bool selected = p.get<bool>("selected");
    std::string detail = p.get<std::string>("detail");
    std::vector<Element> parts;
    parts.push_back(h("span", {{"className", "rx-item__label"}}, {p.get<std::string>("label")}));
    if (!detail.empty()) parts.push_back(h("span", {{"className", "rx-item__detail"}}, {detail}));
    return h("li", {}, {
        h("button", {{"type", "button"},
                     {"scrollIntoView", selected},  // a newly selected item scrolls into view
                     {"className", selected ? "rx-item rx-item--selected" : "rx-item"},
                     {"aria-pressed", selected ? "true" : "false"},
                     {"onClick", p.get<Callback>("onSelect")},
                     {"onKey", p.get<KeyHandler>("onKey")}},
          std::move(parts)),
    });
}

Element Text(const Props& p) {
    ensureStyles();
    return h("p", {{"className", p.get<bool>("muted") ? "rx-text rx-text--muted" : "rx-text"}}, {p.get<std::string>("text")});
}

Element Card(const Props& p) {
    ensureStyles();
    std::string title = p.get<std::string>("title");
    std::vector<Element> parts;
    std::string tag = "h" + std::to_string(p.get<int>("level", 3));
    if (!title.empty()) parts.push_back(h(tag, {{"className", "rx-card__title"}}, {title}));
    for (const Element& child : p.children()) parts.push_back(child);
    return h("section", {{"className", "rx-card"}, {"aria-label", title}}, std::move(parts));
}

Element Stack(const Props& p) {
    std::string align = p.get<std::string>("align", "stretch");
    std::string justify = p.get<std::string>("justify", "start");
    auto flex = [](const std::string& v) { return v == "start" ? "flex-start" : v == "end" ? "flex-end" : v; };
    std::string style = "display: flex; flex-direction: " + p.get<std::string>("direction", "column") +
                        "; gap: " + std::to_string(p.get<int>("gap", 1) * 8) + "px; align-items: " + flex(align) +
                        "; justify-content: " + (justify == "between" ? std::string("space-between") : flex(justify)) +
                        (p.get<bool>("wrap") ? "; flex-wrap: wrap" : "");
    bool row = p.get<std::string>("direction", "column").starts_with("row");
    return h("div", {{"className", row ? "rx-stack rx-stack--row" : "rx-stack"}, {"style", style}}, p.children());
}

Element Select(const Props& p) {
    ensureStyles();
    std::string label = p.get<std::string>("label");
    std::vector<Element> options;
    for (const Element& o : p.children()) {
        std::string value = o.props.get<std::string>("value");
        options.push_back(h("option", {{"key", value}, {"value", value}}, {o.props.get<std::string>("label")}));
    }
    std::vector<Element> parts;
    if (!label.empty()) parts.push_back(h("span", {{"className", "rx-field__label"}}, {label}));
    parts.push_back(invalid(h("select", {{"className", "rx-input rx-select"},
                                         {"value", p.get<std::string>("value")},
                                         {"onChange", p.get<InputHandler>("onChange")}},
                              std::move(options)), p));
    addError(parts, p);
    return h("label", {{"className", p.get<bool>("labelAbove") ? "rx-field rx-field--above" : "rx-field"}}, std::move(parts));
}

Element Textarea(const Props& p) {
    ensureStyles();
    std::string label = p.get<std::string>("label");
    std::vector<Element> parts;
    if (!label.empty()) parts.push_back(h("span", {{"className", "rx-field__label"}}, {label}));
    parts.push_back(invalid(h("textarea", {{"className", "rx-input rx-textarea"},
                                           {"rows", p.get<int>("rows", 4)},
                                           {"value", p.get<std::string>("value")},
                                           {"placeholder", p.get<std::string>("placeholder")},
                                           {"onInput", p.get<InputHandler>("onChange")}}),
                            p));
    addError(parts, p);
    return h("label", {{"className", "rx-field rx-field--input rx-field--above"}}, std::move(parts));
}

Element RadioGroup(const Props& p) {
    ensureStyles();
    std::string name = "rx-radio-" + useId();  // one group: arrows move inside it
    std::string value = p.get<std::string>("value");
    auto onChange = p.get<InputHandler>("onChange");
    std::string label = p.get<std::string>("label");
    std::vector<Element> parts;
    if (!label.empty()) parts.push_back(h("legend", {}, {label}));
    for (const Element& o : p.children()) {
        std::string v = o.props.get<std::string>("value");
        parts.push_back(h("label", {{"key", v}, {"className", "rx-radio"}}, {
            h("input", {{"type", "radio"}, {"name", name}, {"value", v}, {"checked", v == value},
                        {"onChange", Callback([onChange, v] { if (onChange) onChange(v); })}}),
            h("span", {}, {o.props.get<std::string>("label")}),
        }));
    }
    addError(parts, p);
    Element group = h("fieldset", {{"className", p.get<bool>("row") ? "rx-radios rx-radios--row" : "rx-radios"}}, std::move(parts));
    if (!p.get<std::string>("error").empty()) group.props.set("aria-invalid", "true");
    if (p.get<bool>("disabled")) group.props.set("disabled", true);  // a disabled fieldset disables its radios
    return group;
}

Element Slider(const Props& p) {
    ensureStyles();
    std::string label = p.get<std::string>("label");
    int value = p.get<int>("value");
    std::vector<Element> parts;
    if (!label.empty()) parts.push_back(h("span", {{"className", "rx-field__label"}}, {label}));
    parts.push_back(invalid(h("input", {{"type", "range"},
                                        {"min", p.get<int>("min")},
                                        {"max", p.get<int>("max", 100)},
                                        {"step", p.get<int>("step", 1)},
                                        {"value", value},
                                        {"aria-label", label},
                                        {"onInput", p.get<InputHandler>("onChange")}}),
                            p));
    if (p.get<bool>("showValue", true)) parts.push_back(h("span", {{"className", "rx-slider__value"}}, {std::to_string(value)}));
    addError(parts, p);
    return h("label", {{"className", "rx-field rx-slider"}}, std::move(parts));
}

Element Progress(const Props& p) {
    ensureStyles();
    int value = p.get<int>("value"), max = std::max(p.get<int>("max", 100), 1);
    int percent = value * 100 / max;
    std::string label = p.get<std::string>("label");
    return h("div", {{"className", "rx-progress rx-progress--" + p.get<std::string>("variant", "primary")}}, {
        h("div", {{"className", "rx-progress__head"}}, {h("span", {}, {label}), h("span", {}, {std::to_string(percent) + "%"})}),
        h("div", {{"className", "rx-progress__track"}, {"role", "progressbar"}, {"aria-label", label.empty() ? std::string("Progress") : label},
                  {"aria-valuemin", "0"}, {"aria-valuemax", std::to_string(max)}, {"aria-valuenow", std::to_string(value)}},
          {h("div", {{"className", "rx-progress__bar"}, {"style", "width: " + std::to_string(percent) + "%"}})}),
    });
}

Element Tooltip(const Props& p) {
    ensureStyles();
    auto [shown, setShown] = useState(false);
    std::string id = useId();
    std::vector<Element> parts = p.children();
    if (shown) parts.push_back(h("span", {{"className", "rx-tip__bubble"}, {"role", "tooltip"}, {"id", id}}, {p.get<std::string>("text")}));
    return h("span", {{"className", "rx-tip"},
                      {"aria-describedby", shown ? id : std::string{}},
                      {"onMouseEnter", [setShown] { setShown(true); }},
                      {"onMouseLeave", [setShown] { setShown(false); }},
                      {"onFocus", [setShown] { setShown(true); }},
                      {"onBlur", [setShown] { setShown(false); }}},
             std::move(parts));
}

namespace {

struct ToastViewProps {
    detail::ToastItem item{};
    std::string key{};
    bool operator==(const ToastViewProps&) const = default;
};

Element ToastView(const ToastViewProps& p) {
    int id = p.item.id;
    useTimeout([id] { detail::dismissToast(id); },
               p.item.duration.count() > 0 ? std::optional(p.item.duration) : std::nullopt);
    return h("button", {{"type", "button"},
                        {"className", "rx-toast rx-toast--" + p.item.variant},
                        {"title", "Close"},
                        {"onClick", [id] { detail::dismissToast(id); }}},
             {p.item.text});
}

}  // namespace

Element Toaster(const Props&) {
    ensureStyles();
    const auto& list = useStore(detail::toasts());
    std::vector<Element> items;
    for (const auto& t : list) items.push_back(h(ToastView, {.item = t, .key = std::to_string(t.id)}));
    // Always on the page, so screen readers hear what is added to it.
    return h("div", {{"className", "rx-toaster"}, {"role", "status"}, {"aria-live", "polite"}}, std::move(items));
}

}  // namespace rx::ui

namespace rx::ui::detail {

Element TableView(const TableProps& p) {
    ensureStyles();
    // Fixed columns get their characters plus the cell's padding (~3ch);
    // the others share the rest, at least 10ch each. Narrower than that (a
    // phone), the table scrolls sideways inside itself.
    std::string cols;
    int least = 2;  // the vertical scroll bar
    for (const auto& c : p.columns) {
        cols += (c.width > 0 ? std::to_string(c.width + 3) + "ch" : std::string("minmax(10ch, 1fr)")) + " ";
        least += c.width > 0 ? c.width + 3 : 10;
    }
    auto cellClass = [](const TableColumn& c) {
        return std::string(c.align == Align::End      ? "rx-table__cell rx-table__cell--end"
                           : c.align == Align::Center ? "rx-table__cell rx-table__cell--center"
                                                      : "rx-table__cell");
    };
    std::vector<Element> head;
    for (std::size_t i = 0; i < p.columns.size(); ++i) {
        const auto& c = p.columns[i];
        bool sorted = static_cast<int>(i) == p.sortColumn;
        std::string sort = !sorted ? "none" : p.sortDescending ? "descending" : "ascending";
        Element label = c.title + (sorted ? (p.sortDescending ? " ▼" : " ▲") : "");
        if (c.sortable && p.onSort)
            label = h("button", {{"type", "button"}, {"className", "rx-table__sort"},
                                 {"onClick", [onSort = p.onSort, i] { onSort(static_cast<int>(i)); }}},
                      {std::move(label)});
        Element cell = h("div", {{"role", "columnheader"}, {"className", cellClass(c)}}, {std::move(label)});
        if (c.sortable) cell.props.set("aria-sort", sort);
        head.push_back(std::move(cell));
    }
    std::vector<Element> rows;
    rows.reserve(p.rows.size());
    for (std::size_t r = 0; r < p.rows.size(); ++r) {
        const auto& row = p.rows[r];
        std::vector<Element> cells;
        for (std::size_t i = 0; i < p.columns.size(); ++i)
            cells.push_back(h("div", {{"role", "cell"}, {"className", cellClass(p.columns[i])}},
                              {i < row.cells.size() ? row.cells[i] : Element{}}));
        std::string cls = "rx-table__row";
        if (row.onSelect) cls += " rx-table__row--click";
        if (row.selected) cls += " rx-table__row--selected";
        Element e = h("div", {{"role", "row"}, {"className", cls}}, std::move(cells));
        if (row.selected) e.props.set("aria-selected", "true");
        if (row.onSelect) {
            e.props.set("tabindex", "0");
            e.props.set("data-rx-skip-controls", "");  // a checkbox or button in a cell doesn't select the row too
            e.props.set("onClick", row.onSelect);
            e.props.set("onKey", KeyHandler([select = row.onSelect](const KeyEvent& k) {
                            if (k.key == "Enter" || k.key == " ") select();
                        }));
        }
        e.key = row.key.empty() ? std::to_string(r) : row.key;
        rows.push_back(std::move(e));
    }
    std::vector<Element> parts;
    parts.push_back(h("div", {{"role", "row"}, {"className", "rx-table__head"}}, std::move(head)));
    if (p.rows.empty() && !p.emptyText.empty())
        parts.push_back(h("p", {{"className", "rx-table__empty"}}, {p.emptyText}));
    else if (p.list)
        parts.push_back(p.list->view(std::move(rows)));
    else
        parts.push_back(h("div", {{"role", "rowgroup"}, {"className", "rx-table__body"}}, std::move(rows)));
    return h("div", {{"role", "table"}, {"className", "rx-table"}, {"style", "--rx-table-cols: " + cols + "; --rx-table-min: " + std::to_string(least) + "ch"}}, std::move(parts));
}

}  // namespace rx::ui::detail
