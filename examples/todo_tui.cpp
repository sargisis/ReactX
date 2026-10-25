// Interactive todo list in the terminal.
//   Tab / Up / Down   move focus
//   type + Enter      add a task (in the "New" field)
//   Enter / Space     toggle the focused task
//   Delete / d        remove the focused task
//   Ctrl+C            quit
#include <algorithm>
#include <iostream>
#include <ranges>
#include <string>
#include <vector>

// The VS Code C/C++ extension can't parse GCC modules yet, so the editor reads
// the headers while the compiler uses the module.
#if defined(REACTX_USE_MODULES) && !defined(__INTELLISENSE__)
import reactx;
#else
#include "reactx/reactx.hpp"
#include "reactx/terminal.hpp"
#endif

using namespace rx;

struct Todo {
    int id;
    std::string title;
    bool done = false;
    bool operator==(const Todo&) const = default;
};
using List = std::vector<Todo>;

// Accent color for the whole UI, passed down with a context.
const auto Accent = createContext<std::string>("cyan");

// Bordered frame; reads the accent color from the context.
Element Frame(const Props& p) {
    return h("box", {{"border", true},
                     {"title", p.get<std::string>("title")},
                     {"padding", 1},
                     {"width", 58},
                     {"color", useContext(Accent)}},
             p.children());
}

Element TodoItem(const Props& p) {
    bool done = p.get<bool>("done");
    const std::string& accent = useContext(Accent);
    auto onRemove = p.get<Callback>("onRemove");
    return h("item", {{"focusable", true},
                      {"onClick", p.get<Callback>("onToggle")},
                      {"onKey", [onRemove](const KeyEvent& e) {
                           if (e.key == "Delete" || e.key == "d") onRemove();
                       }}}, {
        h("text", {{"color", done ? "gray" : accent}},
          {std::string(done ? "[x] " : "[ ] ") + p.get<std::string>("title")}),
    });
}

Element App(const Props& props) {
    auto [todos, setTodos] = useState(List{
        {1, "Write the reconciler", true}, {2, "Add hooks", true}, {3, "Make a terminal backend"}});
    auto [draft, setDraft] = useState(std::string{});
    auto [accent, setAccent] = useState(std::string("cyan"));
    int& nextId = useRef(4);

    // Same function object on every render: it only uses setTodos.
    Callback clearDone = useCallback([setTodos] {
        setTodos([](List list) {
            std::erase_if(list, [](const Todo& t) { return t.done; });
            return list;
        });
    }, deps());

    std::vector<Element> items;
    for (const Todo& t : todos) {
        int id = t.id;
        items.push_back(h(TodoItem, {
            {"key", id},
            {"title", t.title},
            {"done", t.done},
            {"onToggle", [setTodos, id] {
                 setTodos([id](List list) {
                     for (auto& t : list) if (t.id == id) t.done = !t.done;
                     return list;
                 });
             }},
            {"onRemove", [setTodos, id] {
                 setTodos([id](List list) {
                     std::erase_if(list, [id](const Todo& t) { return t.id == id; });
                     return list;
                 });
             }},
        }));
    }
    if (items.empty()) items.push_back(h("text", {{"dim", true}}, {"nothing to do"}));

    auto left = std::ranges::count_if(todos, [](const Todo& t) { return !t.done; });

    return Accent.provide(accent, {
        h(Frame, {{"title", "ReactX todo"}}, {
            h("text", {{"bold", true}, {"color", left ? "yellow" : "green"}},
              {left ? std::to_string(left) + " left" : std::string("all done!")}),
            h("box", {{"direction", "row"}}, {
                h("text", {{"color", "white"}}, {"New:"}),
                h("input", {{"value", draft},
                            {"placeholder", "type a task, press Enter"},
                            {"color", "white"},
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
            }),
            h("text", {}, {""}),
            h("box", {{"color", "white"}}, std::move(items)),
            h("text", {}, {""}),
            h("box", {{"direction", "row"}, {"gap", 2}, {"color", "white"}}, {
                h("button", {{"onClick", [setTodos] {
                     setTodos([](List list) {
                         std::ranges::reverse(list);
                         return list;
                     });
                 }}}, {"reverse"}),
                h("button", {{"onClick", clearDone}}, {"clear done"}),
                h("button", {{"onClick", [setAccent, accent] {
                     static const std::vector<std::string> colors{"cyan", "magenta", "green", "yellow"};
                     auto it = std::ranges::find(colors, accent);
                     auto next = it == colors.end() || it + 1 == colors.end() ? colors.begin() : it + 1;
                     setAccent(*next);
                 }}}, {"theme"}),
                h("button", {{"onClick", props.get<Callback>("onQuit")}}, {"quit"}),
            }),
        }),
        h("text", {{"dim", true}}, {" Tab/↑↓ move · Enter toggle · Del remove · Ctrl+C quit"}),
    });
}

int main() {
    if (!Terminal::isInteractive()) {
        std::cerr << "todo_tui needs an interactive terminal\n";
        return 1;
    }
    TerminalApp app;
    app.run(h(App, {{"onQuit", Callback([&app] { app.quit(); })}}));
}
