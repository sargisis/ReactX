// Live demos of the libraries in library/ (charts, icons, markdown, query,
// i18n) for web_libraries.html. Included by web_libraries.cpp inside its
// namespace, after tr() and useEnglish(); the library headers are included at
// the top of that file.

// ---- charts ----

Element ChartsDemo(const Props&) {
    bool en = useEnglish();
    const std::vector<std::string> days = en ? std::vector<std::string>{"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"}
                                             : std::vector<std::string>{"Пн", "Вт", "Ср", "Чт", "Пт", "Сб", "Вс"};
    return h("div", {{"className", "live-pane lib-grid"}}, {
        h("div", {}, {charts::bar({.labels = days,
                                   .series = {{.name = tr(en, "Steps", "Шаги"), .values = {6200, 8100, 4300, 9900, 7400, 12100, 5300}}},
                                   .title = tr(en, "Steps this week", "Шаги за неделю"),
                                   .height = 180})}),
        h("div", {}, {charts::line({.labels = days,
                                    .series = {{.name = tr(en, "Visits", "Визиты"), .values = {120, 180, 150, 260, 230, 310, 280}},
                                               {.name = tr(en, "Sign-ups", "Регистрации"), .values = {20, 35, 30, 52, 41, 66, 58}}},
                                    .title = tr(en, "Traffic", "Трафик"),
                                    .height = 180})}),
        h("div", {}, {charts::pie({.labels = {tr(en, "Rent", "Аренда"), tr(en, "Food", "Еда"), tr(en, "Fun", "Развлечения")},
                                   .values = {1200, 450, 200},
                                   .title = tr(en, "Budget", "Бюджет"),
                                   .unit = " $",
                                   .donut = true})}),
        ui::text({.text = tr(en, "rx::charts — the same calls draw block and braille characters in a terminal.",
                             "rx::charts — те же вызовы рисуют блоки и шрифт Брайля в терминале."),
                  .muted = true}),
    });
}

// ---- icons ----

Element IconsDemo(const Props&) {
    bool en = useEnglish();
    std::vector<Element> cells;
    for (const auto& name : icons::names())
        cells.push_back(h("div", {{"className", "lib-icon"}, {"key", name}, {"title", name}},
                          {icons::icon({.name = name, .size = 22}), h("small", {}, {name})}));
    return h("div", {{"className", "live-pane"}}, {
        h("div", {{"className", "lib-icons"}}, std::move(cells)),
        ui::text({.text = tr(en, "rx::icons::icon({.name = \"search\"}) — they take the text color; in a terminal each is one character.",
                             "rx::icons::icon({.name = \"search\"}) — берут цвет текста; в терминале каждая — один символ."),
                  .muted = true}),
    });
}

// ---- markdown ----

constexpr const char* kMarkdownSample = R"(## Markdown → elements

Type on the left: **bold**, *emphasis*, `code`, ~~strikethrough~~ and [links](https://github.com/sargisis/ReactX).

- [x] lists and tasks
- [ ] tables:

| Library | Since |
|:--|--:|
| query | 1.3 |
| icons | 1.4 |

> `<script>` stays text: safe for what users type.
)";

Element MarkdownDemo(const Props&) {
    auto [text, setText] = useState(std::string(kMarkdownSample));
    return h("div", {{"className", "live-pane lib-split"}}, {
        ui::textarea({.value = text, .onChange = [setText](const std::string& t) { setText(t); }, .label = "Markdown", .rows = 14}),
        h("div", {{"className", "lib-preview"}}, {markdown::view({.text = text})}),
    });
}

// ---- query ----

struct DemoTodo {
    int id = 0;
    std::string title;
    bool done = false;
};

// A "server" inside the page for /demo-api/todos: no backend needed, every
// request is counted, and the next one can be made to fail.
struct DemoServer {
    std::vector<DemoTodo> todos{{1, "Read the docs", true}, {2, "Try reactx::query", false}};
    int nextId = 3;
    int requests = 0;
    bool failNext = false;
};
DemoServer& demoServer() {
    static DemoServer s;
    return s;
}
const auto DemoRequests = createStore(0);

