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
#include "site_docs_en.hpp"

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

std::string currentTheme() {
    char* t = site_js_theme();
    std::string out(t);
    std::free(t);
    return out;
}

// The site language, shared by every component ("en" or "ru"); main() sets it.
const auto SiteLang = createStore(std::string("en"));

bool useEnglish() { return useStore(SiteLang) == "en"; }

// The text in the current language.
std::string tr(bool en, std::string_view english, std::string_view russian) {
    return std::string(en ? english : russian);
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
    bool en = useEnglish();
    Lang lang = detect(p.code);
    const auto& tokens = useMemo([&] { return Highlighter(lang).run(p.code); }, deps(p.code));

    std::vector<Element> copy{tr(en, "copy", "копировать")};
    // A new key on each click restarts the "скопировано" animation.
    if (copies > 0) copy.push_back(h("span", {{"className", "copied"}, {"key", std::to_string(copies)}}, {tr(en, "copied", "скопировано")}));
    return h("figure", {{"className", "code"}}, {
        h("div", {{"className", "code-bar"}}, {
            h("span", {}, {p.title.empty() ? std::string(langName(lang)) : p.title}),
            h("button", {{"className", "copy"},
                         {"aria-label", tr(en, "Copy the code", "Скопировать код")},
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

Element Feature(std::string title, std::vector<Element> text) {
    return h("div", {{"className", "feature"}, {"key", title}}, {h("b", {}, {title}), h("p", {}, std::move(text))});
}

Element code(std::string s) { return h("code", {}, {std::move(s)}); }

Element Home(const Props&) {
    bool en = useEnglish();
    useTitle(tr(en, "ReactX — React for C++26", "ReactX — React на C++26"));
    return h("div", {{"className", "wrap"}}, {
        h("section", {{"className", "hero"}}, {
            h("div", {}, {
                h("span", {{"className", "eyebrow"}}, {h("b", {}, {"●"}), std::format("v{} · C++26 · WebAssembly", rx::version())}),
                en ? h("h1", {}, {"React", h("br"), "for ", h("em", {}, {"C++26"}), "."})
                   : h("h1", {}, {"React", h("br"), "на ", h("em", {}, {"C++26"}), "."}),
                h("p", {{"className", "lead"}},
                  {tr(en, "Components, hooks and a reconciler — like React, in C++. The same code draws in the terminal and in the browser.",
                      "Компоненты, хуки и reconciler — как в React. Один и тот же код рисуется в терминале и в браузере.")}),
                h("div", {{"className", "btn-row"}}, {
                    router::link({.to = "/docs/start", .label = tr(en, "Get started →", "Быстрый старт →"), .className = "btn primary", .activeClassName = ""}),
                    h("a", {{"className", "btn"}, {"href", "https://github.com/sargisis/ReactX"}}, {"GitHub"}),
                }),
            }),
            h(LiveCounter, {.start = 3}),
        }),

        h("section", {{"className", "section"}}, {
            h("div", {{"className", "section-head"}}, {h("h2", {}, {tr(en, "What's inside", "Что внутри")}),
                                                       h("span", {}, {tr(en, "all C++, no JavaScript", "всё на C++, без JavaScript")})}),
            h("div", {{"className", "features"}}, {
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
            }),
        }),

        h("section", {{"className", "section"}}, {
            h("div", {{"className", "section-head"}}, {h("h2", {}, {tr(en, "Get started", "Быстрый старт")}), h("span", {}, {tr(en, "three commands", "три команды")})}),
            h("div", {{"className", "steps"}}, {
                h("div", {{"className", "step"}, {"key", "1"}}, {
                    h("div", {}, {h("h3", {}, {tr(en, "Install reactx", "Установите reactx")}),
                                  h("p", {}, {tr(en, "Download it from GitHub Releases (Linux x86-64). Browser apps also need Emscripten.",
                                                 "Скачайте из GitHub Releases (Linux x86-64). Для браузера нужен ещё Emscripten.")}),}),
                    h(CodeBlock, {.code = "# github.com/sargisis/ReactX/releases\n"
                                          "tar xzf reactx-*-linux-x86_64.tar.gz\n"
                                          "cp reactx-*/bin/* ~/.local/bin/"}),
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
        {"Apps", "Приложения", {"router", "data", "server"}},
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

struct SidebarProps {
    std::string current{};  // slug of the open page
};

Element Sidebar(const SidebarProps& p) {
    auto [query, setQuery] = useState(std::string{});
    auto [open, setOpen] = useState(false);
    bool en = useEnglish();
    std::string location = router::useLocation();
    useEffect([setOpen] { setOpen(false); }, deps(location));  // close the mobile menu after a click

    std::string q = lower(query);
    std::vector<Element> blocks;
    for (const auto& g : groups()) {
        std::vector<Element> links;
        for (auto slug : g.slugs) {
            const auto* page = findPage(slug, en);
            if (!page) continue;
            if (!q.empty() && lower(page->title).find(q) == std::string::npos && lower(page->summary).find(q) == std::string::npos)
                continue;
            links.push_back(router::link({.to = "/docs/" + page->slug, .label = page->title, .key = page->slug}));
        }
        if (links.empty()) continue;
        std::string title(en ? g.en : g.ru);
        std::vector<Element> group{h("div", {{"className", "group-title"}}, {title})};
        for (auto& l : links) group.push_back(std::move(l));
        blocks.push_back(h("div", {{"className", "group"}, {"key", title}}, std::move(group)));
    }
    if (blocks.empty()) blocks.push_back(h("div", {{"className", "nothing"}}, {tr(en, "nothing found", "ничего не найдено")}));

    const auto* page = findPage(p.current, en);
    return fragment({
        h("button", {{"className", "menu-btn"}, {"aria-expanded", open ? "true" : "false"},
                     {"onClick", [open, setOpen] { setOpen(!open); }}},
          {page ? page->title : tr(en, "Sections", "Разделы"), h("span", {}, {open ? "✕" : "☰"})}),
        h("aside", {{"className", open ? "sidebar open" : "sidebar"}, {"aria-label", tr(en, "Documentation sections", "Разделы документации")}}, {
            h("div", {{"className", "search-wrap"}}, {
                h("input", {{"className", "search"}, {"type", "text"}, {"placeholder", tr(en, "search", "поиск")}, {"value", query},
                            {"aria-label", tr(en, "Search the docs", "Поиск по разделам")},
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
    bool en = useEnglish();
    std::string title = tr(en, "On this page", "На странице");
    std::vector<Element> items{h("div", {{"className", "toc-title"}}, {title})};
    for (std::size_t i = 0; i < p.headings.size(); ++i) {
        std::string id = "s-" + std::to_string(i);
        items.push_back(h("button", {{"key", id}, {"onClick", [id] { site_js_scroll_to(id.c_str()); }}}, {p.headings[i]}));
    }
    return h("nav", {{"className", "toc"}, {"aria-label", title}}, std::move(items));
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

Element DocsIndex(bool en) {
    std::vector<Element> blocks;
    for (const auto& g : groups()) {
        for (auto slug : g.slugs) {
            const auto* page = findPage(slug, en);
            if (!page) continue;
            blocks.push_back(router::link({.to = "/docs/" + page->slug,
                                           .children = {h("small", {}, {std::string(en ? g.en : g.ru)}), h("b", {}, {page->title}),
                                                        h("span", {}, {page->summary})},
                                           .className = "doc-card", .activeClassName = "", .key = page->slug}));
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
    useTitle(page ? page->title + " — ReactX" : tr(en, "Documentation — ReactX", "Документация — ReactX"));

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
                                          .children = {h("small", {}, {tr(en, "← previous", "← назад")}), h("b", {}, {(it - 1)->title})},
                                          .className = "prev", .activeClassName = "", .key = "prev"}));
        if (it + 1 != all.end())
            pager.push_back(router::link({.to = "/docs/" + (it + 1)->slug,
                                          .children = {h("small", {}, {tr(en, "next →", "дальше →")}), h("b", {}, {(it + 1)->title})},
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
                h("span", {}, {std::format("ReactX v{} · C++26 → WebAssembly", rx::version())}),
                h("span", {}, {tr(en, "This site is built with ReactX · ", "Этот сайт написан на ReactX · "),
                               h("a", {{"href", "https://github.com/sargisis/ReactX"}}, {tr(en, "source", "исходники")})}),
            }),
        }),
    });
}

}  // namespace

int main() {
    char* lang = site_js_lang();
    SiteLang.set(lang);
    std::free(lang);
    site_js_remove_boot();
    mountDom(h(Site));
}
