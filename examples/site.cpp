// The ReactX website (https://sargisis.github.io/ReactX/), written in ReactX:
//
//   /              about ReactX: a live counter drawn for the browser and
//                  for the terminal, features, quick start
//   /docs[/:page]  documentation (texts in examples/site_docs.hpp)
//   /demos         links to the demo pages
//   anything else  404
//
// Built by the "pages" preset into build/pages/site with <base href="/ReactX/">.
// The home page is prerendered: the same file built with REACTX_SITE_PRERENDER
// (site_prerender in CMakeLists.txt) writes its HTML into index.html.
// The look is in web/site_shell.html; this file is only ReactX components
// (most of them with props structs: h(CodeBlock, {.code = ...})).
#include <emscripten.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <numeric>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

#include "reactx/dom.hpp"
#include "reactx/html.hpp"
#include "reactx/reactx.hpp"
#include "reactx/router.hpp"
#include "reactx/ui.hpp"
#include "reactx/version.hpp"
#include "site_docs.hpp"
#include "site_docs_en.hpp"

using namespace rx;

namespace {

// ---- the few things only JavaScript can do ----

#ifndef REACTX_SITE_PRERENDER

EM_JS(char*, site_js_theme, (), {
    const t = document.documentElement.dataset.theme ||
              (matchMedia("(prefers-color-scheme: dark)").matches ? "dark" : "light");
    return stringToNewUTF8(t);
});
EM_JS(void, site_js_set_theme, (const char* theme), {
    const t = UTF8ToString(theme);
    document.documentElement.dataset.theme = t;
    try { localStorage.setItem("rx-theme", t); } catch (e) {}
});
EM_JS(void, site_js_scroll_to, (const char* id), {
    const el = document.getElementById(UTF8ToString(id));
    if (el) el.scrollIntoView({behavior: "smooth", block: "start"});
});
EM_JS(void, site_js_copy, (const char* text), {
    if (navigator.clipboard) navigator.clipboard.writeText(UTF8ToString(text)).catch(function() {});
});

// The language: saved choice, else the browser's (Russian or English).
EM_JS(char*, site_js_lang, (), {
    let l = null;
    try { l = localStorage.getItem("rx-lang"); } catch (e) {}
    if (l !== "ru" && l !== "en") l = (navigator.language || "").toLowerCase().startsWith("ru") ? "ru" : "en";
    document.documentElement.lang = l;
    return stringToNewUTF8(l);
});
EM_JS(void, site_js_set_lang, (const char* lang), {
    const l = UTF8ToString(lang);
    document.documentElement.lang = l;
    try { localStorage.setItem("rx-lang", l); } catch (e) {}
});
#else
// The prerender (REACTX_SITE_PRERENDER, run under Node when the site is built)
// has no page: English or Russian is chosen by main(), the theme is light, and
// the rest is never called (effects and handlers don't run in renderToString).
char* siteDup(std::string_view s) {
    auto* out = static_cast<char*>(std::malloc(s.size() + 1));
    std::memcpy(out, s.data(), s.size());
    out[s.size()] = '\0';
    return out;
}
char* site_js_theme() { return siteDup("light"); }
void site_js_set_theme(const char*) {}
void site_js_scroll_to(const char*) {}
void site_js_copy(const char*) {}
void site_js_set_lang(const char*) {}
#endif  // REACTX_SITE_PRERENDER

std::string currentTheme() {
    char* t = site_js_theme();
    std::string out(t);
    std::free(t);
    return out;
}

// The site language, shared by every component ("en" or "ru"); main() sets it.
const auto SiteLang = createStore(std::string("en"));

bool useEnglish() { return useStore(SiteLang) == "en"; }

// The search dialog (Ctrl+K, "/", the search buttons) is open.
const auto SearchOpen = createStore(false);

// The text in the current language.
std::string tr(bool en, std::string_view english, std::string_view russian) {
    return std::string(en ? english : russian);
}

// ---- syntax highlighting (a small lexer; tokens become <span>s) ----

enum class Lang { Cpp, Shell, CMake, Python };

Lang detect(std::string_view code) {
    for (std::string_view word : {"target_link_libraries", "find_package", "reactx_add_app", "FetchContent"})
        if (code.find(word) != std::string_view::npos && code.find(';') == std::string_view::npos) return Lang::CMake;
    std::string_view first = code.substr(0, code.find('\n'));
    for (std::string_view cmd : {"cmake ", "reactx ", "./", "git ", "cd ", "emcmake ", "source ", "sudo ", "ctest ", "curl ", "export ", "node ", "mkdir "})
        if (first.starts_with(cmd)) return Lang::Shell;
    if (code.find("def ") != std::string_view::npos || code.find("from flask") != std::string_view::npos ||
        code.find("import ") != std::string_view::npos || code.find("app = Flask") != std::string_view::npos)
        if (code.find("#include") == std::string_view::npos && code.find("using namespace") == std::string_view::npos)
            return Lang::Python;
    return Lang::Cpp;
}

std::string_view langName(Lang lang) {
    switch (lang) {
        case Lang::Shell: return "shell";
        case Lang::CMake: return "cmake";
        case Lang::Python: return "python";
        default: return "c++";
    }
}

bool isIdentStart(char c) { return std::isalpha(static_cast<unsigned char>(c)) || c == '_'; }
bool isIdent(char c) { return std::isalnum(static_cast<unsigned char>(c)) || c == '_'; }

constexpr std::array kKeywords = {
    "auto",    "bool",     "break",   "case",     "catch",  "char",     "class",    "co_await", "co_return",
    "co_yield", "concept", "const",   "consteval", "constexpr", "continue", "default", "delete", "do",
    "double",  "else",     "enum",    "explicit", "export", "extern",   "false",    "float",    "for",
    "friend",  "if",       "import",  "inline",   "int",    "long",     "module",   "mutable",  "namespace",
    "new",     "noexcept", "nullptr", "operator", "override", "private", "protected", "public", "requires",
    "return",  "short",    "signed",  "sizeof",   "static", "static_assert", "struct", "switch", "template",
    "this",    "throw",    "true",    "try",      "typename", "union",  "unsigned", "using",    "virtual",
    "void",    "while",    "contract_assert", "pre", "post",
};

constexpr std::array kPyKeywords = {
    "and", "as", "assert", "async", "await", "break", "class", "continue", "def", "del", "elif",
    "else", "except", "False", "finally", "for", "from", "global", "if", "import", "in", "is",
    "lambda", "None", "not", "or", "pass", "raise", "return", "True", "try", "while", "with", "yield",
};

bool isKeyword(std::string_view word) { return std::ranges::find(kKeywords, word) != kKeywords.end(); }
bool isPyKeyword(std::string_view word) { return std::ranges::find(kPyKeywords, word) != kPyKeywords.end(); }

class Highlighter {
public:
    explicit Highlighter(Lang lang) : lang_(lang) {}

    std::vector<Element> run(std::string_view code) {
        if (lang_ == Lang::Shell) shell(code);
        else source(code);
        flush();
        return std::move(out_);
    }

private:
    void plain(std::string_view s) { text_ += s; }
    void token(std::string_view cls, std::string_view s) {
        flush();
        out_.push_back(h("span", {{"className", std::string(cls)}}, {std::string(s)}));
    }
    void flush() {
        if (!text_.empty()) out_.push_back(Element(std::exchange(text_, {})));
    }

