## Сводный отчёт #1 — Kimi (kimi-k2.5), 2026-10-29

Источники: llms.txt (подлинность подтверждена), README репозитория,
сборка условного проекта TaskFlow (роутер, стор, useFetch, ui::, тесты).

### API / эргономика

1. **[P0][api]** `onChange` чекбокса отдаёт строки `"true"/"false"` вместо `bool`.
В типизированном языке это stringly-typed утечка; каждый пользователь пишет
`v == "true"` сам. Фикс: отдельный тип обработчика `BoolHandler` для чекбоксов
(InputHandler оставить для текстовых полей). Ожидаемый эффект: -1 класс багов у каждого пользователя.
2. **[P0][api]** `useLocalStorage` в нативной сборке **молча** работает как memory-only.
Код «работает» в терминале, данные теряются, предупреждения нет.
Фикс: либо `REACTX_PRE`-подобный warn при первом использовании нативно,
либо переименовать в `useStorage` и честно документировать, либо file-backed
реализация для терминала (~50 строк).
3. **[P0][api]** Контролируемые инпуты: «если хендлер не изменил state — значение
вернётся обратно». Побочный эффект зависит от совпадения данных — классический
источник хитрых багов (например, валидация «запретить ввод символа X» ломается
неочевидно). Фикс: опциональный проп `enforce`/`immutable` или честная дока с примером валидации.
4. **[P1][api]** Магические соглашения в props-структурах: поле `key` становится
ключом элемента, `children` — детьми, дефолт нужен на каждом поле, `operator==`
руками для memo. Это неявные протоколы через имена. Фикс: концепты/трейты
(`Keyed`, `WithChildren`) или хотя бы статическая проверка + понятная ошибка
компиляции при нарушении, а не молчание.
5. **[P1][api]** Два стиля props (структуры vs строковая `Props`-мапа) навсегда.
Фикс: пометить `Props`-стиль deprecated в v1.0 с путём миграции (скрипт/макрос),
чтобы к v2.0 его можно было удалить.
6. **[P1][api]** События мыши: только клики. Нет hover/drag/move — для ui-фреймворка
это дыра (кнопки без hover-стейта выглядят мёртвыми даже в терминале).
Фикс: хотя бы `onMouseEnter/Leave` в браузере; в терминале честно задокументировать невозможность.
7. **[P2][api]** `ScrollEvent`: поля `top/left` вместо привычных `scrollTop/scrollLeft`.
Документация объясняет, но каждый React-разработчик споткнётся. Фикс: алиасы
`scrollTop()`/`scrollLeft()` методами.

### Документация (docs)

