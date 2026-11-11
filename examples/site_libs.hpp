// Live demos of the UI libraries in library/ (query, forms, grid, markdown,
// calendar, dnd, motion, charts, icons, i18n) for web_libraries.html. Included by web_libraries.cpp inside its
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

// ---- forms ----

struct Signup {
    std::string email, password, confirm;
    bool terms = false;
};

Element FormsDemo(const Props&) {
    bool en = useEnglish();
    static const auto schema = forms::Schema<Signup>()
                                   .field("email", &Signup::email, {forms::required(), forms::email()})
                                   .field("password", &Signup::password, {forms::required(), forms::minLength(8)})
                                   .check("confirm", [](const Signup& s) { return s.confirm == s.password ? "" : "The passwords differ"; })
                                   .field("terms", &Signup::terms, {forms::accepted()});
    auto form = ui::useForm<Signup>({.validate = schema.validator(), .onSubmit = [en](const Signup& s) {
                                         ui::toast({.text = tr(en, "Signed up: ", "Регистрация: ") + s.email, .variant = ui::Variant::Success});
                                     }});
    return h("div", {{"className", "live-pane"}}, {
        h("div", {{"className", "lib-form"}}, {
            ui::input({.value = form.values.email, .onChange = form.set("email", &Signup::email), .label = "Email",
                       .labelAbove = true, .error = form.error("email"), .type = ui::InputType::Email}),
            ui::input({.value = form.values.password, .onChange = form.set("password", &Signup::password),
                       .label = tr(en, "Password", "Пароль"), .labelAbove = true, .error = form.error("password"),
                       .type = ui::InputType::Password}),
            ui::input({.value = form.values.confirm, .onChange = form.set("confirm", &Signup::confirm),
                       .label = tr(en, "Repeat it", "Повторите"), .labelAbove = true, .error = form.error("confirm"),
                       .type = ui::InputType::Password}),
            ui::checkbox({.label = tr(en, "I accept the terms", "Принимаю условия"), .checked = form.values.terms,
                          .onChange = form.set("terms", &Signup::terms), .error = form.error("terms")}),
            ui::button({.label = tr(en, "Sign up", "Зарегистрироваться"), .onClick = form.submit, .variant = ui::Variant::Primary}),
        }),
        ui::text({.text = tr(en, "forms::Schema<T> — the same rules check JSON on the server (forms::JsonSchema, reactx::api).",
                             "forms::Schema<T> — те же правила проверяют JSON на сервере (forms::JsonSchema, reactx::api)."),
                  .muted = true}),
    });
}

// ---- motion ----

Element MotionDemo(const Props&) {
    bool en = useEnglish();
    auto [open, setOpen] = useState(true);
    auto [done, setDone] = useState(false);
    const double progress = motion::useTween(done ? 100.0 : 12.0, {.duration = std::chrono::milliseconds(600)});
    const double spring = motion::useSpring(open ? 1.0 : 0.0, {.stiffness = 220, .damping = 14});
    auto panel = [](std::string text) { return h("div", {{"className", "lib-card"}}, {h("b", {}, {std::move(text)})}); };
    return h("div", {{"className", "live-pane"}}, {
        ui::stack({.direction = ui::Direction::Row, .wrap = true, .children = {
            ui::button({.label = open ? tr(en, "Hide", "Скрыть") : tr(en, "Show", "Показать"),
                        .onClick = [setOpen] { setOpen([](bool v) { return !v; }); }, .variant = ui::Variant::Primary}),
            ui::button({.label = tr(en, "Toggle progress", "Переключить прогресс"), .onClick = [setDone] { setDone([](bool v) { return !v; }); }}),
        }}),
        h("div", {{"className", "lib-grid"}}, {
            motion::presence({.show = open, .kind = motion::Kind::Fade, .children = {panel("Fade")}}),
            motion::presence({.show = open, .kind = motion::Kind::Slide, .duration = std::chrono::milliseconds(300), .children = {panel("Slide")}}),
            motion::presence({.show = open, .kind = motion::Kind::Scale, .children = {panel("Scale")}}),
            motion::presence({.show = open, .kind = motion::Kind::Collapse, .duration = std::chrono::milliseconds(300),
                              .children = {panel("Collapse"), panel(tr(en, "…the height opens and closes", "…высота открывается и закрывается"))}}),
        }),
        h("div", {{"className", "lib-bar"}}, {h("div", {{"style", "width:" + std::to_string(static_cast<int>(progress)) + "%"}})}),
        h("div", {{"className", "lib-spring"}, {"style", "transform:translateX(" + std::to_string(static_cast<int>(spring * 220)) + "px)"}}, {"spring"}),
    });
}