    // Shell: a prompt before each command, "# ..." comments.
    void shell(std::string_view code) {
        while (!code.empty()) {
            auto nl = code.find('\n');
            std::string_view line = code.substr(0, nl);
            code = nl == std::string_view::npos ? std::string_view{} : code.substr(nl + 1);
            auto hash = line.find('#');
            std::string_view cmd = line.substr(0, hash), comment = hash == std::string_view::npos ? "" : line.substr(hash);
            if (auto start = cmd.find_first_not_of(' '); start != std::string_view::npos) {  // a command
                token("t-prompt", "$ ");
                std::string_view words = cmd.substr(start);
                auto sp = words.find(' ');
                token("t-fn", words.substr(0, sp));  // the program
                if (sp != std::string_view::npos) plain(words.substr(sp));
            } else {
                plain(cmd);  // only spaces before a comment
            }
            if (!comment.empty()) token("t-com", comment);
            if (nl != std::string_view::npos) plain("\n");
        }
    }

    // C++ and CMake.
    void source(std::string_view s) {
        bool lineStart = true;
        std::size_t i = 0;
        while (i < s.size()) {
            char c = s[i];
            auto rest = s.substr(i);
            if (c == '\n') {
                plain("\n");
                lineStart = true;
                ++i;
                continue;
            }
            if (c == ' ' || c == '\t') {
                plain(rest.substr(0, 1));
                ++i;
                continue;
            }
            bool wasLineStart = std::exchange(lineStart, false);
            if ((lang_ == Lang::CMake || lang_ == Lang::Python) && c == '#') {
                auto end = rest.find('\n');
                token("t-com", rest.substr(0, end));
                i += end == std::string_view::npos ? rest.size() : end;
            } else if (lang_ == Lang::Python && c == '@') {
                auto end = rest.find('\n');
                token("t-pp", rest.substr(0, end));
                i += end == std::string_view::npos ? rest.size() : end;
            } else if (lang_ == Lang::Cpp && c == '#' && wasLineStart) {  // #include, #embed, ...
                auto end = rest.find('\n');
                token("t-pp", rest.substr(0, end));
                i += end == std::string_view::npos ? rest.size() : end;
            } else if (rest.starts_with("//")) {
                auto end = rest.find('\n');
                token("t-com", rest.substr(0, end));
                i += end == std::string_view::npos ? rest.size() : end;
            } else if (rest.starts_with("/*")) {
                auto end = rest.find("*/");
                std::size_t n = end == std::string_view::npos ? rest.size() : end + 2;
                token("t-com", rest.substr(0, n));
                i += n;
            } else if (rest.starts_with("R\"(")) {  // raw string
                auto end = rest.find(")\"");
                std::size_t n = end == std::string_view::npos ? rest.size() : end + 2;
                token("t-str", rest.substr(0, n));
                i += n;
            } else if (c == '"' || c == '\'') {
                std::size_t j = 1;
                while (j < rest.size() && rest[j] != c && rest[j] != '\n') j += rest[j] == '\\' ? 2 : 1;
                std::size_t n = std::min(j + 1, rest.size());
                token("t-str", rest.substr(0, n));
                i += n;
            } else if (std::isdigit(static_cast<unsigned char>(c))) {
                std::size_t j = 0;
                while (j < rest.size() && (isIdent(rest[j]) || rest[j] == '.' || rest[j] == '\'')) ++j;
                token("t-num", rest.substr(0, j));
                i += j;
            } else if (isIdentStart(c)) {
                std::size_t j = 0;
                while (j < rest.size() && isIdent(rest[j])) ++j;
                std::string_view word = rest.substr(0, j);
                std::size_t k = j;
                while (k < rest.size() && rest[k] == ' ') ++k;
                bool call = k < rest.size() && rest[k] == '(';
                if (lang_ == Lang::Cpp && isKeyword(word)) token("t-kw", word);
                else if (lang_ == Lang::Python && isPyKeyword(word)) token("t-kw", word);
                else if (call) token("t-fn", word);
                else if (lang_ == Lang::CMake && std::ranges::all_of(word, [](char x) { return !std::islower(static_cast<unsigned char>(x)); }))
                    token("t-kw", word);  // PRIVATE, SOURCES, ...
                else if (lang_ == Lang::Cpp && (std::isupper(static_cast<unsigned char>(word[0])) || word == "std" || word == "rx"))
                    token("t-type", word);
                else plain(word);
                i += j;
            } else {
                // One UTF-8 character of punctuation or text (keep sequences whole).
                std::size_t n = 1;
                while (i + n < s.size() && (static_cast<unsigned char>(s[i + n]) & 0xC0) == 0x80) ++n;
                plain(s.substr(i, n));
                i += n;
            }
        }
    }