8. **[P0][docs]** Сигнатура `setLocalFetch` не показана: неизвестен тип аргумента-хендлера
и возвращаемый тип. ИИ при сборке проекта вынужден «угадывать»
(у меня: `FetchRequest -> FetchResponse::json(...)`). Фикс: добавить сигнатуру
и полный пример в llms.txt и docs/data.
9. **[P0][docs]** Итерация по JSON-массиву (`asArray()`) есть в README, но **нет в llms.txt** —
а llms.txt позиционируется как «the whole API» для ИИ. Фикс: синхронизировать
llms.txt с README по полноте примеров.
10. **[P1][docs]** Противоречие: README говорит «Native builds need GCC 16+», но в разделе
macOS команда с `g++-14`. Фикс: поправить или пояснить (brew-нумерация?).
11. **[P1][docs]** Нет списка «частых ошибок компиляции и что они значат». С шаблонами
первый контакт с ошибкой — 200 строк мясорубки. Фикс: раздел
«Troubleshooting» с 5–10 типовыми ошибками и человеческими объяснениями
(это самый дешёвый способ улучшить DX).
12. **[P2][docs]** `docs` ссылки в llms.txt ведут на sargisis.github.io/ReactX/docs/* —
стоит проверить, что все разделы реально существуют (dead links убивают доверие).

### DX / качество (dx)

13. **[P0][dx]** Канал: `mountDom` нативно — намеренная ошибка компиляции. Работает,
но первый нативный билд веб-проекта всегда падает с трюком. Фикс: `#ifdef`-обёртка
с `static_assert` и понятным сообщением — эффект тот же, впечатление мягче.
14. **[P1][dx]** `reactx add <git-url>` ставит зависимости напрямую из git без упоминаний
lockfile/хешей — supply-chain риск. Фикс: фиксация commit-sha в reactx.json

    - `reactx update` с проверкой.

15. **[P1][dx]** Тестирование через строки: `findByProp("id", "x")`, `dispatch(..., "onClick")`.
Ренейм пропа молча ломает тесты. Фикс: типизированные хелперы
(`findByProp<&TaskCardProps::id>(...)`) или генерация из props-структуры.

### Риски / зрелость (bug-risk)

16. **[P0][bug-risk]** Однопоточная модель («никогда не трогай state из другого потока»)
без компиляторной проверки. При росте кодовой базы (и контрибьюторах) одна лямбда
из worker-thread = UB. Фикс: debug-режим с проверкой thread-id в set/update сторов
(дешёво, ловит 90% случаев).
17. **[P1][bug-risk]** «Терминал: layout пересчитывается целиком на каждый рендер»
(из README). На длинных списках будет деградация — и это задокументировано, но
пользователи всё равно ударятся. Фикс: инкрементальный layout для Yoga или
хотя бы проп `static`/`frozen` для поддеревьев.
18. **[P1][bug-risk]** Нет скролла в терминале: контент молча обрезается. Для v1.0
критично — это не edge case, а базовое ожидание от любого UI. Фикс: scroll-container
(offset + viewport) — задача на неделю, эффект огромный.
19. **[P2][bug-risk]** Emoji/CJK ломают ширину строк в терминале. Фикс: wcwidth
или честное ограничение + документация.

### Tests

20. **[P1][tests]** В llms.txt про тесты не сказано, что после `setState` вне `dispatch`
нужен ручной `root.flush()` (это есть в README). ИИ пишет тесты, которые «молча
не проверяют ничего». Фикс: добавить в блок Testing в llms.txt.


## Сводный отчёт #2 — Flint 40B (2026-10-30)

Версия ReactX: v0.8.1 (rev из `reactx.json` проекта: v0.8.1)

## Проверка

- useState / useRef / useMemo / useDebounce / useLocalStorage / useCallback / useKeyboard /
  errorBoundary / memo / useStore — используются и работают (компоненты Today/Stats/App).
- Router: `router::routes`, `router::link`, `router::navigate`, `router::useLocation`, `router::useQuery`-подобные пути.
- UI: `ui::themeProvider(ui::Theme, ...)`, `ui::button`, `ui::input`-паттерн (controlled input через `onInput`).
- Reconciler: keyed lists через props-структуру с полем `key`, `memo<HabitRow>`.
- Controlled input/checkbox: `value` + `onInput`, `checked` + `onChange` ("true"/"false") — работает, ресинхронизация DOM.
- Тесты: `rx::TextRenderer` + `rx::Root`, `root.dispatch`, `root.flush()`, `r.toString()`.

## Найденные проблемы (issues)

### F1. «Одна» заголовка `reactx/reactx.hpp` не покрывает весь API (минорный)

- **Что неожидало:** после `#include "reactx/reactx.hpp"` у меня не появились
  `rx::router`, `rx::ui`, `rx::fetch`, `rx::json`, `rx::dom`, `rx::terminal`,
  `rx::text_renderer` — пришлось добавлять отдельные заголовки.
- **Почему это проблема:** из названия «reactx.hpp» ожидалось «весь фреймворк».
  Для продакшн-кода это лишний шаг: забываешь про один из заголовков и получаешь
  «`rx::router` has not been declared».
- **Статус:** не баг фреймворка, но DX-трэш: либо umbrella-заголовок должен включать
  всё, либо документация должна явно перечислять, что входит в «reactx.hpp».

### F2. `memo` для typed-компонентов требует самоокончательный `operator==` в props

- **Что неожидало:** `memo<HabitRow>` компилировался только после того, как я добавил
  `operator==(const HabitRowProps&) const = default`-аналог с ручным сравнением полей
  (значения), игнорируя обработчики. Без `operator==` — `static_assert` в `makeMemo`:
  «the props struct needs operator==».
- **Почему это проблема:** для props-структур с `Callback`/`InputHandler` (std::function)
  `= default` может не работать как ожидается; приходится писать `operator==` вручную —
  и сравнивать только бизнес-поля, а не хендлеры (они сравниваются по identity из `useCallback`).
- **Статус:** осознанный паттерн, но магия «типа React» (auto-==) не работает для функтор-полей.

### F3. Controlled checkbox: `onChange` получает строку "true"/"false"

- **Что неожявалось/проверялось:** у `<input type="checkbox">` событие уходит как
  `onChange` с текстом `"true"`/`"false"`, а не как bool; фреймворк сам ресинхронизирует
  DOM (`checked`) с C++ состоянием, если хендлер не изменил его.
- **Почему это проблема:** неочевидное соглашение (как у React — событие change), которое
  легко перепутать с `onClick` у checkbox. Работает, но стоит явно документировать
  «checkbox == onChange с строкой, text input == onInput с текстом».

### F4. Инструментальная тонкость: `std::println` / `std::localtime_r`

- **Что неожидало:** `std::println(...)` требует `<print>` (иначе «no matching function»);
  `std::localtime_r` нет в `std` (нужен `std::localtime` / POSIX). ReactX сам пишет через
  `std::println` — для приложений это значит: надо помнить про `<print>`.
- **Статус:** это API-уровень stdlib, но в проекте на C++26 такие мелочи бьют по скорости
  разработки и иногда ломают «просто должно работать».

### F5. Нет программного фокуса (programmatic focus)

- **Что не хватило:** чтобы повесить глобальный `useKeyboard` на конкретное поле ввода,
  нужен imperative focus API (`el.focus()`). Вместо этого я сделал глобальный хоткей
  `Ctrl+Enter` на уровне компонента (Today) — решение, но не то, что было бы «как в React».
- **Статус:** известное ограничение (нет focus API в backend), но ощущение: фреймворк
  покрывает «кнопки/input берут фокус» из коробки, программный фокус — нет.

### F6. Ограничения, с которыми столкнулся при реальной сборке (из документации, подтверждено кодом)

- Browser: нет mouse move/hover/drag; `onScroll` только у прокручиваемого элемента;
  `href`/`src` с `javascript:` — блокируются.
- Terminal: `TerminalApp` требует интерактивный терминал (`Terminal::isInteractive`);
  каждый символ = 1 колонка; фиксированный flush.
- Fetch: без кастомных заголовков/таймаутов; в терминале — только полные `http://` URL.

## Итоги по issues

- **Багов-фреймворку в моей сборке не найдено** — проект собрался нативно (GCC 16) и в
  WebAssembly (Emscripten) с первой итерации после исправлений.
- Основной friction был в DX: umbrella header, memo + operator==, соглашения об обработчиках.
- Конструктивные замечания F1–F6 стоит закрыть/документировать в следующем минорном релизе.

## Что понравилось больше всего

- Props-структуры + designated initializers как «типизация на лету» — опечатка = ошибка компиляции.
- `errorBoundary` + `retry()` — реально полезно.
- `Store`/`useStore` — то, что в React обычно пишут руками.
- Batched updates + flush после render/dispatch/fetch/timers — меньше ручной работы.


## сводный отчёт #3 — GROK-4.1 (2026-10-30)



AI_REPORT.md — ReactX Framework AnalysisDate: 2026-10-29

Framework: https://github.com/sargisis/ReactX (C++26 React-like UI)

Task: Study the framework, build a medium/complex project, report problems and limitations.1. Summary of the FrameworkReactX is a React-inspired UI framework written in modern C++26. Key selling points:
Function components + hooks + reconciler (with keys, batching, memo).
Same component code runs in:
Browser (Emscripten → WASM + DOM)
Terminal (ANSI + Yoga Flexbox)

Typed props structs (h(Counter, {.start = 5})) — typos caught at compile time.
Full-ish app stack in pure C++: router, useFetch, own JSON (reflection on native), HTTP server, CLI (reactx new/dev/build/add/...).
Ready UI atoms (rx::ui: button, input, checkbox, tabs, modal, list).
Strong focus on safety: std::expected, contracts, ASan/UBSan/TSan in CI, fuzzing of parsers.
The documentation site itself is built with ReactX. There is a solid llms.txt aimed at AI assistants and an AGENTS.md for contributors.LOC (rough, implementation): ~5k lines of core C++ (reconciler ~500, terminal renderer ~770, http ~650, json ~430, etc.).2. Project BuiltCreated a medium-complexity Notes application:
Location: /home/workdir/artifacts/my-reactx-project/
Single file: src/main.cpp (~400 lines)
Features intentionally exercised:
Multi-route SPA (router::routes, link, navigate, useParam, useQuery)
Shared state via createStore / useStore
useLocalStorage, useDebounce (search), useKeyboard (Ctrl+N)
useMemo for filtered list, controlled inputs, errorBoundary
ui::button / input / list / modal
Context for theme
Keyed list items, add/edit/delete/pin/archive flows
Dual main (#ifdef __EMSCRIPTEN__ mountDom vs TerminalApp)

Important caveat: The sandbox environment has only GCC 13 and no Emscripten / Ninja / Docker. Therefore the project was not compiled or executed. It is a faithful exercise of the public API as documented in llms.txt + examples (web_app.cpp, todo.cpp, etc.). Minor API mismatches (exact method names on store, textarea support, some ui:: field names) may exist; they would surface immediately under a real reactx build.Existing official complex examples already cover a lot of the same ground (examples/web_app.cpp is a full-stack todo with router + API server + ui::).3. Problems & Limitations Found3.1 Toolchain & Ecosystem (Critical for adoption)

IssueSeverityNotesRequires GCC 16+ (C++26)HighExtremely bleeding-edge. Most distros / CI images still on 13–15. macOS needs Homebrew GCC. Windows effectively only via WSL.Emscripten 6+ required for webHighNon-trivial install; version pinning matters.No official binary packages for all platforms yetMediumPrebuilt reactx CLI exists for Linux x86_64; otherwise build from source.clangd vs MSVC/IntelliSenseMediumProject templates now ship .clangd (good). Older projects need manual copy.Module + sanitizers / reflection interactionsMediumDocumented traps in AGENTS.md (inplace_vector in modules, -freflection breaks import, sanitizers disable modules).Impact: “Just try it” friction is high compared with Preact/React or even Qt.3.2 Runtime / Feature Gaps (from code + TODO.md + docs)
No mouse hover / move / drag / wheel beyond basic scroll
Documented: only onClick, onInput/onChange, onKey, onFocus/onBlur, onScroll, onSubmit.
Makes rich interactions (tooltips on hover, drag-and-drop Kanban, context menus) hard or impossible without custom backend work.
memo does not compare children
Explicitly noted in TODO and in the reconciler comments / docs. Any element with children always re-renders under memo. This limits optimisation of larger trees.
First-load WASM experience
User feedback quoted in TODO: “does not feel native”. Planned mitigations (preload, skeleton, service worker, Brotli) are still open. Current size for a counter is ~135 KB — respectable but not Preact-level.
Limited built-in UI library
rx::ui has the basics (button, input, checkbox, tabs, modal, list). Missing (explicitly in TODO):
Virtualized lists (critical for 10k+ rows)
Rich forms + validation driven by C++26 reflection
Charts / graphs
Toast / snackbar
Dropdown / menu with keyboard nav
Tooltip
More button variants, loading states, etc.

Networking
useFetch + low-level fetch exist.
No WebSockets or Server-Sent Events (called out as “главный пробел” in TODO).
Terminal build supports full http:// URLs only (no HTTPS).
Server is native-only (expected).

Terminal vs Browser divergence
Different host tags (box/text/input/button vs arbitrary HTML + SVG).
Layout model is the same (Yoga) but defaults and clipping differ.
Focus / keyboard behaviour is thoughtfully unified, but pixel-perfect parity is impossible.
Some props (e.g. style CSS string) are browser-only.

Reflection & JSON
json::to / json::from<T> with reflection only available on native builds (link reactx::reflection). Browser builds must stay within Emscripten’s libc++ subset (no reflection, no std::generator, no <stacktrace>, no threads/sockets/FS).
No official JSX-like syntax
There is a DSL (tag::div.cls(...)) and the classic h(), plus props structs. Comfortable for C++ but still more verbose than JSX. Lambda components are supported (good recent addition).
3.3 Correctness / Robustness Observations
Reconciler implements a proper keyed algorithm with longest-increasing-subsequence optimisation (good).
Batching + flush model is solid; setters only mark dirty.
Error boundaries catch render-time exceptions (not event handlers / effects) — same as React.
Recent CHANGELOG shows multiple bugs found by AI agents while writing real apps (mountDom link error, base-path static files, lambda component deduction, etc.). The project is actively fixing AI-discovered issues — a positive signal.
Fuzzing of parsers + static analyser in CI is excellent for a young project.
Memory optimisation already done (−28 % per list node in 0.8.0).
3.4 Documentation & DXStrengths:
Excellent llms.txt for AI.
Bilingual docs (EN/RU).
Clear dual-target main pattern.
CLI is thoughtful (reactx new, hot-reload dev server written in C++, proxy support).
Weaknesses:
Some docs still primarily Russian (CHANGELOG, parts of site).
API surface is still evolving (MAJOR = 0).
No large public gallery of real applications yet.
Editor experience depends on clangd + compile_commands; first-time setup can still surprise.
3.5 Missing / Incomplete Pieces for “Complex” Apps
Global modal / toast manager (currently each modal is local state).
Optimistic updates / offline-first patterns (possible but must be hand-rolled).
Accessibility beyond basic focus trap (data-rx-trap) and useId for labels.
Internationalisation helpers.
Animation / transition primitives.
Testing story beyond TextRenderer + dispatch (no visual regression, limited component testing helpers).
4. Positive Observations
Extremely ambitious and coherent design: one language, two runtimes, typed props, modern C++ idioms (std::expected, ranges, contracts).
The reconciler and fiber model look carefully engineered.
Safety culture (sanitizers, fuzzing, SECURITY.md) is better than many larger C++ projects.
CLI + project templates + .devcontainer lower the barrier once the toolchain is present.
Active iteration; AI agents are already used as testers and their findings are fixed quickly.
5. Recommendations (for the framework authors)
Ship easier onboarding: official Docker image is already there — promote it harder; consider a one-click GitHub Codespace / DevPod template.
Prioritise virtualized list + richer form primitives — these unblock the majority of “real” business UIs.
Add WebSocket / SSE support — the biggest remaining gap for modern web apps.
Make memo children-aware (or document a clear pattern for memoising subtrees).
First-load polish (preload, skeleton, SW) — directly addresses the “not native” feedback.
Keep the AI surface excellent — llms.txt + AGENTS.md are already better than most frameworks.
Consider a stable “C++20 subset” mode or a compatibility layer so more compilers can at least run the terminal target.
6. ConclusionReactX is a high-quality, ambitious experiment that successfully brings React’s mental model into modern C++ with dual terminal/browser backends. For developers who already live in bleeding-edge C++ toolchains it is already usable for non-trivial apps (the official site and the full-stack todo example prove it).The main practical problems today are:
Toolchain barrier (GCC 16 / Emscripten),
Missing interaction primitives (hover/drag) and higher-level UI components,
Incomplete real-time networking,
First-load UX of the WASM payload.
None of these are fundamental design flaws; most are already tracked in the author’s TODO. With continued focus on DX and a richer component set, ReactX could become a compelling niche for C++-centric full-stack and TUI applications.

## Свободно отчет номер 4 Cursor (model Grok 4.6) 

# AI_REPORT — ReactX после реальной работы
Репозиторий: [sargisis/ReactX](https://github.com/sargisis/ReactX) (v0.8.1).  
Я читал README, `llms.txt`, `AGENTS.md`, примеры и публичные заголовки, затем собрал приложение **Tempo** (трекер времени) на этом API.
## Что нравится
- **Идея «React, но C++26» честная.** Функции-компоненты, хуки, keyed-reconciler, батчинг `setState` — модель знакомая, и она не разваливается на среднем приложении с несколькими страницами.
- **Типизированные props-структуры.** `h(Card, {.title = "x"})` ловит опечатки на этапе компиляции. После JavaScript-React это ощущается как роскошь.
- **Документация для агентов.** `llms.txt` — лучшая часть проекта: полный API, готовые программы, список ограничений и типичных ошибок. Без него легко выдумать `onMouseMove` или HTTPS в терминале.
- **`reactx new` / `reactx dev`.** Шаблон сразу даёт роутер, `rx::ui`, CSS, devcontainer и `AGENTS.md`. Для старта приложения CLI полезнее, чем голый CMake фреймворка.
- **`setLocalFetch`.** Один и тот же `handle()` крутится в тестах и в браузере без Node. Tempo так и устроен: REST в заголовке, WASM без бэкенда, данные в `localStorage`.
- **Два бэкенда из коробки.** Терминал (Yoga + ANSI) и DOM/WASM — редкая и цельная задумка. Сайт документации, собранный на себе же, добавляет доверия.
- **Инженерия.** `std::expected`, контракты, hardening, ASan/UBSan/TSan в CI, отдельный JSON, свой HTTP. Это не «обёртка над Qt», а аккуратный стек.
- **Тесты UI без браузера.** `TextRenderer` + `Root::dispatch` — правильный уровень для компонентов.
## Что не нравится
- **Порог входа высокий и хрупкий.** Нужны GCC 16 (experimental) и Emscripten 6. На Ubuntu из коробки это не «скачал и поехал». `reactx new` прописал `rev: v0.8.0`, а на GitHub живой тег только `v0.8.1` — FetchContent сразу падает на `invalid reference`.
- **`rx::ui` всё ещё терминальный.** В браузере кнопки/инпуты работают, но выглядят как приезжие: мало плотности, мало HTML-семантики, нет нормального `select`/`color` в типизированном API. Для веб-приложения средней сложности всё равно пишешь свои `div`/`svg`.
- **Событий мало.** Нет hover, drag, pointermove. Канбан с перетаскиванием честно не сделать — в README это сказано, но для «среднего» продукта это сразу потолок.
- **`useFetch` прячет данные на время reload.** `ok()` становится false, пока тело ещё старое. Список проектов на мгновение вспыхивает пустым. Приходится смотреть в `response.ok()`, а не в `ok()`. Это расхождение с привычным React Query / SWR.
- **Локальный fetch асинхронный в WASM и синхронный нативно.** Тесты API простые, UI-тесты с `useFetch` в браузере уже про таймеры и гонки `pending` id. Для агента это сюрприз.
- **Два мира стандартной библиотеки.** Reflection / contracts / `std::generator` только нативно. Пишешь приложение «на оба бэкенда» — и сразу режешь C++26 пополам. Tempo из‑за этого веб-only.
- **Терминал сырой для реальных данных:** нет скролла, курсор инпута всегда в конце, эмодзи ломают ширину. Dual-render обещание сильнее, чем текущий TUI.
- **Сеть тонкая.** Нет HTTPS в native fetch, нет заголовков, таймаутов, отмены, WebSocket. `HttpServer` сам автор помечает как dev-only — хорошо, что честно, плохо, что «fullstack на C++» пока скорее демо, чем прод.
- **Пустой README на GitHub в веб-превью** (на диске README нормальный). Со стороны «зайди на github.com/sargisis/ReactX» репозиторий выглядит пустее, чем он есть.
- **Версия в текстах плавает.** В примерах ещё мелькает «C++20», CLI 0.8.0, тег 0.8.1. Мелочь, но агент на этом спотыкается.
## Итог
ReactX — сильный авторский фреймворк, не игрушечный reconciler. На нём реально собрать среднее SPA: роутер, хуки, JSON, локальный API, SVG. Я бы рекомендовал его как лабораторию современного C++ и как альтернативу Electron для узкого класса инструментов.
Я бы не рекомендовал его ещё как замену React+Vite для продукта с богатой мышью, мобильным UX или простым онбордингом. Главный долг — веб-полировка `rx::ui`, события указателя, предсказуемый `useFetch` и совпадение тегов CLI с GitHub Releases.
Tempo лежит рядом с этим файлом: `src/`, `public/style.css`, `tests/test_tempo.cpp`. Запуск: `reactx dev` (нужны `~/.local/bin` и EMSDK).

# FRAMEWORK_ISSUES - Atlas Flint 40B model

Версия ReactX: v0.8.1 (rev из `reactx.json` проекта: v0.8.1)

## Проверка

- useState / useReducer / useMemo / useEffect / useDebounce / useFetch / useLocalStorage /
  useWindowSize / useInterval / useTimeout / useKeyboard / useStore / useContext — используются и
  работают (App/Board/Stats/Settings).
- Router: `router::routes`, `router::link`, `router::navigate`, `router::useLocation`.
- UI: `ui::themeProvider(ui::Theme, ...)`, `ui::button`, `ui::input`, `ui::checkbox`, `ui::Variant`.
- Reconciler: keyed lists через props-структуру с полем `key`, `memo<TaskCard>`.
- Controlled input/checkbox: `value` + `onInput`, `checked` + `onChange` ("true"/"false").
- Тесты: `rx::TextRenderer` + `rx::Root`, `root.render(h(Board, ...))`, `root.flush()`, `r.toString()`.

## Найденные проблемы (issues)

### F1. «Одна» заголовок `reactx/reactx.hpp` не покрывает весь API (минорный)

- **Что неожидало:** после `#include "reactx/reactx.hpp"` не оказались `rx::router`, `rx::ui`,
  `rx::fetch`, `rx::json`, `rx::dom`, `rx::terminal`, `rx::text_renderer` — пришлось добавлять
  отдельные заголовки.
- **Статус:** DX-трэш. Файл называется «reactx.hpp», но это не umbrella-заголовок.

### F2. `memo` для typed-компонентов требует самоокончательный `operator==` в props

- **Что неожидало:** `memo<TaskCard>` требует `operator==` в props-структуре (static_assert:
  «the props struct needs operator==»); для props с `Callback`/`InputHandler` (= std::function) нужно
  писать `operator==` вручную, сравнивая только бизнес-поля.
- **Статус:** осознанный паттерн, но «auto-==» из мира React здесь не работает.

### F3. Controlled checkbox: `onChange` получает строку "true"/"false"

- **Что проверялось:** `<input type="checkbox">` → `onChange` с текстом `"true"`/`"false"`;
  `checked` ресинхронизируется из C++ состояния.
- **Статус:** работает, но неочевидно (как в React: change-событие).

### F4. Инструментальные тонкости C++/ReactX

- `std::println` требует `<print>`; `TerminalApp` требует интерактивный терминал
  (`Terminal::isInteractive`); `mountDom` — только в `#ifdef __EMSCRIPTEN__`.
- `Dispatch` из `useReducer` имеет только конструктор с параметрами — в props-структурах
  поля типа `Dispatch` с дефолт-значением использовать нельзя.

### F5. Программный фокус (programmatic focus)

- Нет imperative focus API; для глобальных хоткеев использовал `useKeyboard` на уровне `App`
  (`Ctrl+Enter` → `router::navigate("/")`).

### F6. Ограничения, с которыми столкнулся при реальной сборке (из документации, подтверждено кодом)

- Browser: нет mouse move/hover/drag; `onScroll` только у прокручиваемого элемента.
- Terminal: фиксированный flush.
- Fetch: без кастомных заголовков/таймаутов; в терминале — только полные `http://` URL.

## Итоги по issues

- Багов-фреймворку в моей сборке не найдено — проект собрался нативно (GCC 16) и в WebAssembly
  (Emscripten) с первой итерации после исправлений.
- Основной friction — тот же, что и в pulse: umbrella header, memo + operator==, соглашения об
  обработчиках, плюс одна новая: имя компонента не может совпадать с именем типизированного props.

## Что понравилось больше всего

- Props-структуры + designated initializers как «типизация на лету» — опечатка = ошибка компиляции.
- `errorBoundary` + reducer + store + fetch + json — полный цикл «данные → состояние → UI».
- Batched updates + flush после render/dispatch/fetch/timers.


## Сводный отчёт Runable 

AI_REPORT — опыт разработки ForgeDesk на ReactX

Дата: 29 октября 2026. Объект: sargisis/ReactX, версия 0.8.1, commit 3f563c64cb0834d85b736c75ea3302dd0a4aa0ac. Сценарий: браузерная локальная система управления разработкой, а не счётчик или todo-demo.

Вывод

ReactX пригоден для нетривиального браузерного приложения, но пока требует заметного прикладного слоя вокруг себя. Компоненты, состояния, маршруты, управляемые поля и DOM-бэкенд действительно работают. ForgeDesk собирается в WASM и проходит проверки зависимостей, транзакций и браузерных взаимодействий.

Главные неприятные места для меня: подавление ошибок localStorage, отсутствие строгой сериализации обычных структур в браузерной сборке, узкий контракт событий и стоимость владения состоянием на C++. Из-за этого часть обещания «весь стек на C++» на практике означает «основное приложение на C++, а интеграции с браузером всё равно через небольшой JavaScript-мост».

Я бы использовал ReactX для C++-ориентированных инструментов, исследований WASM и локальных инженерных приложений. Для массового SaaS с совместной работой, развитой доступностью, сложными жестами и большим количеством интеграций сначала потребовал бы дополнительные библиотеки, профилирование и более зрелые контракты браузерных APIs. Это инженерная рекомендация, не доказательство непригодности фреймворка.

Что было построено и проверено

ForgeDesk содержит типизированную модель проектов/спринтов/задач, DAG зависимостей, чеклисты, комментарии, аудит, фильтры, канбан, таблицу, атомарные массовые операции, метрики, JSON-backup и bounded undo/redo.

В проекте непосредственно используются h, типизированные props компонентов, useState, функциональные setter-updates, useEffect, deps, useStore, createStore, useDebounce, useKeyboard, router, errorBoundary, mountDom, rx::json. Это не другой UI-фреймворк под названием ReactX.

Проверка

Фактический результат

Чистая конфигурация с FetchContent по закреплённому commit

Успешна; не зависит от локального checkout фреймворка

Release, Emscripten 6.0.13, CMake 3.31.6, Ninja

Успешная сборка C++26 / WASM

Доменный executable под Node

84 проверки пройдены

Цепь зависимостей из 5 000 задач

Валидация проходит; это проверка отсутствия рекурсивного переполнения, не UI-бенчмарк

Реальный Chromium, Playwright

27 утверждений пройдены, необработанных исключений страницы не зарегистрировано

Мобильная ширина

Проверена 390 px, нет горизонтального overflow документа; канбан прокручивается внутри себя

Повреждённый localStorage

Исходная строка сохраняется, не перезаписывается при редактировании демо

QuotaExceededError

Симулирован; приложение показывает «Не сохранено», а не ложный успех

ctest запускает один тестовый executable, внутри которого 84 проверки. Это не 84 независимых CTest-теста. Браузерные 27 утверждений проходят последовательно в smoke-сценарии, не являются полной матрицей браузеров.

Состав проверенной поставляемой сборки: WASM около 794 КБ, JS-загрузчик около 24 КБ, CSS около 12 КБ без gzip/Brotli. Это измерения ForgeDesk в данной Release-конфигурации, не минимальный размер ReactX и не скорость загрузки по сети.

Что понравилось

1. Компонентная модель не декоративная

Функции действительно описывают дерево элементов, reconciler обновляет интерфейс, ключи применяются к спискам. Управляемые input, textarea, select и checkbox прошли браузерные сценарии. Маршрутизация через History API работает без перезагрузки и поддерживает Back/Forward.

Это позволило не писать ручной код обновления DOM для каждого действия. Проверено на ForgeDesk, реализация также видна в src/dom.cpp и src/reconciler.cpp ReactX.[^dom]

2. Типизированные props — хорошая граница

EditorProps и BoardProps в ForgeDesk — обычные C++-структуры. Для пользовательских компонентов поля проверяются компилятором, в отличие от строковых словарей. Можно отделить доменную модель от UI и тестировать правила до браузера.[^element]

3. Есть полезные меры защиты, которые нельзя игнорировать в критике

У JSON-парсера есть ограничение глубины; tryParse возвращает std::expected. DOM-бэкенд блокирует javascript: в URL-атрибутах, управляемые поля синхронизируются, имеются focus trap и восстановление фокуса для модальных окон. Поэтому утверждения вроде «фреймворк совсем не заботится о безопасности» были бы несправедливы.[^json-parser][^dom]

В репозитории присутствует CI-конфигурация native/Web, ASan/UBSan/LSan, TSan и анализатора. Я изучил конфигурацию, но не запускал эти upstream-задачи и не подтверждаю их актуальный статус.[^ci]

4. CMake-интеграция практичная

Встраивание как dependency отключает примеры и инструменты; reactx_add_app задаёт shell, копирует public и формирует выходной каталог. ForgeDesk не потребовал изменений исходников библиотеки. Удалось повторить сборку с чистым скачиванием по commit.[^cmake]

Что не понравилось — конкретно

P1. Ошибка сохранения превращается в молчаливый успех

Факт из исходников: rx_js_storage_set оборачивает localStorage.setItem в try/catch, но catch пуст. storageSet имеет void-контракт. useLocalStorage сохраняет состояние через effect и не возвращает status/error.[^storage][^hooks]

Почему это плохо: задача «состояние переживает перезагрузку» важнее удобства простого хука. Quota, запрет доступа или sandbox могут оставить изменения только в памяти, а UI не сможет честно сообщить пользователю, что они не сохранены. Ошибка чтения тоже превращается в пустую строку — невозможно отличить недоступность хранилища от отсутствия записи.

Что пришлось сделать: собственный src/browser.cpp возвращает std::expected для чтения и записи; persist() показывает ошибку. Дополнительно повреждённые данные не заменяются демо автоматически. Симуляция quota проверяет поведение этого адаптера, а не upstream-хука.

Что я изменил бы в ReactX: публичный storage adapter с expected, различением missing/error и возможностью подмены для тестов. useLocalStorage должен отдавать состояние сохранения либо принимать обработчик ошибки.

P1. «JSON для структур» не переносится в браузер

Факт: json::to/from для агрегатов доступны под __cpp_lib_reflection. Заголовок прямо предупреждает, что Emscripten не поддерживает эту рефлексию. Browser branch CMake также не даёт нативные C++26-возможности автоматически.[^json][^cmake]

Влияние: объявления Task, Sprint, Workspace недостаточно. Для экспорта/импорта пришлось вручную описать поля в encode/decode, проверять ссылки, числа, пределы и versioned schema. При добавлении поля надо обновлять модель, encode, decode, validation, UI и тесты.

Это не баг компилятора или ReactX: ограничение честно задокументировано. Но с точки зрения автора приложения именно браузерный путь оказался существенно более ручным, чем привлекательный пример json::from<MyStruct>.

Предложение: переносимый serializer customization point, generator или descriptors для полей, не зависящие от экспериментальной рефлексии. Доменную валидацию это не заменяет, но убирает дублирование структуры.

P1/P2. Удобный get<int> слишком мягок для внешних данных

Факт, подтверждённый отдельным framework_probes executable:

get<int> from 2.9: 2
get<int> missing field: -1
get<int> wrong type: -1
duplicate JSON keys: {"id":2}
tryParse rejects malformed: yes

Value::get<T> при числовом значении делает static_cast<T>, а missing/wrong type возвращает fallback. Парсер при повторном ключе использует insert_or_assign.[^json][^json-parser]

Проблема: это удобно для необязательных настроек, но плохо как единственная граница импорта. Поле points: 2.9 тихо становится 2; отсутствие поля и неправильный тип выглядят одинаково. Дублированные ключи получают last-wins semantics.

Обход в ForgeDesk: проверка точного типа, целочисленности, диапазона и всех обязательных полей до построения модели. Затем проверка отношений и DAG. Дубли ключей на уровне parser пока не запрещаются — это известное ограничение нашего импорта, а не решённая нами проблема.

Предложение: require<T>(key) с путём ошибки и проверкой диапазона; optional strict duplicate-key mode. Существующий fallback-getter можно оставить для удобного чтения необязательных данных.

P2. События достаточны для формы, но узки для сложного браузерного UI

Факт: публичные event payloads — monostate, KeyEvent, string, ScrollEvent. Нет pointer/mouse/drag payload и доступа к DataTransfer/FileList. В llms.txt отсутствие mouse move, hover и drag прямо указано.[^element][^llms]

Практический эффект: у канбана ForgeDesk нет штатного drag-and-drop. Я не стал показывать декоративные drag handles, которые ничего не делают; перемещение выполняется сменой статуса. JSON-импорт сделан вставкой текста, экспорт — Blob через EM_JS.

Предложение: browser-specific event types/extension bridge, файловый input adapter, pointer events и явный cancel/default propagation contract. Не нужно делать всё это обязательным для terminal backend.

P2. Владение состоянием и callbacks заметно увеличивает объём кода

Факт: useState<T> возвращает pair<T, Setter<T>>, то есть значение состояния копируется. Setter обновляет hook после проверки equality и планирует rerender; store поддерживает selectors. useCallback и memo доступны, поэтому утверждать, что оптимизации совсем отсутствуют, неверно.[^hooks][^store]

Мой опыт: C++-лямбды надо аккуратно захватывать по значению; нельзя переносить привычные ссылочные захваты в отложенные callbacks. Для одного поля редактора приходится передавать изменение через функцию, которая копирует Task. Сложные вложенные h(...) требуют внимательности к initializer lists и типам handlers. При первой сборке я сам допустил ошибки скобок, затем исправил их — это ошибки моего кода, не дефект библиотеки.

Компромисс ForgeDesk: workspace хранится в Session, ReactX-store содержит revision-сигнал. Транзакции и undo используют полные снимки. Это упрощает correctness, но не предназначено для частых обновлений огромной области. Производительность крупных UI-наборов я не измерял.

Предложение: рекомендации по ownership/lifetime, линтеры или диагностика hook usage, примеры reducers/stores на действительно больших формах. Автоматическое копирование не нужно объявлять багом, но цена должна быть ясной в документации.

P2. Error boundary имеет ожидаемую, но ограниченную область действия

Факт: boundary ловит исключения при рендере потомков; исключения handlers/effects туда не входят. Это прямо описано в API.[^element]

Следствие: нельзя завернуть App в boundary и считать импорт, storage и действия безопасными. Ожидаемые ошибки команды должны быть значениями, а не неконтролируемыми exceptions. ForgeDesk возвращает expected<void,string> и показывает notice.

Моя претензия не к выбору семантики, которая похожа на React, а к отсутствию более целостного прикладного примера: boundary + expected commands + storage/network errors + пользовательские состояния восстановления.

P2/P3. Network API не даёт полного управления жизненным циклом запроса

Факт по статическому чтению: fetch возвращает void, FetchRequest содержит method/url/body/contentType. Публичного cancellation handle, timeout и произвольного headers-map здесь нет. Cleanup useFetch ставит alive=false, чтобы не применять устаревший ответ, но не отменяет сам запрос.[^fetch]

Почему это важно для следующего шага: если превращать ForgeDesk в сетевое приложение, понадобится собственный слой для токенов, дедлайнов, отмены и retries. Игнорирование устаревшего результата защищает состояние, но не экономит запрос и ресурсы сервера.

Граница доказательств: ForgeDesk локален и useFetch не использует. Я не проверял сеть экспериментально; это вывод из публичного API и реализации pinned commit, а не найденный у пользователя сбой.

P3. Документация местами отстаёт от текущего требования C++26

README объясняет GCC 16 / Emscripten 6 и ограничения стандартных библиотек. Но examples/web_app.cpp всё ещё выводит пользователю «C++20», часть комментариев заголовков тоже сохраняет старую терминологию.[^example][^json]

Это не ломает приложение, но создаёт путаницу для первого знакомства: где нужен GCC 16, где достаточно Emscripten, почему reflection-путь не работает в браузере. Для ForgeDesk я указал точные версии, зафиксировал commit и проверил чистый FetchContent build.

Что не понравилось в собственной реализации

Критику фреймворка нельзя смешивать с долгом приложения:

Снимки whole-workspace в undo/redo. До 30 копий плюс сериализация после изменения. Есть лимиты, но это не высокопроизводительное event sourcing. Дальше нужны patches/commands history и IndexedDB.

Нет межвкладочной синхронизации. Последняя запись выигрывает. Нужны revision/CAS, storage events или backend; это сознательно не было в утверждённом локальном scope.

UI не виртуализирован. Таблица и канбан создают все соответствующие элементы. Цепь из 5 000 узлов проверяет validator, не комфортность интерфейса на 5 000 карточках.

Редактор закрывается без предупреждения о dirty draft. Для рабочего продукта добавил бы guard и draft recovery.

Audit не является security log. Комментарии локальные, автор условный, JSON изменяем. Нельзя использовать это как подтверждение действий реальных сотрудников.

Проекты, участники и спринты имеют ограниченный management API. Создание есть; полноценное редактирование, архивирование и переносы не реализованы.

Импорт заменяет область целиком. Нет merge, миграций между версиями схемы и запрета duplicate JSON keys.

Стили и формы написаны вручную. views.cpp стоит разбить на feature modules при дальнейшей работе; доступность вне проверенного focus/label/mobile-smoke требует отдельного аудита.

История SP не хранится. Аналитика — current-state metrics. Выдавать её за burndown было бы неверно.

Приоритетный список улучшений ReactX

Приоритет

Изменение

Польза

P1

Storage errors как публичные значения

Нельзя тихо потерять пользовательские изменения

P1

Переносимый strict serialization/field validation API

Надёжный browser import без копирования схемы вручную

P2

Browser events/extensions, file APIs

Настоящий канбан, upload, сложные жесты

P2

Cancel/timeout/headers для fetch

Управляемая сеть для приложений с backend

P2

Большой пример с transaction/state ownership/error handling

Показывает реальные архитектурные компромиссы, не только базовые hooks

P3

Согласовать версии и terminology в примерах

Меньше ошибочных предположений при запуске

Метод и границы достоверности

Источники — pinned исходники самого ReactX, README/llms и воспроизводимые пробы API. Для поведения конкретного commit это первичный источник; я не выдаю единственный репозиторий за независимое исследование рынка. Пользовательская оценка удобства — мой инженерный опыт, а не объективный рейтинг.

Проверялась браузерная сборка и локальный сервер. Не проверялись: native terminal, upstream CLI, GCC 16, авторизация, реальный HTTP backend, Safari/Firefox, стресс-тест concurrent tabs, ASan/TSan, производственная эксплуатация. Тесты не доказывают отсутствие всех ошибок или безопасную обработку любого hostile input.

В исходниках ReactX ничего не исправлялось. Обходы находятся в проекте и явно описаны. Артефакты проверок — qa/, тесты — tests/; инструкции повторения — README.md.

Первичные источники

Все ссылки зафиксированы на изученном commit; дата чтения — 2026-10-29.

[^storage]: src/utility_hooks.cpp, L111–121 — подавление storage exceptions. [^hooks]: include/reactx/hooks.hpp — Setter, useState, useLocalStorage, эффекты. [^json]: include/reactx/json.hpp, L147–170 и reflection block — getter/fallback/casts и conditional reflection API. [^json-parser]: src/json.cpp, L95–126 — глубина, object parser, insert_or_assign. [^element]: include/reactx/element.hpp — event types, typed props, boundary. [^dom]: src/dom.cpp — DOM, URL guard, controlled fields, focus, delegated events. [^store]: include/reactx/store.hpp — shared state и selector subscriptions. [^fetch]: include/reactx/fetch.hpp, src/fetch.cpp, L163–184 — request contract и alive-flag cleanup. [^cmake]: CMakeLists.txt, cmake/ReactXApp.cmake — browser branch и app helper. [^llms]: llms.txt — явные ограничения events и browser standard library. [^ci]: .github/workflows/pages.yml — конфигурация upstream-проверок, не результаты их выполнения. [^example]: examples/web_app.cpp, L48 и L186, README.ru.md — различие terminology и требования запуска.


## Сводный отчёт #7 — Claude (claude-opus-5-5), 2026-11-01

Как писал: только по `llms.txt` и `AGENTS.md` нового проекта, CLI v0.10.0 с install-пути, проекты вне репозитория
(`~/Desktop/reactx-trials/`). Исходники ReactX открывал только после того, как баг воспроизведён, — чтобы найти причину.
Проверка: веб — безголовый Brave через CDP (клики, ввод, Escape, скриншоты, 1000 px и 390 px);
терминал — настоящий pty 80×24 + эмулятор pyte (клавиши, мышь, resize).

Проекты:
1. **budget** (веб): роутер с `:id` и `?cat=`, `createStore`, `useFetch` + `setLocalFetch`, `storageRead/Write`,
   `ui::input/select/button/confirm/toast/card/stack/text`, SVG-график, тесты на `TextRenderer`.
2. **notes** (терминал): 60 заметок с `rx::net::HttpServer` (отдельная цель), `useFetch` по http, `useDebounce`-поиск,
   `useKeyboard` (↑↓, Ctrl+O/D/Q), `useInterval`-часы, `ui::list/listItem/modal/confirm/toaster`.

Итог: оба проекта собрались почти сразу (веб — после 3 правок, терминал — с первого раза), консоль чистая,
localStorage, deep links, назад, Escape в веб-диалоге, тосты, resize — работают. Ниже то, что сломалось.

### Баги

**C1. [P0][bug][web] `ui::select`: вариант с `value = ""` не выбирается и отдаёт свою подпись вместо значения.**
`ui::select({.value = "", .options = {{"", "All"}, {"food", "Food"}}})` → в DOM `<option>` без атрибута `value`,
`select.selectedIndex == -1` (поле пустое), а выбор «All» вызывает `onChange("All")` вместо `onChange("")`.
У меня фильтр стал `?cat=All` и список опустел. Типовой случай «Все / не выбрано» сломан.
Причина: `src/dom.cpp` `rxSetProp`, ветка `name === "value"`: `if (el.value !== v) el.value = v;` — у нового
`<option>` без текста `el.value` уже `""`, присваивание пропускается, атрибута нет; потом добавляется текст, и `value`
становится «All». Фикс: для `<option>` (и вообще не-полей ввода) писать `setAttribute("value", v)`.
Тест: `h("option", {{"value", ""}}, {"All"})` → `getAttribute("value") === ""`. Терминальная версия select в порядке.

**C2. [P0][bug][terminal] `ui::modal` без кнопок внутри нельзя закрыть: Escape не доходит.**
Модалка с одним текстом (`.children = {h("text", ...)}`) открывается, но Escape, Tab+Enter ничего не делают — пользователь
заперт (Ctrl+C убивает программу). В браузере та же модалка закрывается по Escape. `ui::confirm` (есть кнопки) — работает.
Причина: `TerminalRenderer` шлёт `onKey` от элемента в фокусе вверх по предкам (`if (!focused_) return false;`); в модалке
нет фокусируемых узлов, ловушка фокуса сбрасывает `focused_ = 0`, и `onKey` модалки (`ui_terminal.cpp:106`) не вызывается.
Фикс: когда фокуса нет, а ловушка есть — слать `onKey` в узел ловушки; либо делать коробку модалки `focusable`.

**C3. [P1][bug][terminal] Содержимое выше экрана выталкивает модалки и тосты за край.**
Корневой `box` c `flexGrow: 1` растёт вместе со списком (60 строк при 24 на экране). `ui::modal`/`ui::confirm`
(`position: absolute; top/bottom: 0`) центрируются по этой высоте — окно рисуется ниже экрана, его не видно, но фокус в нём,
ввод «пропадает». То же с `ui::toaster()`. Ожидание: корень `TerminalApp` = размер терминала, лишнее обрезается
(или прокручивается — см. D1). Сейчас llms.txt советует «toaster последним в box, который заполняет экран», но такой box
заполняет не экран, а контент.

### Пробелы (то, чего нет, а пользователь ждёт)

**D1. [P1][gap][terminal] Нет прокрутки.** Выделение `ui::listItem` уходит ниже экрана после ~18 пунктов;
колесо мыши и PageUp/PageDown игнорируются. (Известно, в роадмапе — подтверждаю как главный барьер для терминальных
приложений с данными: любой список из сервера упирается в это.)

### Документация

**E1. [P1][docs] `AGENTS.md` в шаблоне `reactx new` устарел:** «Only click, input, change, keydown and submit events exist».
С v0.9.0 есть pointer/hover/focus/blur/scroll. ИИ-агент, который читает его первым, не станет пользоваться мышью и фокусом.
Там же стоит упомянуть `reactx/ui.hpp` (компоненты) — шаблон им пользуется, а правила о них молчат.

**E2. [P1][docs] `llms.txt` не называет тип варианта `ui::select`.** Литерал `{{"m", "Medium"}}` работает, но список,
собранный в цикле, требует имя типа; я угадал `ui::SelectOption` — ошибка. Правильно `ui::Option{.value, .label}`.

**E3. [P1][docs] Сигнатура `ui::input` `.onSubmit` не указана.** Я передал `Callback` (как у `ui::button`) — ошибка
«no viable conversion to InputHandler». Нужно: `onSubmit(text)` получает текст. Одно слово в llms.txt.

**E4. [P2][docs] Как читать число из `json::Value` без ключа?** Документированы `get<T>(key, fb)`, `require<T>(key)`,
`asString()`; для значения в `asObject()`-цикле (`{"EUR": 0.92}`) пришлось угадать `asNumber()` (угадал). Добавить
`asNumber()/asInt()/asBool()` в строку про JSON.

**E5. [P2][docs] Тесты в веб-проекте.** Рецепт `TextRenderer` есть, но не сказано, как завести тестовую цель в проекте
от `reactx new`. Нативная конфигурация собирает и приложение, и падает на `mountDom` в `main.cpp`; работает только
`cmake --build build/native --target <tests>`. Нужен готовый кусок CMake (или `reactx test`), и чтобы `reactx_add_app`
нативно не собирал веб-`main` (или шаблон сразу с `#ifdef __EMSCRIPTEN__`).

**E6. [P2][dx] Сообщение компилятора, когда в props нет поля `key`.** `h(Row, {.key = "1", ...})` без `std::string key{}`
в структуре → 40 строк «cannot convert initializer list», слово `key` в ошибке не встречается. Добавить в «Common errors».

### Размер

**S1. [P2][size]** Пустой шаблон `reactx new` — 119 КБ `.wasm`; budget — 238 КБ (102 КБ gzip). `reactx build` печатает
размер, но ничего не называет (format/print нет). По линкер-карте: libc++ `from_chars` для double (JSON), `fetch`, `json`,
`ui_web`, сам код приложения. Ничего неправильного, но совет из llms.txt «build назовёт виновника» тут молчит;
полезно было бы печатать топ-5 объектников из `*.link.map`, раз он и так создаётся.

### Что понравилось

- Props-структуры + designated initializers: опечатки ловит компилятор, автодополнение работает.
- `setLocalFetch` + `useFetch` с сохранением старого ответа во время reload — без мигания.
- `ui::confirm` и `ui::toast` в обоих бэкендах: одна строка вместо собственного диалога; Escape, фокус и
  возврат фокуса в поле в терминале — сделаны аккуратно.
- Тот же `App` под `TextRenderer` рисует терминальные теги — тест увидел `‹ All ›` и `Total: 1.99` без браузера.
- `useFetch` к выключенному серверу: понятный текст «can't connect to 127.0.0.1:8097 (Connection refused)».

### Приоритет на исправление

C1, C2 (P0, маленькие фиксы) → E1–E3 (по строке) → C3 + D1 (вместе: прокрутка и размер корня терминала) → E4–E6, S1.

**Статус (2026-11-01, после отчёта):** исправлено в `[Unreleased]` — C1 (`<option value="">`), C2 (клавиши идут в модалку без фокуса),
C3 (`position: "fixed"` у `ui::modal`/`ui::confirm`/`ui::toaster`), D1 (`overflow: "scroll"`, `ui::list({.scroll = true})`,
`ref.scrollIntoView()`, `onScroll` в терминале), E1–E6 (AGENTS.md шаблона, llms.txt). Проверено повторным прогоном
budget (браузер) и notes (pty). Не сделано: S1 (топ объектников в выводе `reactx build`), `reactx test` для веб-проектов.

## Сводный отчёт #8 — Claude (claude-opus-5-5), 2026-11-01, ReactX v0.11.0

Как и в #7: CLI v0.11.0 с install-пути, проекты в `~/Desktop/reactx-trials/`, только `llms.txt` + `AGENTS.md` проекта;
исходники ReactX — только чтобы найти причину уже воспроизведённой проблемы. Браузер — безголовый Brave через CDP
(с эмуляцией фокуса), терминал — pty + pyte.

Проекты:
1. **chat** — один исходник для браузера и терминала: `rx::net::HttpServer` с сообщениями, `reactx dev --proxy`,
   первая страница + опрос раз в секунду (`useInterval` + `fetch`), отправка POST, `useReducer`, `useLocalStorage` (имя),
   автопрокрутка вниз (`ref.scrollIntoView`), подгрузка старых по `onScroll` у верхнего края, `ui::toast` при ошибке.
2. **top** (терминал) — 467 процессов из `/proc` раз в секунду, `useReducer` (сортировка/фильтр/выбор), `useKeyboard`,
   прокрутка `overflow: "scroll"` с выбранной строкой на виду, фильтр в `ui::modal`, `kill` через `ui::confirm`,
   подробности в `errorBoundary`, пауза (`useInterval` с `std::nullopt`).
3. **site** (веб) — `web.base = "/docs-site/"`, `router::routes` с `:slug`, 404, форма регистрации с проверкой
   (`useId`, `aria-invalid`, `aria-describedby`, `onBlur`, `<form onSubmit>`), ответ через `setLocalFetch`,
   пререндер главной через `renderToString`, раздача через `HttpServer::serveDirectory` + `setUrlPrefix`.

Итог: веб-сборки всех трёх — с первого раза; нативные — после одной правки (F5). Работают: прокси dev-сервера,
опрос и отправка, прокрутка и `scrollIntoView` в обоих бэкендах, базовый путь в ссылках и прямых ссылках, 404,
форма с проверкой и отправкой по Enter, модалки и тосты `fixed` поверх длинной таблицы, `kill` через подтверждение.
Консоль браузера чистая, горизонтальной прокрутки на 390 px нет.

### Баги

**F1. [P1][bug] Скрипт перезагрузки `reactx dev` остаётся в `reactx build` и на SPA-хостинге опрашивает сервер вечно.**
`web/project_shell.html` всегда вставляет опрос `__reactx/version` («вне dev запрос упадёт и опрос остановится»).
Но хостинг с SPA-fallback (Netlify/Vercel rewrite, и `HttpServer::serveDirectory(dir, /*spaFallback=*/true)`, который
советует llms.txt) отвечает на `__reactx/version` страницей `index.html` с кодом 200 → опрос каждые 700 мс бесконечно.
Замерено: 25 запросов за ~5 с у одной вкладки. Фикс: скрипт только в `reactx dev` (сервер dev вставляет его в отдаваемый
`index.html`), либо проверять ответ (`/^\d+$/`) и останавливаться.

**F2. [P1][bug] `renderToString` в нативной сборке рисует терминальные версии `ui::` и `router::link`.**
Пререндер главной вне браузера дал `<text focusable underline>Home</text>`, `<box border title="Install">…` вместо
`<a href>` и `<section class="rx-card">`. Причина: нативно линкуются `ui_terminal.cpp`/терминальный `router::link`.
Сайт ReactX пререндерит внутри веб-сборки под Node (`site_prerender.wasm`), но пользователю этот путь не описан,
а llms.txt говорит лишь «put it into `<div id="app">` at build time». Фикс: `reactx build --prerender /,/docs/faq`
(собрать ту же программу Emscripten’ом под Node, выставить location маршрутизатору, вставить HTML) — и описать.

**F3. [P2][bug] Собранный `index.html` минифицирован: `<div id=app></div>`.** Любая самодельная вставка пререндера
по `<div id="app">` (как в llms.txt) не находит место. Решится вместе с F2; до того — сказать в llms.txt.

### Пробелы

**G1. [P1][gap] Нет общего для обоих бэкендов способа прочитать/задать позицию прокрутки.** Чат подгружает старые
сообщения сверху: после вставки 20 строк позиция остаётся `top = 0`, читатель прыгает на 20 сообщений вверх,
а следующий `onScroll` не приходит (позиция не меняется) — подгрузка останавливается. Нужно сохранить позицию
(`scrollHeight` до/после). В браузере — только через `js.hpp`, в терминале — `TerminalRenderer::scrollBy`, у компонента
доступа нет. Предложение: `ref.scrollTo(top)` / `ref.scrollBy(dy)` / `ref.scrollMetrics()` у `NodeRef` (оба бэкенда),
или готовое `{"keepScrollOnPrepend", true}`.

**G2. [P1][gap] Живые данные — только опросом.** Чат делает 1 запрос/с на клиента и видит сообщение с задержкой до 1 с.
Нужны `useWebSocket` / SSE и WebSocket в `HttpServer` (в плане 0.12–0.19 — подтверждаю приоритет).

**G3. [P2][perf] `top` ест 5,7 % CPU** (467 строк, перерисовка раз в секунду; `htop` ~1–2 %). Каждая строка —
компонент с `useNodeRef` + `useEffect`; вся таблица раскладывается Yoga, даже невидимые строки. Стоит измерить:
reconcile vs layout vs вывод; кандидаты — не раскладывать/не рисовать строки вне видимой части scroll-box,
виртуальный список.

**G4. [P2][gap] Держать выбранную строку на виду — 4 строки на каждую строку таблицы** (`useNodeRef` + `useEffect`
+ `ref`). Внутри ReactX уже есть host-проп `scrollIntoView` (им пользуется `ui::listItem`), но он не задокументирован.
Сделать публичным: `{"scrollIntoView", selected}` — одна строка, оба бэкенда.

**G5. [P2][gap] Нет `ui::table`.** Колонки выравниваются вручную `std::format("{:>7}…")`; в браузере так не выйдет.

**G6. [P2][gap] Заголовок вкладки.** Сайт всегда «site»: сменить `document.title` по странице можно только через `js.hpp`.
Нужен `useTitle("FAQ — Tool")` (в терминале — ничего не делает).

**G7. [P2][gap] `ui::input` не растягивается в строке `ui::stack`.** Поле сообщения в чате в терминале — ~10 клеток,
в браузере — половина строки; ни у `ui::input`, ни у детей `ui::stack` нет «займи остаток» (`.grow`).

### Документация

**F4. [P1][docs] `Request::param(name)` возвращает `std::optional<std::string>`** — в llms.txt (`r.param("q")`) это не видно;
первая нативная сборка чата упала на `parseNumber(r.param("limit"))`. Написать `r.param("q").value_or("")`.

**F5. [P2][dx] Нативная сборка веб-проекта по-прежнему собирает веб-`main`** (`mountDom` → ошибка), поэтому
вспомогательные нативные программы (пререндер, свой сервер) собираются только `--target <имя>`. Рецепт есть в AGENTS.md
для тестов, но не для других целей; `reactx_add_app` мог бы нативно не собирать веб-приложение без `--native`.

### Что понравилось

- `reactx dev --proxy http://127.0.0.1:8097` — заработал сразу, и тот же код ходит в `/api` из браузера.
- Один исходник чата для браузера и терминала: отличия — две константы и тег контейнера прокрутки.
- `ref.scrollIntoView()` и `overflow: "scroll"` из v0.11.0 — в чате и в `top` без единой правки фреймворка.
- `ui::confirm` поверх длинной таблицы, `kill` дошёл (`sleep` получил SIGTERM), тост показался поверх всего.
- Форма: `useId` + `htmlFor` дают настоящие подписи (`input.labels`), `aria-invalid`/`aria-describedby` обновляются,
  Enter в поле пароля отправляет `<form>`; базовый путь `/docs-site/` прозрачно добавляется в ссылки.