// ---- dnd ----

Element DndDemo(const Props&) {
    bool en = useEnglish();
    auto [order, setOrder] = useState(std::vector<std::string>{"design", "build", "test", "ship"});
    auto title = [en](const std::string& id) {
        if (id == "design") return tr(en, "Design the page", "Нарисовать страницу");
        if (id == "build") return tr(en, "Build it in C++", "Собрать на C++");
        if (id == "test") return tr(en, "Test in the terminal too", "Проверить и в терминале");
        return tr(en, "Ship it", "Выпустить");
    };
    std::string line;
    for (const auto& id : order) line += (line.empty() ? "" : " → ") + id;
    return h("div", {{"className", "live-pane"}}, {
        h("div", {{"style", "max-width:420px"}}, {
            dnd::sortable({.ids = order,
                           .item = [title](const std::string& id, bool) {
                               return h("div", {{"className", "lib-card lib-drag"}}, {icons::icon({.name = "menu", .size = 16}), h("span", {}, {title(id)})});
                           },
                           .onChange = [setOrder](std::vector<std::string> next) { setOrder(std::move(next)); },
                           .label = tr(en, "Tasks, drag to reorder", "Задачи, перетащите, чтобы поменять порядок"),
                           .gap = 2})}),
        ui::text({.text = tr(en, "Order: ", "Порядок: ") + line, .muted = true}),
        ui::text({.text = tr(en, "Drag with the mouse, or Tab to a task and press Alt+↑ / Alt+↓.",
                             "Тащите мышью или Tab на задачу и Alt+↑ / Alt+↓."),
                  .muted = true}),
    });
}

// ---- calendar ----

Element CalendarDemo(const Props&) {
    bool en = useEnglish();
    // The next weekday (weekends are off in this demo).
    auto [day, setDay] = useState([] {
        auto d = std::chrono::sys_days(calendar::today());
        while (std::chrono::weekday(d) == std::chrono::Saturday || std::chrono::weekday(d) == std::chrono::Sunday) d += std::chrono::days(1);
        return calendar::Date(d);
    }());
    auto [due, setDue] = useState(calendar::Date{});
    const std::string lang = en ? "en" : "ru";
    return h("div", {{"className", "live-pane"}}, {
        h("div", {{"className", "lib-split"}}, {
            calendar::calendar({.value = day, .onChange = [setDay](calendar::Date d) { setDay(d); },
                                .disabled = [](calendar::Date d) {
                                    const auto wd = std::chrono::weekday(std::chrono::sys_days(d));
                                    return wd == std::chrono::Saturday || wd == std::chrono::Sunday;
                                },
                                .lang = lang}),
            h("div", {}, {
                ui::text({.text = tr(en, "Picked: ", "Выбрано: ") + i18n::formatDate(day, i18n::DateStyle::Full, lang)}),
                calendar::datePicker({.value = due, .onChange = [setDue](calendar::Date d) { setDue(d); },
                                      .label = tr(en, "Due", "Срок"), .min = calendar::today(), .lang = lang}),
            }),
        }),
        ui::text({.text = tr(en, "Weekends are off. Keys: arrows, PageUp / PageDown, Home / End, Enter.",
                             "Выходные недоступны. Клавиши: стрелки, PageUp / PageDown, Home / End, Enter."),
                  .muted = true}),
    });
}

// ---- grid ----

struct Incident {
    std::string id, title, severity, status;
    int age = 0;  // hours
};

std::vector<Incident> makeIncidents(bool en) {
    const char* titlesEn[] = {"Login page is slow", "Payments time out", "Search returns nothing", "Emails sent twice",
                              "Dark mode flickers", "Export to CSV fails", "Avatar upload 500", "Cache misses after deploy",
                              "Wrong time zone in reports", "Mobile menu won't close"};
    const char* titlesRu[] = {"Медленный вход", "Платежи отваливаются по таймауту", "Поиск ничего не находит", "Письма уходят дважды",
                              "Тёмная тема мигает", "Экспорт в CSV падает", "Загрузка аватара: 500", "Промахи кеша после выкладки",
                              "Не тот часовой пояс в отчётах", "Мобильное меню не закрывается"};
    const char* severities[] = {"Critical", "High", "Medium", "Low"};
    const char* statuses[] = {"open", "investigating", "fixed"};
    std::vector<Incident> out;
    for (int i = 1; i <= 60; ++i) {
        const auto n = static_cast<std::size_t>(i);
        out.push_back({.id = "INC-" + std::to_string(i),
                       .title = en ? titlesEn[(n * 7) % 10] : titlesRu[(n * 7) % 10],
                       .severity = severities[(n * 5 + n / 3) % 4],
                       .status = statuses[(n * n + n / 2) % 3],
                       .age = static_cast<int>((n * 37) % 240) + 1});
    }
    return out;
}