void installDemoServer() {
    static bool done = false;
    if (done) return;
    done = true;
    setLocalFetch([](const FetchRequest& r) -> std::optional<FetchResponse> {
        if (!r.url.starts_with("/demo-api/")) return std::nullopt;
        auto& s = demoServer();
        ++s.requests;
        DemoRequests.set(s.requests);
        if (s.failNext) {
            s.failNext = false;
            return FetchResponse{.status = 503, .body = "{}"};
        }
        if (r.method == "POST") {
            auto body = json::tryParse(r.body);
            s.todos.push_back({s.nextId++, body ? (*body)["title"].asString() : std::string("?"), false});
        } else if (r.method == "PATCH") {
            auto id = parseNumber<int>(r.url.substr(r.url.rfind('/') + 1));
            for (auto& t : s.todos)
                if (id && t.id == *id) t.done = !t.done;
        }
        json::Value list = json::array();
        for (const auto& t : s.todos) list.push(json::object({{"id", t.id}, {"title", t.title}, {"done", t.done}}));
        return FetchResponse{.status = 200, .body = list.dump()};
    });
}

std::vector<DemoTodo> parseTodos(const json::Value& v) {
    std::vector<DemoTodo> out;
    for (const auto& t : v.asArray()) out.push_back({static_cast<int>(t["id"].asInt()), t["title"].asString(), t["done"].asBool()});
    return out;
}

struct TodoListProps {
    std::string title{};
    std::string key{};
};

// Two of these on the page share one cache entry and one request.
Element TodoList(const TodoListProps& p) {
    bool en = useEnglish();
    auto todos = query::useQuery<std::vector<DemoTodo>>({.key = "demo-todos", .url = "/demo-api/todos", .parse = parseTodos, .retry = 0});
    auto toggle = query::useMutation<int>({
        .request = [](const int& id) { return FetchRequest{.method = "PATCH", .url = "/demo-api/todos/" + std::to_string(id)}; },
        .onMutate = [](const int& id) -> Cleanup {  // flip it right away; undone if the request fails
            auto before = query::getData<std::vector<DemoTodo>>("demo-todos");
            query::updateData<std::vector<DemoTodo>>("demo-todos", [id](std::vector<DemoTodo> list) {
                for (auto& t : list)
                    if (t.id == id) t.done = !t.done;
                return list;
            });
            return [before] {
                if (before) query::setData("demo-todos", *before);
            };
        },
        .onError = [en](const std::string& error, const int&) {
            ui::toast({.text = tr(en, "Undone: ", "Отменено: ") + error, .variant = ui::Variant::Danger});
        },
        .invalidates = {"demo-todos"}});
    std::vector<Element> rows;
    if (todos.data)
        for (const auto& t : *todos.data)
            rows.push_back(ui::checkbox({.label = t.title, .checked = t.done, .onToggle = [toggle, id = t.id] { toggle.mutate(id); },
                                         .key = std::to_string(t.id)}));
    if (todos.loading) rows.push_back(ui::text({.text = tr(en, "Loading…", "Загрузка…"), .muted = true}));
    return h("div", {{"className", "lib-card"}}, {h("b", {}, {p.title}), ui::stack({.gap = 0, .children = std::move(rows)})});
}

Element QueryDemo(const Props&) {
    bool en = useEnglish();
    installDemoServer();
    const int requests = useStore(DemoRequests);
    auto [title, setTitle] = useState(std::string());
    auto add = query::useMutation<std::string>({
        .request = [](const std::string& t) {
            return FetchRequest{.method = "POST", .url = "/demo-api/todos", .body = json::object({{"title", t}}).dump()};
        },
        .onError = [en](const std::string& error, const std::string&) {
            ui::toast({.text = tr(en, "Not added: ", "Не добавлено: ") + error, .variant = ui::Variant::Danger});
        },
        .invalidates = {"demo-todos"}});
    auto submit = [add, title, setTitle] {
        if (title.empty()) return;
        add.mutate(title);
        setTitle(std::string());
    };
    return h("div", {{"className", "live-pane"}}, {
        h("div", {{"className", "lib-split"}}, {
            h(TodoList, TodoListProps{.title = tr(en, "This list", "Этот список"), .key = "a"}),
            h(TodoList, TodoListProps{.title = tr(en, "…and this one: same key, same request", "…и этот: тот же ключ, тот же запрос"), .key = "b"}),
        }),
        ui::stack({.direction = ui::Direction::Row, .wrap = true, .children = {
            ui::input({.value = title, .onChange = [setTitle](const std::string& t) { setTitle(t); },
                       .onSubmit = [add, setTitle](const std::string& t) {
                           if (t.empty()) return;
                           add.mutate(t);
                           setTitle(std::string());
                       },
                       .placeholder = tr(en, "A new task", "Новая задача")}),
            ui::button({.label = add.pending ? "…" : tr(en, "Add", "Добавить"), .onClick = submit, .variant = ui::Variant::Primary}),
            ui::button({.label = tr(en, "Fail the next request", "Сломать следующий запрос"),
                        .onClick = [en] {
                            demoServer().failNext = true;
                            ui::toast({.text = tr(en, "Now tick a task: it flips at once, then flips back",
                                                  "Отметьте задачу: она переключится сразу, а потом вернётся")});
                        }}),
        }}),
        ui::text({.text = tr(en, "Requests to the server: ", "Запросов к серверу: ") + std::to_string(requests) +
                          tr(en, " — both lists share each one; a change reloads them (invalidates).",
                             " — оба списка делят каждый; изменение перезагружает их (invalidates)."),
                  .muted = true}),
    });
}