### Приоритет на исправление (v0.12)

F1 (маленький, затрагивает все опубликованные сайты на SPA-хостингах) → F4, G4 (по строке) → G1 (`scrollTo/scrollBy`,
нужен любому чату/ленте) → F2+F3 (`reactx build --prerender`) → G2 (`useWebSocket`, отдельный релиз) → G3, G5–G7, F5.

**Статус (2026-11-01, после отчёта #8):** исправлено в `[Unreleased]` — F1 (скрипт перезагрузки только в `reactx dev`,
через новый `HttpServer::onFile`), F2+F3 (`"web": {"prerender": [...]}` в `reactx.json`: та же программа под Node,
`mountDom` пишет страницы; `200.html` для SPA-хостингов, `serveDirectory` отдаёт `/x` из `x.html`), F4 (llms.txt),
G1 (`ref.scrollMetrics()` / `scrollTo` / `scrollBy` в обоих бэкендах — проверено на chat: место чтения сохраняется,
подгрузка идёт до начала и в браузере, и в терминале), G4 (публичный проп `scrollIntoView`). Не сделано: G2
(`useWebSocket`, следующий релиз), G3 (скорость `top`), G5 (`ui::table`), G6 (`useTitle`), G7 (`ui::input` в строке), F5.

## Сводный отчёт #9 — Claude (claude-opus-5-5), 2026-11-01, ReactX v0.13.0

