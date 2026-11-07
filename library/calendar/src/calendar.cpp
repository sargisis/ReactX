// reactx::calendar: one component for both backends. The browser gets
// buttons in a CSS grid; a terminal gets focusable cells (the focused one is
// drawn reversed, the picked one in blue).
#include "rx/calendar.hpp"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "rx/i18n.hpp"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>

#include "reactx/dom.hpp"
#endif

namespace rx::calendar {

namespace {

using namespace std::chrono;

#ifdef __EMSCRIPTEN__
constexpr const char* kCss = R"css(
.rx-cal { display: inline-flex; flex-direction: column; width: max-content; max-width: 100%; align-self: flex-start; gap: 6px; padding: 10px; border-radius: 12px;
  border: 1px solid var(--rx-border, #d4d4d8); background: var(--rx-surface, #fff); font-variant-numeric: tabular-nums; }
.rx-cal__head { display: flex; align-items: center; justify-content: space-between; gap: 8px; font-weight: 600; }
.rx-cal__nav { font: inherit; border: none; background: none; color: inherit; cursor: pointer; padding: 4px 10px; border-radius: 8px; }
.rx-cal__nav:hover { background: color-mix(in srgb, currentColor 10%, transparent); }
.rx-cal__grid { display: grid; grid-template-columns: repeat(7, 2.25em); gap: 2px; }
.rx-cal__wd { text-align: center; font-size: .75em; color: var(--rx-muted, #71717a); padding: 2px 0; }
.rx-cal__day { font: inherit; height: 2.25em; border: none; border-radius: 8px; background: none; color: inherit; cursor: pointer; }
.rx-cal__day:hover:not(:disabled) { background: color-mix(in srgb, currentColor 10%, transparent); }
.rx-cal__day--other { opacity: .4; }
.rx-cal__day--today { box-shadow: inset 0 0 0 1px var(--rx-accent, #2563eb); }
.rx-cal__day--picked { background: var(--rx-accent, #2563eb); color: #fff; }
.rx-cal__day:disabled { opacity: .25; cursor: default; }
.rx-cal__nav:focus-visible { outline: 2px solid var(--rx-accent, #2563eb); outline-offset: 1px; }
.rx-cal__grid { border-radius: 8px; }
.rx-cal__grid:focus { outline: none; }
.rx-cal__grid:focus-visible .rx-cal__day--focus { outline: 2px solid var(--rx-accent, #2563eb); outline-offset: 1px; }
.rx-datepicker { display: inline-flex; flex-direction: column; gap: 6px; }
.rx-datepicker__field { font: inherit; text-align: left; min-width: 11em; padding: 8px 12px; border-radius: 9px; cursor: pointer;
  border: 1px solid var(--rx-border, #d4d4d8); background: var(--rx-surface, #fff); color: inherit; }
.rx-datepicker__label { font-size: .875em; color: var(--rx-muted, #71717a); }
)css";

// clang-format off  (JavaScript: a formatter would split === into == =)
EM_JS(int, rx_cal_install_styles, (const char* css), {
    if (typeof document === "undefined") return 0;
    if (document.getElementById("rx-cal-styles")) return 1;
    const style = document.createElement("style");
    style.id = "rx-cal-styles";
    style.textContent = UTF8ToString(css);
    document.head.prepend(style);
    return 1;
});
EM_JS(int, rx_cal_today, (), {
    const d = new Date();
    return d.getFullYear() * 10000 + (d.getMonth() + 1) * 100 + d.getDate();
});
// clang-format on

void ensureStyles() {
    static bool done = false;
    if (done) return;
    done = true;
    if (!rx_cal_install_styles(kCss)) rx::detail::addPrerenderHead(std::string("<style id=\"rx-cal-styles\">") + kCss + "</style>");
}
#endif

std::string languageOf(const std::string& lang) {
    if (!lang.empty()) return lang;
    std::string current = i18n::language();
    return current.empty() ? std::string("en") : current;
}

bool allowed(Date d, const Date& min, const Date& max, const std::function<bool(Date)>& disabled) {
    if (min.ok() && sys_days(d) < sys_days(min)) return false;
    if (max.ok() && sys_days(d) > sys_days(max)) return false;
    return !(disabled && disabled(d));
}

Date plusDays(Date d, int n) { return Date(sys_days(d) + days(n)); }

// The first `n` characters (UTF-8 code points, not bytes: "пн" is 4 bytes).
[[maybe_unused]] std::string firstChars(const std::string& s, std::size_t n) {
    std::size_t i = 0, chars = 0;
    while (i < s.size() && chars < n) {
        ++i;
        while (i < s.size() && (static_cast<unsigned char>(s[i]) & 0xC0) == 0x80) ++i;
        ++chars;
    }
    return s.substr(0, i);
}

// The same day in the month `n` months away (the 31st becomes the last day).
Date plusMonths(Date d, int n) {
    const year_month ym = year_month(d.year(), d.month()) + months(n);
    const day last = year_month_day_last(ym.year(), month_day_last(ym.month())).day();
    return Date(ym.year(), ym.month(), std::min(d.day(), last));
}

struct State {
    Date focus{};  // the day the keyboard is on; it decides the month shown
    bool operator==(const State&) const = default;
};

Element Calendar(const CalendarProps& p) {
    (void)i18n::useLanguage();  // re-render when the language changes
    const std::string lang = languageOf(p.lang);
    const i18n::Locale& loc = i18n::locale(lang);
    const Date now = today();
    auto [state, setState] = useState(State{p.value.ok() ? p.value : now});
    const std::string uid = useId();
    const Date focus = state.focus;
    const year_month shown(focus.year(), focus.month());
#ifdef __EMSCRIPTEN__
    ensureStyles();
#endif

    auto pick = [p](Date d) {
        if (allowed(d, p.min, p.max, p.disabled) && p.onChange) p.onChange(d);
    };
    auto moveFocus = [setState](Date d) { setState(State{d}); };
    auto onKey = [focus, pick, moveFocus, first = loc.firstWeekday](const KeyEvent& k) {
        const int wd = static_cast<int>(weekday(sys_days(focus)).c_encoding());
        const int fromStart = (wd - first + 7) % 7;
        if (k.key == "Left") moveFocus(plusDays(focus, -1));
        else if (k.key == "Right") moveFocus(plusDays(focus, 1));
        else if (k.key == "Up") moveFocus(plusDays(focus, -7));
        else if (k.key == "Down") moveFocus(plusDays(focus, 7));
        else if (k.key == "PageUp") moveFocus(plusMonths(focus, -1));
        else if (k.key == "PageDown") moveFocus(plusMonths(focus, 1));
        else if (k.key == "Home") moveFocus(plusDays(focus, -fromStart));
        else if (k.key == "End") moveFocus(plusDays(focus, 6 - fromStart));
        else if (k.key == "Enter" || k.key == " ") pick(focus);
    };

    const std::string title = i18n::monthName(static_cast<unsigned>(shown.month()), false, lang) + " " +
                              std::to_string(static_cast<int>(shown.year()));
    auto nav = [&](const char* label, [[maybe_unused]] const char* aria, int delta) {
        return h(
#ifdef __EMSCRIPTEN__
            "button", {{"className", "rx-cal__nav"}, {"aria-label", aria},
#else
            "button", {
#endif
                       {"onClick", [moveFocus, focus, delta] { moveFocus(plusMonths(focus, delta)); }}},
            {label});
    };

    // The grid starts on the first day of the week on or before the 1st.
    const Date firstOfMonth(shown.year(), shown.month(), day(1));
    const int lead = (static_cast<int>(weekday(sys_days(firstOfMonth)).c_encoding()) - loc.firstWeekday + 7) % 7;
    const Date gridStart = plusDays(firstOfMonth, -lead);

    std::vector<Element> cells;
    for (int i = 0; i < 7; ++i) {
        const unsigned wd = static_cast<unsigned>((loc.firstWeekday + i) % 7);
        std::string name = i18n::weekdayName(wd, true, lang);
#ifdef __EMSCRIPTEN__
        cells.push_back(h("div", {{"className", "rx-cal__wd"}, {"key", "wd" + std::to_string(i)}, {"aria-hidden", "true"}}, {name}));
#else
        cells.push_back(h("text", {{"width", 4}, {"dim", true}, {"wrap", false}, {"key", "wd" + std::to_string(i)}}, {" " + firstChars(name, 2)}));
#endif
    }
    for (int i = 0; i < 42; ++i) {
        const Date d = plusDays(gridStart, i);
        const bool inMonth = d.month() == shown.month();
        const bool isPicked = p.value.ok() && d == p.value;
        const bool isToday = d == now;
        const bool isFocus = d == focus;
        const bool ok = allowed(d, p.min, p.max, p.disabled);
        const std::string n = std::to_string(static_cast<unsigned>(d.day()));
        const std::string key = std::to_string(i);
#ifdef __EMSCRIPTEN__
        std::string cls = "rx-cal__day";
        if (isFocus) cls += " rx-cal__day--focus";
        if (!inMonth) cls += " rx-cal__day--other";
        if (isToday) cls += " rx-cal__day--today";
        if (isPicked) cls += " rx-cal__day--picked";
        cells.push_back(h("button", {{"className", cls},
                                     {"key", key},
                                     {"id", uid + "-" + key},
                                     {"tabindex", -1},
                                     {"disabled", !ok},
                                     {"aria-pressed", isPicked ? "true" : "false"},
                                     {"aria-label", i18n::formatDate(d, i18n::DateStyle::Long, lang)},
                                     {"onClick", [pick, moveFocus, d] {
                                          moveFocus(d);
                                          pick(d);
                                      }}},
                          {n}));
#else
        // The keyboard's day in brackets, today with a dot.
        const std::string text = std::string(isFocus ? "[" : " ") + (n.size() == 1 ? " " : "") + n +
                                 (isFocus ? "]" : isToday ? "•" : " ");
        cells.push_back(h("box", {{"key", key},
                                  {"width", 4},
                                  {"onClick", [pick, moveFocus, d] {
                                       moveFocus(d);
                                       pick(d);
                                   }}},
                          {h("text", {{"wrap", false},
                                      {"dim", !inMonth || !ok},
                                      {"bold", isPicked},
                                      {"color", isPicked ? "blue" : ""}},
                             {text})}));
#endif
    }

#ifdef __EMSCRIPTEN__
    // The grid holds the keyboard focus; the arrows move the marked day
    // (aria-activedescendant), so there is one Tab stop for the whole month.
    const int focusIndex = static_cast<int>((sys_days(focus) - sys_days(gridStart)).count());
    return h("div", {{"className", "rx-cal"}, {"role", "group"}, {"aria-label", title}}, {
        h("div", {{"className", "rx-cal__head"}}, {nav("‹", "previous month", -1), h("span", {{"aria-live", "polite"}}, {title}),
                                                   nav("›", "next month", 1)}),
        h("div", {{"className", "rx-cal__grid"},
                  {"tabindex", 0},
                  {"aria-activedescendant", uid + "-" + std::to_string(focusIndex)},
                  {"aria-label", title},
                  {"onKey", onKey}},
          std::move(cells)),
    });
#else
    std::vector<Element> rows;
    rows.push_back(h("box", {{"direction", "row"}, {"key", "head"}, {"gap", 1}},
                     {nav("‹", "", -1), h("text", {{"bold", true}, {"wrap", false}}, {title}), nav("›", "", 1)}));
    for (int r = 0; r < 7; ++r) {
        std::vector<Element> row(std::make_move_iterator(cells.begin() + r * 7), std::make_move_iterator(cells.begin() + r * 7 + 7));
        rows.push_back(h("box", {{"direction", "row"}, {"key", "r" + std::to_string(r)}}, std::move(row)));
    }
    // arrowKeys: Up / Down move the day here instead of the focus.
    return h("box", {{"focusable", true}, {"arrowKeys", true}, {"onKey", onKey}}, std::move(rows));
#endif
}

Element DatePicker(const DatePickerProps& p) {
    (void)i18n::useLanguage();
    const std::string lang = languageOf(p.lang);
    auto [open, setOpen] = useState(false);
    const std::string shown = p.value.ok() ? i18n::formatDate(p.value, i18n::DateStyle::Medium, lang) : p.placeholder;
    auto onChange = p.onChange;
    Element cal = calendar({.value = p.value,
                            .onChange = [onChange, setOpen](Date d) {
                                setOpen(false);
                                if (onChange) onChange(d);
                            },
                            .min = p.min,
                            .max = p.max,
                            .disabled = p.disabled,
                            .lang = p.lang});
#ifdef __EMSCRIPTEN__
    ensureStyles();
    std::vector<Element> children;
    if (!p.label.empty()) children.push_back(h("span", {{"className", "rx-datepicker__label"}}, {p.label}));
    children.push_back(h("button", {{"className", "rx-datepicker__field"},
                                    {"aria-expanded", open ? "true" : "false"},
                                    {"aria-label", (p.label.empty() ? std::string() : p.label + ": ") + shown},
                                    {"onClick", [setOpen, open] { setOpen(!open); }},
                                    {"onKey", [setOpen](const KeyEvent& k) {
                                         if (k.key == "Escape") setOpen(false);
                                     }}},
                         {shown + "  ▾"}));
    if (open) children.push_back(std::move(cal));
    return h("div", {{"className", "rx-datepicker"}}, std::move(children));
#else
    std::vector<Element> children;
    children.push_back(h("box", {{"direction", "row"}, {"gap", 1}},
                         {h("text", {{"wrap", false}}, {p.label.empty() ? std::string() : p.label + ":"}),
                          h("button", {{"onClick", [setOpen, open] { setOpen(!open); }}}, {shown + " ▾"})}));
    if (open) children.push_back(std::move(cal));
    return h("box", {}, std::move(children));
#endif
}

}  // namespace

Date today() {
#ifdef __EMSCRIPTEN__
    const int v = rx_cal_today();
    return Date(year(v / 10000), month(static_cast<unsigned>(v / 100 % 100)), day(static_cast<unsigned>(v % 100)));
#else
    try {
        const auto local = zoned_time(current_zone(), system_clock::now()).get_local_time();
        return Date(floor<days>(local));
    } catch (...) {  // no time zone database: UTC
        return Date(floor<days>(system_clock::now()));
    }
#endif
}

Element calendar(CalendarProps props) {
    std::string key = std::move(props.key);
    Element e = h(Calendar, std::move(props));
    if (!key.empty()) e.key = std::move(key);
    return e;
}

Element datePicker(DatePickerProps props) {
    std::string key = std::move(props.key);
    Element e = h(DatePicker, std::move(props));
    if (!key.empty()) e.key = std::move(key);
    return e;
}

}  // namespace rx::calendar