    Lang lang_;
    std::vector<Element> out_;
    std::string text_;
};

// ---- small components ----

struct CodeProps {
    std::string code{};
    std::string title{};  // shown in the bar; the language by default
    std::string key{};
};

// A code block with highlighting and a copy button.
Element CodeBlock(const CodeProps& p) {
    bool en = useEnglish();
    Lang lang = detect(p.code);
    const auto& tokens = useMemo([&] { return Highlighter(lang).run(p.code); }, deps(p.code));
    return h("figure", {{"className", "code"}}, {
        h("div", {{"className", "code-bar"}}, {
            h("span", {}, {p.title.empty() ? std::string(langName(lang)) : p.title}),
            h("button", {{"className", "copy"},
                         {"aria-label", tr(en, "Copy the code", "Скопировать код")},
                         {"onClick", [code = p.code, en] {
                              site_js_copy(code.c_str());
                              ui::toast({.text = tr(en, "Copied", "Скопировано"), .variant = ui::Variant::Success,
                                         .duration = std::chrono::milliseconds(1600)});
                          }}},
              {tr(en, "copy", "копировать")}),
        }),
        h("pre", {}, {h("code", {}, tokens)}),
    });
}

Element ThemeToggle(const Props&) {
    auto [theme, setTheme] = useState(currentTheme());
    bool dark = theme == "dark";
    bool en = useEnglish();
    return h("button", {{"className", "icon-btn"},
                        {"title", dark ? tr(en, "Light theme", "Светлая тема") : tr(en, "Dark theme", "Тёмная тема")},
                        {"aria-label", tr(en, "Switch theme", "Переключить тему")},
                        {"onClick", [dark, setTheme] {
                             std::string next = dark ? "light" : "dark";
                             site_js_set_theme(next.c_str());
                             setTheme(next);
                         }}},
             {dark ? "☀" : "☾"});
}

// EN | RU: switches the whole site, remembered in localStorage.
Element LangToggle(const Props&) {
    bool en = useEnglish();
    return h("button", {{"className", "icon-btn lang-btn"},
                        {"title", en ? "Русский" : "English"},
                        {"aria-label", en ? "Switch to Russian" : "Switch to English"},
                        {"onClick", [en] {
                             std::string next = en ? "ru" : "en";
                             site_js_set_lang(next.c_str());
                             SiteLang.set(next);
                         }}},
             {en ? "RU" : "EN"});
}

// ---- home ----

struct LiveCounterProps {
    int start = 0;
};

// The same counter drawn twice: as the browser shows it (a real ReactX
// button) and as TerminalRenderer would draw it in a terminal.
Element LiveCounter(const LiveCounterProps& p) {
    auto [count, setCount] = useState(p.start);
    bool en = useEnglish();
    auto inc = [setCount] { setCount([](int c) { return c + 1; }); };
    auto reset = [setCount] { setCount(0); };

    // A 24-column box like TerminalRenderer draws: "│ " + 20 columns + " │".
    auto row = [](std::vector<Element> cells, int used) {
        std::vector<Element> line{h("span", {{"className", "c-dim"}}, {"│ "})};
        for (auto& c : cells) line.push_back(std::move(c));
        line.push_back(Element(std::string(static_cast<std::size_t>(std::max(0, 20 - used)), ' ')));
        line.push_back(h("span", {{"className", "c-dim"}}, {" │\n"}));
        return fragment(std::move(line));
    };
    std::string label = "Count: " + std::to_string(count);
    return h("div", {{"className", "demo-pair"}}, {
        h(CodeBlock, {.code = "struct CounterProps { int start = 0; };\n\n"
                              "Element Counter(const CounterProps& p) {\n"
                              "    auto [count, setCount] = useState(p.start);\n"
                              "    return h(\"box\", {}, {\n"
                              "        h(\"text\", {}, {\"Count: \" + std::to_string(count)}),\n"
                              "        h(\"button\", {{\"onClick\", [setCount] {\n"
                              "            setCount([](int c) { return c + 1; });\n"
                              "        }}}, {\"+1\"}),\n"
                              "    });\n"
                              "}\n\n"
                              "h(Counter, {.start = 3})",
                      .title = "counter.cpp"}),
        h("div", {{"className", "outputs"}}, {
            h("div", {{"className", "panel"}}, {
                h("div", {{"className", "panel-bar"}}, {
                    h("span", {{"className", "dots"}}, {h("i"), h("i"), h("i")}),
                    tr(en, "browser", "браузер"),
                    h("span", {{"className", "tag"}}, {"DomRenderer"}),
                }),
                h("div", {{"className", "browser-view"}}, {
                    h("div", {{"className", "count"}, {"aria-live", "polite"}}, {std::to_string(count)}),
                    h("div", {{"className", "btn-row"}}, {
                        h("button", {{"className", "btn primary"}, {"onClick", inc}}, {"+1"}),
                        h("button", {{"className", "btn"}, {"onClick", reset}}, {"reset"}),
                    }),
                    h("div", {{"className", "hint"}}, {tr(en, "click — both update", "нажмите — обновятся оба")}),
                }),
            }),
            h("div", {{"className", "panel"}}, {
                h("div", {{"className", "panel-bar"}}, {
                    h("span", {{"className", "dots"}}, {h("i"), h("i"), h("i")}),
                    tr(en, "terminal", "терминал"),
                    h("span", {{"className", "tag"}}, {"TerminalRenderer"}),
                }),
                h("pre", {{"className", "term-view"}, {"aria-hidden", "true"}}, {
                    h("span", {{"className", "c-dim"}}, {"┌─ "}),
                    h("span", {{"className", "c-acc"}}, {"counter"}),
                    h("span", {{"className", "c-dim"}}, {" ────────────┐\n"}),
                    row({Element(label)}, static_cast<int>(label.size())),
                    row({h("span", {{"className", "c-inv"}}, {"[ +1 ]"}), Element(" [ reset ]")}, 16),
                    h("span", {{"className", "c-dim"}}, {"└──────────────────────┘"}),
                }),
            }),
        }),
    });
}

// ---- live rx::ui demos on the home page ----

// 100,000 parts, computed from the row number: nothing is stored but the
// sort order.
constexpr int kParts = 100000;
std::string partName(int i) {
    static constexpr const char* kinds[] = {"Bolt", "Nut", "Washer", "Gear", "Spring", "Valve", "Hinge", "Pulley"};
    return std::string(kinds[i % 8]) + " M" + std::to_string(3 + (i / 8) % 10);
}
int partQty(int i) { return static_cast<int>((static_cast<long long>(i) * 7919) % 500); }
int partCents(int i) { return 50 + static_cast<int>((static_cast<long long>(i) * 104729) % 99950); }
std::string money(int cents) {
    std::string c = std::to_string(cents % 100);
    return std::to_string(cents / 100) + "." + (c.size() < 2 ? "0" + c : c);
}

Element TableDemo(const Props&) {
    bool en = useEnglish();
    auto [sortBy, setSortBy] = useState(0);
    auto [desc, setDesc] = useState(false);
    auto [picked, setPicked] = useState(-1);
    const auto& order = useMemo([sortBy, desc] {
        std::vector<int> o(kParts);
        std::iota(o.begin(), o.end(), 0);
        if (sortBy == 2) std::ranges::stable_sort(o, {}, partQty);
        if (sortBy == 3) std::ranges::stable_sort(o, {}, partCents);
        if (desc) std::ranges::reverse(o);
        return o;
    }, deps(sortBy, desc));
    auto list = ui::useVirtualList({.count = kParts, .rowHeight = 37, .height = 333});
    std::vector<ui::TableRow> rows;
    for (int r = list.first; r < list.last; ++r) {
        int i = order[static_cast<std::size_t>(r)];
        rows.push_back({.cells = {h("span", {{"className", "mono"}}, {"#" + std::to_string(i + 1)}), partName(i),
                                  std::to_string(partQty(i)), money(partCents(i))},
                        .key = std::to_string(i),
                        .onSelect = [setPicked, i] { setPicked(i); },
                        .selected = picked == i});
    }
    return h("div", {{"className", "live-pane"}}, {
        ui::table({.columns = {{.title = "#", .width = 7, .sortable = true}, {.title = tr(en, "Part", "Деталь")},
                               {.title = tr(en, "Qty", "Кол-во"), .width = 6, .align = ui::Align::End, .sortable = true},
                               {.title = tr(en, "Price", "Цена"), .width = 7, .align = ui::Align::End, .sortable = true}},
                   .rows = std::move(rows), .sortColumn = sortBy, .sortDescending = desc,
                   .onSort = [setSortBy, setDesc, sortBy](int column) {
                       if (column == sortBy) setDesc([](bool d) { return !d; });
                       else { setSortBy(column); setDesc(false); }
                   },
                   .list = list}),
        h("p", {{"className", "live-note"}},
          {picked < 0 ? tr(en, "100,000 rows, about 20 in the page at a time. Scroll, sort, click a row.",
                           "100 000 строк, на странице из них около 20. Прокрутите, отсортируйте, нажмите на строку.")
                      : tr(en, "Picked: ", "Выбрано: ") + partName(picked) + " · #" + std::to_string(picked + 1)}),
    });
}

struct Order {
    std::string name;
    std::string size = "m";
    int count = 1;
    std::string note;
    bool gift = false;
    bool operator==(const Order&) const = default;
};

// ui::useForm with every kind of field; errors show after a field changed or a submit.
Element FormDemo(const Props&) {
    bool en = useEnglish();
    auto form = ui::useForm<Order>({.validate = [en](const Order& o) {
                                        ui::FormErrors e;
                                        if (o.name.size() < 2) e["name"] = tr(en, "At least two letters", "Хотя бы две буквы");
                                        if (o.note.size() > 80) e["note"] = tr(en, "80 characters at most", "Не больше 80 символов");
                                        return e;
                                    },
                                    .onSubmit = [en](const Order& o) {
                                        ui::toast({.text = tr(en, "Ordered: ", "Заказ: ") + std::to_string(o.count) + " × " + o.size +
                                                           tr(en, " for ", " для ") + o.name + (o.gift ? " 🎁" : ""),
                                                   .variant = ui::Variant::Success});
                                    }});
    const Order& o = form.values;
    int filled = (o.name.size() >= 2) + (!o.note.empty()) + 1;  // size and count always have a value
    return h("div", {{"className", "live-pane live-form"}}, {
        ui::input({.value = o.name, .onChange = form.set("name", &Order::name), .onSubmit = [submit = form.submit](const std::string&) { submit(); },
                   .placeholder = "Ada Lovelace", .label = tr(en, "Name", "Имя"), .labelAbove = true, .error = form.error("name")}),
        ui::radioGroup({.label = tr(en, "Size", "Размер"), .value = o.size, .options = {{"s", "S"}, {"m", "M"}, {"l", "L"}, {"xl", "XL"}},
                        .onChange = form.set("size", &Order::size), .row = true}),
        ui::slider({.label = tr(en, "How many", "Сколько"), .value = o.count, .min = 1, .max = 10, .onChange = form.set("count", &Order::count)}),
        ui::textarea({.value = o.note, .onChange = form.set("note", &Order::note), .placeholder = tr(en, "Anything else?", "Что-нибудь ещё?"),
                      .label = tr(en, "Note", "Комментарий"), .rows = 2, .error = form.error("note")}),
        ui::checkbox({.label = tr(en, "Gift wrap", "Подарочная упаковка"), .checked = o.gift, .onChange = form.set("gift", &Order::gift)}),
        ui::progress({.value = filled, .max = 3, .label = tr(en, "Filled in", "Заполнено"),
                      .variant = filled == 3 ? ui::Variant::Success : ui::Variant::Primary}),
        ui::stack({.direction = ui::Direction::Row, .gap = 1, .children = {
            ui::button({.label = tr(en, "Order", "Заказать"), .onClick = form.submit, .variant = ui::Variant::Primary}),
            ui::button({.label = tr(en, "Clear", "Очистить"), .onClick = form.reset}),
        }}),
    });
}

Element DialogDemo(const Props&) {
    bool en = useEnglish();
    auto [asking, setAsking] = useState(false);
    auto [deleted, setDeleted] = useState(0);
    return h("div", {{"className", "live-pane live-dialog"}}, {
        ui::text({.text = tr(en, "Projects deleted: ", "Удалено проектов: ") + std::to_string(deleted), .muted = true}),
        ui::button({.label = tr(en, "Delete the project…", "Удалить проект…"), .onClick = [setAsking] { setAsking(true); },
                    .variant = ui::Variant::Danger}),
        ui::confirm({.title = tr(en, "Delete the project?", "Удалить проект?"),
                     .message = tr(en, "It's only a demo: nothing is really deleted.", "Это демо: на самом деле ничего не удалится."),
                     .open = asking, .confirmLabel = tr(en, "Delete", "Удалить"), .cancelLabel = tr(en, "Cancel", "Отмена"),
                     .danger = true,
                     .onConfirm = [en, setAsking, setDeleted] {
                         setAsking(false);
                         setDeleted([](int n) { return n + 1; });
                         ui::toast({.text = tr(en, "Deleted (not really)", "Удалено (понарошку)")});
                     },
                     .onCancel = [setAsking] { setAsking(false); }}),
    });
}

Element LiveDemos(const Props&) {
    bool en = useEnglish();
    return h("div", {{"className", "live"}}, {
        ui::tabs({.tabs = {{tr(en, "Table · 100,000 rows", "Таблица · 100 000 строк"), {h(TableDemo)}},
                           {tr(en, "Form · useForm", "Форма · useForm"), {h(FormDemo)}},
                           {tr(en, "Dialog and toast", "Диалог и уведомление"), {h(DialogDemo)}}}}),
    });
}

struct SpotlightProps {
    std::string className{};
    std::vector<Element> children{};
    std::string key{};
};

// A soft glow in the site's color that follows the mouse (onPointerMove):
// the position goes into --mx / --my, the CSS draws the light. Touch screens
// don't hover, so only the mouse lights it.
Element Spotlight(const SpotlightProps& p) {
    auto [at, setAt] = useState(std::pair<int, int>{-1, -1});
    bool lit = at.first >= 0;
    std::string style = lit ? "--mx: " + std::to_string(at.first) + "px; --my: " + std::to_string(at.second) + "px" : std::string();
    return h("div", {{"className", p.className + (lit ? " spot lit" : " spot")},
                     {"style", style},
                     {"onPointerMove", [setAt](const PointerEvent& e) {
                          if (e.type == "mouse") setAt(std::pair{static_cast<int>(e.localX), static_cast<int>(e.localY)});
                      }},
                     {"onMouseLeave", [setAt] { setAt(std::pair{-1, -1}); }}},
             p.children);
}

Element Feature(std::string title, std::vector<Element> text) {
    return h("div", {{"className", "feature"}, {"key", title}}, {h("b", {}, {title}), h("p", {}, std::move(text))});
}

Element code(std::string s) { return h("code", {}, {std::move(s)}); }

Element Home(const Props&) {
    bool en = useEnglish();
    useTitle(tr(en, "ReactX — React for C++26", "ReactX — React на C++26"));
    return h("div", {{"className", "wrap"}}, {
        h(Spotlight, {.className = "hero", .children = {
            h("div", {}, {
                h("a", {{"className", "eyebrow"}, {"href", "https://github.com/sargisis/ReactX/releases/tag/v" + std::string(rx::version())}},
                  {h("b", {}, {"●"}), "v" + std::string(rx::version()) + tr(en, " is out — what's new →", " вышла — что нового →")}),
                en ? h("h1", {}, {"React", h("br"), "for ", h("em", {}, {"C++26"}), "."})
                   : h("h1", {}, {"React", h("br"), "на ", h("em", {}, {"C++26"}), "."}),
                h("p", {{"className", "lead"}},
                  {tr(en, "Components, hooks and a reconciler — like React, in C++. The same code draws in the terminal and in the browser.",
                      "Компоненты, хуки и reconciler — как в React. Один и тот же код рисуется в терминале и в браузере.")}),
                h("div", {{"className", "btn-row"}}, {
                    router::link({.to = "/docs/start", .label = tr(en, "Get started →", "Быстрый старт →"), .className = "btn primary", .activeClassName = ""}),
                    h("a", {{"className", "btn"}, {"href", "https://github.com/sargisis/ReactX"}}, {"GitHub"}),
                }),
                h("ul", {{"className", "stats"}}, {
                    h("li", {}, {h("b", {}, {"~120 KB"}), tr(en, ".wasm for a new app", ".wasm у нового приложения")}),
                    h("li", {}, {h("b", {}, {"2"}), tr(en, "backends, one code", "бэкенда, один код")}),
                    h("li", {}, {h("b", {}, {"0"}), tr(en, "lines of JavaScript", "строк JavaScript")}),
                }),
            }),
            h(LiveCounter, {.start = 3}),
        }}),

        h("section", {{"className", "section"}}, {
            h("div", {{"className", "section-head"}}, {h("h2", {}, {tr(en, "Try it live", "Попробуйте вживую")}),
                                                       h("span", {}, {tr(en, "rx::ui components, running right here", "компоненты rx::ui работают прямо здесь")})}),
            h(LiveDemos),
        }),

        h("section", {{"className", "section"}}, {
            h("div", {{"className", "section-head"}}, {h("h2", {}, {tr(en, "What's inside", "Что внутри")}),
                                                       h("span", {}, {tr(en, "all C++, no JavaScript", "всё на C++, без JavaScript")})}),
            h(Spotlight, {.className = "features", .children = {
                Feature(tr(en, "Hooks", "Хуки"), {code("useState"), " ", code("useEffect"), " ", code("useReducer"), " ", code("useStore"), " ",
                                                  code("useContext"), tr(en, " — like in React.", " — как в React.")}),
                Feature(tr(en, "Typed props", "Props — структура"), {code("h(Counter, {.start = 5})"),
                                                                     tr(en, ": a typo in a field name is a compile error.", ": опечатку в имени поля ловит компилятор.")}),
                Feature(tr(en, "Two backends", "Два бэкенда"), {tr(en, "Terminal (ANSI, Flexbox layout) and browser (the DOM through WebAssembly).",
                                                                  "Терминал (ANSI, Flexbox на Yoga) и браузер (DOM через WebAssembly).")}),
                Feature(tr(en, "Router and data", "Роутер и данные"), {tr(en, "Pages, links, ", "Страницы, ссылки, "), code("useFetch"),
                                                                       tr(en, ", JSON with C++26 reflection.", ", свой JSON с рефлексией C++26.")}),
                Feature(tr(en, "C++ dev server", "Сервер на C++"), {code("reactx dev"), tr(en, " rebuilds and reloads the page on every save; ", " пересобирает и обновляет страницу при сохранении; "),
                                                                    code("--proxy"), tr(en, " for your own backend.", " — для своего бэкенда.")}),
                Feature(tr(en, "Safe by default", "Надёжность"), {tr(en, "Error boundaries, ", "Границы ошибок, "), code("std::expected"),
                                                                  tr(en, ", contracts; every change runs sanitizers and fuzzing.",
                                                                     ", контракты; каждое изменение проверяется санитайзерами и фаззингом.")}),
            }}),
        }),

        h("section", {{"className", "section"}}, {
            h("div", {{"className", "section-head"}}, {h("h2", {}, {tr(en, "Get started", "Быстрый старт")}), h("span", {}, {tr(en, "three commands", "три команды")})}),
            h("div", {{"className", "steps"}}, {
                h("div", {{"className", "step"}, {"key", "1"}}, {
                    h("div", {}, {h("h3", {}, {tr(en, "Install reactx", "Установите reactx")}),
                                  h("p", {}, {tr(en, "One line on Linux: reactx and the compiler, into your home folder, no sudo.",
                                                 "Одна строка в Linux: reactx и компилятор, в домашнюю папку, без sudo.")}),}),
                    h(CodeBlock, {.code = "curl -fsSL https://sargisis.github.io/ReactX/install.sh | sh"}),
                }),
                h("div", {{"className", "step"}, {"key", "2"}}, {
                    h("div", {}, {h("h3", {}, {tr(en, "Create an app", "Создайте проект")}),
                                  h("p", {}, {tr(en, "A web app, or with --terminal a terminal app.", "Веб-приложение или, с --terminal, программа для терминала.")}),}),
                    h(CodeBlock, {.code = "reactx new my-app\ncd my-app"}),
                }),
                h("div", {{"className", "step"}, {"key", "3"}}, {
                    h("div", {}, {h("h3", {}, {tr(en, "Run it", "Запустите")}),
                                  h("p", {}, {tr(en, "The page rebuilds and reloads every time you save.", "Страница пересобирается и обновляется при каждом сохранении.")}),}),
                    h(CodeBlock, {.code = "reactx dev"}),
                }),
            }),
        }),

        h("section", {{"className", "section"}}, {
            h("div", {{"className", "cta"}}, {
                h("div", {}, {h("h2", {}, {tr(en, "This site is built with ReactX", "Этот сайт написан на ReactX")}),
                              h("p", {}, {tr(en, "The router, the components and the code highlighting — C++ compiled to WebAssembly.",
                                             "Роутер, компоненты и подсветка кода — C++, скомпилированный в WebAssembly.")})}),
                router::link({.to = "/docs", .label = tr(en, "Documentation →", "Документация →"), .className = "btn primary", .activeClassName = ""}),
            }),
        }),
    });
}

// ---- docs ----

struct Group {
    std::string_view en, ru;
    std::vector<std::string_view> slugs;
};

const std::vector<Group>& groups() {
    static const std::vector<Group> all = {
        {"Start", "Начало", {"start", "cli", "versions", "safety"}},
        {"Basics", "Основы", {"elements", "hooks", "context", "events"}},
        {"Platforms", "Платформы", {"terminal", "browser", "ui"}},
        {"Apps", "Приложения", {"router", "data", "server", "backend"}},
    };
    return all;
}

const std::vector<site_docs::Page>& docPages(bool en) { return en ? site_docs::pagesEn() : site_docs::pages(); }

const site_docs::Page* findPage(std::string_view slug, bool en) {
    const auto& all = docPages(en);
    auto it = std::ranges::find(all, slug, &site_docs::Page::slug);
    return it == all.end() ? nullptr : &*it;
}

std::string groupOf(std::string_view slug, bool en) {
    for (const auto& g : groups())
        if (std::ranges::find(g.slugs, slug) != g.slugs.end()) return std::string(en ? g.en : g.ru);
    return tr(en, "Documentation", "Документация");
}

// Lower case for ASCII and Russian letters (enough for the search box).
std::string lower(std::string_view s) {
    std::string out(s);
    for (std::size_t i = 0; i < out.size(); ++i) {
        auto b = static_cast<unsigned char>(out[i]);
        if (b < 0x80) {
            out[i] = static_cast<char>(std::tolower(b));
        } else if (b == 0xD0 && i + 1 < out.size()) {
            auto n = static_cast<unsigned char>(out[i + 1]);
            if (n >= 0x90 && n <= 0x9F) out[i + 1] = static_cast<char>(n + 0x20);                       // А-П -> а-п
            else if (n >= 0xA0 && n <= 0xAF) out[i] = static_cast<char>(0xD1), out[i + 1] = static_cast<char>(n - 0x20);  // Р-Я
            else if (n == 0x81) out[i] = static_cast<char>(0xD1), out[i + 1] = static_cast<char>(0x91);    // Ё
            ++i;
        }
    }
    return out;
}

// ---- search (Ctrl+K) ----

struct Hit {
    std::string slug, page, heading, snippet;
    int section = -1;  // -1: the page itself
    bool operator==(const Hit&) const = default;
};

// Pages and sections whose words contain the query: titles first, then text.
std::vector<Hit> searchDocs(std::string_view query, bool en) {
    std::string q = lower(query);
    std::vector<Hit> titles, texts;
    if (q.empty()) return {};
    auto snippet = [&](std::string_view text) {
        std::string low = lower(text);
        auto at = low.find(q);
        if (at == std::string::npos) at = 0;
        std::size_t from = at > 40 ? at - 40 : 0;
        while (from > 0 && (static_cast<unsigned char>(text[from]) & 0xC0) == 0x80) --from;  // whole UTF-8 characters
        std::size_t to = std::min(text.size(), from + 120);
        while (to < text.size() && (static_cast<unsigned char>(text[to]) & 0xC0) == 0x80) ++to;
        std::string out = (from ? "…" : "") + std::string(text.substr(from, to - from)) + (to < text.size() ? "…" : "");
        std::ranges::replace(out, '\n', ' ');
        std::erase(out, '`');
        return out;
    };
    for (const auto& page : docPages(en)) {
        if (lower(page.title).contains(q) || lower(page.summary).contains(q))
            titles.push_back({std::string(page.slug), std::string(page.title), {}, std::string(page.summary), -1});
        for (std::size_t i = 0; i < page.sections.size(); ++i) {
            const auto& sec = page.sections[i];
            if (lower(sec.heading).contains(q))
                titles.push_back({std::string(page.slug), std::string(page.title), std::string(sec.heading), snippet(sec.text), static_cast<int>(i)});
            else if (lower(sec.text).contains(q) || lower(sec.code).contains(q))
                texts.push_back({std::string(page.slug), std::string(page.title), std::string(sec.heading),
                                 snippet(lower(sec.text).contains(q) ? sec.text : sec.code), static_cast<int>(i)});
        }
    }
    titles.insert(titles.end(), texts.begin(), texts.end());
    if (titles.size() > 12) titles.resize(12);
    return titles;
}

void openHit(const Hit& hit) {
    SearchOpen.set(false);
    router::navigate("/docs/" + hit.slug);
    if (hit.section >= 0)  // after the page is drawn
        detail::setTimer(std::chrono::milliseconds(60), [id = "s-" + std::to_string(hit.section)] { site_js_scroll_to(id.c_str()); });
}

Element SearchDialog(const Props&) {
    bool open = useStore(SearchOpen);
    bool en = useEnglish();
    auto [query, setQuery] = useState(std::string{});
    auto [chosen, setChosen] = useState(0);
    const auto& hits = useMemo([&] { return searchDocs(query, en); }, deps(query, en));
    int current = hits.empty() ? 0 : std::clamp(chosen, 0, static_cast<int>(hits.size()) - 1);
    auto close = [setQuery] {
        SearchOpen.set(false);
        setQuery(std::string{});
    };
    // Up / Down pick a result, Enter opens it (the field keeps the focus).
    useKeyboard([&hits, current, setChosen, open](const KeyEvent& k) {
        if (!open || hits.empty()) return false;
        if (k.key == "Down") setChosen(std::min(current + 1, static_cast<int>(hits.size()) - 1));
        else if (k.key == "Up") setChosen(std::max(current - 1, 0));
        else return false;
        return true;
    });

    std::vector<Element> results;
    for (std::size_t i = 0; i < hits.size(); ++i) {
        const auto& hit = hits[i];
        bool on = static_cast<int>(i) == current;
        results.push_back(h("button", {{"key", hit.slug + std::to_string(hit.section)},
                                       {"className", on ? "hit hit--on" : "hit"},
                                       {"scrollIntoView", on},
                                       {"onClick", [hit, setQuery] { openHit(hit); setQuery(std::string{}); }}}, {
            h("b", {}, {hit.heading.empty() ? hit.page : hit.heading}),
            h("small", {}, {hit.heading.empty() ? tr(en, "page", "раздел") : hit.page}),
            h("span", {}, {hit.snippet}),
        }));
    }
    if (results.empty())
        results.push_back(h("p", {{"className", "hits-empty"}, {"key", "empty"}},
                            {query.empty() ? tr(en, "Hooks, router, table, prerender…", "Хуки, роутер, таблица, prerender…")
                                           : tr(en, "Nothing found", "Ничего не найдено")}));
    return ui::modal({.title = tr(en, "Search the docs", "Поиск по документации"), .open = open, .onClose = close, .width = 64,
                      .children = {
                          ui::input({.value = query,
                                     .onChange = [setQuery, setChosen](const std::string& t) { setQuery(t); setChosen(0); },
                                     .onSubmit = [&hits, current, setQuery](const std::string&) {
                                         if (hits.empty()) return;
                                         openHit(hits[static_cast<std::size_t>(current)]);
                                         setQuery(std::string{});
                                     },
                                     .placeholder = tr(en, "Type to search, Enter to open", "Введите запрос, Enter — открыть")}),
                          h("div", {{"className", "hits"}}, std::move(results)),
                      }});
}

Element SearchButton(const Props& p) {
    bool en = useEnglish();
    return h("button", {{"className", "search-btn " + p.get<std::string>("className")}, {"onClick", [] { SearchOpen.set(true); }},
                        {"aria-label", tr(en, "Search the docs", "Поиск по документации")}}, {
        h("span", {}, {tr(en, "Search", "Поиск")}),
        h("kbd", {}, {"Ctrl K"}),
    });
}

struct SidebarProps {
    std::string current{};  // slug of the open page
};

Element Sidebar(const SidebarProps& p) {
    auto [open, setOpen] = useState(false);
    bool en = useEnglish();
    std::string location = router::useLocation();
    useEffect([setOpen] { setOpen(false); }, deps(location));  // close the mobile menu after a click

    std::vector<Element> blocks;
    for (const auto& g : groups()) {
        std::vector<Element> links;
        for (auto slug : g.slugs) {
            const auto* page = findPage(slug, en);
            if (!page) continue;
            links.push_back(router::link({.to = "/docs/" + std::string(page->slug), .label = std::string(page->title), .key = std::string(page->slug)}));
        }
        if (links.empty()) continue;
        std::string title(en ? g.en : g.ru);
        std::vector<Element> group{h("div", {{"className", "group-title"}}, {title})};
        for (auto& l : links) group.push_back(std::move(l));
        blocks.push_back(h("div", {{"className", "group"}, {"key", title}}, std::move(group)));
    }

    const auto* page = findPage(p.current, en);
    return fragment({
        h("button", {{"className", "menu-btn"}, {"aria-expanded", open ? "true" : "false"},
                     {"onClick", [open, setOpen] { setOpen(!open); }}},
          {page ? std::string(page->title) : tr(en, "Sections", "Разделы"), h("span", {}, {open ? "✕" : "☰"})}),
        h("aside", {{"className", open ? "sidebar open" : "sidebar"}, {"aria-label", tr(en, "Documentation sections", "Разделы документации")}}, {
            h(SearchButton, {{"className", "search-btn--wide"}}),
            fragment(std::move(blocks)),
        }),
    });
}

struct TocProps {
    std::vector<std::string> headings{};
};

// "On this page": buttons that scroll to the sections (s-0, s-1, ...).
Element Toc(const TocProps& p) {
    bool en = useEnglish();
    std::string title = tr(en, "On this page", "На странице");
    std::vector<Element> items{h("div", {{"className", "toc-title"}}, {title})};
    for (std::size_t i = 0; i < p.headings.size(); ++i) {
        std::string id = "s-" + std::to_string(i);
        items.push_back(h("button", {{"key", id}, {"onClick", [id] { site_js_scroll_to(id.c_str()); }}}, {p.headings[i]}));
    }
    return h("nav", {{"className", "toc"}, {"aria-label", title}}, std::move(items));
}

std::vector<Element> formatInline(std::string_view s) {
    std::vector<Element> out;
    std::string text;
    auto flush = [&] {
        if (!text.empty()) {
            out.push_back(Element(std::exchange(text, {})));
        }
    };
    std::size_t i = 0;
    while (i < s.size()) {
        if (s[i] == '`') {
            auto end = s.find('`', i + 1);
            if (end != std::string_view::npos) {
                flush();
                out.push_back(h("code", {}, {std::string(s.substr(i + 1, end - i - 1))}));
                i = end + 1;
                continue;
            }
        } else if (s.substr(i).starts_with("**")) {
            auto end = s.find("**", i + 2);
            if (end != std::string_view::npos) {
                flush();
                out.push_back(h("b", {}, {std::string(s.substr(i + 2, end - i - 2))}));
                i = end + 2;
                continue;
            }
        }
        text += s[i];
        ++i;
    }
    flush();
    return out;
}

std::vector<Element> renderSectionText(const std::string& text) {
    std::vector<Element> out;
    for (std::size_t start = 0; start < text.size();) {
        auto end = text.find("\n\n", start);
        std::string_view block = std::string_view(text).substr(start, end == std::string::npos ? std::string_view::npos : end - start);

        std::vector<std::string_view> lines;
        for (std::size_t lstart = 0; lstart < block.size();) {
            auto lend = block.find('\n', lstart);
            lines.push_back(block.substr(lstart, lend == std::string_view::npos ? std::string_view::npos : lend - lstart));
            if (lend == std::string_view::npos) break;
            lstart = lend + 1;
        }

        std::vector<Element> currentList;
        std::string currentPara;

        auto flushList = [&] {
            if (!currentList.empty()) {
                out.push_back(h("ul", {}, std::exchange(currentList, {})));
            }
        };
        auto flushPara = [&] {
            if (!currentPara.empty()) {
                out.push_back(h("p", {}, formatInline(std::exchange(currentPara, {}))));
            }
        };

        for (std::string_view line : lines) {
            auto trimmed = line;
            while (!trimmed.empty() && trimmed.front() == ' ') trimmed.remove_prefix(1);

            bool isBullet = false;
            std::string_view content;
            if (trimmed.starts_with("- ") || trimmed.starts_with("* ")) {
                isBullet = true;
                content = trimmed.substr(2);
            } else if (trimmed.starts_with("• ")) {
                isBullet = true;
                content = trimmed.substr(std::string_view("• ").size());
            }

            if (isBullet) {
                flushPara();
                currentList.push_back(h("li", {}, formatInline(content)));
            } else {
                flushList();
                if (!currentPara.empty()) currentPara += ' ';
                currentPara += line;
            }
        }
        flushPara();
        flushList();

        if (end == std::string::npos) break;
        start = end + 2;
    }
    return out;
}

Element DocsIndex(bool en) {
    std::vector<Element> blocks;
    for (const auto& g : groups()) {
        for (auto slug : g.slugs) {
            const auto* page = findPage(slug, en);
            if (!page) continue;
            blocks.push_back(router::link({.to = "/docs/" + std::string(page->slug),
                                           .children = {h("small", {}, {std::string(en ? g.en : g.ru)}), h("b", {}, {std::string(page->title)}),
                                                        h("span", {}, {std::string(page->summary)})},
                                           .className = "doc-card", .activeClassName = "", .key = std::string(page->slug)}));
        }
    }
    return h("div", {{"className", "article"}}, {
        h("div", {{"className", "crumbs"}}, {"~/docs"}),
        h("h1", {}, {tr(en, "Documentation", "Документация")}),
        h("p", {{"className", "lead"}}, {tr(en, "Everything ReactX has, from your first component to a C++ server. New here? Start with Getting started.",
                                            "Всё, что есть в ReactX: от первого компонента до сервера на C++. Впервые здесь? Начните с «Быстрого старта».")}),
        h("div", {{"className", "doc-index"}}, std::move(blocks)),
    });
}

Element Docs(const Props&) {
    bool en = useEnglish();
    std::string slug = router::useParam("page");
    const auto* page = findPage(slug, en);
    useTitle(page ? std::string(page->title) + " — ReactX" : tr(en, "Documentation — ReactX", "Документация — ReactX"));

    Element body;
    Element toc = fragment({});
    if (slug.empty()) {
        body = DocsIndex(en);
    } else if (!page) {
        body = h("div", {{"className", "article"}}, {
            h("h1", {}, {tr(en, "No such page", "Нет такого раздела")}),
            h("p", {{"className", "lead"}}, {tr(en, "There is no page \"" + slug + "\". Pick another one in the menu.",
                                                "Раздела «" + slug + "» нет. Выберите другой в меню.")}),
            router::link({.to = "/docs", .label = tr(en, "← All docs", "← Вся документация"), .className = "btn", .activeClassName = ""}),
        });
    } else {
        const auto& all = docPages(en);
        std::vector<Element> article{
            h("div", {{"className", "crumbs"}}, {router::link({.to = "/docs", .label = "~/docs", .activeClassName = ""}),
                                                 " / " + groupOf(slug, en)}),
            h("h1", {}, {std::string(page->title)}),
            h("p", {{"className", "lead"}}, {std::string(page->summary)}),
        };
        std::vector<std::string> headings;
        for (std::size_t i = 0; i < page->sections.size(); ++i) {
            const auto& s = page->sections[i];
            std::vector<Element> section{h("h2", {}, {std::string(s.heading)})};
            for (auto& elem : renderSectionText(std::string(s.text))) section.push_back(std::move(elem));
            if (!s.code.empty()) section.push_back(h(CodeBlock, {.code = std::string(s.code)}));
            article.push_back(h("section", {{"id", "s-" + std::to_string(i)}, {"key", slug + std::string(s.heading)}}, std::move(section)));
            headings.push_back(std::string(s.heading));
        }
        auto it = std::ranges::find(all, slug, &site_docs::Page::slug);
        std::vector<Element> pager;
        if (it != all.begin())
            pager.push_back(router::link({.to = "/docs/" + std::string((it - 1)->slug),
                                          .children = {h("small", {}, {tr(en, "← previous", "← назад")}), h("b", {}, {std::string((it - 1)->title)})},
                                          .className = "prev", .activeClassName = "", .key = "prev"}));
        if (it + 1 != all.end())
            pager.push_back(router::link({.to = "/docs/" + std::string((it + 1)->slug),
                                          .children = {h("small", {}, {tr(en, "next →", "дальше →")}), h("b", {}, {std::string((it + 1)->title)})},
                                          .className = "next", .activeClassName = "", .key = "next"}));
        article.push_back(h("nav", {{"className", "pager"}, {"aria-label", tr(en, "Previous and next", "Соседние разделы")}}, std::move(pager)));
        body = h("article", {{"className", "article"}, {"key", slug}}, std::move(article));
        toc = h(Toc, {.headings = std::move(headings)});
    }

    return h("div", {{"className", "wrap"}}, {
        h("div", {{"className", "docs"}}, {
            h("div", {}, {h(Sidebar, {.current = slug})}),
            std::move(body),
            h("div", {}, {std::move(toc)}),
        }),
    });
}

// ---- other pages ----

Element Demos(const Props&) {
    bool en = useEnglish();
    useTitle(tr(en, "Demos — ReactX", "Демо — ReactX"));
    auto demo = [en](const char* file, std::string title, std::string text) {
        // Plain <a>: separate pages, not routes of this app.
        return h("a", {{"className", "doc-card"}, {"href", file}, {"key", file}},
                 {h("small", {}, {tr(en, "demo", "демо")}), h("b", {}, {title}), h("span", {}, {text})});
    };
    return h("div", {{"className", "wrap page"}}, {
        h("h1", {}, {tr(en, "Demos", "Демо")}),
        h("p", {{"className", "lead"}}, {tr(en, "Separate pages, each one ReactX in WebAssembly.", "Отдельные страницы — каждая это ReactX в WebAssembly.")}),
        h("div", {{"className", "doc-index"}}, {
            demo("web_counter.html", tr(en, "Counter", "Счётчик"), tr(en, "useState and events", "useState и события")),
            demo("web_ui_demo.html", tr(en, "rx::ui components", "Компоненты rx::ui"),
                 tr(en, "tabs, a list, an input, a theme, a dialog", "вкладки, список, поле ввода, тема, модальное окно")),
            demo("web_kanban.html", tr(en, "Kanban: drag and drop", "Канбан: перетаскивание"),
                 tr(en, "pointer events and hover; the same source runs in the terminal with the mouse",
                    "события указателя и наведение; тот же код работает в терминале с мышью")),
            demo("web_chart.html", tr(en, "Chart.js from C++", "Chart.js из C++"),
                 tr(en, "a JavaScript library on C++ state: useNodeRef and rx::domElement",
                    "JavaScript-библиотека на состоянии из C++: useNodeRef и rx::domElement")),
        }),
    });
}

Element NotFound(const Props&) {
    bool en = useEnglish();
    useTitle("404 — ReactX");
    return h("div", {{"className", "wrap page"}}, {
        h("p", {{"className", "big-404"}}, {"404"}),
        h("h1", {}, {tr(en, "No such page", "Такой страницы нет")}),
        h("p", {{"className", "lead mono"}}, {"$ cd " + router::useLocation() + "\nno such file or directory"}),
        router::link({.to = "/", .label = tr(en, "← Home", "← На главную"), .className = "btn primary", .activeClassName = ""}),
    });
}

Element Site(const Props&) {
    bool en = useEnglish();
    // Ctrl+K anywhere, "/" outside text fields: search.
    useKeyboard([](const KeyEvent& k) {
        if ((k.ctrl && k.key == "k") || (!k.ctrl && !k.alt && k.key == "/")) {
            SearchOpen.set(true);
            return true;
        }
        return false;
    });
    return fragment({
        h("header", {{"className", "header"}}, {
            h("div", {{"className", "wrap"}}, {
                router::link({.to = "/",
                              .children = {h("span", {{"className", "brand-mark"}}, {"rx"}),
                                           h("span", {{"className", "brand-text"}},
                                             {"ReactX", h("span", {{"className", "brand-cursor"}}, {"_"})})},
                              .className = "brand", .activeClassName = ""}),
                h("nav", {{"className", "topnav"}, {"aria-label", tr(en, "Main menu", "Основное меню")}}, {
                    router::link({.to = "/docs", .label = "docs", .key = "docs"}),
                    router::link({.to = "/demos", .label = "demos", .key = "demos"}),
                    h("a", {{"href", "https://github.com/sargisis/ReactX"}, {"className", "gh"}, {"key", "gh"}}, {"github"}),
                }),
                h(SearchButton),
                h(LangToggle),
                h(ThemeToggle),
            }),
        }),
        h("main", {}, {
            router::routes({
                {"/", h(Home)},
                {"/docs", h(Docs)},
                {"/docs/:page", h(Docs)},
                {"/demos", h(Demos)},
                {"*", h(NotFound)},
            }),
        }),
        h("footer", {{"className", "footer"}}, {
            h("div", {{"className", "wrap"}}, {
                h("span", {}, {"ReactX v" + std::string(rx::version()) + " · C++26 → WebAssembly"}),
                h("span", {}, {tr(en, "This site is built with ReactX · ", "Этот сайт написан на ReactX · "),
                               h("a", {{"href", "https://github.com/sargisis/ReactX"}}, {tr(en, "source", "исходники")})}),
            }),
        }),
        h(SearchDialog),
        ui::toaster(),
    });
}

}  // namespace

