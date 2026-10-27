// Todo list: keyed children, nested components, state with a struct,
// text input (onInput) and keys (onKey).
#include <algorithm>
#include <iostream>
#include <print>
#include <ranges>
#include <string>
#include <vector>

// Editors (clangd, the VS Code C/C++ extension) can't read GCC's module files,
// so the editor reads the headers while the compiler uses the module.
#if defined(REACTX_USE_MODULES) && !defined(__INTELLISENSE__) && !defined(__clang__)
import reactx;
#else
#include "reactx/reactx.hpp"
#include "reactx/text_renderer.hpp"
#endif

using namespace rx;

struct Todo {
    int id;
    std::string title;
    bool done = false;
    bool operator==(const Todo&) const = default;
};

struct TodoItemProps {
    int key = 0;  // the element key: items keep their state when the list is reordered
    std::string title{};
    bool done = false;
    Callback onToggle{};
    Callback onRemove{};
};

Element TodoItem(const TodoItemProps& p) {
    std::string id = "todo-" + std::to_string(p.key);
    std::string mark = p.done ? "[x] " : "[ ] ";
    return h("item", {{"id", id}, {"onClick", p.onToggle}}, {
        h("text", {}, {mark + p.title}),
        h("button", {{"id", id + "-remove"}, {"onClick", p.onRemove}}, {"x"}),
    });
}

Element App(const Props&) {
    auto [todos, setTodos] = useState(std::vector<Todo>{
        {1, "Write the reconciler"}, {2, "Add hooks"}, {3, "Make a real backend"}});
    auto [draft, setDraft] = useState(std::string{});
    int& nextId = useRef(4);

    using List = std::vector<Todo>;
    std::vector<Element> items;
    for (const Todo& t : todos) {
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

    return h("app", {}, {
        h("text", {}, {std::to_string(left) + " left"}),
        h("input", {{"id", "new"},
                    {"value", draft},
                    {"onInput", [setDraft](const std::string& text) { setDraft(text); }},
                    {"onKey", [setTodos, setDraft, draft, &nextId](const KeyEvent& e) {
                         if (e.key == "Escape") setDraft(std::string{});
                         if (e.key != "Enter" || draft.empty()) return;
                         int id = nextId++;
                         setTodos([id, draft](List list) {
                             list.push_back({id, draft});
                             return list;
                         });
                         setDraft(std::string{});
                     }}}),
        h("button", {{"id", "reverse"}, {"onClick", [setTodos] {
              setTodos([](List list) {
                  std::ranges::reverse(list);
                  return list;
              });
          }}}, {"reverse"}),
        h("list", {}, std::move(items)),
    });
}

int main() {
    TextRenderer renderer(&std::cout);
    Root root(renderer);

    auto click = [&](const std::string& id) {
        std::println("\n>>> click #{}", id);
        root.dispatch(*renderer.findByProp("id", id), "onClick");
        std::print("{}", renderer.toString());
    };

    std::println(">>> mount");
    root.render(h(App));
    std::print("{}", renderer.toString());

    click("todo-2");         // toggle
    click("reverse");        // only moves nodes, nothing is recreated

    std::println("\n>>> type \"Buy milk\" + Enter");
    NodeHandle input = *renderer.findByProp("id", "new");
    root.dispatch(input, "onInput", std::string("Buy milk"));
    root.dispatch(input, "onKey", KeyEvent{"Enter"});
    std::print("{}", renderer.toString());
    click("todo-1-remove");
}
