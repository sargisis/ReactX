# ReactX — Personal Roadmap & Research Notes

Личные заметки по развитию проекта. Не публичный roadmap — это рабочий список.

---

## 🔥 Приоритет 1 — Производительность и UX первой загрузки

Комментарий от пользователя: *«Good job but it does not feel native»* — скорее всего имеется в виду ощущение задержки при первой загрузке `.wasm`.

### Задачи:
- [ ] **`<link rel="preload">` для `index.wasm`** в `web/site_shell.html`
  ```html
  <link rel="preload" href="index.wasm" as="fetch" type="application/wasm" crossorigin>
  ```
  Цель: срезать LCP с 2.6s до ~2.0s.

- [ ] **Skeleton screen / loading placeholder** — пока `.wasm` грузится, показывать заглушку с контуром страницы, а не пустой экран или просто «Loading…». Цель: FCP остаётся быстрым, пользователь видит структуру сразу.

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
- [ ] **`Button`** (primary, secondary, outline, ghost, loading state, disabled)
- [ ] **`Input` / `TextField`** (controlled/uncontrolled, error states, clearable, icons)
- [ ] **`Badge` / `Tag`** (статусные метки, варианты цветов)
- [ ] **`Card`** (header, content, footer, border/elevation tokens)
- [ ] **`Stack` / `Box`** (типизированные Flexbox-примитивы: Row, Column, gap, wrap)
- [ ] **`Dialog` / `Modal`** (оверлей, a11y, focus trap, Esc для закрытия)
- [ ] **`Dropdown` / `Menu`** (всплывающее меню, навигация стрелками)
- [ ] **`Tooltip`** (подсказки при наведении с автопозиционированием)

### Высокоуровневые компоненты (`rx::ui` расширение):
- [ ] Виртуализированный список (для таблиц 10k+ строк без лагов)
- [ ] Богатые формы с валидацией через C++26 рефлексию
  - Идея: `rx::form<MyStruct>()` автоматически генерирует форму из struct-полей
- [ ] Графики и чарты (SVG-based, без внешних зависимостей)
- [ ] Toast / Snackbar уведомления

### Бэкенд (`rx::net` расширение):
- [ ] WebSockets и Server-Sent Events (SSE) — сейчас главный пробел
- [ ] JWT/Bearer авторизация middleware
- [ ] SQLite-обёртка (типизированные запросы через рефлексию)
- [ ] gzip middleware для ответов

---

## 🏎 Приоритет 4 — Производительность рантайма

- [ ] **Бенчмарк: ReactX reconciler vs Preact diff** на одинаковом сценарии
  - 1000 элементов списка, перемешивание ключей
  - Цель: убедиться что C++ даёт реальное преимущество
- [ ] **Виртуализация DOM-дерева** — для очень глубоких деревьев
- [ ] **Мемоизация** — `memo` пока не сравнивает children, исправить

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
- [x] CLI: `new`, `dev`, `build`, `add`, `remove`, `update`, `--help`
- [x] HTTP/1.1 сервер + `reactx dev --proxy` для внешних бэкендов (Flask и т.д.)
- [x] Docker образ + devcontainer
- [x] Санитайзеры, фаззинг, статический анализ в CI
- [x] SECURITY.md (EN + RU)
- [x] Lighthouse: 96 Performance / 100 SEO / 100 Best Practices
- [x] 10k+ просмотров на LinkedIn за первые сутки 🚀