#ifndef REACTX_SITE_PRERENDER
// The app is running: the shell's "buttons don't work" notice stays away.
EM_JS(void, site_js_live, (), {
    document.documentElement.dataset.rxLive = "1";
    const d = document.getElementById("rx-fail");
    if (d) d.remove();
});

int main() {
    char* lang = site_js_lang();
    SiteLang.set(lang);
    std::free(lang);
    mountDom(h(Site));
    site_js_live();
}
#else
// site_prerender <shell.html> <out/index.html>: every page that has a fixed
// address — the home page into index.html, the docs into docs/index.html and
// docs/<page>.html — rendered in English and in Russian, in place of the
// loading text. A small script picks the language before the first paint; the
// app then replaces it with the same markup, so pages show long before
// index.wasm arrives (and search engines read them).
std::string titleOf(std::string_view route, bool en) {
    if (route == "/") return tr(en, "ReactX — React for C++26", "ReactX — React на C++26");
    if (route == "/docs") return tr(en, "Documentation — ReactX", "Документация — ReactX");
    const auto* page = findPage(route.substr(std::string_view("/docs/").size()), en);
    return page ? std::string(page->title) + " — ReactX" : "ReactX";
}

// "Hooks — ReactX" as a JavaScript string literal.
std::string jsString(std::string_view s) {
    std::string out = "\"";
    for (char c : s) out += c == '"' || c == '\\' ? std::string("\\") + c : std::string(1, c);
    return out + "\"";
}