// ---- i18n ----

constexpr std::pair<const char*, const char*> kFiles[] = {
    {"en", "{n, plural, one {# file} other {# files}}"},
    {"ru", "{n, plural, one {# файл} few {# файла} other {# файлов}}"},
    {"uk", "{n, plural, one {# файл} few {# файли} other {# файлів}}"},
    {"hy", "{n} ֆայլ"},
    {"de", "{n, plural, one {# Datei} other {# Dateien}}"},
    {"fr", "{n, plural, one {# fichier} other {# fichiers}}"},
    {"es", "{n, plural, one {# archivo} other {# archivos}}"},
    {"it", "{n, plural, one {# file} other {# file}}"},
    {"pt", "{n, plural, one {# arquivo} other {# arquivos}}"},
    {"pl", "{n, plural, one {# plik} few {# pliki} other {# plików}}"},
};

Element I18nDemo(const Props&) {
    bool en = useEnglish();
    auto [lang, setLang] = useState(std::string(en ? "en" : "ru"));
    auto [n, setN] = useState(3);
    std::vector<ui::Option> options;
    for (auto [code, message] : kFiles) {
        (void)message;
        options.push_back({.value = code, .label = code});
    }
    std::string files;
    for (auto [code, message] : kFiles)
        if (lang == code) files = i18n::format(message, {{"n", n}}, lang);
    const auto today = std::chrono::year_month_day(std::chrono::floor<std::chrono::days>(std::chrono::system_clock::now()));
    auto row = [](std::string label, std::string value) {
        return h("div", {{"className", "lib-row"}, {"key", label}}, {h("small", {}, {label}), h("b", {}, {std::move(value)})});
    };
    return h("div", {{"className", "live-pane lib-split"}}, {
        ui::stack({.children = {
            ui::select({.label = tr(en, "Language", "Язык"), .value = lang, .options = std::move(options),
                        .onChange = [setLang](const std::string& v) { setLang(v); }, .labelAbove = true}),
            ui::slider({.label = "n", .value = n, .min = 0, .max = 30, .onChange = [setN](int v) { setN(v); }}),
        }}),
        h("div", {}, {
            row("plural", files),
            row("formatNumber(1234567.891)", i18n::formatNumber(1234567.891, {}, lang)),
            row("formatCurrency(-1234.5, \"EUR\")", i18n::formatCurrency(-1234.5, "EUR", -1, lang)),
            row("formatPercent(0.256)", i18n::formatPercent(0.256, 0, lang)),
            row("formatDate(today, Full)", i18n::formatDate(today, i18n::DateStyle::Full, lang)),
            row("formatTime(14, 5)", i18n::formatTime(14, 5, -1, lang)),
            row("formatRelative(-n h)", i18n::formatRelative(std::chrono::hours(-n), lang)),
        }),
    });
}

constexpr const char* kLibraryTabs[] = {"query", "markdown", "charts", "icons", "i18n"};

struct LibraryDemosProps {
    int initial = 0;  // the open tab (web_libraries.html#icons opens "icons")
    std::string key{};
};

Element LibraryDemos(const LibraryDemosProps& p) {
    bool en = useEnglish();
    return h("div", {{"className", "live"}}, {
        ui::tabs({.tabs = {{kLibraryTabs[0], {h(QueryDemo)}},
                           {kLibraryTabs[1], {h(MarkdownDemo)}},
                           {kLibraryTabs[2], {h(ChartsDemo)}},
                           {kLibraryTabs[3], {h(IconsDemo)}},
                           {kLibraryTabs[4], {h(I18nDemo)}}},
                  .initial = p.initial}),
        ui::text({.text = tr(en, "Each library: target_link_libraries(my_app PRIVATE reactx::<name>).",
                             "Каждая библиотека: target_link_libraries(my_app PRIVATE reactx::<имя>)."),
                  .muted = true}),
    });
}
