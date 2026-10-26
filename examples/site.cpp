// The ReactX website (https://sargisis.github.io/ReactX/), written in ReactX:
//
//   /              about ReactX: a live counter drawn for the browser and
//                  for the terminal, features, quick start
//   /docs[/:page]  documentation (texts in examples/site_docs.hpp)
//   /demos         links to the demo pages
//   anything else  404
//
// Built by the "pages" preset into build/pages/site with <base href="/ReactX/">.
// The look is in web/site_shell.html; this file is only ReactX components
// (most of them with props structs: h(CodeBlock, {.code = ...})).
#include <emscripten.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdlib>
#include <format>
#include <string>
#include <string_view>
#include <vector>

#include "reactx/dom.hpp"
#include "reactx/reactx.hpp"
#include "reactx/router.hpp"
#include "reactx/version.hpp"
#include "site_docs.hpp"

using namespace rx;

namespace {

// ---- the few things only JavaScript can do ----

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
EM_JS(void, site_js_title, (const char* title), { document.title = UTF8ToString(title); });
EM_JS(void, site_js_remove_boot, (), {
    const boot = document.querySelector("#app > .boot");
    if (boot) boot.remove();
});

std::string currentTheme() {
    char* t = site_js_theme();
    std::string out(t);
    std::free(t);
    return out;
}

void useTitle(const std::string& title) {
    useEffect([title] { site_js_title(title.c_str()); }, deps(title));
}

// ---- syntax highlighting (a small lexer; tokens become <span>s) ----

enum class Lang { Cpp, Shell, CMake };

Lang detect(std::string_view code) {
    for (std::string_view word : {"target_link_libraries", "find_package", "reactx_add_app", "FetchContent"})
        if (code.find(word) != std::string_view::npos && code.find(';') == std::string_view::npos) return Lang::CMake;
    std::string_view first = code.substr(0, code.find('\n'));
    for (std::string_view cmd : {"cmake ", "reactx ", "./", "git ", "cd ", "emcmake ", "source ", "sudo ", "ctest "})
        if (first.starts_with(cmd)) return Lang::Shell;
    return Lang::Cpp;
}

std::string_view langName(Lang lang) {
    switch (lang) {
        case Lang::Shell: return "shell";
        case Lang::CMake: return "cmake";
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

bool isKeyword(std::string_view word) { return std::ranges::find(kKeywords, word) != kKeywords.end(); }

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
            if (lang_ == Lang::CMake && c == '#') {
                auto end = rest.find('\n');
                token("t-com", rest.substr(0, end));
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
            } else if (c == '"' || (c == '\'' && lang_ == Lang::Cpp)) {
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
    auto [copies, setCopies] = useState(0);
    Lang lang = detect(p.code);
    const auto& tokens = useMemo([&] { return Highlighter(lang).run(p.code); }, deps(p.code));

    std::vector<Element> copy{"копировать"};
    // A new key on each click restarts the "скопировано" animation.
    if (copies > 0) copy.push_back(h("span", {{"className", "copied"}, {"key", std::to_string(copies)}}, {"скопировано"}));
    return h("figure", {{"className", "code"}}, {
        h("div", {{"className", "code-bar"}}, {
            h("span", {}, {p.title.empty() ? std::string(langName(lang)) : p.title}),
            h("button", {{"className", "copy"},
                         {"aria-label", "Скопировать код"},
                         {"onClick", [code = p.code, setCopies] {
                              site_js_copy(code.c_str());
                              setCopies([](int n) { return n + 1; });
                          }}},
              std::move(copy)),
        }),
        h("pre", {}, {h("code", {}, tokens)}),
    });
}

Element ThemeToggle(const Props&) {
    auto [theme, setTheme] = useState(currentTheme());
    bool dark = theme == "dark";
    return h("button", {{"className", "icon-btn"},
                        {"title", dark ? "Светлая тема" : "Тёмная тема"},
                        {"aria-label", "Переключить тему"},
                        {"onClick", [dark, setTheme] {
                             std::string next = dark ? "light" : "dark";
                             site_js_set_theme(next.c_str());
                             setTheme(next);
                         }}},
             {dark ? "☀" : "☾"});
}

// ---- home ----

struct LiveCounterProps {
    int start = 0;
};

// The same counter drawn twice: as the browser shows it (a real ReactX
// button) and as TerminalRenderer would draw it in a terminal.
Element LiveCounter(const LiveCounterProps& p) {
    auto [count, setCount] = useState(p.start);
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
    std::string label = std::format("Count: {}", count);
    return h("div", {{"className", "demo-pair"}}, {
        h(CodeBlock, {.code = "struct CounterProps { int start = 0; };\n\n"
                              "Element Counter(const CounterProps& p) {\n"
                              "    auto [count, setCount] = useState(p.start);\n"
                              "    return h(\"box\", {}, {\n"
                              "        h(\"text\", {}, {std::format(\"Count: {}\", count)}),\n"
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
                    "браузер",
                    h("span", {{"className", "tag"}}, {"DomRenderer"}),
                }),
                h("div", {{"className", "browser-view"}}, {
                    h("div", {{"className", "count"}, {"aria-live", "polite"}}, {std::to_string(count)}),
                    h("div", {{"className", "btn-row"}}, {
                        h("button", {{"className", "btn primary"}, {"onClick", inc}}, {"+1"}),
                        h("button", {{"className", "btn"}, {"onClick", reset}}, {"reset"}),
                    }),
                    h("div", {{"className", "hint"}}, {"нажмите — обновятся оба"}),
                }),
            }),
            h("div", {{"className", "panel"}}, {
                h("div", {{"className", "panel-bar"}}, {
                    h("span", {{"className", "dots"}}, {h("i"), h("i"), h("i")}),
                    "терминал",
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

Element Feature(std::string title, std::vector<Element> text) {
    return h("div", {{"className", "feature"}, {"key", title}}, {h("b", {}, {title}), h("p", {}, std::move(text))});
}

Element code(std::string s) { return h("code", {}, {std::move(s)}); }

Element Home(const Props&) {
    useTitle("ReactX — React на C++26");
    return h("div", {{"className", "wrap"}}, {
        h("section", {{"className", "hero"}}, {
            h("div", {}, {
                h("span", {{"className", "eyebrow"}}, {h("b", {}, {"●"}), std::format("v{} · C++26 · WebAssembly", rx::version())}),
                h("h1", {}, {"React", h("br"), "на ", h("em", {}, {"C++26"}), "."}),
                h("p", {{"className", "lead"}},
                  {"Компоненты, хуки и reconciler — как в React. Один и тот же код рисуется в терминале и в браузере."}),
                h("div", {{"className", "btn-row"}}, {
                    router::link({.to = "/docs/start", .label = "Быстрый старт →", .className = "btn primary", .activeClassName = ""}),
                    h("a", {{"className", "btn"}, {"href", "https://github.com/sargisis/ReactX"}}, {"GitHub"}),
                }),
            }),
            h(LiveCounter, {.start = 3}),
        }),

        h("section", {{"className", "section"}}, {
            h("div", {{"className", "section-head"}}, {h("h2", {}, {"Что внутри"}), h("span", {}, {"всё на C++, без JavaScript"})}),
            h("div", {{"className", "features"}}, {
                Feature("Хуки", {code("useState"), " ", code("useEffect"), " ", code("useMemo"), " ", code("useCallback"), " ",
                                 code("useContext"), " — как в React."}),
                Feature("Props — структура", {code("h(Counter, {.start = 5})"), ": опечатку в имени поля ловит компилятор."}),
                Feature("Два бэкенда", {"Терминал (ANSI, Flexbox на Yoga) и браузер (DOM через WebAssembly)."}),
                Feature("Роутер и данные", {"Страницы, ссылки, ", code("useFetch"), ", свой JSON с рефлексией C++26."}),
                Feature("Сервер на C++", {"HTTP-сервер с маршрутами; CLI ", code("reactx dev"), " пересобирает и обновляет страницу."}),
                Feature("Современный C++", {"Контракты, ", code("std::expected"), ", ", code("std::mdspan"), ", ", code("std::flat_map"),
                                            ", модуль ", code("import reactx;"), "."}),
            }),
        }),

        h("section", {{"className", "section"}}, {
            h("div", {{"className", "section-head"}}, {h("h2", {}, {"Быстрый старт"}), h("span", {}, {"три команды"})}),
            h("div", {{"className", "steps"}}, {
                h("div", {{"className", "step"}, {"key", "1"}}, {
                    h("div", {}, {h("h3", {}, {"Установите CLI"}), h("p", {}, {"Нужны GCC 16, CMake и Ninja; для браузера — Emscripten. Один раз."}),}),
                    h(CodeBlock, {.code = "cmake --preset release -DCMAKE_CXX_COMPILER=g++-16\n"
                                          "cmake --build --preset release\n"
                                          "cmake --install build/release --prefix ~/.local"}),
                }),
                h("div", {{"className", "step"}, {"key", "2"}}, {
                    h("div", {}, {h("h3", {}, {"Создайте проект"}), h("p", {}, {"Веб-приложение или, с --terminal, программа для терминала."}),}),
                    h(CodeBlock, {.code = "reactx new my-app\ncd my-app"}),
                }),
                h("div", {{"className", "step"}, {"key", "3"}}, {
                    h("div", {}, {h("h3", {}, {"Запустите"}), h("p", {}, {"Сервер на C++ пересобирает и обновляет страницу при каждом сохранении."}),}),
                    h(CodeBlock, {.code = "reactx dev"}),
                }),
            }),
        }),

        h("section", {{"className", "section"}}, {
            h("div", {{"className", "cta"}}, {
                h("div", {}, {h("h2", {}, {"Этот сайт написан на ReactX"}),
                              h("p", {}, {"Роутер, компоненты и подсветка кода — C++, скомпилированный в WebAssembly."})}),
                router::link({.to = "/docs", .label = "Документация →", .className = "btn primary", .activeClassName = ""}),
            }),
        }),
    });
}

// ---- docs ----

struct Group {
    std::string_view title;
    std::vector<std::string_view> slugs;
};

const std::vector<Group>& groups() {
    static const std::vector<Group> all = {
        {"Начало", {"start", "cli", "versions", "safety"}},
        {"Основы", {"elements", "hooks", "context", "events"}},
        {"Платформы", {"terminal", "browser", "ui"}},
        {"Приложения", {"router", "data", "server"}},
    };
    return all;
}

const site_docs::Page* findPage(std::string_view slug) {
    const auto& all = site_docs::pages();
    auto it = std::ranges::find(all, slug, &site_docs::Page::slug);
    return it == all.end() ? nullptr : &*it;
}

std::string_view groupOf(std::string_view slug) {
    for (const auto& g : groups())
        if (std::ranges::find(g.slugs, slug) != g.slugs.end()) return g.title;
    return "Документация";
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

struct SidebarProps {
    std::string current{};  // slug of the open page
};

Element Sidebar(const SidebarProps& p) {
    auto [query, setQuery] = useState(std::string{});
    auto [open, setOpen] = useState(false);
    std::string location = router::useLocation();
    useEffect([setOpen] { setOpen(false); }, deps(location));  // close the mobile menu after a click

    std::string q = lower(query);
    std::vector<Element> blocks;
    for (const auto& g : groups()) {
        std::vector<Element> links;
        for (auto slug : g.slugs) {
            const auto* page = findPage(slug);
            if (!page) continue;
            if (!q.empty() && lower(page->title).find(q) == std::string::npos && lower(page->summary).find(q) == std::string::npos)
                continue;
            links.push_back(router::link({.to = "/docs/" + page->slug, .label = page->title, .key = page->slug}));
        }
        if (links.empty()) continue;
        std::vector<Element> group{h("div", {{"className", "group-title"}}, {std::string(g.title)})};
        for (auto& l : links) group.push_back(std::move(l));
        blocks.push_back(h("div", {{"className", "group"}, {"key", std::string(g.title)}}, std::move(group)));
    }
    if (blocks.empty()) blocks.push_back(h("div", {{"className", "nothing"}}, {"ничего не найдено"}));

    const auto* page = findPage(p.current);
    return fragment({
        h("button", {{"className", "menu-btn"}, {"aria-expanded", open ? "true" : "false"},
                     {"onClick", [open, setOpen] { setOpen(!open); }}},
          {page ? page->title : std::string("Разделы"), h("span", {}, {open ? "✕" : "☰"})}),
        h("aside", {{"className", open ? "sidebar open" : "sidebar"}, {"aria-label", "Разделы документации"}}, {
            h("div", {{"className", "search-wrap"}}, {
                h("input", {{"className", "search"}, {"type", "text"}, {"placeholder", "поиск"}, {"value", query},
                            {"aria-label", "Поиск по разделам"},
                            {"onInput", [setQuery](const std::string& text) { setQuery(text); }}}),
            }),
            fragment(std::move(blocks)),
        }),
    });
}

struct TocProps {
    std::vector<std::string> headings{};
};

// "On this page": buttons that scroll to the sections (s-0, s-1, ...).
Element Toc(const TocProps& p) {
    std::vector<Element> items{h("div", {{"className", "toc-title"}}, {"На странице"})};
    for (std::size_t i = 0; i < p.headings.size(); ++i) {
        std::string id = "s-" + std::to_string(i);
        items.push_back(h("button", {{"key", id}, {"onClick", [id] { site_js_scroll_to(id.c_str()); }}}, {p.headings[i]}));
    }
    return h("nav", {{"className", "toc"}, {"aria-label", "На странице"}}, std::move(items));
}

std::vector<Element> paragraphs(const std::string& text) {
    std::vector<Element> out;
    for (std::size_t start = 0; start < text.size();) {
        auto end = text.find("\n\n", start);
        out.push_back(h("p", {}, {text.substr(start, end - start)}));
        if (end == std::string::npos) break;
        start = end + 2;
    }
    return out;
}

Element DocsIndex() {
    std::vector<Element> blocks;
    for (const auto& g : groups()) {
        for (auto slug : g.slugs) {
            const auto* page = findPage(slug);
            if (!page) continue;
            blocks.push_back(router::link({.to = "/docs/" + page->slug,
                                           .children = {h("small", {}, {std::string(g.title)}), h("b", {}, {page->title}),
                                                        h("span", {}, {page->summary})},
                                           .className = "doc-card", .activeClassName = "", .key = page->slug}));
        }
    }
    return h("div", {{"className", "article"}}, {
        h("div", {{"className", "crumbs"}}, {"~/docs"}),
        h("h1", {}, {"Документация"}),
        h("p", {{"className", "lead"}}, {"Всё, что есть в ReactX: от первого компонента до сервера на C++."}),
        h("div", {{"className", "doc-index"}}, std::move(blocks)),
    });
}

Element Docs(const Props&) {
    std::string slug = router::useParam("page");
    const auto* page = findPage(slug);
    useTitle(page ? page->title + " — ReactX" : std::string("Документация — ReactX"));

    Element body;
    Element toc = fragment({});
    if (slug.empty()) {
        body = DocsIndex();
    } else if (!page) {
        body = h("div", {{"className", "article"}}, {
            h("h1", {}, {"Нет такого раздела"}),
            h("p", {{"className", "lead"}}, {"Раздела «" + slug + "» нет. Выберите другой в меню."}),
            router::link({.to = "/docs", .label = "← Вся документация", .className = "btn", .activeClassName = ""}),
        });
    } else {
        const auto& all = site_docs::pages();
        std::vector<Element> article{
            h("div", {{"className", "crumbs"}}, {router::link({.to = "/docs", .label = "~/docs", .activeClassName = ""}),
                                                 " / " + std::string(groupOf(slug))}),
            h("h1", {}, {page->title}),
            h("p", {{"className", "lead"}}, {page->summary}),
        };
        std::vector<std::string> headings;
        for (std::size_t i = 0; i < page->sections.size(); ++i) {
            const auto& s = page->sections[i];
            std::vector<Element> section{h("h2", {}, {s.heading})};
            for (auto& para : paragraphs(s.text)) section.push_back(std::move(para));
            if (!s.code.empty()) section.push_back(h(CodeBlock, {.code = s.code}));
            article.push_back(h("section", {{"id", "s-" + std::to_string(i)}, {"key", slug + s.heading}}, std::move(section)));
            headings.push_back(s.heading);
        }
        auto it = std::ranges::find(all, slug, &site_docs::Page::slug);
        std::vector<Element> pager;
        if (it != all.begin())
            pager.push_back(router::link({.to = "/docs/" + (it - 1)->slug,
                                          .children = {h("small", {}, {"← назад"}), h("b", {}, {(it - 1)->title})},
                                          .className = "prev", .activeClassName = "", .key = "prev"}));
        if (it + 1 != all.end())
            pager.push_back(router::link({.to = "/docs/" + (it + 1)->slug,
                                          .children = {h("small", {}, {"дальше →"}), h("b", {}, {(it + 1)->title})},
                                          .className = "next", .activeClassName = "", .key = "next"}));
        article.push_back(h("nav", {{"className", "pager"}, {"aria-label", "Соседние разделы"}}, std::move(pager)));
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
    useTitle("Демо — ReactX");
    auto demo = [](const char* file, const char* title, const char* text) {
        // Plain <a>: separate pages, not routes of this app.
        return h("a", {{"className", "doc-card"}, {"href", file}, {"key", file}},
                 {h("small", {}, {"демо"}), h("b", {}, {title}), h("span", {}, {text})});
    };
    return h("div", {{"className", "wrap page"}}, {
        h("h1", {}, {"Демо"}),
        h("p", {{"className", "lead"}}, {"Отдельные страницы — каждая это ReactX в WebAssembly."}),
        h("div", {{"className", "doc-index"}}, {
            demo("web_counter.html", "Счётчик", "useState и события"),
            demo("web_ui_demo.html", "Компоненты rx::ui", "вкладки, список, поле ввода, тема, модальное окно"),
        }),
    });
}

Element NotFound(const Props&) {
    useTitle("404 — ReactX");
    return h("div", {{"className", "wrap page"}}, {
        h("p", {{"className", "big-404"}}, {"404"}),
        h("h1", {}, {"Такой страницы нет"}),
        h("p", {{"className", "lead mono"}}, {"$ cd " + router::useLocation() + "\nno such file or directory"}),
        router::link({.to = "/", .label = "← На главную", .className = "btn primary", .activeClassName = ""}),
    });
}

Element Site(const Props&) {
    return fragment({
        h("header", {{"className", "header"}}, {
            h("div", {{"className", "wrap"}}, {
                router::link({.to = "/",
                              .children = {h("span", {{"className", "brand-mark"}}, {"rx"}),
                                           h("span", {{"className", "brand-text"}},
                                             {"ReactX", h("span", {{"className", "brand-cursor"}}, {"_"})})},
                              .className = "brand", .activeClassName = ""}),
                h("nav", {{"className", "topnav"}, {"aria-label", "Основное меню"}}, {
                    router::link({.to = "/docs", .label = "docs", .key = "docs"}),
                    router::link({.to = "/demos", .label = "demos", .key = "demos"}),
                    h("a", {{"href", "https://github.com/sargisis/ReactX"}, {"className", "gh"}, {"key", "gh"}}, {"github"}),
                }),
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
                h("span", {}, {std::format("ReactX v{} · C++26 → WebAssembly", rx::version())}),
                h("span", {}, {"Этот сайт написан на ReactX · ",
                               h("a", {{"href", "https://github.com/sargisis/ReactX"}}, {"исходники"})}),
            }),
        }),
    });
}

}  // namespace

int main() {
    site_js_remove_boot();
    mountDom(h(Site));
}