int main(int argc, char** argv) {
    if (argc != 3) {
        std::fputs("usage: site_prerender <shell.html> <out/index.html>\n", stderr);
        return 2;
    }
    std::ifstream in(argv[1], std::ios::binary);
    std::string shell{std::istreambuf_iterator<char>(in), {}};
    // Emscripten minifies the shell in Release builds: quotes may be gone.
    std::size_t at = shell.find(R"(<div id="app">)");
    if (at == std::string::npos) at = shell.find("<div id=app>");
    constexpr std::string_view kBootEnd = "loading ReactX</div></div>";
    std::size_t bootEnd = at == std::string::npos ? at : shell.find(kBootEnd, at);
    std::size_t baseAt = shell.find("<base href=");
    if (!in || bootEnd == std::string::npos || baseAt == std::string::npos) {
        std::fputs("site_prerender: no <base href> or loading text in the shell\n", stderr);
        return 1;
    }
    baseAt += std::string_view("<base href=").size();
    if (shell[baseAt] == '"') ++baseAt;
    router::setBase(shell.substr(baseAt, shell.find_first_of("\" >", baseAt) - baseAt));

    std::filesystem::path outDir = std::filesystem::path(argv[2]).parent_path();
    std::vector<std::pair<std::string, std::filesystem::path>> routes{{"/", "index.html"}, {"/docs", "docs/index.html"}};
    for (const auto& page : site_docs::pagesEn())
        routes.emplace_back("/docs/" + std::string(page.slug), "docs/" + std::string(page.slug) + ".html");

    for (const auto& [route, file] : routes) {
        router::reset(route);
        SiteLang.set("en");
        std::string en = renderToString(h(Site));
        SiteLang.set("ru");
        std::string ru = renderToString(h(Site));

        std::string page = shell;
        std::string html = R"(<div id="app">)" + en + R"(</div><template id="rx-prerender-ru">)" + ru +
                           R"js(</template><script>
    // The prerendered page in the visitor's language and theme (main() in site.cpp does the same).
    (function () {
      var l = null;
      try { l = localStorage.getItem("rx-lang"); } catch (e) {}
      if (l !== "ru" && l !== "en") l = (navigator.language || "").toLowerCase().indexOf("ru") === 0 ? "ru" : "en";
      var app = document.getElementById("app");
      if (l === "ru") {
        document.documentElement.lang = "ru";
        document.title = )js" + jsString(titleOf(route, false)) + R"js(;
        app.innerHTML = document.getElementById("rx-prerender-ru").innerHTML;
      }
      var t = document.documentElement.dataset.theme || (matchMedia("(prefers-color-scheme: dark)").matches ? "dark" : "light");
      var b = app.querySelector(".icon-btn:not(.lang-btn)");
      if (t === "dark" && b) { b.textContent = "☀"; b.title = l === "ru" ? "Светлая тема" : "Light theme"; }
    })();
  </script>)js";
        page.replace(at, bootEnd + kBootEnd.size() - at, html);
        // The page's own title, and rx::ui's stylesheet (live demos, search) so it looks right before the .wasm arrives.
        if (auto t = page.find("<title>"), e = page.find("</title>"); t != std::string::npos && e != std::string::npos)
            page.replace(t + 7, e - t - 7, titleOf(route, true));
        if (auto head = page.find("</head>"); head != std::string::npos) page.insert(head, detail::prerenderHead());

        std::filesystem::create_directories((outDir / file).parent_path());
        std::ofstream out(outDir / file, std::ios::binary);
        out << page;
        if (!out.flush()) {
            std::fprintf(stderr, "site_prerender: can't write %s\n", (outDir / file).c_str());
            return 1;
        }
    }
}
#endif
