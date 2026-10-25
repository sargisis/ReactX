// A small full-stack app: pages with rx::router, data from the C++ api_server
// (examples/api_server.cpp) with useFetch, components from rx::ui.
//
//   /              home with stats
//   /todos         list (?show=active|done), add a todo
//   /todos/:id     one todo: mark done, delete (with a confirm dialog)
//   /about
//   anything else  404
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
    if (f.loading && f.response.body.empty()) return h("p", {{"className", "lead"}}, {"Loading..."});
    if (!f.loading && !f.ok()) return h("p", {{"className", "error"}}, {"Could not load: " + f.errorText()});
    return fragment({});
}

// ---- pages ----

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
    return h("div", {{"className", "page"}}, {
        h("h1", {}, {"Hello from C++"}),
        h("p", {{"className", "lead"}}, {"This page is ReactX compiled to WebAssembly. The data comes from a C++ server."}),
        status(todos),
        h("div", {{"className", "stats"}}, {stat(total, "todos"), stat(done, "done"), stat(total - done, "left")}),
        h("div", {{"className", "row"}}, {
            ui::button({.label = "Open todos", .onClick = [] { router::navigate("/todos"); }, .variant = ui::Variant::Primary}),
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
                .detail = done ? "done" : "todo",
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

    return h("div", {{"className", "page"}}, {
        h("h1", {}, {"Todos"}),
        ui::input({.value = draft,
                   .onChange = [setDraft](const std::string& v) { setDraft(v); },
                   .onSubmit = add,
                   .placeholder = saving ? "Saving..." : "What needs doing? Press Enter",
                   .label = "New"}),
        h("div", {{"className", "filters"}}, {filter("", "All"), filter("active", "Active"), filter("done", "Done")}),
        status(todos),
        todos.ok() ? ui::list({.title = "Click a todo to open it", .emptyText = "Nothing here", .items = std::move(items)})
                   : fragment({}),
    });
}

Element TodoPage(const Props&) {
    std::string id = router::useParam("id");
    auto todo = useFetch("/api/todos/" + id);
    auto [confirm, setConfirm] = useState(false);

    std::vector<Element> body;
    body.push_back(router::link({.to = "/todos", .label = "← All todos", .className = "back", .activeClassName = ""}));
    if (todo.ok()) {
        json::Value t = todo.json();
        bool done = t.get<bool>("done");
        std::string title = t.get<std::string>("title");
        body.push_back(h("h1", {}, {title}));
        body.push_back(h("p", {}, {h("span", {{"className", done ? "status done" : "status"}}, {done ? "done" : "to do"}),
                                   std::string("  #") + id}));
        body.push_back(h("div", {{"className", "row"}}, {
            ui::button({.label = done ? "Mark as not done" : "Mark as done",
                        .onClick = [id, done, reload = todo.reload] {
                            fetch({.method = "PATCH", .url = "/api/todos/" + id, .body = json::object({{"done", !done}}).dump()},
                                  [reload](const FetchResponse&) { reload(); });
                        },
                        .variant = done ? ui::Variant::Default : ui::Variant::Success}),
            ui::button({.label = "Delete", .onClick = [setConfirm] { setConfirm(true); }, .variant = ui::Variant::Danger}),
        }));
        body.push_back(ui::modal({
            .title = "Delete “" + title + "”?",
            .open = confirm,
            .onClose = [setConfirm] { setConfirm(false); },
            .width = 34,
            .children = {
                h("p", {{"style", "margin: 0"}}, {"It is removed from the server for everyone."}),
                h("div", {{"className", "row"}, {"style", "justify-content: flex-end"}}, {
                    ui::button({.label = "Cancel", .onClick = [setConfirm] { setConfirm(false); }}),
                    ui::button({.label = "Delete",
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
    return h("div", {{"className", "page"}}, std::move(body));
}

Element About(const Props&) {
    return h("div", {{"className", "page"}}, {
        h("h1", {}, {"About"}),
        h("p", {}, {"ReactX: a React-like UI framework in C++20. Components, hooks, context and memo; "
                    "a terminal and a browser backend; rx::ui components; a router; useFetch; and a C++ HTTP server."}),
        h("p", {{"className", "lead"}}, {"Everything on this page, front end and back end, is C++."}),
    });
}

Element NotFound(const Props&) {
    return h("div", {{"className", "page"}}, {
        h("h1", {}, {"404"}),
        h("p", {{"className", "lead"}}, {"There is no page at " + router::useLocation() + "."}),
        router::link({.to = "/", .label = "Go home", .className = "back", .activeClassName = ""}),
    });
}

Element App(const Props&) {
    return ui::themeProvider(ui::Theme{.accent = "blue"}, {
        h("header", {{"className", "topbar"}}, {
            router::link({.to = "/", .label = "ReactX", .className = "brand", .activeClassName = ""}),
            h("nav", {{"className", "nav"}}, {
                router::link({.to = "/", .label = "Home"}),
                router::link({.to = "/todos", .label = "Todos"}),
                router::link({.to = "/about", .label = "About"}),
            }),
        }),
        h("main", {}, {
            router::routes({
                {"/", h(Home)},
                {"/todos", h(TodoList)},
                {"/todos/:id", h(TodoPage)},
                {"/about", h(About)},
                {"*", h(NotFound)},
            }),
        }),
    });
}

}  // namespace

int main() { mountDom(h(App)); }