Как и раньше: CLI v0.13.0 из релиза (`~/.local/bin/reactx`), проекты в `~/Desktop/reactx-trials/`, только `llms.txt` +
`AGENTS.md` проекта; исходники ReactX — только после того, как проблема воспроизведена. Браузер — безголовый Brave
через CDP, терминал — pty + pyte, CPU — из `/proc/<pid>/stat`.

Проекты:
1. **poll** — живые опросы, один исходник для браузера и терминала: `HttpServer` с REST (создать опрос) и WebSocket
   (голос, рассылка всем), `useWebSocket` с переподключением, форма с динамическим списком вариантов и проверкой
   (пусто, меньше двух, дубликаты), `ui::toast`, `useLocalStorage` (за что голосовал), `reactx dev --proxy`.
2. **inventory** (веб) — таблица на 2000 строк: поиск, `ui::select`, `ui::checkbox`, сортировка по колонкам,
   `memo`-строки, правка в `ui::modal` с проверкой полей (количество, цена «12.50»), удаление через `ui::confirm`,
   сохранение в `localStorage` (184 КБ), нативные тесты на `TextRenderer`.
3. **logs** (терминал) — живая лента логов: 200 строк/с через `useInterval`, до 10 000 строк в `overflow: "scroll"`,
   фильтр, уровень (`ui::select`), пауза, «следить за концом» через `ref.scrollMetrics()` / `scrollTo` и `onScroll`.

