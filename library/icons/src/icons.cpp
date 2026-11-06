// reactx::icons: the icon set (drawn for ReactX on a 24x24 grid: 2 px lines,
// round ends, no fill) and how each backend shows one.
#include "rx/icons.hpp"

#include <algorithm>
#include <map>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>

#include "reactx/dom.hpp"
#endif

namespace rx::icons {

namespace {

struct Glyph {
    std::string path;    // SVG path data, 24x24
    std::string symbol;  // terminal
};

std::map<std::string, Glyph, std::less<>>& glyphs() {
    static std::map<std::string, Glyph, std::less<>> g = [] {
        const std::string ring9 = "M12 3a9 9 0 1 0 0 18a9 9 0 1 0 0-18z";
        std::map<std::string, Glyph, std::less<>> m{
            // actions
            {"check", {"M5 12.5l4.5 4.5L19 7.5", "✓"}},
            {"x", {"M6 6l12 12M18 6L6 18", "✕"}},
            {"plus", {"M12 5v14M5 12h14", "+"}},
            {"minus", {"M5 12h14", "−"}},
            {"search", {"M10.5 4a6.5 6.5 0 1 0 0 13a6.5 6.5 0 1 0 0-13zM15.5 15.5L20 20", "⌕"}},
            {"edit", {"M4 20h4L19 9l-4-4L4 16v4zM13 7l4 4", "✎"}},
            {"trash", {"M4 7h16M9 7V4h6v3M6 7l1 13h10l1-13M10 11v6M14 11v6", "⌦"}},
            {"copy", {"M9 9h11v11H9zM5 15H4V4h11v1", "⎘"}},
            {"download", {"M12 4v11M7 10l5 5 5-5M4 20h16", "↓"}},
            {"upload", {"M12 16V5M7 10l5-5 5 5M4 20h16", "↑"}},
            {"send", {"M21 3L3 10.5l7 3 3 7.5zM21 3L10 13.5", "➤"}},
            {"refresh", {"M20 11a8 8 0 1 0-2.3 5.7M20 4v7h-7", "⟳"}},
            {"filter", {"M3 5h18l-7 8v6l-4 2v-8z", "▽"}},
            {"settings", {"M4 7h10M18 7h2M4 17h2M10 17h10M16 4v6M8 14v6", "⚙"}},
            {"more", {"M6 12h.01M12 12h.01M18 12h.01", "⋯"}},
            {"menu", {"M4 6h16M4 12h16M4 18h16", "☰"}},
            {"external", {"M14 4h6v6M20 4l-9 9M18 14v6H4V6h6", "↗"}},
            {"link",
             {"M10 14a4 4 0 0 0 5.7 0l3-3a4 4 0 0 0-5.7-5.7l-1 1M14 10a4 4 0 0 0-5.7 0l-3 3a4 4 0 0 0 5.7 5.7l1-1",
              "∞"}},
            {"loader", {"M12 3a9 9 0 1 0 9 9", "◌"}},
            // arrows
            {"arrow-left", {"M19 12H5M11 6l-6 6 6 6", "←"}},
            {"arrow-right", {"M5 12h14M13 6l6 6-6 6", "→"}},
            {"arrow-up", {"M12 19V5M6 11l6-6 6 6", "↑"}},
            {"arrow-down", {"M12 5v14M6 13l6 6 6-6", "↓"}},
            {"chevron-left", {"M15 6l-6 6 6 6", "‹"}},
            {"chevron-right", {"M9 6l6 6-6 6", "›"}},
            {"chevron-up", {"M6 15l6-6 6 6", "˄"}},
            {"chevron-down", {"M6 9l6 6 6-6", "˅"}},
            // status
            {"info", {ring9 + "M12 11v6M12 7.5h.01", "ℹ"}},
            {"warning", {"M12 4L2.5 20h19zM12 10v4M12 17h.01", "⚠"}},
            {"error", {ring9 + "M12 7.5v5.5M12 16.5h.01", "⊗"}},
            {"success", {ring9 + "M8 12.5l3 3 5-6", "✓"}},
            {"bell", {"M6 16v-5a6 6 0 0 1 12 0v5l2 2H4zM10 20a2 2 0 0 0 4 0", "♪"}},
            {"lock", {"M5 11h14v10H5zM8 11V8a4 4 0 0 1 8 0v3", "⊡"}},
            {"unlock", {"M5 11h14v10H5zM8 11V8a4 4 0 0 1 7.5-2", "⊟"}},
            {"eye", {"M2 12s3.6-6.5 10-6.5S22 12 22 12s-3.6 6.5-10 6.5S2 12 2 12zM12 9a3 3 0 1 0 0 6a3 3 0 1 0 0-6z", "◉"}},
            {"eye-off", {"M2 12s3.6-6.5 10-6.5S22 12 22 12s-3.6 6.5-10 6.5S2 12 2 12zM3 3l18 18", "◌"}},
            {"star", {"M12 3.5l2.6 5.3 5.9.9-4.3 4.1 1 5.8L12 16.9l-5.2 2.7 1-5.8-4.3-4.1 5.9-.9z", "★"}},
            {"heart", {"M12 20s-7-4.4-7-10a4 4 0 0 1 7-2.6A4 4 0 0 1 19 10c0 5.6-7 10-7 10z", "♥"}},
            // things
            {"home", {"M4 11l8-7 8 7M6 9.5V20h12V9.5M10 20v-5h4v5", "⌂"}},
            {"user", {"M12 4a4 4 0 1 0 0 8a4 4 0 1 0 0-8zM4 20c0-4 3.6-6 8-6s8 2 8 6", "☺"}},
            {"mail", {"M3 6h18v12H3zM3 7l9 6 9-6", "✉"}},
            {"calendar", {"M4 6h16v14H4zM4 10h16M8 3v4M16 3v4", "▦"}},
            {"clock", {ring9 + "M12 7v5l3 2", "◷"}},
            {"file", {"M6 3h8l4 4v14H6zM14 3v4h4", "▤"}},
            {"folder", {"M3 6h6l2 2h10v11H3z", "▭"}},
            {"image", {"M4 5h16v14H4zM4 16l5-5 4 4 2-2 5 5M15.5 9.5h.01", "▨"}},
            {"sun",
             {"M12 8a4 4 0 1 0 0 8a4 4 0 1 0 0-8zM12 2v2M12 20v2M4.9 4.9l1.4 1.4M17.7 17.7l1.4 1.4M2 12h2M20 12h2"
              "M4.9 19.1l1.4-1.4M17.7 6.3l1.4-1.4",
              "☀"}},
            {"moon", {"M20 14.5A8 8 0 0 1 9.5 4a8 8 0 1 0 10.5 10.5z", "☾"}},
            // media
            {"play", {"M7 4.5v15l12-7.5z", "▶"}},
            {"pause", {"M8 5v14M16 5v14", "‖"}},
            {"stop", {"M6 6h12v12H6z", "■"}},
        };
        return m;
    }();
    return g;
}

#ifdef __EMSCRIPTEN__
constexpr const char* kCss = R"css(
:where(.rx-icon) { display: inline-block; vertical-align: -0.125em; flex-shrink: 0; }
.rx-icon--spin { animation: rx-icon-spin 0.9s linear infinite; }
@keyframes rx-icon-spin { to { transform: rotate(360deg); } }
@media (prefers-reduced-motion: reduce) { .rx-icon--spin { animation-duration: 2.7s; } }
)css";

// clang-format off  (JavaScript: a formatter would split === into == =)
EM_JS(int, rx_icons_install_styles, (const char* css), {
    if (typeof document === "undefined") return 0;
    if (document.getElementById("rx-icons-styles")) return 1;
    const style = document.createElement("style");
    style.id = "rx-icons-styles";
    style.textContent = UTF8ToString(css);
    document.head.prepend(style);
    return 1;
});
// clang-format on

void ensureStyles() {
    static bool done = false;
    if (done) return;
    done = true;
    if (!rx_icons_install_styles(kCss))
        rx::detail::addPrerenderHead(std::string("<style id=\"rx-icons-styles\">") + kCss + "</style>");
}

// Palette names mean the same colors as rx::ui and rx-charts in the browser.
std::string cssColor(const std::string& c) {
    static constexpr std::string_view palette[] = {"red", "green", "blue", "yellow", "magenta", "cyan", "gray", "grey"};
    if (std::ranges::find(palette, c) != std::end(palette)) return "var(--rx-c-" + c + ", " + c + ")";
    return c;
}
#endif

}  // namespace

