// rx::ui in the browser: the same components and props as the terminal
// ui_demo (tabs, list, select, tooltip, confirm, toasts, cards), rendered as
// HTML + CSS.
#include <algorithm>
#include <string>
#include <vector>

#include "reactx/dom.hpp"
#include "reactx/reactx.hpp"
#include "reactx/ui.hpp"

using namespace rx;

struct Task {
    int id;
    std::string title;
    bool done = false;
    bool operator==(const Task&) const = default;
};
using Tasks = std::vector<Task>;

Element App(const Props&) {
    auto [tasks, setTasks] = useState(Tasks{{1, "Try rx::ui on the web", true}, {2, "Build a real app"}, {3, "Show it to friends"}});
    auto [draft, setDraft] = useState(std::string{});
    auto [showDone, setShowDone] = useState(true);
    auto [accent, setAccent] = useState(std::string("blue"));
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
            .label = (t.done ? "✓ " : "") + t.title,
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
                   .placeholder = "New task, press Enter",
                   .label = "Add"}),
        ui::list({.title = std::to_string(left) + " left · click to toggle, Delete to remove",
                  .emptyText = "No tasks",
                  .children = std::move(items)}),
        ui::stack({.direction = ui::Direction::Row, .justify = ui::Justify::Between, .wrap = true, .children = {
            ui::select({.label = "Sort", .value = sort,
                        .options = {{"added", "As added"}, {"name", "By name"}, {"todo", "Todo first"}},
                        .onChange = [setSort](const std::string& v) { setSort(v); }}),
            ui::tooltip({.text = "Asks before removing", .children = {
                ui::button({.label = "Clear all", .onClick = [setConfirm] { setConfirm(true); },
                            .variant = ui::Variant::Danger, .disabled = tasks.empty()})}}),
        }}),
    };

    // ---- Settings tab ----
    std::vector<Element> colors;
    for (std::string color : {"blue", "magenta", "green", "red"}) {
        colors.push_back(ui::button({
            .label = color,
            .onClick = [setAccent, color] { setAccent(color); },
            .variant = color == accent ? ui::Variant::Primary : ui::Variant::Default,
            .key = color,
        }));
    }
    std::vector<Element> settingsTab = {
        ui::checkbox({.label = "Show finished tasks", .checked = showDone,
                      .onToggle = [setShowDone, showDone] { setShowDone(!showDone); }}),
        h("p", {{"className", "hint"}, {"style", "text-align: left; margin: 0"}}, {"Accent color"}),
        h("div", {{"className", "row"}, {"style", "flex-wrap: wrap"}}, std::move(colors)),
    };

    // ---- About tab ----
    std::vector<Element> aboutTab = {
        ui::card({.title = "ReactX", .children = {
            ui::text({.text = "A React-like UI framework in C++. This page is WebAssembly."}),
            ui::text({.text = "The same rx::ui code also runs in a terminal.", .muted = true}),
        }, .level = 2}),
        ui::stack({.direction = ui::Direction::Row, .children = {
            ui::button({.label = "Say hi", .onClick = [] { ui::toast({.text = "Hi from rx::ui", .variant = ui::Variant::Primary}); },
                        .variant = ui::Variant::Primary}),
        }}),
    };

    ui::Theme theme;
    theme.accent = accent;

    return ui::themeProvider(theme, {
        h("div", {{"className", "card"}}, {
            h("h1", {}, {"ReactX ui"}),
            ui::tabs({.tabs = {{"Tasks", std::move(tasksTab)},
                               {"Settings", std::move(settingsTab)},
                               {"About", std::move(aboutTab)}}}),
        }),
        ui::confirm({
            .title = "Clear all tasks?",
            .message = "This removes every task. Press Esc or click outside to cancel.",
            .open = confirm,
            .confirmLabel = "Clear",
            .cancelLabel = "Keep them",
            .danger = true,
            .onConfirm = [setTasks, setConfirm] {
                setTasks(Tasks{});
                setConfirm(false);
                ui::toast({.text = "All tasks cleared", .variant = ui::Variant::Danger});
            },
            .onCancel = [setConfirm] { setConfirm(false); },
        }),
        ui::toaster(),
    });
}

int main() { mountDom(h(App)); }