Итог: WebSocket из v0.13.0 работает с первого раза в обоих бэкендах — голос из терминала виден в браузере сразу,
после перезапуска сервера страница снова «live» через ~200 мс, `send` без связи возвращает `false`. Таблица на 2000
строк фильтруется за 6–27 мс на нажатие (`memo` работает), сортировка ~100 мс — почти как голый JS (~85 мс), время
уходит на раскладку таблицы браузером. Главная проблема раунда — скорость терминала на длинных списках (G1).

### Баги

**F1. [P1][bug] `useState(std::vector<MyStruct>{})` не компилируется, если у `MyStruct` нет `operator==`.** Ошибка —
глубоко в `stl_algobase.h` («no match for operator== (const Poll, const Poll)»), путь ведёт в `hooks.hpp:179`.
Причина: `Setter` сравнивает значения под `if constexpr (std::equality_comparable<T>)`, а для `std::vector<T>` (и
`optional`, `pair`, `map`) это `true` при любом `T` — их `operator==` не ограничен. То же в store, контексте, `memo`.

**F2. [P1][bug] Терминал не успевает на длинном списке.** logs на 6 600 строках ест 66 % CPU, а таймер на 100 мс
срабатывает вдвое реже (за 66 с пришло 6 620 строк вместо ~13 000). Замер (`reactx_bench`, 10 000 строк в scroll-box):
одна перерисовка 124 мс — дерево Yoga строится заново каждый кадр (41 мс разбор пропсов, 41 мс расчёт), и рисуются
даже невидимые строки (25 мс). Это и есть G3 из #8 (`top` 5,7 % CPU).

