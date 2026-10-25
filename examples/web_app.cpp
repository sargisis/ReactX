// Full-stack todo example: pages with rx::router, data from the C++
// api_server (examples/api_server.cpp, examples/todo_api.hpp) via useFetch,
// components from rx::ui.
//
//   /              home with stats
//   /todos         list (?show=active|done), add a todo
//   /todos/:id     one todo: mark done, delete (with a confirm dialog)
//   anything else  404
//
//   cmake --build --preset web && ./build/debug/api_server build/web/app
#include <string>
#include <vector>

#include "reactx/dom.hpp"
#include "reactx/fetch.hpp"
#include "reactx/json.hpp"
#include "reactx/reactx.hpp"
#include "reactx/router.hpp"
#include "reactx/ui.hpp"

using namespace rx;

namespace {

// Shown while loading or when a request failed; nothing once there is data.
Element status(const FetchState& f) {
    if (f.loading && f.response.body.empty()) return h("p", {{"className", "lead"}}, {"Загрузка..."});
    if (!f.loading && !f.ok()) return h("p", {{"className", "error"}}, {"Не удалось загрузить: " + f.errorText()});
    return fragment({});
}

Element page(std::vector<Element> children) { return h("div", {{"className", "page"}}, std::move(children)); }

// ---- todos ----

Element Home(const Props&) {
    auto todos = useFetch("/api/todos");
    int total = 0, done = 0;
    if (todos.ok())
        for (const auto& t : todos.json().asArray()) {
            ++total;
            done += t.get<bool>("done") ? 1 : 0;
        }
    auto stat = [](int n, const char* label) {
        return h("div", {{"className", "stat"}}, {h("b", {}, {std::to_string(n)}), h("span", {}, {label})});
    };
    return page({
        h("h1", {}, {"ReactX — React на C++20"}),
        h("p", {{"className", "lead"}},
          {"Эта страница — ReactX, скомпилированный в WebAssembly. Задачи хранятся на сервере на C++."}),
        status(todos),
        h("div", {{"className", "stats"}}, {stat(total, "задач"), stat(done, "выполнено"), stat(total - done, "осталось")}),
        h("div", {{"className", "row"}, {"style", "flex-wrap: wrap"}}, {
            ui::button({.label = "Открыть задачи", .onClick = [] { router::navigate("/todos"); }, .variant = ui::Variant::Primary}),
        }),
    });
}

Element TodoList(const Props&) {
    auto todos = useFetch("/api/todos");
    auto [draft, setDraft] = useState(std::string{});
    auto [saving, setSaving] = useState(false);
    std::string show = router::useQuery("show");

    std::vector<Element> items;
    if (todos.ok()) {
        for (const auto& t : todos.json().asArray()) {
            bool done = t.get<bool>("done");
            if ((show == "active" && done) || (show == "done" && !done)) continue;
            std::string id = std::to_string(t.get<int>("id"));
            items.push_back(ui::listItem({
                .label = (done ? "✓ " : "") + t.get<std::string>("title"),
                .detail = done ? "готово" : "в работе",
                .selected = !done,
                .onSelect = [id] { router::navigate("/todos/" + id); },
                .key = id,
            }));
        }
    }

    auto add = [setDraft, setSaving, reload = todos.reload](const std::string& title) {
        if (title.empty()) return;
        setSaving(true);
        fetch({.method = "POST", .url = "/api/todos", .body = json::object({{"title", title}}).dump()},
              [setDraft, setSaving, reload](const FetchResponse& r) {
                  setSaving(false);
                  if (r.ok()) setDraft(std::string{});
                  reload();
              });
    };
    auto filter = [&](const std::string& value, const char* label) {
        return router::link({.to = value.empty() ? "/todos" : "/todos?show=" + value, .label = label,
                             .className = show == value ? "active" : "", .activeClassName = "", .key = label});
    };

    return page({
        h("h1", {}, {"Задачи"}),
        ui::input({.value = draft,
                   .onChange = [setDraft](const std::string& v) { setDraft(v); },
                   .onSubmit = add,
                   .placeholder = saving ? "Сохраняю..." : "Что нужно сделать? Enter — добавить",
                   .label = "Новая"}),
        h("div", {{"className", "filters"}}, {filter("", "Все"), filter("active", "В работе"), filter("done", "Готово")}),
        status(todos),
        todos.ok() ? ui::list({.title = "Нажмите на задачу, чтобы открыть", .emptyText = "Пусто", .items = std::move(items)})
                   : fragment({}),
    });
}

Element TodoPage(const Props&) {
    std::string id = router::useParam("id");
    auto todo = useFetch("/api/todos/" + id);
    auto [confirm, setConfirm] = useState(false);

    std::vector<Element> body;
    body.push_back(router::link({.to = "/todos", .label = "← Все задачи", .className = "back", .activeClassName = ""}));
    if (todo.ok()) {
        json::Value t = todo.json();
        bool done = t.get<bool>("done");
        std::string title = t.get<std::string>("title");
        body.push_back(h("h1", {}, {title}));
        body.push_back(h("p", {}, {h("span", {{"className", done ? "status done" : "status"}}, {done ? "готово" : "в работе"}),
                                   std::string("  #") + id}));
        body.push_back(h("div", {{"className", "row"}}, {
            ui::button({.label = done ? "Вернуть в работу" : "Отметить готовой",
                        .onClick = [id, done, reload = todo.reload] {
                            fetch({.method = "PATCH", .url = "/api/todos/" + id, .body = json::object({{"done", !done}}).dump()},
                                  [reload](const FetchResponse&) { reload(); });
                        },
                        .variant = done ? ui::Variant::Default : ui::Variant::Success}),
            ui::button({.label = "Удалить", .onClick = [setConfirm] { setConfirm(true); }, .variant = ui::Variant::Danger}),
        }));
        body.push_back(ui::modal({
            .title = "Удалить «" + title + "»?",
            .open = confirm,
            .onClose = [setConfirm] { setConfirm(false); },
            .width = 34,
            .children = {
                h("p", {{"style", "margin: 0"}}, {"Задача удалится с сервера для всех."}),
                h("div", {{"className", "row"}, {"style", "justify-content: flex-end"}}, {
                    ui::button({.label = "Отмена", .onClick = [setConfirm] { setConfirm(false); }}),
                    ui::button({.label = "Удалить",
                                .onClick = [id] {
                                    fetch({.method = "DELETE", .url = "/api/todos/" + id},
                                          [](const FetchResponse&) { router::navigate("/todos", true); });
                                },
                                .variant = ui::Variant::Danger}),
                }),
            },
        }));
    } else {
        body.push_back(status(todo));
    }
    return page(std::move(body));
}

// ---- other pages ----

Element NotFound(const Props&) {
    return page({
        h("h1", {}, {"404"}),
        h("p", {{"className", "lead"}}, {"Страницы " + router::useLocation() + " нет."}),
        router::link({.to = "/", .label = "На главную", .className = "back", .activeClassName = ""}),
    });
}

Element App(const Props&) {
    std::vector<Element> nav = {
        router::link({.to = "/", .label = "Главная", .key = "home"}),
        router::link({.to = "/todos", .label = "Задачи", .key = "todos"}),
    };

    return ui::themeProvider(ui::Theme{.accent = "blue"}, {
        h("header", {{"className", "topbar"}}, {
            router::link({.to = "/", .label = "ReactX", .className = "brand", .activeClassName = ""}),
            h("nav", {{"className", "nav"}}, std::move(nav)),
        }),
        h("main", {}, {
            router::routes({
                {"/", h(Home)},
                {"/todos", h(TodoList)},
                {"/todos/:id", h(TodoPage)},
                {"*", h(NotFound)},
            }),
        }),
        h("footer", {{"className", "footer-note"}}, {"ReactX · C++20 · WebAssembly"}),
    });
}

}  // namespace

int main() { mountDom(h(App)); }
