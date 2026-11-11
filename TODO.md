# ReactX — Personal Roadmap & Research Notes

Личные заметки по развитию проекта. Не публичный roadmap — это рабочий список.

---

## 🎯 Путь к 1.0 (план от 2026-11-01)

- [x] **v0.15 — хвосты:** `ui::table`, `useTitle`, повторный `mountDom` убирает старое дерево, `reactx build` показывает, что раздувает `.wasm`, нативная сборка web-проекта, CSS шаблона сжимает строки `ui::stack`
- [x] **v0.16 — формы:** `useForm` + валидация, `textarea`, `radio`, `slider`, `progress`
- [x] **v0.17 — текст в терминале:** ширина эмодзи/CJK, курсор в поле, `textarea`
- [x] **v0.18 — `reactx test`**
- [x] **v0.19 — роутер:** вложенные маршруты, guards, 404
- [x] **v0.20 — аудит API:** deprecated вместо переименований; рефлексия остаётся опциональной
- [x] **v0.21 — финал:** полная документация, Lighthouse a11y 100, ревью безопасности сервера, заморозка API
- [x] **v1.0.0** (без -rc, решение мейнтейнера 2026-11-02)

**1.0 готова, когда:** список закрыт; два раунда пробных приложений подряд без багов; новичок делает приложение по llms.txt без вопросов; CI зелёный.
**После 1.0** (без ломки API): i18n, signals, графики, инспектор, SSE, гидрация, формы через рефлексию.

---

## 🔥 Приоритет 1 — Производительность и UX первой загрузки

Комментарий от пользователя: *«Good job but it does not feel native»* — скорее всего имеется в виду ощущение задержки при первой загрузке `.wasm`.

### Задачи:
- [x] **`<link rel="preload">` для `index.wasm`** в `web/site_shell.html` (сделано)
  ```html
  <link rel="preload" href="index.wasm" as="fetch" type="application/wasm" crossorigin>
  ```
  Цель: срезать LCP с 2.6s до ~2.0s.

- [ ] **Skeleton screen / loading placeholder** (главная и docs уже пререндерятся в HTML; заглушка нужна остальным страницам вместо «loading ReactX») — пока `.wasm` грузится, показывать заглушку с контуром страницы, а не пустой экран или просто «Loading…». Цель: FCP остаётся быстрым, пользователь видит структуру сразу.

- [ ] **Service Worker для кеширования `.wasm`** — при повторном заходе страница открывается мгновенно (< 0.5s). `.wasm` скачивается один раз.

- [ ] **Brotli сжатие** — GitHub Pages поддерживает. `.wasm` с brotli сжимается на 20-30% лучше gzip. Проверить что CI выдаёт правильные заголовки.

---

## 🏗 Приоритет 2 — Архитектурные исследования

### Изучить: Preact (https://github.com/preactjs/preact)

Пользователь справедливо заметил: *«Why not just use 3kb Preact?»*

Preact — это самый компактный и чистый React-совместимый фреймворк. Изучить его архитектуру чтобы найти идеи для ReactX:

- [ ] **Как Preact достигает 3 KB?**
  - Читать: `src/component.js`, `src/diff/index.js`
  - Минимальный reconciler без лишних абстракций
  - Никаких synthetic events — прямые DOM events
  - Вопрос для ReactX: можно ли срезать размер `.wasm` аналогично?

- [ ] **Алгоритм диффинга в Preact vs ReactX reconciler**
  - Preact: `diff()` в `src/diff/index.js` — простой keyed-алгоритм
  - ReactX: `src/reconciler.cpp` — похожий подход, но на C++
  - Что можно позаимствовать из оптимизаций Preact?

- [ ] **Signals (Preact Signals)** — `@preact/signals`
  - Реактивность без перерисовки всего дерева
  - Только затронутые компоненты обновляются
  - Идея для ReactX: аналог `createStore` / `useStore` с гранулярными подписками

- [ ] **Preact's `h()` и JSX-трансформация**
  - Как они обходятся без JSX-компилятора
  - Сравнить с ReactX DSL (`tag::div`) и `h()` — что более эргономично для C++?

---

## 📦 Приоритет 3 — Экосистема и библиотеки

Когда фреймворк стабилен — сюда добавить конкретные задачи по библиотекам.

### Базовые UI-атомы (Atomic Design по совету Aton B.):
> *«A component library of atoms to compose any UI pattern is more useful than having to build whole UIs.»*
- [x] **`Button`** (`ui::button`) (primary, secondary, outline, ghost, loading state, disabled)
- [x] **`Input` / `TextField`** (`ui::input`, `.error`, `.type`) (controlled/uncontrolled, error states, clearable, icons)
- [ ] **`Badge` / `Tag`** (статусные метки, варианты цветов)
- [x] **`Card`** (`ui::card`) (header, content, footer, border/elevation tokens)
- [x] **`Stack` / `Box`** (`ui::stack`) (типизированные Flexbox-примитивы: Row, Column, gap, wrap)
- [x] **`Dialog` / `Modal`** (`ui::modal`, `ui::confirm`) (оверлей, a11y, focus trap, Esc для закрытия)
- [ ] **`Dropdown` / `Menu`** (всплывающее меню, навигация стрелками; есть только `ui::select`)
- [x] **`Tooltip`** (`ui::tooltip`) (подсказки при наведении с автопозиционированием)

### Высокоуровневые компоненты (`rx::ui` расширение):
- [x] Виртуализированный список (`ui::useVirtualList`, v0.14; `ui::table` + `reactx::grid`)
- [ ] Богатые формы с валидацией через C++26 рефлексию (валидация есть: `ui::useForm`, `reactx::forms`; генерации формы из struct нет)
  - Идея: `rx::form<MyStruct>()` автоматически генерирует форму из struct-полей
