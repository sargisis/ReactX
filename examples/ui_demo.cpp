// rx::ui demo: tabs, input, list, checkboxes, buttons, a confirm modal and
// a theme switch.
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
    std::vector<Element> items;
    for (const Task& t : tasks) {
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
                   },
                   .placeholder = "new task, press Enter",
                   .label = "Add:"}),
        ui::list({.title = std::to_string(left) + " left", .emptyText = "no tasks", .items = std::move(items)}),
        h("box", {{"direction", "row"}, {"gap", 2}}, {
            ui::button({.label = "clear all", .onClick = [setConfirm] { setConfirm(true); },
                        .variant = ui::Variant::Danger, .disabled = tasks.empty()}),
        }),
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
        h("text", {}, {"ReactX: a React-like UI framework in C++26, drawn in your terminal. "
                       "Layout by Yoga (Flexbox), components from rx::ui."}),
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
            ui::modal({
                .title = "Clear all tasks?",
                .open = confirm,
                .onClose = [setConfirm] { setConfirm(false); },
                .width = 36,
                .children = {
                    h("text", {}, {"This removes every task. Esc to cancel."}),
                    h("box", {{"direction", "row"}, {"justifyContent", "space-between"}}, {
                        ui::button({.label = "keep them", .onClick = [setConfirm] { setConfirm(false); }}),
                        ui::button({.label = "clear", .onClick = [setTasks, setConfirm] {
                                        setTasks(Tasks{});
                                        setConfirm(false);
                                    }, .variant = ui::Variant::Danger}),
                    }),
                },
            }),
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
