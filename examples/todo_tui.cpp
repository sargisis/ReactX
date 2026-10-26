// Interactive todo list in the terminal.
//   Tab / Up / Down   move focus
//   type + Enter      add a task (in the "New" field)
//   Enter / Space     toggle the focused task
//   Delete / d        remove the focused task
//   Ctrl+C            quit
#include <algorithm>
#include <print>
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
struct FrameProps {
    std::string title{};
    std::vector<Element> children{};  // h(Frame, {.title = ...}, {children})
};

Element Frame(const FrameProps& p) {
    return h("box", {{"border", true},
                     {"title", p.title},
                     {"paddingX", 2},
                     {"paddingY", 1},
                     {"gap", 1},
                     {"width", 58},
                     {"maxWidth", "100%"},
                     {"color", useContext(Accent)}},
             p.children);
}

struct TodoItemProps {
    int key = 0;
    std::string title{};
    bool done = false;
    Callback onToggle{};
    Callback onRemove{};  // Delete or "d"
};

Element TodoItem(const TodoItemProps& p) {
    bool done = p.done;
    const std::string& accent = useContext(Accent);
    auto onRemove = p.onRemove;
    return h("item", {{"focusable", true},
                      {"onClick", p.onToggle},
                      {"onKey", [onRemove](const KeyEvent& e) {
                           if (e.key == "Delete" || e.key == "d") onRemove();
                       }}}, {
        h("text", {{"color", done ? "gray" : accent}},
          {std::string(done ? "[x] " : "[ ] ") + p.title}),
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
    if (items.empty()) items.push_back(h("text", {{"dim", true}}, {"nothing to do"}));

    auto left = std::ranges::count_if(todos, [](const Todo& t) { return !t.done; });

    // Flexbox (Yoga): the frame is centered on the screen.
    return Accent.provide(accent, {h("box", {{"flexGrow", 1}, {"justifyContent", "center"}, {"alignItems", "center"}}, {
        h(Frame, {.title = "ReactX todo"}, {
            h("box", {{"direction", "row"}, {"justifyContent", "space-between"}}, {
                h("text", {{"bold", true}, {"color", left ? "yellow" : "green"}},
                  {left ? std::to_string(left) + " left" : std::string("all done!")}),
                h("text", {{"dim", true}}, {std::to_string(todos.size()) + " total"}),
            }),
            h("box", {{"direction", "row"}, {"gap", 1}}, {
                h("text", {{"color", "white"}}, {"New:"}),
                h("input", {{"flexGrow", 1},
                            {"value", draft},
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
            h("box", {{"color", "white"}}, std::move(items)),
            h("box", {{"direction", "row"}, {"justifyContent", "space-between"}, {"flexWrap", "wrap"},
                      {"columnGap", 2}, {"color", "white"}}, {
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
        h("text", {{"dim", true}}, {"Tab/↑↓ move · Enter toggle · Del remove · Ctrl+C quit"}),
    })});
}

int main() {
    if (!Terminal::isInteractive()) {
        std::println(stderr, "todo_tui needs an interactive terminal");
        return 1;
    }
    TerminalApp app;
    app.run(h(App, {{"onQuit", Callback([&app] { app.quit(); })}}));
}
