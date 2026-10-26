// The ReactX website (https://sargisis.github.io/ReactX/), written in ReactX:
//
//   /              about ReactX, quick start
//   /docs[/:page]  documentation (texts in examples/site_docs.hpp)
//   /demos         links to the demo pages
//   anything else  404
//
// Built by the "pages" preset into build/pages/site with <base href="/ReactX/">.
#include <algorithm>
#include <string>
#include <vector>

#include "reactx/dom.hpp"
#include "reactx/reactx.hpp"
#include "reactx/router.hpp"
#include "reactx/ui.hpp"
#include "site_docs.hpp"

using namespace rx;

namespace {

Element page(std::vector<Element> children) { return h("div", {{"className", "page"}}, std::move(children)); }

// Paragraphs separated by blank lines.
std::vector<Element> paragraphs(const std::string& text) {
    std::vector<Element> out;
    std::size_t start = 0;
    while (start < text.size()) {
        auto end = text.find("\n\n", start);
        out.push_back(h("p", {}, {text.substr(start, end - start)}));
        if (end == std::string::npos) break;
        start = end + 2;
    }
    return out;
}

Element card(const std::string& to, const std::string& title, const std::string& text) {
    return router::link({.to = to, .children = {h("b", {}, {title}), h("span", {}, {text})},
                         .className = "doc-card", .activeClassName = "", .key = to});
}

// ---- pages ----

Element Home(const Props&) {
    auto feature = [](const char* title, const char* text) {
        return h("div", {{"className", "stat"}, {"key", title}}, {h("b", {{"className", "feature"}}, {title}), h("span", {}, {text})});
    };
    return page({
        h("h1", {}, {"ReactX — React на современном C++"}),
        h("p", {{"className", "lead"}},
          {"Компоненты, хуки и reconciler как в React — только на C++. Один и тот же код работает в терминале и в "
           "браузере (WebAssembly). Этот сайт тоже написан на ReactX."}),
        h("div", {{"className", "row"}, {"style", "flex-wrap: wrap"}}, {
            ui::button({.label = "Быстрый старт", .onClick = [] { router::navigate("/docs/start"); },
                        .variant = ui::Variant::Primary}),
            ui::button({.label = "Вся документация", .onClick = [] { router::navigate("/docs"); }}),
            ui::button({.label = "Демо", .onClick = [] { router::navigate("/demos"); }}),
        }),
        h("pre", {}, {h("code", {}, {
            "Element Counter(const Props&) {\n"
            "    auto [count, setCount] = useState(0);\n"
            "    return h(\"button\", {{\"onClick\", [setCount] { setCount([](int c) { return c + 1; }); }}},\n"
            "             {\"Нажато: \" + std::to_string(count)});\n"
            "}\n\n"
            "int main() { mountDom(h(Counter)); }   // или TerminalApp().run(h(Counter))"})}),
        h("div", {{"className", "stats"}}, {
            feature("Хуки", "useState, useEffect, useRef, useMemo, useCallback, useContext"),
            feature("Два бэкенда", "терминал (ANSI + Flexbox на Yoga) и браузер (DOM)"),
            feature("rx::ui", "кнопки, поля, вкладки, окна, списки с темой"),
            feature("Роутер и данные", "страницы, ссылки, useFetch, свой JSON"),
            feature("Сервер на C++", "HTTP-сервер с маршрутами и раздачей файлов"),
            feature("C++26", "весь C++23 и части C++26: std::expected, предусловия, hardening; модуль import reactx;"),
        }),
    });
}

Element DocsNav(const Props&) {
    std::vector<Element> links;
    for (const auto& p : site_docs::pages())
        links.push_back(router::link({.to = "/docs/" + p.slug, .label = p.title, .key = p.slug}));
    return h("nav", {{"className", "docs-nav"}, {"aria-label", "Разделы документации"}}, std::move(links));
}

Element Docs(const Props&) {
    std::string slug = router::useParam("page");
    const auto& all = site_docs::pages();

    if (slug.empty()) {  // index of all pages
        std::vector<Element> cards;
        for (const auto& p : all) cards.push_back(card("/docs/" + p.slug, p.title, p.summary));
        return page({
            h("h1", {}, {"Документация"}),
            h("p", {{"className", "lead"}}, {"Всё, что есть в ReactX: от первого компонента до сервера на C++."}),
            h("div", {{"className", "doc-cards"}}, std::move(cards)),
        });
    }

    auto it = std::ranges::find(all, slug, &site_docs::Page::slug);
    if (it == all.end())
        return page({h("h1", {}, {"Нет такого раздела"}),
                     router::link({.to = "/docs", .label = "← Вся документация", .className = "back", .activeClassName = ""})});

    std::vector<Element> article;
    article.push_back(h("h1", {}, {it->title}));
    article.push_back(h("p", {{"className", "lead"}}, {it->summary}));
    for (const auto& s : it->sections) {
        std::vector<Element> section;
        section.push_back(h("h2", {}, {s.heading}));
        for (auto& p : paragraphs(s.text)) section.push_back(std::move(p));
        if (!s.code.empty()) section.push_back(h("pre", {}, {h("code", {}, {s.code})}));
        article.push_back(h("section", {{"key", s.heading}}, std::move(section)));
    }
    std::vector<Element> pager;  // previous / next page
    if (it != all.begin())
        pager.push_back(router::link({.to = "/docs/" + (it - 1)->slug, .label = "← " + (it - 1)->title,
                                      .className = "back", .activeClassName = "", .key = "prev"}));
    if (it + 1 != all.end())
        pager.push_back(router::link({.to = "/docs/" + (it + 1)->slug, .label = (it + 1)->title + " →",
                                      .className = "back", .activeClassName = "", .key = "next"}));
    article.push_back(h("div", {{"className", "pager"}}, std::move(pager)));

    return h("div", {{"className", "docs"}}, {h(DocsNav), h("article", {{"className", "page"}}, std::move(article))});
}

Element Demos(const Props&) {
    auto demo = [](const char* file, const char* title, const char* text) {
        // Plain <a>: separate pages, not routes of this app.
        return h("a", {{"className", "doc-card"}, {"href", file}, {"key", file}}, {h("b", {}, {title}), h("span", {}, {text})});
    };
    return page({
        h("h1", {}, {"Демо"}),
        h("p", {{"className", "lead"}}, {"Отдельные страницы, каждая — ReactX в WebAssembly."}),
        h("div", {{"className", "doc-cards"}}, {
            demo("web_counter.html", "Счётчик", "useState и события"),
            demo("web_ui_demo.html", "Компоненты rx::ui", "вкладки, список, поле ввода, тема, модальное окно"),
        }),
    });
}

Element NotFound(const Props&) {
    return page({
        h("h1", {}, {"404"}),
        h("p", {{"className", "lead"}}, {"Страницы " + router::useLocation() + " нет."}),
        router::link({.to = "/", .label = "На главную", .className = "back", .activeClassName = ""}),
    });
}

Element Site(const Props&) {
    return ui::themeProvider(ui::Theme{.accent = "blue"}, {
        h("header", {{"className", "topbar"}}, {
            router::link({.to = "/", .label = "ReactX", .className = "brand", .activeClassName = ""}),
            h("nav", {{"className", "nav"}}, {
                router::link({.to = "/", .label = "Главная", .key = "home"}),
                router::link({.to = "/docs", .label = "Документация", .key = "docs"}),
                router::link({.to = "/demos", .label = "Демо", .key = "demos"}),
                h("a", {{"href", "https://github.com/sargisis/ReactX"}, {"key", "gh"}}, {"GitHub"}),
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
        h("footer", {{"className", "footer-note"}}, {"ReactX · C++26 · WebAssembly"}),
    });
}

}  // namespace

int main() { mountDom(h(Site)); }