- [x] Графики и чарты (`reactx::charts`, v1.2)
- [x] Toast / Snackbar уведомления (`ui::toast`, v0.10)

### Бэкенд (`rx::net` расширение):
- [x] WebSockets (`useWebSocket`, v0.13; `reactx::realtime`, v1.7)
- [ ] Server-Sent Events (SSE)
- [ ] JWT/Bearer авторизация middleware (есть `reactx::auth`: пароли, сессии, CSRF — без JWT)
- [x] ~~SQLite-обёртка~~ → встроенное хранилище `reactx::db` (JSON-документы, v1.7)
- [ ] gzip middleware для ответов

---

## 🏎 Приоритет 4 — Производительность рантайма

- [ ] **Бенчмарк: ReactX reconciler vs Preact diff** на одинаковом сценарии
  - 1000 элементов списка, перемешивание ключей
  - Цель: убедиться что C++ даёт реальное преимущество
- [ ] **Виртуализация DOM-дерева** — для очень глубоких деревьев
- [ ] **Мемоизация** — `memo` пока не сравнивает children (элементы с children всегда перерисовываются, см. `include/reactx/memo.hpp`)

---

## 🔮 Далёкое будущее (v1.0+)

- [ ] **Отказ от Yoga Flexbox** → собственный TUI Layout Engine
  - Только целые координаты (`int32_t`), без float-погрешностей в терминале
  - ~1000-1500 строк вместо внешней зависимости
  - Делать ТОЛЬКО когда проект уже популярен и есть реальные пользователи с жалобами на Yoga
  - **Не раньше v0.12–v1.0**

- [ ] **macOS arm64 бинарник** в GitHub Releases (runner `macos-14` в CI)
- [ ] **AUR-пакет** (`reactx-bin`) для пользователей Arch Linux
- [ ] **Homebrew tap** (`brew install sargisis/tap/reactx`)

---

## 📝 Фидбек из комментариев (LinkedIn / GitHub)

| Комментарий | Автор | Что делать |
|:---|:---|:---|
| *«think first set of atoms to compose any ui pattern would be elegant. A component libary is more useful than only having to build whole UIs»* | Aton B. (LinkedIn) | 🔥 **Atomic Design:** сначала базовые строительные кирпичики (`Button`, `Input`, `Card`, `Modal`), из которых разработчики соберут любой интерфейс |
| *«Previously I thought QT was the best option for implementing UI with C++ and now this»* | Bruce NKUNDABAGENZI (LinkedIn) | Использовать сравнение с Qt в постах (легковесность, DOM вместо Canvas, без MOC) |
| *«What if you need a package that is available in JS? A transpiler could be better addition»* | Lead Consultant (LinkedIn) | Документировать JS Interop (`emscripten::val`) для использования готовых npm-пакетов при необходимости |
| *«Good job but it does not feel native»* | LinkedIn | Улучшить first-load UX (preload, skeleton, SW) и фокусы/hover-анимации |
| *«Why not Preact?»* | LinkedIn | Изучить Preact архитектуру (см. выше), нишевые преимущества ReactX задокументированы |
| *«Tsoding vibes»* | LinkedIn | Позитив, ничего делать не надо 😄 |
| *«I will give it a test»* | Ahmed El-Azab (Sr. Backend) | Следить за фидбеком, ответить если появятся вопросы |
| *«because people tired from handyman UI which start as spaceship for "hello world"»* | Senior dev (защищал ReactX от Preact-критики) | 🔥 **Это готовый слоган для главной страницы!** Использовать в маркетинге |

### 💡 Идея для главной страницы (от сеньора):
> *«No Node. No npm. No 300MB of node_modules. Just C++ and a browser.»*

---

## ✅ Уже сделано

- [x] Полный фреймворк: reconciler, хуки, роутер, DOM, терминал
- [x] WebAssembly + нативный терминал из одного кода
- [x] Сайт документации (EN + RU) на GitHub Pages
- [x] CLI: `new`, `dev`, `build`, `test`, `add`, `remove`, `update`, `upgrade`, `install`, `--help`
- [x] HTTP/1.1 сервер + `reactx dev --proxy` для внешних бэкендов (Flask и т.д.)
- [x] Docker образ + devcontainer
- [x] Санитайзеры, фаззинг, статический анализ в CI
- [x] SECURITY.md (EN + RU)
- [x] Lighthouse: 96 Performance / 100 SEO / 100 Best Practices
- [x] 10k+ просмотров на LinkedIn за первые сутки 🚀
- [x] v1.0.0 (2026-11-02); 17 библиотек в `library/` (v1.2–v1.7)
- [x] JS interop (`reactx/js.hpp`, v0.9)

---

## 2.0: std::function lighter (measured 2026-11-07)

On the site `std::function` code is ~67 KB of the .wasm (~10%): typeinfo of every lambda 18.5 KB, the calls 12 KB, copy/move 9.7 KB, clone 7 KB, target()/target_type() 3.4 KB.
- [ ] `-fno-rtti` for web builds (~22 KB): blocked only by `std::function::target<T>()` in `src/element.cpp` (useCallback's StableFn, NodeRef) and `src/reconciler.cpp` (attaching NodeRef). Needs refs and stable handlers stored as their own `PropValue` alternatives — a change to a public type, so 2.0. dynamic_cast is already gone (v1.6).
- [ ] `std::copyable_function` / `std::move_only_function` for stored handlers once Emscripten's libc++ has them (GCC 16 does; libc++ in Emscripten 6 has neither, nor `function_ref`).
- `std::function_ref` doesn't fit: handlers are stored and called later; a non-owning ref would dangle.