### Пробелы и неудобства

**G1. [P1][gap] `useCallback` принимает только `void()`, клавиши, текст и прокрутку.** Обычный приём для `memo`-строк —
`onOpen(id)` — не компилируется: `static_assert "useCallback: not a ReactX event handler"`; `PointerEvent`-обработчик
тоже.

**G2. [P2][gap] `ui::input` не растягивается в строке `ui::stack`** (G7 из #8, подтверждено в обоих бэкендах): поля
вариантов в poll — узкие рядом с кнопкой «×».

**G3. [P2][dx] `ui::checkbox` `.onToggle` — `Callback` без аргумента.** Естественно написать `[set](bool on) {...}` —
и получить огромную ошибку «could not convert» про весь окружающий `ui::stack`. В llms.txt сигнатуры нет.

**G4. [P2][dx] У `FetchResponse` нет `errorText()`** (у `FetchState` есть) — в колбэке `fetch` пишешь `r.errorText()`
и ловишь ошибку компиляции.

**G5. [P2][docs] Ключи у `ui::*`.** В llms.txt не сказано, что у всех `ui::...Props` есть `key`, и он последнее поле:
с designated initializers он пишется после `.children = {...}`. Без ключей строка формы, добавленная над
сфокусированной кнопкой, заменяет её, и фокус в терминале уходит в соседнее поле (семантика как в React, но неожиданно).

**G6. [P2][gap] Подписи `ui::input` стоят слева от поля**, и в форме (Name / Quantity / Price) поля начинаются с разных
мест. Нужен вариант «подпись сверху» или выравнивание — в теме форм 0.20–0.29.

**G7. [P2][perf] Перестановка строк в браузере:** `rx_js_insert` делает `removeChild` + `childNodes[index]` на каждое
перемещение (67 мс `removeChild` за 6 сортировок по 2000 строк). Можно вставлять перед соседним узлом без удаления.
~10–20 % от времени сортировки, не срочно.

### Что понравилось

- `useWebSocket` + `HttpServer::webSocket`: сервер с рассылкой — 30 строк, клиенты в браузере и терминале — один код.
- Переподключение после падения сервера без единой строки в приложении.
- `memo` на 2000 строках таблицы: набор в поиске — единицы миллисекунд.
- `ui::modal` + `ui::confirm` + `ui::toast` в редакторе: Escape закрывает только подтверждение, модалка остаётся.
- `ref.scrollMetrics()` / `scrollTo` и `onScroll(nearBottom)` дают «следить за концом» в терминале в три строки.

### Приоритет на исправление (v0.14)

F1 и G1 (компиляция обычного кода) → F2 (терминал на длинных списках) → G2, G3, G4, G5 → G6, G7 (темы форм и скорости).

**Статус (2026-11-01, после отчёта #9):** исправлено в `[Unreleased]` — F1 (`rx::EqualityComparable<T>` смотрит внутрь
контейнеров; такое состояние перерисовывается при каждом set, `deps(...)` объясняет, чего не хватает), F2 (раскладка
терминала хранится между кадрами и обновляется только там, где дерево изменилось; невидимые строки scroll-box не
рисуются; 10 000 строк: перерисовка 124 → 0,3 мс, +20 строк 143 → 35 мс, logs 66 % → 32 % CPU и успевает за таймером;
рандомизированный тест сравнивает кадр с кадром свежего рендерера), G1 (`useCallback` с любой сигнатурой), G2
(`ui::input` растягивается в строке `ui::stack`), G3 (`ui::checkbox .onChange(bool)`), G4 (`FetchResponse::errorText()`),
G5 (llms.txt). Затем доделано и остальное: G6 (`.labelAbove` у `ui::input` / `ui::select`), G7 (перемещение в DOM —
`insertBefore` рядом с соседом, без `removeChild`: сортировка 2000 строк как на чистом JS), сверка 10 000 компонентов
на кадр — `ui::useVirtualList` (рисуются только видимые строки; logs: 66 % → 2,6 % CPU, без мерцания благодаря
`followEnd`; в браузере 100 000 строк — ~40 в DOM). Плюс `reactx upgrade` и ежедневное напоминание о новом релизе.

---

## Сводный отчёт #10 — Claude (claude-opus-5-5), 2026-11-02, ReactX v0.21.0

Раунд «новичок по одному llms.txt» перед 1.0: CLI v0.21.0 из релиза, проекты в `~/Desktop/reactx-trials/`, только
`llms.txt` + `AGENTS.md` проекта; исходники ReactX — только после того, как проблема воспроизведена. Браузер —
безголовый Brave через CDP (+ axe-core 4.10.3), терминал — pty + pyte, тесты — `reactx test`.

Проекты:
1. **helpdesk** (веб + свой C++ сервер) — вход (`useForm`, `fetch`, сессия в `storageRead/Write`), защищённые вложенные
   маршруты (`/tickets/*` с `.allow` / `.redirect`, внутри `/`, `/new`, `/:id`), `ui::table` с сортировкой, поиском
   (`useDebounce`) и фильтром (`ui::select`), форма заявки (`input`, `textarea`, `radioGroup`, `slider`, проверка
   полей), карточка заявки с `ui::confirm`, `ui::toast`, `useTitle`, живые обновления по `useWebSocket` (вторая
   вкладка видит новую заявку сразу). Сервер: `HttpServer` с REST, токеном в `Authorization`, WebSocket-рассылкой и
   `serveDirectory`. 5 тестов на `testing::Screen` + `setLocalFetch`. `reactx dev --proxy` с WebSocket через прокси.
2. **journal** (терминал) — дневник: список записей (`ui::list .scroll`), поиск, `textarea` с эмодзи, кириллицей и
   китайским, настроение (`radioGroup` в ряд), энергия (`slider`), счётчик слов (`progress`), Ctrl+S/N/D/Q через
   `useKeyboard`, удаление через `ui::confirm`, файл `~/.journal.json` из `main`.

Итог: оба приложения собрались почти с первого раза, всё главное работает в обоих бэкендах. Защита маршрутов,
вложенные маршруты, `useForm`, таблица (на телефоне прокручивается внутри себя, страница не уезжает), WebSocket и
переподключение, сессия после перезагрузки, тесты. Сервер отвечает 403 на чужой `Host` и чужой `Origin`, не отдаёт
dot-файлы. axe: 0 нарушений в светлой теме. Четыре раза «баг» оказывался моим скриптом (клик по ячейке вместо кнопки,
поиск текста «Title» при подписи «Title ▲», позиция в символах вместо колонок, плейсхолдер скрыт у поля в фокусе) —
в отчёт не вошли.

### Баги

**F1. [P1][bug] `reactx build` не копирует изменённые файлы `public/`.** Правка `public/style.css` без правки C++:
«ninja: no work to do», в `build/web/dist/style.css` остаётся старая версия — стиль не доходит до сайта. Причина:
`copy_directory` висит на `POST_BUILD` цели (`cmake/ReactXApp.cmake:77`), а цель не пересобирается.

**F2. [P1][bug] Стрелки в терминале не меняют `ui::radioGroup`.** llms.txt и README: «(•) with Up / Down». На деле
Up/Down только двигают фокус (а Left/Right ничего не делают): `TerminalRenderer::handleKeyNow` обрабатывает Up/Down
как переход фокуса раньше `onKey` элемента (`src/terminal_renderer.cpp:1094`), исключение есть только у textarea.
Кроме того, каждый вариант — отдельная остановка Tab (в браузере группа — одна остановка, стрелки выбирают).

**F3. [P1][gap] У `ui::input` нет пароля.** `InputProps` без `type`: форма входа на `ui::` показывает пароль открытым
текстом (в браузере и в терминале). Нет и `email` / `number` (мобильная клавиатура).

**F4. [P2][bug] llms.txt: «Every field takes `.error`» — у `ui::slider` его нет** (ошибка компиляции «field designator
'error' does not refer to any field in type 'SliderProps'»).

**F5. [P2][bug] Тёмная тема нового проекта не проходит проверку контраста.** В шаблоне CSS у `body` нет фона
(`color-scheme: light dark` и только), поэтому axe/Lighthouse считают фон белым, а подписи `rx::ui` в тёмной теме
светло-серые: 4–6 нарушений color-contrast на каждой странице с формой. С `body { background: Canvas; color:
CanvasText; }` — 0.

**F6. [P2][bug] `ui::progress` в узкой колонке обрезает «%».** Подпись, полоса (24 клетки) и процент сжимаются
поровну; в колонке шириной 30 видно «Today 9/200 ░░░ 4» без «%». Полоса должна сжиматься первой (то же у `slider`).

### Пробелы и неудобства

**G1. [P2][gap] Клик по подписи поля в терминале не ставит фокус в поле** (в браузере `<label>` это делает). В тестах
`s.click("Title")` не фокусирует поле — приходится кликать по плейсхолдеру.

**G2. [P2][gap] `.redirect` защищённого маршрута теряет, куда шёл пользователь.** `/tickets/2` → `/login` → после входа
приложение не знает, что вернуть на `/tickets/2`. Нужен способ узнать исходный путь (например, `?next=` или
`router::redirectedFrom()`).

**G3. [P2][docs] Как собрать свой сервер в веб-проекте?** llms.txt говорит только «keep it in its own CMake target».
Пришлось угадать: `if(NOT EMSCRIPTEN) add_executable(x server/main.cpp) target_link_libraries(x PRIVATE
reactx::reactx) endif()` и отдельный `cmake -S . -B build/server`; `reactx build --native` его не собирает.

**G4. [P2][gap] `ui::text` в браузере склеивает строки** — текст из `textarea` («Since Monday,\nevery morning.»)
показывается одной строкой (в терминале переносы сохраняются).

**G5. [P2][dx] `testing::Screen::click(text)` берёт первое совпадение** — `click("Log in")` попадает в заголовок
карточки «Log in», а не в кнопку; помогает `click("[ Log in ]")`. Это описано, но не очевидно; стоит упомянуть в
llms.txt.

**G6. [P2][size] helpdesk — 304 КБ .wasm (128 КБ gzip)**, из них `src/pages.cpp` 24 %. Стоит посмотреть, что из
`useForm` / `table` / `std::ranges` раздувает код приложения.

### Что понравилось

- Защита маршрута одной строкой: `{.path = "/tickets/*", .element = ..., .allow = loggedIn, .redirect = "/login"}`.
- `useForm` с проверкой и `.error` на полях: форма заявки — 30 строк, ошибки появляются в нужный момент.
- `testing::Screen` + `setLocalFetch`: тесты входа и формы без сервера и браузера, 5 тестов за секунды.
- WebSocket сквозь `reactx dev --proxy` без настройки; «live» во второй вкладке сразу.
- Эмодзи, кириллица и китайский в терминальном textarea — рамки на месте, курсор правильный.

### Приоритет на исправление (v0.22 / 1.0-rc)

F1, F2, F3 → F4, F5, F6 → G1, G2, G3, G4, G5 (документация) → G6.

**Статус (2026-11-02, после отчёта #10):** исправлено в `[Unreleased]` — F1 (`public/` копируется отдельным шагом,
который зависит от его файлов и от сборки программы; e2e-проверка «правка CSS доходит до dist»), F2 (терминальный
проп `arrowKeys`: ↑ ↓ идут в `onKey` элемента; `ui::radioGroup` — одна остановка Tab, стрелки во все стороны по кругу,
фокус следует за выбором), F3 (`ui::InputType` и `InputProps::type`: точки в терминале, `type="password"` в браузере,
плюс Email/Number/Search/Tel/Url), F4 (`SliderProps::error`), F5 (шаблон: `body { background: Canvas; color:
CanvasText; }` — axe в тёмной теме: 4 нарушения → 0), F6 (у `ui::progress` сжимается только полоса), G1 (клик по
подписи `input` / `textarea` ставит фокус), G2 (`router::redirectedFrom()`), G3 и G5 (llms.txt), G4 (`white-space:
pre-line` у `ui::text`). Проверено на helpdesk и journal с локальным ReactX (браузер и pty). `.wasm`: счётчик без
изменений (97 022 Б), сайт +2,9 КБ (+1 КБ gzip). Осталось: G6 (размер helpdesk).

---

## Сводный отчёт #11 — Claude (claude-opus-5-5), 2026-11-03, ReactX v1.0.0

Первый раунд после 1.0: CLI v1.0.0 из релиза, проекты в `~/Desktop/reactx-trials/`, только `llms.txt` + `AGENTS.md`
проекта; исходники ReactX — только после того, как проблема воспроизведена. Браузер — безголовый Brave через CDP
(+ axe-core), терминал — pty + pyte, тесты — `reactx test`. Цель: то, что раньше почти не проверялось.

Проекты:
1. **habits** (веб + терминал из одного исходника, `--native`) — трекер привычек на неделю: `useReducer` с
   `std::variant` действий, состояние и `dispatch` через `createContext`, настройки в `createStore` + `useStore` с
   селектором, `memo`-строки с `useCallback`-обработчиками своих сигнатур, перетаскивание строк указателем (pointer
   events, в браузере и мышью в терминале), фильтр `?tag=` (`useQuery` + ссылки), график недели — SVG в браузере и
   `ui::progress` в терминале, страница привычки `/habit/:id` под `errorBoundary`, `ui::confirm`, `ui::toast`,
   `useTitle`, `ui::themeProvider`, сохранение в `localStorage` (JSON через `require`), prerender `/` и `/stats`.
   7 тестов на `testing::Screen`.
2. **focus** (терминал) — помодоро: `useInterval` с выключением через `std::nullopt`, крупные цифры по
   `useWindowSize`, `ui::tabs` (таймер / задачи / журнал), `ui::list .scroll`, `ui::table`, `ui::modal` со `slider`,
   `ui::confirm` на выход, `useKeyboard`, задачи с эмодзи / кириллицей / китайским в `~/.focus.json`. 4 теста.

Итог: оба приложения собрались почти с первого раза; habits прошёл 7 тестов с первой попытки. Работают перетаскивание
в обоих бэкендах, контекст через `memo`-границу, `useReducer` с вариантом, SVG, `errorBoundary` с `retry`, prerender,
изменение размера терминала, таймеры (тики в пределах интервала 250 мс). axe: 0 нарушений на всех страницах habits,
светлая и тёмная тема. Трижды «баг» оказывался моим: `click("Task")` попадал во вкладку «Tasks», `click("Quit")` — в
заголовок «Quit?» (это описано в llms.txt), «таймер отстаёт» — мой замер был короче интервала.

### Баги

**F1. [P1][bug] Терминал: контейнер с прокруткой в нерастянутом `box` или во вкладке `ui::tabs` не показывает строк.**
`ui::tabs({.tabs = {{.content = {ui::list({.children = items, .scroll = true})}}}})` — видна только рамка, строк нет;
то же `h("box", {}, {ui::list(...)})` и голый `overflow: "scroll"`. Добавленная задача «пропадала», и следующий ввод
уходил в невидимый список. llms.txt: «takes the rows that are left in a column (no height needed)». Причина:
scroll-контейнер получал `flexBasis 0; flexGrow 1` (`src/terminal_renderer.cpp`, `applyStyleTo`), а Yoga считает
высоту родителя «по содержимому» без него — 0 (браузерный CSS тут учёл бы содержимое).

**F2. [P2][bug] `router::link` с query активна при любом query.** На `/?tag=fun` класс `active` у «All», «Health»,
«Work» и «Fun» сразу: `Link` сравнивает только путь (`withoutQuery`). Фильтры-ссылки не показывают, какой выбран.

**F3. [P2][bug] У заранее отрисованных страниц один `<title>`.** `index.html` и `stats.html` — `<title>habits</title>`,
хотя страницы вызывают `useTitle`: при prerender эффекты не выполняются. Вкладки и поисковики видят одно имя.

**F4. [P2][bug] Prerender печатает предупреждение Node.** Приложение читает сохранённое (`storageRead`) при первой
отрисовке → при каждой `reactx build` / `dev`: «ExperimentalWarning: localStorage is not available because
--localstorage-file was not provided». Node 22+ сам определяет `localStorage` и предупреждает при обращении.

**F5. [P2][bug] `useWindowSize()` в `testing::Screen(app, 40, 12)` — всегда 80x24**, и `s.resize()` его не меняет:
натив читает `ioctl(STDOUT)`, а `Screen` свой размер никому не сообщает. Тест «маленький экран → без крупных цифр»
написать нельзя.

**F6. [P2][bug] `rx::ui` в браузере: поля с `.labelAbove` в строке `ui::stack` на телефоне налезают друг на друга.**
`input` (226 px) шире своей подписи (182 px) и уходит под `select`; подпись `select` на 3 px ниже подписи `input`.
Причины в CSS: `.rx-field` с `flex-wrap: wrap` в колонке растягивает поле до ширины строки flex, а не до ширины
подписи; `select` в Chrome ниже `input` (`line-height: normal`), а `flex: 1` в колонке перебивает высоту.

### Пробелы и неудобства

**G1. [P2][gap] `ui::tabs` только неуправляемые** (`.initial`): нельзя переключить вкладку из кода или горячей клавишей
(«1/2/3»), нельзя узнать открытую. llms.txt не называл поля: `.content` — список элементов, а не элемент; ошибка
компиляции про `TabsProps` длинная. Предложение для 1.1: `.active` + `.onChange(int)` после `.key`.

**G2. [P2][gap] `errorBoundary` на странице маршрута «залипает».** `/habit/zzz` → fallback; «Назад» на рабочий
`/habit/h2` — всё ещё «Not found». Так и в React (граница держит ошибку до `retry()`), но в роутере это ловушка;
решение уже есть — ключ: `errorBoundary(fb, {h(Page)}, router::useLocation())`. Нужно сказать об этом в документации.

**G3. [P2][gap] Терминал: в сжатой строке короткий текст исчезает.** На ширине 50 у строки привычки пропал значок «⠿»
(1 клетка → 0): Yoga сжимает текст до нуля, в браузере `min-width: auto` этого не даёт.

**G4. [P2][size] Код приложения — главная часть `.wasm`.** habits: 275 КБ (118 КБ gzip), `app.cpp` — 32 %, около
100 КБ кода на ~400 строк (`Week` 11,7 КБ, `App` 8 КБ; 141 копия кода `std::string` и 158 — `std::vector`). Каждый проп
`{"className", x}` разворачивается на месте вызова (строка + `PropValue` + элемент вектора). То же, что G6 отчёта #10.

### Что понравилось

- Перетаскивание по рецепту из llms.txt (`onPointerDown` / `onMouseEnter` / `onPointerUp`) заработало в браузере и
  мышью в терминале одним кодом.
- `memo` + `useCallback` с сигнатурой `(std::string, int)` — строки не перерисовываются при чужих изменениях.
- `useInterval(fn, running ? std::optional(250ms) : std::nullopt)` — таймер в одну строку.
- `testing::Screen` + `settle()` проверяют обратный отсчёт и смену режима без терминала.
- SVG (`svg:text`, `rect`) и prerender без единой правки кода.

### Приоритет на исправление (v1.0.1)

F1 → F2, F3, F4, F5, F6 → G2 (документация), G1 (поля в llms.txt; управляемые вкладки — 1.1) → G3, G4.

**Статус (2026-11-03, после отчёта #11):** исправлено в `[Unreleased]` — F1 (scroll-контейнер в колонке — по высоте
своих строк, а если не помещаются — оставшееся место; его предки по пути уступают место: `holdsScroll` в
`Layout::sync`; тест: список в `box`, во вкладке и длинный список под заголовком), F2 (ссылка с query активна, только
пока в query те же значения; ссылка без query — как раньше), F3 (`useTitle` сообщает заголовок при prerender,
`prerender.cpp` пишет его в `<title>`; шаблон: About с `useTitle`; e2e-проверка), F4 (вне браузера `storageRead` —
пусто, `localStorage` Node не трогается), F5 (`notifyResize` из `Screen` и `TerminalApp`; `useWindowSize` берёт
сообщённый размер), F6 (`.rx-field--above` без переноса, поля в колонке не тянутся, `select` той же высоты, что
`input`), G2 и G1 — в документации (README, сайт, llms.txt). Остались: G1 (управляемые `ui::tabs` — 1.1), G3, G4.

## Сводный отчёт #12 — Claude (claude-opus-5-5), 2026-11-07, ReactX v1.5.0

Источник: пробное приложение OpsDeck (`~/Desktop/unideck`, написано ChatGPT): инциденты, таблица с сортировкой,
форма, графики `reactx::charts`, виртуальный список на 240+ событий, вкладки, `useLocalStorage`. Проверено в
терминале (`testing::Screen` и настоящий pty) и в браузере (headless Brave, 1280 и 390 px). Ошибки самого приложения
(13 штук: сдвинутые колонки сортировки, форма без `reset`, выбор скрытого фильтром инцидента и др.) исправлены в
проекте и описаны в его `AI_REPORT.md`; неверного использования API нет. Ниже — только ReactX.

**F1. [P1][charts] Терминальный график шире своего блока.** `bar` / `pie` без `.width` брали «ширину окна − 4»; в
карточке внутри карточки значение справа обрезалось рамкой («4 incide»). Воспроизведение: `bar` с `.unit = " incidents"`
в двух `box` с рамкой и `paddingX: 1`, экран 80 — `shows("4 incidents")` ложно.

**F2. [P2][ui] Заголовок `ui::card` обрезается.** В ряду карточка шириной в содержимое: «Resolved» над «3» → «Resolve».

**F3. [P0][регрессия, не выпущена] CI на main красный с 9a861ae и afa951c.** (а) Фокус: позиция для соседа бралась
в момент удаления, а узел, вставленный в том же коммите (чекбокс заменяется, когда пропадает его ошибка), сдвигал
её — Tab перескакивал поле; радиогруппа, переносящая свою единственную остановку Tab, брала устаревшую позицию.
Тест `form_shows_errors_after_submit_and_sends_valid_values`. (б) Короткий вывод `reactx build` скрыл
«reactx: prerendered /about -> about.html»; e2e-тест ждал «Configuring done» в выводе.

Замечание (не ошибка): `useState(x)` вычисляет `x` на каждом рендере и возвращает копию состояния — в OpsDeck это
укладывается в 1,2 мс полного рендера, но ленивый `useState([]{ ... })` пригодился бы.

**Статус (2026-11-07, 64ccbbc):** F1 — `bar` и `pie` без `.width` растягиваются по флексу (ряд █ шириной в процент
блока), с `.width` как раньше; `line` по-прежнему по окну (документировано). F2 — невидимая строка шириной в заголовок с
`maxWidth: 100%`. F3 — позиция фокуса берётся до того, как узел перестанет принимать фокус, без узлов, ставших
фокусируемыми в этом коммите; строки «reactx: prerendered» показываются в коротком выводе; e2e — с `--verbose`.
Тесты: `bars_fit_a_box_in_a_box`, `pie_fills_its_box`, `card_is_at_least_as_wide_as_its_title`. Остался ленивый `useState`.
