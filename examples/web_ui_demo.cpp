// rx::ui in the browser: the same components and props as the terminal
// ui_demo, rendered as HTML + CSS.
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
                   },
                   .placeholder = "New task, press Enter",
                   .label = "Add"}),
        ui::list({.title = std::to_string(left) + " left · click to toggle, Delete to remove",
                  .emptyText = "No tasks",
                  .items = std::move(items)}),
        h("div", {{"className", "row"}}, {
            ui::button({.label = "Clear all", .onClick = [setConfirm] { setConfirm(true); },
                        .variant = ui::Variant::Danger, .disabled = tasks.empty()}),
        }),
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
        h("p", {{"style", "margin: 0"}}, {"ReactX is a React-like UI framework in C++20. This page is WebAssembly; "
                                          "the same rx::ui code also runs in a terminal."}),
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
        ui::modal({
            .title = "Clear all tasks?",
            .open = confirm,
            .onClose = [setConfirm] { setConfirm(false); },
            .width = 34,
            .children = {
                h("p", {{"style", "margin: 0"}}, {"This removes every task. Press Esc or click outside to cancel."}),
                h("div", {{"className", "row"}, {"style", "justify-content: flex-end"}}, {
                    ui::button({.label = "Keep them", .onClick = [setConfirm] { setConfirm(false); }}),
                    ui::button({.label = "Clear", .onClick = [setTasks, setConfirm] {
                                    setTasks(Tasks{});
                                    setConfirm(false);
                                }, .variant = ui::Variant::Danger}),
                }),
            },
        }),
    });
}

int main() { mountDom(h(App)); }