Element icon(IconProps p) {
    const auto& all = glyphs();
    auto it = all.find(p.name);
#ifdef __EMSCRIPTEN__
    ensureStyles();
    const std::string size = p.size > 0 ? std::to_string(p.size) : std::string("1em");
    // An unknown name: a dashed box, so the mistake shows.
    const std::string path = it != all.end() ? it->second.path : "M5 5h14v14H5z";
    Element svg = h("svg", {{"viewBox", "0 0 24 24"},
                            {"width", size},
                            {"height", size},
                            {"fill", "none"},
                            {"stroke", "currentColor"},
                            {"stroke-width", 2},
                            {"stroke-linecap", "round"},
                            {"stroke-linejoin", "round"},
                            {"className", p.spin ? "rx-icon rx-icon--spin" : "rx-icon"}},
                    {h("path", {{"d", path}, {"stroke-dasharray", it != all.end() ? "none" : "2 2"}})});
    if (p.label.empty()) {
        svg.props.set("aria-hidden", std::string("true"));
    } else {
        svg.props.set("role", std::string("img"));
        svg.props.set("aria-label", p.label);
    }
    if (!p.color.empty()) svg.props.set("style", "color:" + cssColor(p.color));
#else
    Element svg = h("text", {{"wrap", false}}, {it != all.end() ? it->second.symbol : std::string("?")});
    if (!p.color.empty()) svg.props.set("color", p.color);
#endif
    if (!p.key.empty()) svg.key = std::move(p.key);
    return svg;
}

std::vector<std::string> names() {
    std::vector<std::string> out;
    for (const auto& [name, glyph] : glyphs()) out.push_back(name);
    return out;
}

void add(std::string name, std::string path, std::string symbol) {
    glyphs().insert_or_assign(std::move(name), Glyph{std::move(path), std::move(symbol)});
}

}  // namespace rx::icons
