// rx::ui demo: tabs, input, list, select, checkboxes, buttons, a tooltip,
// a confirm dialog, toasts, cards and a theme switch.
//   Tab / Up / Down  move focus      Enter / Space  press / toggle / select
//   Delete           remove task     Escape         close the dialog
//   Ctrl+C           quit
#include <algorithm>
#include <print>
#include <string>
#include <vector>

// The VS Code C/C++ extension can't parse GCC modules yet, so the editor reads
// the headers while the compiler uses the module.
#if defined(REACTX_USE_MODULES) && !defined(__INTELLISENSE__) && !defined(__clang__)
import reactx;
#else
#include "reactx/reactx.hpp"
#include "reactx/terminal.hpp"
#include "reactx/ui.hpp"
#endif

using namespace rx;

struct Task {
    int id;
    std::string title;
    bool done = false;
    bool operator==(const Task&) const = default;
};
using Tasks = std::vector<Task>;

Element App(const Props& props) {
    auto [tasks, setTasks] = useState(Tasks{{1, "Try rx::ui", true}, {2, "Build a real app"}, {3, "Show it to friends"}});
    auto [draft, setDraft] = useState(std::string{});
    auto [showDone, setShowDone] = useState(true);
    auto [compact, setCompact] = useState(false);
    auto [accent, setAccent] = useState(std::string("cyan"));
    auto [confirm, setConfirm] = useState(false);
    auto [sort, setSort] = useState(std::string("added"));
    int& nextId = useRef(4);

    auto toggle = [setTasks](int id) {
        setTasks([id](Tasks list) {
            for (auto& t : list) if (t.id == id) t.done = !t.done;
            return list;
        });
    };
    auto remove = [setTasks](int id) {
        setTasks([id](Tasks list) {
            std::erase_if(list, [id](const Task& t) { return t.id == id; });
            return list;
        });
    };

    // ---- Tasks tab ----
    Tasks shown = tasks;
    if (sort == "name") std::ranges::sort(shown, {}, &Task::title);
    if (sort == "todo") std::ranges::stable_partition(shown, [](const Task& t) { return !t.done; });
    std::vector<Element> items;
    for (const Task& t : shown) {
        if (!showDone && t.done) continue;
        int id = t.id;
        items.push_back(ui::listItem({
            .label = (t.done ? "[x] " : "[ ] ") + t.title,
            .detail = t.done ? "done" : "todo",
            .selected = !t.done,
            .onSelect = [toggle, id] { toggle(id); },
            .onKey = [remove, id](const KeyEvent& e) { if (e.key == "Delete") remove(id); },
            .key = std::to_string(id),
        }));
    }
    auto left = std::ranges::count_if(tasks, [](const Task& t) { return !t.done; });

    std::vector<Element> tasksTab = {
        ui::input({.value = draft,
                   .onChange = [setDraft](const std::string& v) { setDraft(v); },
                   .onSubmit = [setTasks, setDraft, &nextId](const std::string& v) {
                       if (v.empty()) return;
                       int id = nextId++;
                       setTasks([id, v](Tasks list) {
                           list.push_back({id, v});
                           return list;
                       });
                       setDraft(std::string{});
                       ui::toast({.text = "Added: " + v, .variant = ui::Variant::Success});
                   },
                   .placeholder = "new task, press Enter",
                   .label = "Add:"}),
        ui::list({.title = std::to_string(left) + " left", .emptyText = "no tasks", .children = std::move(items)}),
        ui::stack({.direction = ui::Direction::Row, .gap = 2, .justify = ui::Justify::Between, .children = {
            ui::select({.label = "Sort:", .value = sort,
                        .options = {{"added", "as added"}, {"name", "by name"}, {"todo", "todo first"}},
                        .onChange = [setSort](const std::string& v) { setSort(v); }}),
            ui::tooltip({.text = "asks first", .children = {
                ui::button({.label = "clear all", .onClick = [setConfirm] { setConfirm(true); },
                            .variant = ui::Variant::Danger, .disabled = tasks.empty()})}}),
        }}),
    };

    // ---- Settings tab ----
    std::vector<Element> themeButtons;
    for (std::string color : {"cyan", "magenta", "green", "yellow"}) {
        themeButtons.push_back(ui::button({
            .label = color,
            .onClick = [setAccent, color] { setAccent(color); },
            .variant = color == accent ? ui::Variant::Primary : ui::Variant::Default,
            .key = color,
        }));
    }
    std::vector<Element> settingsTab = {
        ui::checkbox({.label = "Show finished tasks", .checked = showDone,
                      .onToggle = [setShowDone, showDone] { setShowDone(!showDone); }}),
        ui::checkbox({.label = "Compact window", .checked = compact,
                      .onToggle = [setCompact, compact] { setCompact(!compact); }}),
        h("text", {{"color", "gray"}}, {"Accent color:"}),
        h("box", {{"direction", "row"}, {"gap", 1}, {"flexWrap", "wrap"}}, std::move(themeButtons)),
    };

    // ---- About tab ----
    std::vector<Element> aboutTab = {
        ui::card({.title = "ReactX", .children = {
            ui::text({.text = "A React-like UI framework in C++26, drawn in your terminal."}),
            ui::text({.text = "Layout by Yoga (Flexbox), components from rx::ui.", .muted = true}),
        }}),
        ui::button({.label = "say hi", .onClick = [] { ui::toast({.text = "Hi from rx::ui", .variant = ui::Variant::Primary}); }}),
    };

    ui::Theme theme;
    theme.accent = accent;

    return ui::themeProvider(theme, {
        h("box", {{"flexGrow", 1}, {"justifyContent", "center"}, {"alignItems", "center"}}, {
            h("box", {{"border", true}, {"title", "ReactX ui"}, {"color", accent},
                      {"width", compact ? 46 : 64}, {"maxWidth", "100%"},
                      {"paddingX", 2}, {"paddingY", 1}, {"gap", 1}}, {
                ui::tabs({.tabs = {{"Tasks", std::move(tasksTab)},
                                   {"Settings", std::move(settingsTab)},
                                   {"About", std::move(aboutTab)}}}),
                h("box", {{"direction", "row"}, {"justifyContent", "space-between"}, {"alignItems", "center"}}, {
                    h("text", {{"color", "gray"}, {"flexShrink", 1}}, {"Tab move · Enter select · Del remove"}),
                    ui::button({.label = "quit", .onClick = props.get<Callback>("onQuit")}),
                }),
            }),
            ui::confirm({
                .title = "Clear all tasks?",
                .message = "This removes every task. Esc to cancel.",
                .open = confirm,
                .confirmLabel = "clear",
                .cancelLabel = "keep them",
                .danger = true,
                .onConfirm = [setTasks, setConfirm] {
                    setTasks(Tasks{});
                    setConfirm(false);
                    ui::toast({.text = "All tasks cleared", .variant = ui::Variant::Danger});
                },
                .onCancel = [setConfirm] { setConfirm(false); },
            }),
            ui::toaster(),
        }),
    });
}

int main() {
    if (!Terminal::isInteractive()) {
        std::println(stderr, "ui_demo needs an interactive terminal");
        return 1;
    }
    TerminalApp app;
    app.run(h(App, {{"onQuit", Callback([&app] { app.quit(); })}}));
}
