// rx::ui for the browser: the same components as ui_terminal.cpp, rendered as
// HTML with rx-* classes. The stylesheet is added to the page on first use.
#include <emscripten.h>

#include <algorithm>
#include <cstdint>
#include <string>
#include <utility>

#include "reactx/hooks.hpp"
#include "reactx/ui.hpp"

namespace {

// ---- stylesheet ----

constexpr const char* kCss = R"css(
:root {
  --rx-accent: #2563eb; --rx-danger: #dc2626; --rx-success: #16a34a; --rx-warning: #ca8a04;
  --rx-muted: #6b7280; --rx-border: #d1d5db; --rx-surface: #ffffff; --rx-hover: rgb(0 0 0 / 0.04);
  --rx-on-accent: #ffffff; --rx-radius: 10px;
}
@media (prefers-color-scheme: dark) {
  :root { --rx-border: #374151; --rx-surface: #1f2430; --rx-muted: #9ca3af; --rx-hover: rgb(255 255 255 / 0.06); }
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

.rx-field { display: flex; align-items: center; gap: 8px; }
.rx-field__label { color: var(--rx-muted); white-space: nowrap; }
.rx-input {
  flex: 1; min-width: 0; font: inherit; padding: 8px 12px; border-radius: var(--rx-radius);
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
.rx-list__empty { margin: 0; padding: 14px 12px; color: var(--rx-muted); text-align: center; }
.rx-item {
  width: 100%; display: flex; justify-content: space-between; align-items: center; gap: 12px;
  font: inherit; text-align: left; padding: 10px 12px; background: none; border: none; color: inherit; cursor: pointer;
}
.rx-item:hover { background: var(--rx-hover); }
.rx-item:focus-visible { outline: 2px solid var(--rx-accent); outline-offset: -2px; }
.rx-item--selected { color: var(--rx-accent); font-weight: 600; }
.rx-item__detail { color: var(--rx-muted); font-weight: normal; white-space: nowrap; }
)css";

EM_JS(void, rx_ui_install_styles, (const char* css), {
    if (document.getElementById("rx-ui-styles")) return;
    const style = document.createElement("style");
    style.id = "rx-ui-styles";
    style.textContent = UTF8ToString(css);
    document.head.appendChild(style);
});

void ensureStyles() {
    static bool done = false;
    if (!done) {
        rx_ui_install_styles(kCss);
        done = true;
    }
}

// Terminal color names (used by ui::Theme) as CSS colors. Hex and other CSS
// values pass through; "white" and "" mean "keep the page default".
std::string cssColor(const std::string& name) {
    static const std::pair<const char*, const char*> palette[] = {
        {"black", "#111827"},         {"red", "#dc2626"},          {"green", "#16a34a"},
        {"yellow", "#ca8a04"},        {"blue", "#2563eb"},         {"magenta", "#c026d3"},
        {"cyan", "#0891b2"},          {"gray", "#6b7280"},         {"grey", "#6b7280"},
        {"bright-red", "#ef4444"},    {"bright-green", "#22c55e"}, {"bright-yellow", "#eab308"},
        {"bright-blue", "#3b82f6"},   {"bright-magenta", "#d946ef"}, {"bright-cyan", "#06b6d4"},
    };
    if (name.empty() || name == "white" || name == "bright-white") return {};
    for (auto [n, css] : palette)
        if (name == n) return css;
    return name;
}



}  // namespace

namespace rx::ui {

Element themeProvider(Theme theme, std::vector<Element> children) {
    ensureStyles();
    std::string vars;
    auto add = [&](const char* var, const std::string& value) {
        if (auto css = cssColor(value); !css.empty()) vars += std::string(var) + ": " + css + "; ";
    };
    add("--rx-accent", theme.accent);
    add("--rx-danger", theme.danger);
    add("--rx-success", theme.success);
    add("--rx-warning", theme.warning);
    add("--rx-muted", theme.muted);
    add("--rx-surface", theme.surface);
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
    parts.push_back(h("input", {
        {"className", "rx-input"},
        {"type", "text"},
        {"value", value},
        {"placeholder", p.get<std::string>("placeholder")},
        {"onInput", onChange},
        {"onKey", [onChange, onSubmit, value](const KeyEvent& e) {
             if (e.key == "Enter" && onSubmit) onSubmit(value);
             if (e.key == "Escape" && onChange && !value.empty()) onChange(std::string{});
         }},
    }));
    return h("label", {{"className", "rx-field"}}, std::move(parts));
}

Element Checkbox(const Props& p) {
    ensureStyles();
    return h("label", {{"className", "rx-check"}}, {
        h("input", {{"type", "checkbox"}, {"checked", p.get<bool>("checked")}, {"onChange", p.get<Callback>("onToggle")}}),
        h("span", {}, {p.get<std::string>("label")}),
    });
}

Element Tabs(const Props& p) {
    ensureStyles();
    const auto& tabs = p.children();
    auto [active, setActive] = useState(p.get<int>("initial", 0));
    if (tabs.empty()) return fragment({});
    int current = std::clamp(active, 0, static_cast<int>(tabs.size()) - 1);

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
                                      {"onClick", [setActive, index] { setActive(index); }}},
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
    return h("div", {{"className", "rx-list"}}, std::move(parts));
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
                     {"className", selected ? "rx-item rx-item--selected" : "rx-item"},
                     {"aria-pressed", selected ? "true" : "false"},
                     {"onClick", p.get<Callback>("onSelect")},
                     {"onKey", p.get<KeyHandler>("onKey")}},
          std::move(parts)),
    });
}

}  // namespace rx::ui