Element GridDemo(const Props&) {
    bool en = useEnglish();
    const auto& incidents = useMemo([en] { return makeIncidents(en); }, deps(en));
    auto [status, setStatus] = useState(std::string{});
    auto [clicked, setClicked] = useState(std::string{});
    auto g = grid::useGrid<Incident>(incidents, {
        .columns = {
            {.title = "ID", .text = [](const Incident& i) { return i.id; }, .width = 8},
            {.title = tr(en, "Title", "Название"), .text = [](const Incident& i) { return i.title; }},
            {.title = tr(en, "Severity", "Важность"), .text = [](const Incident& i) { return i.severity; },
             .cell = [](const Incident& i) { return h("span", {{"className", "lib-sev lib-sev-" + i.severity}}, {i.severity}); },
             .order = {"Critical", "High", "Medium", "Low"}, .width = 10},
            {.title = tr(en, "Status", "Статус"), .text = [](const Incident& i) { return i.status; }, .width = 13},
            {.title = tr(en, "Age, h", "Часов"), .text = [](const Incident& i) { return std::to_string(i.age); },
             .width = 7, .align = ui::Align::End},
        },
        .id = [](const Incident& i) { return i.id; },
        .filter = [status](const Incident& i) { return status.empty() || i.status == status; },
        .pageSize = 10,
        .select = true,
        .searchPlaceholder = tr(en, "Search...", "Поиск..."),
        .emptyText = tr(en, "Nothing found", "Ничего не найдено"),
    });
    std::string picked;
    for (const auto& id : g.selected) picked += (picked.empty() ? "" : ", ") + id;
    return h("div", {{"className", "live-pane"}}, {
        h("div", {{"className", "lib-split"}}, {
            g.searchBox(),
            ui::select({.label = tr(en, "Status", "Статус"), .value = status,
                        .options = {{"", tr(en, "Any", "Любой")}, {"open", "open"}, {"investigating", "investigating"}, {"fixed", "fixed"}},
                        .onChange = [setStatus](const std::string& v) { setStatus(v); }}),
        }),
        g.table(),
        g.pager(),
        ui::text({.text = g.selected.empty() ? tr(en, "Tick rows to select them.", "Отметьте строки, чтобы выбрать их.")
                                             : tr(en, "Selected: ", "Выбрано: ") + picked,
                  .muted = true}),
        ui::text({.text = tr(en, "Click a header to sort: severity by its own order, IDs naturally (INC-2 before INC-10). The same code draws it in a terminal.",
                             "Клик по заголовку сортирует: важность — в своём порядке, ID — естественно (INC-2 раньше INC-10). Тот же код рисует таблицу в терминале."),
                  .muted = true}),
    });
}

constexpr const char* kLibraryTabs[] = {"query", "forms", "grid", "markdown", "calendar", "dnd", "motion", "charts", "icons", "i18n"};

struct LibraryDemosProps {
    int initial = 0;  // the open tab (web_libraries.html#icons opens "icons")
    std::string key{};
};

Element LibraryDemos(const LibraryDemosProps& p) {
    bool en = useEnglish();
    return h("div", {{"className", "live"}}, {
        ui::tabs({.tabs = {{kLibraryTabs[0], {h(QueryDemo)}},
                           {kLibraryTabs[1], {h(FormsDemo)}},
                           {kLibraryTabs[2], {h(GridDemo)}},
                           {kLibraryTabs[3], {h(MarkdownDemo)}},
                           {kLibraryTabs[4], {h(CalendarDemo)}},
                           {kLibraryTabs[5], {h(DndDemo)}},
                           {kLibraryTabs[6], {h(MotionDemo)}},
                           {kLibraryTabs[7], {h(ChartsDemo)}},
                           {kLibraryTabs[8], {h(IconsDemo)}},
                           {kLibraryTabs[9], {h(I18nDemo)}}},
                  .initial = p.initial}),
        ui::text({.text = tr(en, "Each library: target_link_libraries(my_app PRIVATE reactx::<name>).",
                             "Каждая библиотека: target_link_libraries(my_app PRIVATE reactx::<имя>)."),
                  .muted = true}),
    });
}
