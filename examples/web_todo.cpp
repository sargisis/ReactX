// Todo list in the browser: form submit, controlled input, checkboxes,
// keyed list, filters, useMemo.
#include <algorithm>
#include <ranges>
#include <string>
#include <vector>

#include "reactx/dom.hpp"
#include "reactx/reactx.hpp"

using namespace rx;

struct Todo {
    int id;
    std::string title;
    bool done = false;
    bool operator==(const Todo&) const = default;
};
using List = std::vector<Todo>;

struct TodoItemProps {
    int key = 0;  // element key and DOM id
    std::string title{};
    bool done = false;
    Callback onToggle{};
    Callback onRemove{};
};

Element TodoItem(const TodoItemProps& p) {
    bool done = p.done;
    std::string id = std::to_string(p.key);
    return h("li", {{"className", done ? "done" : ""}}, {
        h("input", {{"type", "checkbox"}, {"id", "check-" + id}, {"checked", done}, {"onChange", p.onToggle}}),
        h("span", {}, {p.title}),
        h("button", {{"className", "icon"}, {"id", "remove-" + id}, {"title", "Remove"}, {"onClick", p.onRemove}},
          {"✕"}),
    });
}

Element App(const Props&) {
    auto [todos, setTodos] = useState(List{{1, "Write a DOM backend", true}, {2, "Open it in the browser"}, {3, "Publish the site"}});
    auto [draft, setDraft] = useState(std::string{});
    auto [filter, setFilter] = useState(std::string("all"));
    int& nextId = useRef(4);

    const auto& visible = useMemo([&] {
        List out;
        for (const Todo& t : todos)
            if (filter == "all" || (filter == "active") == !t.done) out.push_back(t);
        return out;
    }, deps(todos, filter));

    std::vector<Element> items;
    for (const Todo& t : visible) {
        int id = t.id;
        items.push_back(h(TodoItem, {
            .key = id,
            .title = t.title,
            .done = t.done,
            .onToggle = [setTodos, id] {
                setTodos([id](List list) {
                    for (auto& t : list) if (t.id == id) t.done = !t.done;
                    return list;
                });
            },
            .onRemove = [setTodos, id] {
                setTodos([id](List list) {
                    std::erase_if(list, [id](const Todo& t) { return t.id == id; });
                    return list;
                });
            },
        }));
    }

    auto left = std::ranges::count_if(todos, [](const Todo& t) { return !t.done; });
    auto filterButton = [&](const std::string& name) {
        return h("button", {{"key", name}, {"id", "filter-" + name}, {"className", filter == name ? "active" : ""},
                            {"onClick", [setFilter, name] { setFilter(name); }}}, {name});
    };

    return h("div", {{"className", "card demo"}}, {
        h("h1", {}, {"ReactX todo"}),
        h("form", {{"className", "row"}, {"id", "form"},
                   {"onSubmit", [setTodos, setDraft, draft, &nextId] {
                        if (draft.empty()) return;
                        int id = nextId++;
                        setTodos([id, draft](List list) {
                            list.push_back({id, draft});
                            return list;
                        });
                        setDraft(std::string{});
                    }}}, {
            h("input", {{"type", "text"}, {"id", "new"}, {"placeholder", "What needs doing?"}, {"value", draft},
                        {"autocomplete", "off"},
                        {"onInput", [setDraft](const std::string& text) { setDraft(text); }}}),
            h("button", {{"className", "primary"}, {"type", "submit"}}, {"Add"}),
        }),
        items.empty() ? h("p", {{"className", "empty"}}, {"Nothing here"}) : h("ul", {}, std::move(items)),
        h("div", {{"className", "footer"}}, {
            h("span", {{"id", "left"}}, {std::to_string(left) + (left == 1 ? " item left" : " items left")}),
            h("div", {{"className", "row"}}, {filterButton("all"), filterButton("active"), filterButton("done")}),
        }),
    });
}

int main() { mountDom(h(App)); }
