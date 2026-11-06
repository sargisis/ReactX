# Changelog

All notable changes to ReactX. Versions follow [Semantic Versioning](https://semver.org): `MAJOR.MINOR.PATCH`, git tags are `vMAJOR.MINOR.PATCH`. Since 1.0 incompatible changes bump `MAJOR`, new features `MINOR`, fixes `PATCH` (before 1.0 incompatible changes bumped `MINOR`).

На русском: [CHANGELOG.ru.md](CHANGELOG.ru.md).

## [Unreleased]

## [1.3.0] — 2026-11-06

Two more libraries that come with ReactX — server data with a cache and Markdown — and faster builds with ccache. Nothing changes for existing apps.

**Update:** `reactx upgrade`, then `reactx update` in each project; then `target_link_libraries(my_app PRIVATE reactx::query reactx::markdown)` for the ones you want.

### Added

- `reactx::markdown` in `library/markdown` (`rx/markdown.hpp`): `view({.text})` shows Markdown — semantic HTML in the browser (with a little overridable CSS), colors and indents in a terminal. CommonMark basics plus GitHub tables, task lists, strikethrough and bare links; raw HTML is shown as text and `javascript:` links are dropped, so text from users is safe. `.link` makes internal links router links; `parse()` gives the document (headings with ids, for a table of contents), `plainText()` strips the markup. Link `reactx::markdown`.
- `reactx::query` in `library/query` (`rx/query.hpp`): server data with a shared cache, like React Query / SWR. `useQuery({.key, .url})` gives `data`, `error`, `loading`, `fetching`, `refetch()`; components with the same key share one request; data is shown from the cache at once and refreshed in the background; retries with backoff, `staleTime`, `cacheTime`, `refetchInterval`, `enabled`, typed data with `.parse`, own loaders with `.load`. `useMutation` for POST/PUT/DELETE with `invalidates` and optimistic updates (`onMutate` returns the undo); `invalidate`, `setData`, `getData`, `updateData`, `remove`, `clear` work from anywhere. Link `reactx::query`.
- Faster builds with [ccache](https://ccache.dev): when it is installed, ReactX and apps made with `reactx_add_app` compile through it, so building the same code again (a clean `build/`, a new project) takes seconds — a clean build of a new web project 13 → 6 s, of ReactX itself 37 → 15 s. `-DREACTX_CCACHE=OFF` turns it off; a launcher you set yourself (`CMAKE_CXX_COMPILER_LAUNCHER`) wins. ReactX's own code is no longer scanned for C++ modules (ccache can't cache a scanned compile); `import reactx;` works as before.

## [1.2.0] — 2026-11-06

Libraries that come with ReactX: charts and translations, linked with one line in CMakeLists.txt. Nothing changes for existing apps.

**Update:** `reactx upgrade`, then `reactx update` in each project; then `target_link_libraries(my_app PRIVATE reactx::charts reactx::i18n)` for the ones you want.

### Added

- Libraries that come with ReactX, in `library/`: link them with `target_link_libraries(my_app PRIVATE reactx::charts reactx::i18n)`, no download. Only linked ones are compiled, and an unused one adds nothing to a `.wasm`. They are a preview: their API may still change in a MINOR release.
  - `reactx::charts` (`rx/charts.hpp`): bar, line and pie charts — SVG in the browser, block and braille characters in a terminal. It moved here from [rx-charts](https://github.com/sargisis/rx-charts) 0.1.0 (same code; the old repository keeps working).
  - `reactx::i18n` (`rx/i18n.hpp`): translations with `{name}`, plurals (CLDR rules) and `select`; `useTranslation()`, `setLanguage()` (remembered, `<html lang>` follows it), `languagePicker()`; numbers, percents, money, dates, times and "5 minutes ago" in en, ru, uk, hy, de, fr, es, it, pt, pl; messages from JSON files (`loadMessages`).

## [1.1.0] — 2026-11-03

Tabs that code can switch, and a smaller `.wasm` for those who want it. Nothing changes for existing apps.

**Update:** `reactx upgrade`, then `reactx update` in each project.

### Added

- The CMake option `REACTX_WEB_LTO` (off by default): link-time optimization for Release web builds, a ~5% smaller `.wasm` (gzip) at the cost of slower links — `set(REACTX_WEB_LTO ON)` above `FetchContent_MakeAvailable(reactx)`. The website uses it: its `.wasm` is 6.5% smaller (181.9 → 170.1 KB gzip).
- `ui::tabs` can be controlled: `.active` (the open tab, from your state) and `.onChange(int)` (the tab a click or Enter picked), so a shortcut or code can switch tabs. Without them the tabs keep the open one themselves, as before.

## [1.0.1] — 2026-11-03

Fixes from the first trial round after 1.0: a scroll list inside a terminal tab, filter links with a query, prerendered page titles, `useWindowSize` in tests and form rows on phones. No API changes.

**Update:** `reactx upgrade`, then `reactx update` in each project.

### Changed

- New web projects: the About page sets its tab title with `useTitle("About")`.

### Fixed

- Terminal: a scroll box (`overflow: "scroll"`, `ui::list({.scroll = true})`) in a box that isn't stretched, or in a `ui::tabs` tab, showed no rows, only its border. Now it is as tall as its rows, or takes the rows that are left when they don't fit.
- `router::link` with a query (`"/?tag=work"`) was active on every query of that path, so all filter links were marked; now only while the page's query has those values. A link without a query ignores the query, as before.
- Prerendered pages had the page's default `<title>`; now they get the one `useTitle` sets.
- Prerendering an app that reads `localStorage` (`useLocalStorage`, `storageRead`) printed Node's "localStorage is not available" warning on every build; there it now reads as empty.
- `useWindowSize()` in `testing::Screen` was always 80x24; now it is the screen's size, and `s.resize(w, h)` changes it.
- `rx::ui` in the browser: in a `ui::stack` row, an input with `.labelAbove` ran under the field next to it on a phone, and a `select`'s label sat 3px lower than an input's.
- Docs: an `errorBoundary` on a route page keeps showing its fallback on another address of that route; key it by the path (`errorBoundary(fallback, {h(Page)}, router::useLocation())`). `ui::tabs` fields (`.content` is a list, `.initial`) in llms.txt.

## [1.0.0] — 2026-11-02

ReactX 1.0: the API is stable. Everything in `include/reactx/` outside `detail`, the `reactx` commands, `reactx.json` and the CMake functions now change only in a MAJOR release — nothing is renamed or removed before 2.0 (README "API stability"; the snapshot `api/reactx.api` and the test `reactx_api_check` guard it). This release also brings the fixes of the last trial round: password fields, the arrows in a terminal radio group, `router::redirectedFrom()` for login pages, `.error` on `ui::slider`, and a CSS edit reaching `dist` without a C++ change.

**Update:** `reactx upgrade`, then `reactx update` in each project. Apps written for 0.21 build unchanged.

### Added

- `ui::input` `.type`: `ui::InputType::Password` shows dots in a terminal and is a password field in the browser; `Email`, `Number`, `Search`, `Tel` and `Url` pick the phone's keyboard.
- `ui::slider` `.error`, like every other form field.
- `router::redirectedFrom()`: the path (with its query) a guard's `.redirect` turned the user away from, so a login page can go back there; `""` again after any other navigation.
- The terminal prop `arrowKeys=true`: while the node is focused, Up/Down go to its `onKey` instead of moving the focus.
- Docs: how to build your own server next to a web app (a native CMake target), and which place `testing::Screen::click` picks.

### Changed

- `ui::radioGroup` in a terminal is one Tab stop, like in the browser: the checked option takes the focus and the arrows (Up/Down/Left/Right) pick the next or previous one, going around at the ends.
- A click on a `ui::input` / `ui::textarea` label in a terminal focuses the field, like `<label>` in the browser.
- `ui::text` keeps the line breaks of its text in the browser (as in a terminal).

### Fixed

- `reactx build` / `reactx dev` copy a changed file of `public/` (a CSS edit) into `dist` even when no C++ changed; before, `dist` kept the old file until the program was linked again.
- The arrows did nothing in a terminal `ui::radioGroup` (documented as "Up / Down"): the terminal took Up/Down for moving the focus first.
- New projects: the page has its own background and text color (`Canvas` / `CanvasText`), so contrast checkers (Lighthouse, axe) see the dark theme's background — the `rx::ui` labels failed color-contrast in dark mode before.
- `ui::progress` in a narrow terminal row cut off the label or the percentage; now the bar gets shorter.

## [0.21.0] — 2026-11-02

The last release before 1.0: a security review of `HttpServer`, an accessible website and `rx::ui` (axe-core: 0 violations, light and dark, desktop and phone), complete docs in both languages and a frozen public API (`api/reactx.api`). A MINOR release because `HttpServer` now refuses unknown `Host` names and cross-site `Origin`s.

**Update:** `reactx upgrade`, then `reactx update` in each project. A server reached by a domain name (not `localhost` or an IP) needs `server.allowHosts({"example.com"})`, or `--allow-host` for `reactx dev` / `reactx_serve`.

### Changed

- `HttpServer` (and `reactx dev`, `reactx_serve`) answers only to `localhost`, `*.localhost` and IP addresses; other `Host` names get 403, so a site that points its DNS name at `127.0.0.1` can't read your dev server (DNS rebinding). Add names with `server.allowHosts({...})` / `reactx dev --allow-host NAME`; `reactx dev` allows the GitHub Codespaces domain itself.
- WebSocket upgrades and requests other than GET/HEAD/OPTIONS whose `Origin` is another site get 403 (CSRF, cross-site WebSocket hijacking). Pages on `localhost` at any port still work.
- Dot files (`/.env`, `/.git/...`) aren't served from `serveDirectory`; `/.well-known/` still is.
- Accessibility (axe-core, the engine of Lighthouse's audit: 0 violations on the website, the demos and a new project, light and dark, desktop and phone): `rx::ui` text colors keep 4.5:1 contrast in both themes — each terminal color name of a `ui::Theme` has a light and a dark shade (`--rx-c-*` variables), the dark theme has its own accent, danger and success colors, the light success color is darker. `ui::themeProvider` only writes the colors an app changed, so the defaults follow the page's dark mode.
- The page shells of new projects and demos mount the app into `<main id="app">` (the page's main landmark); prerendering finds both `<main id="app">` and the old `<div id="app">`.

### Added

- API freeze before 1.0: `api/reactx.api` is a snapshot of the public headers, and the test `reactx_api_check` fails when they change without it.
- Docs: `router::useParams`, `currentPath`, `href`, `reset`, `link .activeClassName`, `HttpServer::post/put/patch/del`, `setNotFoundPage`, `setLogging`, `ui::slider .step/.showValue`, `ui::useTheme` are described; the English and Russian docs pages have the same sections.
- `ui::CardProps::level`: the title's heading level in the browser (`<h3>` by default; 2 for cards right under the page's `<h1>`, so no heading level is skipped).
- `HttpServer::allowHosts`, `setRequestTimeout` (30 s by default) and `setMaxConnections` (512 by default); `--allow-host` in `reactx dev` and `reactx_serve`.

### Fixed

- Security review of `HttpServer`: a client sending a byte every few seconds held a thread forever (now 408 after the request timeout); every connection got a thread with no limit (now 503 over the limit); a client that stopped reading blocked a response or a WebSocket broadcast forever (now dropped after 10 s); a body shorter than its `Content-Length` reached the handler (now 400).
- Request heads with control characters (a bare LF in the target or a header could add a header when proxied), a space before a header's colon, folded headers, two different `Host` or `Content-Length` values, `Transfer-Encoding` (501) or over 100 headers (431) are refused.
- A symlink inside the served folder could lead outside it through `/x` → `x.html` or `index.html` (now 403).
- Website: text contrast (light and dark), underlined links in the footer and breadcrumbs, scrollable code blocks reachable by keyboard; the kanban demo has a heading.
- CR and LF in response header values are dropped (response splitting), and request paths are printed in the log with control characters escaped (an ANSI escape in a URL reached the terminal).

## [0.20.0] — 2026-11-02

The API audit before 1.0: every form field takes `.disabled`, `ui::list` takes `.children` like every other component, `ui::radioGroup` lays out with `.direction` like `ui::stack`, and the docs say what the public API is and how it will change: never by renaming in place — old names keep working until 2.0. A MINOR release for the new fields; apps update without changes.

**Update:** `reactx upgrade`, then `reactx update` in each project.

### Added

- `.disabled` on `ui::input`, `textarea`, `select`, `checkbox`, `radioGroup` and `slider` (like `ui::button`): shown, not editable, skipped by Tab — a native `disabled` in the browser, dim and unfocusable in a terminal.
- `ui::ListProps::children` (the items, like every other component) and `ui::RadioGroupProps::direction` (like `ui::stack`).
- Docs: an "API stability" section — what the public API is, and that nothing is renamed in place.

### Deprecated

- `ui::ListProps::items`: use `.children`. `ui::RadioGroupProps::row`: use `.direction = ui::Direction::Row`. Both keep working until 2.0 (no compiler warning: GCC would warn on every use of the struct).

## [0.19.0] — 2026-11-02

The router grows up: nested routes (`"/settings/*"` with its own `routes()` inside, the layout stays mounted), guards (`.allow` / `.redirect`), `router::redirect`, and links that stay active under their path. A MINOR release for the new API; apps update without changes.

**Update:** `reactx upgrade`, then `reactx update` in each project.

### Added

- Nested routes: a route ending in `/*` matches its path and everything under it, and the `router::routes()` inside its element match the rest; the outer element stays mounted (a layout) and its route's parameters are seen inside.
- Route guards: `Route` has `.allow` and `.redirect` — a route that isn't allowed is skipped, or sends to `.redirect` (replacing the history entry).
- `router::redirect(to)`: goes on as soon as it is shown, like React Router's `<Navigate>`.
- `router::link`: `.exact = false` (active on the paths under `to` too) and `.replace = true`.

### Fixed

- Website: the "docs" link in the header stays active on every docs page.

## [0.18.0] — 2026-11-02

Tests for your app with one command: `reactx test` builds `tests/*.cpp` and runs them. `reactx/testing.hpp` draws the app on an in-memory terminal and acts like a user — `click("Save")`, `type("Ann")`, `press("Ctrl+s")`, then `CHECK(s.shows("Saved"))`. New projects keep their components in `src/app.cpp` and come with a test. A MINOR release: new API, and new projects are laid out differently; existing apps update without changes (to test one, move its components out of `main.cpp`).

**Update:** `reactx upgrade`, then `reactx update` in each project.

### Added

- `reactx test [name]`: builds the project's `tests/*.cpp` natively (with the app's sources except `main.cpp`; `reactx_add_app` makes `<app>_tests` of them) and runs them; the exit code says if they passed, a name runs some of them.
- `reactx/testing.hpp` (`reactx::testing`): `rx::testing::Screen` draws an app on an in-memory terminal and acts like a user — `click("Save")`, `type("Ann")`, `press("Ctrl+s")`, `shows("Saved")`, `settle()` for timers, `render()`, `resize()`; `RX_TEST` / `CHECK` and a `main` for test programs.

### Changed

- `reactx new` puts the components into `src/app.cpp` + `app.hpp` (`main.cpp` only starts `App`) and an example test into `tests/app_tests.cpp`; the project's AGENTS.md explains `reactx test`.

## [0.17.0] — 2026-11-02

Text in the terminal: emoji and Chinese, Japanese and Korean characters take their real two columns, so borders and tables stay straight; `input` and `textarea` edit where the cursor is (arrows, Home/End, Ctrl+arrows by words, Delete); and `ui::textarea` has several lines in a terminal too. Also from an outside review of the website: `install.sh` checks the release's SHA-256, outdated limits in the docs are fixed, `/demos` opens directly. A MINOR release for the new terminal tag; apps update without changes.

**Update:** `reactx upgrade`, then `reactx update` in each project.

### Added

- Terminal: a `textarea` tag (`value`, `placeholder`, `rows`) and `ui::textarea` with several lines — Enter starts a line, Up / Down move between lines (past the first or last one the focus moves on), long lines wrap and the field scrolls to the cursor.
- Terminal: `input` and `textarea` edit where the cursor is — Left / Right, Home / End, Ctrl+Left / Right by words, Backspace / Delete at the cursor; a long value scrolls sideways to keep it in view. The cursor is an inverted cell (`TerminalRenderer::caret()` says where).

### Fixed

- Terminal: emoji and Chinese, Japanese and Korean characters take two columns, so borders, titles, table columns and wrapping no longer slip; a character with its marks (é, 👍🏽, ❤️, flags, family emoji) is one for drawing, the cursor and Backspace. Typing an emoji made of several code points works.

### Changed

- `install.sh` checks the downloaded archive against the SHA-256 the release publishes next to it (as `reactx upgrade` already did) and installs nothing if they differ.

### Fixed

- Docs: «Current limitations» of the backend page listed request headers, response headers, timeouts, cancelling and WebSockets as missing — they all exist; the macOS install built with `g++-14` instead of GCC 16. `/demos` on the website answered 404 to direct visits (now prerendered like the docs).

## [0.16.0] — 2026-11-01

Forms: `ui::useForm` keeps a form's values in your own struct, checks them with your function and shows each error under its field once that field changed or a submit was tried. New fields: `ui::textarea`, `ui::radioGroup`, `ui::slider`, `ui::progress`, and `.error` on every field. All of them work in the browser and in a terminal (a terminal textarea is one line until v0.17). Try them on the website's home page. A MINOR release for the new API; apps update without changes.

**Update:** `reactx upgrade`, then `reactx update` in each project.

### Added

- `ui::useForm<T>`: a form's values in your own struct, checked by your `validate` (returns `ui::FormErrors`, field name -> message); `form.set("name", &T::name)` is a field's onChange, `form.error("name")` its message (shown once the field changed or after a submit was tried), `submit` calls `onSubmit` only when valid, plus `reset`, `setValues`, `valid()`.
- `.error` on `ui::input`, `select`, `checkbox` and the new fields: the message under the field, which is marked invalid (`aria-invalid`, red).
- `ui::textarea` (several lines; one line in a terminal for now), `ui::radioGroup` (every option visible; Up / Down in a terminal), `ui::slider` (an `int` in a range; Left / Right / Home / End in a terminal), `ui::progress` (a bar with the percentage).

## [0.15.0] — 2026-11-01

Tables: `ui::table` lines up columns in the browser and in a terminal, with sortable headers, selectable rows and `useVirtualList` for long tables. `useTitle` names the browser tab. `reactx build` says which parts make the `.wasm` big. A second `mountDom` into the same container now stops the old app, and a native build of a web project builds your tests without tripping over `mountDom`. The first release of the road to 1.0. A MINOR release for the new API; apps update without changes.

**Update:** `reactx upgrade`, then `reactx update` in each project (v0.13 and older: `curl -fsSL https://sargisis.github.io/ReactX/install.sh | sh` once).

### Added

- `ui::table`: columns that line up in the browser (CSS grid, `role="table"`, `aria-sort`) and in a terminal (fixed cells), sortable headers (`onSort`; you sort the rows), rows with `onSelect` (click, Enter) and `selected`, `emptyText`, and `.list` for long tables with `ui::useVirtualList`.
- `rx::useTitle(text)`: the browser tab's title while the component is mounted; the previous one comes back on unmount.
- `reactx build` names the biggest parts of the `.wasm` (`biggest parts: src/app.cpp 23%, ReactX reconciler.cpp 15%, ...`), from the linker map.

### Fixed

- A native build of a web project (`cmake -S . -B build/test`, for tests or a helper server) no longer stops on the app's `main()`, which calls `mountDom`: `reactx_add_app` leaves a web app out of a native build unless it is named (`--target`, `reactx build --native`).
- New web projects: `.page` in `style.css` no longer shrinks rows, tables and form fields to their content; only buttons keep their own width.
- `rx::mountDom` into a container that already holds an app unmounts that app first: its effect cleanups run, so its timers and sockets stop (they used to keep running behind the new app).

## [0.14.0] — 2026-11-01

Long lists and live logs without the CPU bill: `ui::useVirtualList` draws only the rows on screen in the browser and in the terminal (a 10,000-row log viewer went from 66% CPU to 2.6%), and the terminal keeps its layout between repaints. Code that didn't compile now does: `useState` of a `std::vector` of your structs, `useCallback` with any signature. Forms line up with `.labelAbove`, `ui::input` fills a row. And `reactx upgrade` gets the newest reactx; from now on reactx tells you once a day when a new release is out. Found by writing a live poll, a 2,000-row inventory table and a log viewer on ReactX. A MINOR release because `Renderer` got a virtual (`insertBefore`, with a default) and `ui::Checkbox`/`InputProps`/`SelectProps` got fields; apps update without changes.

**Update:** `curl -fsSL https://sargisis.github.io/ReactX/install.sh | sh` (this once; from now on `reactx upgrade`), then `reactx update` in each project.

### Added

- `useCallback` takes any handler: `PointerHandler`, and lambdas of their own signature, e.g. `std::function<void(int)> onOpen = useCallback([setOpen](int id) { setOpen(id); }, deps());` for the `onOpen(id)` of a memo row. Before, only `void()`, key, text and scroll handlers compiled.
- `ui::checkbox({.onChange = [setOn](bool on) { ... }})`: the new state, next to `onToggle`.
- `FetchResponse::errorText()` ("network error: ..." / "HTTP 422"), like `FetchState::errorText()`, for `fetch` callbacks.
- `rx::EqualityComparable<T>`: `==` exists and compiles, also inside `std::vector`, `std::map`, `std::optional`, `std::pair`, `std::tuple`, `std::variant`.
- **`ui::useVirtualList`**: lists of any length draw only the rows on screen, in the browser and in the terminal; `.follow` keeps the end in view while rows come (logs, chats). The trial log viewer with 10,000 rows: 66% CPU → 2.6%; 100,000 rows in the browser keep ~40 in the DOM, a jump to row 50,000 takes 20 ms.
- **`reactx upgrade`**: replaces the reactx tool with the newest release from GitHub Releases (checks the SHA-256), then says if the compiler needs an update. Once a day `reactx new` / `dev` / `build` / `update` check for a new release in the background and say how to get it; `REACTX_NO_UPDATE_CHECK=1` (or `CI`) turns that off.
- `ui::input` / `ui::select` `.labelAbove`: the label on its own line, so the fields of a form line up.
- Terminal box prop `followEnd`: an `overflow: "scroll"` box at its end stays there when rows are added, in the same repaint.

### Changed

- Terminal: the layout is kept between repaints and only what changed is laid out again; rows scrolled out of an `overflow: "scroll"` box aren't drawn. A log viewer with 10,000 rows: a repaint with nothing changed 124 ms → 0.3 ms, adding 20 rows 143 ms → 35 ms; CPU of the trial app 66% → 32%, and its 100 ms timer keeps up again.
- `ui::input` in a `ui::stack` row takes the free room (a search box next to a button), in the browser and in the terminal; in a column it stays as it was.
- Browser: moving keyed rows uses `insertBefore` next to their new neighbour (`Renderer::insertBefore`), without `removeChild` and a search by index: sorting 2,000 table rows is as fast as plain JavaScript.

### Fixed

- `useState` (and stores, context values, `memo` props) of a type whose `==` doesn't compile, such as `std::vector<MyStruct>` without `operator==`, was a compile error deep in the standard library; it now re-renders on every set. `deps(...)` with such a value says what is missing.

## [0.13.0] — 2026-11-01

Live data without polling: `useWebSocket` keeps a connection open in the browser and in the terminal, with reconnects after a drop, and `HttpServer` serves WebSockets itself (`server.webSocket(path, ...)`, broadcast with `server.webSockets(path)`), so a chat or a dashboard is one C++ program on both sides. `reactx dev --proxy` passes WebSockets through. Only additions; apps update without changes.

### Added

- **`useWebSocket`** (`reactx/websocket.hpp`): live data without polling, in the browser (the page's WebSocket; relative URLs go to the page's host) and in the terminal (`ws://`, its own thread, events on the UI thread). Status, `send`, reconnect after a drop with a growing delay, handlers that may set state.
- **WebSockets in `HttpServer`**: `server.webSocket(path, {.onOpen, .onMessage, .onClose})`, `server.webSockets(path)` to broadcast, `WebSocketPtr` with `send` / `close` from any thread. Text messages, pings answered, 1 MB limit. `server.proxy` (and so `reactx dev --proxy`) passes WebSockets through.

## [0.12.0] — 2026-11-01

Pages that show before the `.wasm` arrives with one line in `reactx.json` (`"web": {"prerender": ["/", "/docs/faq"]}`), a scroll position API for chats and feeds (`ref.scrollMetrics()` / `scrollTo()` / `scrollBy()`, both backends), and a fix for built sites that kept polling the dev server — forever on hosts with an SPA fallback. Found by writing a chat, a process monitor and a documentation site on ReactX. A MINOR release because `Renderer` got virtuals and `serveDirectory`'s SPA fallback prefers `200.html`; apps update without changes (rebuild to drop the reload script from published pages).

### Added

- **Prerendering from `reactx.json`**: `"web": {"prerender": ["/", "/docs/faq"]}`. The build runs the same program under Node, where `mountDom` writes each route's HTML (with the `rx::ui` CSS) into `dist/index.html`, `dist/docs/faq.html`, … and stops; the page shows before the `.wasm` arrives. No code changes. `200.html` / `404.html` stay the empty page for other routes. `reactx_add_app(... PRERENDER <routes>)` for CMake, `reactx::prerender`.
- `ref.scrollMetrics()`, `ref.scrollTo(top)`, `ref.scrollBy(delta)` (`NodeRef`, `Renderer::scrollMetrics` / `scrollTo`): read and move a scroll container in the browser and in the terminal — e.g. keep the reader's place when older messages are added above.
- The `scrollIntoView` prop (`{"scrollIntoView", selected}`): an element scrolls into view when it becomes `true`, in both backends, without a ref.
- `HttpServer::onFile(hook)`: change the body or headers of served files. `serveDirectory` serves `/docs/faq` from `docs/faq.html` when there is one, and its SPA fallback prefers `200.html`.

### Fixed

- Built sites polled `__reactx/version` (the `reactx dev` reload script was in every page); on a host that answers every path with `index.html` (SPA rewrites, `serveDirectory(dir, true)`) that never stopped, about 1.4 requests a second per open tab. Only `reactx dev` adds the script now.
- llms.txt: `Request::param` returns `std::optional<std::string>`; `renderToString` in a native build draws the terminal form of `ui::` and `router::link`.

## [0.11.0] — 2026-11-01

Long lists in the terminal: a `box` with `overflow: "scroll"` (or `ui::list({.scroll = true})`) scrolls with the mouse wheel and PageUp/PageDown and keeps the focused or selected item on screen, and modals and toasts stay on the screen however tall the page is. `ref.scrollIntoView()` in both backends. Fixes found by writing real apps from llms.txt: `ui::select` with an empty value ("All") in the browser, and a terminal modal that Escape couldn't close. A MINOR release because `Renderer` got a virtual and the terminal modal and toaster are now `position: "fixed"`; apps update without changes.

### Added

- **Scrolling in the terminal**: `overflow: "scroll"` makes a `box` a vertical scroll container. In a column it takes the rows that are left (no height to compute); the mouse wheel over it and PageUp/PageDown scroll it, moving the focus keeps the focused node visible, a border shows the position (`┃`), and `onScroll` now works in the terminal too (a `ScrollEvent` in rows). `TerminalRenderer::scrollBy` / `scrollTop` for code and tests.
- `ui::list({.scroll = true})`: a long list scrolls inside its border instead of growing; `ui::listItem` keeps a newly selected item in view (both backends).
- `ref.scrollIntoView()` (`NodeRef`): scrolls an element into view, the least needed, in the browser and in a terminal (`Renderer::scrollIntoView`).
- `position: "fixed"` in the terminal: placed on the screen and drawn over everything. `ui::modal`, `ui::confirm` and `ui::toaster()` use it, so they show however tall the page is.

### Fixed

- `ui::select` (and any `<option value="">`) in the browser: an option with an empty value wasn't selected and its `onChange` sent the label ("All") instead of `""`.
- A terminal `ui::modal` with nothing focusable inside (only text) couldn't be closed: Escape didn't reach it. Keys now go to the open modal when nothing in it has focus.
- Terminal: a page taller than the screen pushed `ui::modal`, `ui::confirm` and toasts below the bottom edge.
- The `AGENTS.md` of new projects said only click, input, change, keydown and submit events exist; it now lists all of them, the `rx::ui` components, how to add tests to a web project and (terminal) how long lists scroll.
- llms.txt: the type of a `ui::select` option (`ui::Option`), `ui::input`'s `onSubmit(text)`, `asNumber()`/`asInt()`/`asBool()` for JSON values, tests in a web project, the error for a missing `key` field.

## [0.10.0] — 2026-11-01

Ready-made pieces for everyday screens, the same in the browser and the terminal: `ui::confirm` (a "Delete it?" dialog in one line), `ui::toast` (messages in a corner from any handler), `ui::select`, `ui::tooltip`, and `ui::card` / `ui::stack` / `ui::text` for layout without CSS. Also a fix for `mountDom` into a container that already held an app. Apps update without changes.

### Added

- **New `rx::ui` components**, the same in the browser and the terminal: `ui::select` (a native `<select>`; ‹ › with arrows or a click in a terminal), `ui::confirm` (a ready "Delete it?" dialog), `ui::toast` + `ui::toaster()` (messages in a corner that go away by themselves, sent from any handler), `ui::tooltip` (on hover, and on focus in the browser), `ui::card`, `ui::stack` (rows and columns with gaps, no CSS) and `ui::text`. Each also exists as a `Props` component (`ui::Select`, `ui::Confirm`, …). The rx::ui demos use them.

### Fixed

- `mountDom` into a container that already held an app: the old app's event listeners stayed and could undo the new app's `<select>` and inputs. Now the earlier app stops listening.

## [0.9.0] — 2026-10-31

C++ meets the page: `useNodeRef` and the `ref` prop give the real element, `reactx/js.hpp` drives JavaScript libraries from C++ state (a Chart.js demo), and pointer events bring hover, drag and drop and focus from code — the same in the browser and, with the mouse, in the terminal (a kanban demo built from one source for both). `reactx install` now checks for `xz`. A MINOR release because `PropValue` and `EventData` got new alternatives: code that `std::visit`s all of them needs branches for `RefHandler` / `PointerHandler` / `PointerEvent`; other apps update without changes.

### Added

- **Mouse, touch and drag**: `onPointerDown` / `onPointerMove` / `onPointerUp` (`PointerHandler`, `PointerEvent` with `x`, `y`, `localX`, `localY`, `button`, `buttons`, `type`) and `onMouseEnter` / `onMouseLeave` (don't bubble), in the browser and in the terminal. `{"pointerCapture", true}` keeps the pointer on an element after a press (sliders); without it the element under the pointer gets moves and the release (drop targets).
- **The mouse in the terminal**: `TerminalApp` turns on SGR mouse reports — clicks press buttons and focus, hover, drag; `app.setMouse(false)` turns it off. `decodeInput` / `Terminal::readInput` give keys and `MouseInput`s; `TerminalRenderer::handleMouse` and `nodeAt(x, y)`.
- **`ref.focus()`**: a `NodeRef` focuses its element from code, in both backends (`Renderer::focus`).
- **Demo: a kanban board** (`examples/kanban.cpp`): drag cards between columns with the mouse or a finger, or move them with ← →; one source for the browser (`web_kanban`, on the site's demos page) and the terminal (`kanban`).
- **`useNodeRef()` and the `ref` prop**: `{"ref", ref}` on a host element gives C++ its node — set after the commit, before effects run, and back to 0 when the element goes; copies of a `NodeRef` share it. Any `RefHandler` (`std::function<void(NodeHandle)>`) works as a callback ref. Works with every backend.
- **`reactx/js.hpp`** (browser): `rx::domElement(ref)` — the page's element as an `emscripten::val`, to call its methods or hand it to a JavaScript library; `rx::jsValue(json)` — a JS object from JSON. Link `reactx::js` (embind); apps without it don't carry embind.
- **Demo: Chart.js from C++** (`examples/web_chart.cpp`, on the site's demos page): the data lives in C++ state, the chart is created, updated and destroyed from effects.

### Changed

- `PropValue` has one more alternative, `PointerHandler`, and `EventData` one more, `PointerEvent`. `TerminalRenderer::focus` now overrides `Renderer::focus`.
- `PropValue` has one more alternative, `RefHandler`: code that `std::visit`s all of `PropValue` needs a branch for it. `NodeHandle` now lives in `reactx/element.hpp` (still available through `renderer.hpp`).

### Fixed

- `install.sh` tells how to add `reactx` to PATH for the user's shell (zsh: `~/.zshrc`, fish: `fish_add_path`; before, always `~/.bashrc`, which zsh doesn't read) and prints it last, after the output of `reactx install`.
- The install command on the site copies as one line: comment lines in it broke in zsh (`command not found: #`).
- The site's Demos and 404 pages had no side margins on a phone (the text touched the screen edge).
- `reactx install` checks for `xz` before downloading Emscripten (its archives are `.tar.xz`; without `xz`, as in slim containers, `tar` failed halfway), and when emsdk fails it says what usually causes a `tar` error and how to retry. Found by an AI agent (Grok) building a project.
- `llms.txt`: `Store::get()` (reading a store outside components), what `NodeRef::get()` gives, and that browser projects don't need GCC 16.

## [0.8.3] — 2026-10-31

Pages that show before the WebAssembly arrives: `rx::renderToString` renders components to HTML without a browser, and the site's home page is prerendered with it (LCP on a throttled phone ~1.2 s → ~0.3 s). Apps update without changes; `mountDom` now replaces what the container showed (see Changed).

### Added

- **`rx::renderToString(element)`** (`reactx/html.hpp`) — the HTML of components without a browser, to prerender a page at build time or serve it from C++. Props become attributes the way the DOM backend sets them (`className` → `class`, `true` → a present attribute, the `value` of a `<textarea>` / `<select>`), text is escaped, `javascript:` URLs are blocked; effects don't run, like in React. `HtmlRenderer` is the backend behind it.
- **The site's home page is prerendered**: `index.html` holds its HTML in English and Russian, so the page shows before `index.wasm` arrives (on a throttled phone LCP went from ~1.2 s to ~0.3 s, the same as the first paint).

### Changed

- `mountDom` replaces what the container showed (a loading text, prerendered HTML) instead of adding the app after it.
- The router works without a page (under Node): the path is `/` until `reset` or `setBase` say otherwise.

### Fixed

- `install.sh` no longer fails with `unknown command install` when it gets a release older than 0.8.2: it says how to get the compiler instead.

## [0.8.2] — 2026-10-31

Smaller and faster in the browser, and easier to start: WebAssembly 2–3.5× smaller (a counter is 40 KB over the network), `reactx install` downloads the compiler without sudo, a one-line installer, `fetch` with headers, timeouts and cancelling, storage errors as values, strict JSON reading, a setter thread check, and the changelog in English. Apps update without changes, except the rare `return fetch(...)` from a `void` function (see Changed).

### Added

- **The site in English**: all documentation, the home page and the interface are in English and Russian, with an EN/RU switch in the header (the choice is remembered). English by default, Russian if the browser is Russian. The page title and description for search engines and social previews are in English.
- **`json::Value::require<T>(key)`** — strict reading of outside data (an import, a server response): `std::expected<T, json::Error>`, where a missing field, another type, `2.9` for an `int` or a number that doesn't fit the type is an error naming the field (`field "points": expected an integer that fits`), not a silent fallback like `get<T>`. Works in the browser build too (no reflection).
- **`rx::boolHandler(f)`** — a checkbox's `onChange` with a `bool` instead of the strings `"true"` / `"false"`: `{"onChange", boolHandler([setDone](bool on) { setDone(on); })}`.
- `ScrollEvent::scrollTop()` / `scrollLeft()` — the DOM's familiar names for `top` / `left`.
- `Setter<T>` and `Dispatch<S, A>` can be default-constructed (a props field nobody filled in): calling one does nothing. Before, a `Dispatch` field in a props struct couldn't be left without a value.
- **Setters check the thread**: natively, a setter called from a thread other than the one that created the `Root` (e.g. from a `std::thread`) stops the program with `precondition failed: ... onOwnThread()` instead of silently racing with rendering.
- README, `llms.txt`: a table of common compile and run-time errors with explanations; `llms.txt` now lists what `reactx/reactx.hpp` includes and which headers to include separately, the signature and an example of `setLocalFetch`, iterating JSON arrays and objects, `root.flush()` in tests after `setState` outside `dispatch`, and `useFetch` keeping the old response during `reload()`.
- **Headers, timeout and cancelling in `fetch`**: `FetchRequest{.headers = {{"Authorization", "Bearer ..."}}, .timeout = 5s}`; on a timeout the answer has status 0 and `error == "timeout after N ms"`. `FetchResponse::headers` and `header("etag")` (case-insensitive) are the response headers. `fetch` returns a `FetchHandle`: after `cancel()` the callback never runs, and the browser aborts the request (`AbortController`). Works in the browser and the terminal build.
- **`useFetch(FetchRequest{...})`** — a whole request with headers, timeout and method; refetches when the request changes. `useFetch` now cancels the request it replaces (new url, `reload()`) and the request of an unmounted component — before, it only ignored the answer.
- **Storage errors as values**: `rx::storageRead(key)` (`std::expected<std::optional<std::string>, std::string>`: a missing key is `std::nullopt`, not an empty string), `rx::storageWrite`, `rx::storageRemove`. `useLocalStorage(key, default, onError)` calls `onError` when saving fails (quota exceeded, private mode) — before, a failed write in the browser was lost silently. Found by an AI agent writing ForgeDesk.
- **`reactx build` watches the WebAssembly size**: it prints the size of `index.wasm` and, if the app itself pulls in `std::format` / `std::print` or printing of floating-point numbers, names the files and what to use instead (`+` and `std::to_string`) — each adds ~100 KB that the browser downloads before the first paint. For this `reactx_add_app` writes a linker map, `<build>/<target>.link.map`.
- **The changelog in English** (this file); the Russian one moved to `CHANGELOG.ru.md`. GitHub Release notes are now taken from the English file.
- **`reactx install` — the compiler without sudo, like npm**: downloads Emscripten 6.0.13 (the compiler for the browser, via emsdk) into `~/.reactx` and, if the system has none or an old one, portable CMake and Ninja. A system Emscripten (like Arch's) or `$EMSDK` is used as it is. `reactx build` / `dev` then find the tools on their own — no `source emsdk_env.sh`. `--check` only shows what is there. Needs `curl`, `git`, `python3`, `tar`; GCC 16 for terminal apps can't be downloaded this way. Checked from scratch: a `PATH` without cmake, ninja and emcc → `reactx install` → `reactx new` + `reactx build` with no `EMSDK`.
- **One-line install** on Linux x86-64: `curl -fsSL https://sargisis.github.io/ReactX/install.sh | sh` puts the prebuilt `reactx` into `~/.local/bin` and runs `reactx install`. The script is `install.sh` in the repository and is published with the site.

### Changed

- **WebAssembly 2–3.5× smaller, pages open faster**: a counter is 91 KB instead of 332 KB (40 KB instead of 146 KB over the network, gzip), the site is 269 KB instead of 630 KB (109 KB instead of 249 KB). Under Lighthouse's mobile profile (150 ms, 1.6 Mbit/s, 4× CPU) the site's home page LCP is 1.2 s instead of 2.1 s. What changed:
  - `REACTX_PRE` in the browser build no longer prints with `std::println`: that one call pulled all of `std::format` (~100 KB) into every app.
  - Floating-point numbers in props and JSON are turned into text by the browser itself (`String(x)`, also the shortest exact form) instead of libc++'s `std::to_chars` with its tables (~110 KB). Natively it is still `std::to_chars`.
  - Release builds for the browser compile with `-Oz` instead of `-O3` (~25% smaller); `-DREACTX_WEB_OPTIMIZE_SIZE=OFF` brings `-O3` back.
  - The page templates (`web/*_shell.html`) request `index.wasm` right away (`<link rel="preload">`), not after `index.js` has loaded; the site's fonts no longer delay the first paint.
- `rx::fetch` returns a `FetchHandle` instead of `void`. Code that ignores it doesn't change; an effect like `useEffect([] { return fetch(...); })` now cancels the request on cleanup. Strictly speaking this is a source-incompatible change (`void f() { return fetch(...); }` no longer compiles), so it is mentioned here.
- `useLocalStorage`: a saved empty string is read back after a reload as `""`, not as the default value.
- "Getting started" rewritten in plain language for newcomers: what ReactX is, installing the prebuilt `reactx`, an app in three commands, the first component, working with an AI assistant; building ReactX itself from source comes last. On the home page the install step is the prebuilt binary instead of a build from source.

### Fixed

- README: the macOS build from source said `g++-14`, though GCC 16+ is needed; now `g++-16`. The examples no longer call ReactX a "C++20" framework.
- `reactx dev` with `"web": {"base": "/pulseboard/"}` showed a blank page: the page requested `/pulseboard/style.css`, `/pulseboard/index.wasm`, but the server served files from the root only. Now files and live reload work under the base path (like on GitHub Pages), `/` redirects to it, and the console prints the right address. Found by an AI agent writing PulseBoard.

## [0.8.1] — 2026-10-27

Findings of AI agents that wrote large projects with ReactX: lambda components, `rx::urlEncode`, `reactx <command> --help`, a clear error instead of a link failure on `mountDom`, an editor setup with clangd and no false errors. Apps update without changes.

### Added

- **A lambda as a component with a props struct**: `const auto Card = [](const CardProps& p) -> Element { ... };` and `h(Card, {.title = "x"})`, `h(Card)`, `h(Card, {...}, {children})` — like with a function; the same lambda is the same component (state is kept). Before, the `h()` template didn't deduce the props type from a lambda and gave a long error about `TypedComponent<P>`.
- **`rx::urlEncode`** (`reactx/path.hpp`) — the counterpart of `urlDecode`: encodes a value for a query string or a path segment (`"milk & tea"` → `milk%20%26%20tea`).
- **`reactx <command> --help`** and `reactx help <command>` — help for one command with its options and an example. Before, `reactx add --help` failed with "unknown option --help".
- **An editor without false errors**: `reactx new` puts a `.clangd` into the project (flags from the real build's `compile_commands.json`, without flags clang doesn't know) and a `.vscode/` with clangd instead of the C/C++ extension's IntelliSense, which doesn't understand C++26 and GCC 16 and underlined correct code. Web projects use the clangd from Emscripten. Checked with `clangd --check` on new web and terminal projects: 0 errors, and real errors are highlighted.

### Fixed

- `reactx build --native` in a web project failed at link time with a cryptic `undefined reference to rx::mountDom(...)`. Now natively `mountDom` and `flushDom` are a compile error at the call with an explanation: `mountDom` is browser-only, and a `main` for both builds picks `mountDom` or `TerminalApp` with `#ifdef __EMSCRIPTEN__`. This is described in the README, on the site and in `llms.txt`. Found by an AI agent writing a large project with ReactX.
- The repository's VS Code settings pointed at `g++-15` and C++20; now clangd and a `.clangd` over `build/debug`. Examples with `import reactx;` show the headers to the editor (clangd doesn't read GCC modules).

## [0.8.0] — 2026-10-27

Less memory (−28% per node), a DoS in `reactx dev --proxy` closed, fuzzing and static analysis on every push, SECURITY.md. Apps update without changes.

### Added

- **SECURITY.md**: how to report a vulnerability, what counts as one (the threat model) and how the project is checked.
- **Fuzzing in CI**: `tests/fuzz_parsers.cpp` — a minute of random input for JSON, HTTP requests and responses, URLs and routes under ASan + UBSan on every push, with a new seed each time; the input that crashed is saved.
- **Static analysis in CI**: `-DREACTX_ANALYZER=ON` runs `gcc -fanalyzer` on the code that reads outside data; any finding fails the build.
- `rx::net::parseResponse` — parses a whole HTTP/1.x response (the counterpart of `parseRequestHead`).
- `reactx_bench` shows how much memory a mounted list holds per row.

### Changed

- **Less memory**: a fiber stores only what its kind of node needs (text, element, component) — a list of 10,000 rows holds 1,442 bytes per row instead of 2,002 (−28%), at the same speed.

### Fixed

- **Proxy (`reactx dev --proxy`)**: a chunk size near `SIZE_MAX` overflowed a check, and a crafted backend response made it copy again and again (quadratic memory growth up to the 64 MB limit); a `Content-Length` overflowing `size_t` passed a truncated response off as complete.
- Publishing a release: the workflow downloaded Docker's internal artifact and failed at the last step (as with v0.6.0); now it takes only its own files, and `gh workflow run release.yml -f tag=vX.Y.Z` publishes a release for an existing tag.

## [0.7.0] — 2026-10-27

Error boundaries, shared state (`createStore` / `useStore`), `useId`, the network in terminal apps; a faster core on large lists with a minimum of node moves; fixed a memory leak in context, a corrupted tree after an exception, `<select>` and `javascript:` links in the browser. Apps update without changes; in the browser `checked` is now controlled (as in React).

### Added

- **`errorBoundary(fallback, children)`** — an error boundary, as in React: if the children throw while rendering (on mount or later, on a state update), `fallback(RenderError)` shows instead, and the rest of the app keeps working. `RenderError` has `message`, `exception` and `retry()` to try again. If the fallback itself throws, the next boundary up catches it.
- **`createStore` / `useStore`** — shared state outside the tree (like Zustand): `Cart.set(v)` / `Cart.update(f)`; `useStore(Cart)` re-renders on every change, `useStore(Cart, selector)` only when the selected part changes.
- **`useId()`** — an id unique in the app and the same on every render (for `<label for>` and `aria-*`).
- **`fetch` / `useFetch` in the terminal build use the network**: a full `http://` URL, the request on its own thread, the answer on the UI thread at the next `flush()`. Before, without `setLocalFetch`, requests failed at once with "no network".
- `reactx_bench` — measurements of the core on large lists (not a test): `./build/release/reactx_bench [rows]`.

### Changed

- **A faster core on large lists** (`reactx_bench`, 10,000 keyed rows): a re-render with no changes 35 → 8 ms, swapping two rows 56 → 9 ms, 50,000 rows mounted and removed under one context provider 833 → 68 ms.
- **A minimum of node moves**: `sync` keeps the nodes on the longest increasing subsequence of old positions in place and inserts the rest once each. Swapping two rows out of 10,000 is 2 moves instead of 9,997, removing every second row is none instead of 4,998 (in the browser each one is a DOM operation that resets focus and selection).
- Browser: `checked` on checkboxes and radio buttons is controlled, like `value`: if `onChange` didn't change the state, the click is undone (for a radio button — for the whole group), as in React.

### Fixed

- **A memory leak in context**: components that read a context stayed in the provider's list until its value changed; under a constant provider (a theme) memory grew with every row that had ever been on screen, and reading the context slowed down quadratically. Now the subscription is removed on unmount, and lookup is O(1).
- **An exception while rendering corrupted the tree**: half-built fibers stayed registered (dangling pointers the next event went through), and effects weren't cleaned up. Now the failed subtree is unmounted.
- If an uncaught exception interrupted a batch of updates, the other components of the batch stayed marked but out of the queue — their `setState` never re-rendered again. Now they stay in the queue.
- **Browser, security**: `href`, `src`, `action`, `formaction`, `xlink:href`, `poster`, `cite` with a `javascript:` link (by React's rule: with control characters, spaces and line breaks inside, in any case) are replaced with `about:blank#blocked` and an error in the console — a link from user data no longer runs code on click.
- Browser: a controlled `<select value=...>` showed the wrong option on the first render (the value was set before the `<option>`s appeared), and with `onChange` it couldn't be switched at all — the `input` event put the old value back before `change` reported the choice.
- The `import reactx;` module didn't export `useReducer`, `Dispatch`, `useTimeout`, `useInterval` and `useKeyboard` from v0.5.

## [0.6.0] — 2026-10-27

ReactX without installing compilers: a prebuilt `reactx` in GitHub Releases, a Docker image and a devcontainer; `reactx dev --proxy` for your own backend and `--host` for containers. No incompatible changes.

### Added

- **A prebuilt `reactx` in GitHub Releases**: on a pushed `vX.Y.Z` tag the `release.yml` workflow checks that the tag matches the version in `CMakeLists.txt`, builds static `reactx` and `reactx_serve` for Linux x86-64 (they run on any distribution), creates a release with the section from the CHANGELOG and publishes the Docker image. Web projects now need only Emscripten, CMake and Ninja — GCC 16 is needed for terminal ones only.
- **A Docker image** `ghcr.io/sargisis/reactx` (`Dockerfile` in the repository): GCC 16, CMake, Ninja, Emscripten with a warmed-up cache, and `reactx`; a `dev` user so it doesn't run as root.
- **devcontainer**: `.devcontainer/` for working on ReactX in VS Code and GitHub Codespaces; `reactx new` puts the same one into every project — with the image of the same version.
- **`reactx dev --proxy [PREFIX=]URL`** — requests under a prefix (`/api` by default) go to your own backend (Flask, Django, Express, …) and the answer comes back as is; it can also be set in `reactx.json`: `"web": {"proxy": {"/api": "http://127.0.0.1:5000"}}`.
- **`reactx dev --host 0.0.0.0`** — the server is reachable from outside the container and from other devices on the network.
- **`HttpServer::proxy(prefix, upstream)`, `rx::net::forward`, `rx::net::parseUpstream`**: a small HTTP/1.1 client for the proxy — `Content-Length`, `chunked` and responses until the connection closes; 502 with the reason if the backend is unreachable, 504 if it didn't answer.
- `Request::target` — the request path as it arrived, not decoded (`/api/a%2Fb?x=1`).

### Fixed

- A downloaded `reactx` (built on another machine) passed the build machine's compiler path to CMake and broke the first build of a terminal project; now the path is passed only if that file exists.
- The fields of `rx::net::Request`, `Response` and `HttpError` got default values: `{.method = "GET", .path = "/"}` compiles under `-Wextra -Werror`, like props structs.
- The status line knows the codes 202, 206, 303, 307, 308, 401, 409, 410, 415, 422, 429, 501–504 (before — "Unknown").

## [0.5.0] — 2026-10-27

The MIT / Apache-2.0 license, new hooks (`useKeyboard`, `useReducer`, `useTimeout`, `useInterval`), focus and scroll events, SVG in the browser, `reactx update` and documents for AI assistants. Apps update without changes; a custom backend that unpacks `EventData` or prop values with `std::visit` must handle the new `ScrollEvent` / `ScrollHandler`.

### Added

- **License**: ReactX is available under MIT or Apache-2.0 at your choice (`LICENSE-MIT`, `LICENSE-APACHE`), like Rust. Before there was no license, and formally the code couldn't be used.
- **`useKeyboard`** — app-wide keyboard shortcuts (Ctrl+Z, "/", Escape) wherever the focus is: the handler runs before the element's `onKey`, `return true` takes the key (in the browser — the default action too). While the focus is in a text field, keys that type text don't come here. In the terminal — through `TerminalApp`.
- **`useReducer`** — state changed by actions: `auto [s, dispatch] = useReducer(reducer, initial)`; the newest reducer is used.
- **`useTimeout` / `useInterval`** — once after a delay / periodically, on the UI thread; `std::nullopt` switches it off, unmounting cancels it.
- **Events `onFocus`, `onBlur`** (browser; in the terminal — when focus moves with Tab and the arrows) and **`onScroll`** with a `ScrollEvent` (`top`, `scrollHeight`, `clientHeight`, …, `nearBottom()`).
- **SVG** in the browser: `svg`, `rect`, `circle`, `path`, `g`, … are created in the SVG namespace; `svg:text`, `svg:a` for tags that HTML has too.
- `TerminalRenderer::editingText()` — whether the focus is in a text field.
- **For AI assistants**: `llms.txt` (in the llmstxt.org format: a complete example program, the API, events, rules, common mistakes; published on the site) and `AGENTS.md` (building, CI-like checks, rules, known GCC 16 traps, releases). `reactx new` puts its own `AGENTS.md` with commands and rules for the web or the terminal into the project.
- **`reactx update`** — moves a project to the newest ReactX release: tags from the repository in `reactx.json` (`git ls-remote`), chosen by SemVer (`v0.10.0` is newer than `v0.9.0`, `-rc` are skipped), a link to the CHANGELOG and a warning if a new MINOR (while MAJOR = 0) may need changes. `--check` only checks, `--to vX.Y.Z` picks a version; without `--to` the version never goes down.

### Fixed

- After `reactx update` / `add` / `remove` the project was built with the old contents of `reactx.json`: `reactx build` / `dev` configured CMake only once. Now, if `reactx.json` is newer than the CMake cache, they configure again; the `reactx new` template also declares `reactx.json` a configure dependency (for running `cmake` by hand).

## [0.4.0] — 2026-10-26

Fixes for the hooks of 0.3.0 and the browser build; a DSL for elements. If you are on `v0.3.0`, update: no web program builds with it.

### Fixed

- **The browser build builds again**: in `v0.3.0` `useDebounce` called `detail::setTimeoutJs` before its declaration, and no web program (including projects from `reactx new`, which use the `v0.3.0` tag) compiled.
- **`useDebounce` without threads**: natively it started a detached `std::jthread` that changed state in parallel with rendering (a data race, ThreadSanitizer in CI). Now the timer fires on the UI thread: in the browser `setTimeout`, natively at the next `Root::flush()` (`TerminalApp` calls it every 100 ms). A new value or unmounting cancels the timer.
- **`useWindowSize` updates**: in the browser on the `resize` event, in the terminal when `TerminalApp` notices a new size. Before, the size was read once.
- **`useLocalStorage`**: numbers are read with `rx::parseNumber` (no `std::stoll` and exceptions), the saved value is read once on mount, and an unsupported type is a compile error with an explanation rather than a value that silently isn't saved.
- **The `reactx new` (web) template compiles**: `h(Home)` for a component with a props struct takes the default props (`P{}`), like the routes `router::routes({{"/", h(Home)}})`.

### Added

- **A fluent DSL** (`reactx/dsl.hpp`): `tag::div.cls("card")("text", tag::button.onClick(f)("OK"))`, custom tags with `"my-tag"_tag`.

## [0.3.0] — 2026-10-26

Typed props, C++26 reflection and contracts, new hooks (`useDebounce`, `useLocalStorage`, `useWindowSize`).

### Added

- **New hooks**:
  - `useDebounce(value, delay)` — delays a state update, safely cancelled on unmount (WASM setTimeout / C++20 `std::jthread` + `std::stop_token`).
  - `useLocalStorage(key, defaultVal)` — keeps state in `localStorage` (browser) and in memory (native/CLI).
  - `useWindowSize()` — the current size of the browser window or the terminal (`WindowSize`).
- **Props structs**: a component takes its own struct — `Element Counter(const CounterProps&)`, `h(Counter, {.start = 5})`. The compiler catches typos in field names and wrong types; works everywhere, including the browser. A `key` field is the element key, a `children` field receives the children (`h(Card, {.title = "Hi"}, {...})`), `memo<Counter>` compares the structs with their `operator==`. The `reactx new` templates use typed props.
- **C++26 reflection** (`<meta>`, GCC 16): `rx::json::to(value)` and `rx::json::from<T>(json)` for plain structs — fields become keys, types are checked, an error shows the path (`field "dependencies": "fmt": missing field "rev"`). Supports numbers, strings, `bool`, `std::optional`, sequences, maps with string keys, nested structs. Enabled for a target with `reactx::reflection`.
- The CLI reads `reactx.json` as a `Manifest` struct with `json::from`: a forgotten `rev` of a dependency now shows at once, not as a CMake error during the build.
- `reactx/parse.hpp`: `rx::parseNumber<T>` (`std::from_chars` + `std::expected`, no exceptions and locales) and `rx::parsePort`.
- `TerminalRenderer::setUnicode`: ASCII borders (`+-|`) for terminals that aren't UTF-8; `TerminalApp` chooses with `std::text_encoding`.
- CLI: reading and writing files with `std::expected`, `reactx.json` as an `and_then` chain (read → parse → check), walking the sources in `reactx dev` with `std::generator`.

### Changed

- **The native build needs GCC 16** (the C++26 standard library); CI uses the `gcc:16` image. CMake says so at once if the compiler is older.
- **C++26 contracts**: on GCC 16 ReactX turns on `-fcontracts`, and `REACTX_PRE` becomes `contract_assert`. Without contracts a violation prints the call stack (`<stacktrace>`) and stops in the debugger (`<debugging>`).
- `net::parseRequestHead` returns `std::expected<Request, HttpError>` with the reason; the new `net::bodyLength` checks `Content-Length`.
- `Props::values()`, `Request::headers` and props in `TextRenderer` are `std::flat_map` instead of `std::map`; `Props::has` / `find` / `get` take a `std::string_view`.
- Terminal: a screen cell is a `std::inplace_vector<char, 4>` (no heap), the screen is a `std::mdspan` over one array instead of `vector<vector<Cell>>`.
- Output of the CLI, `reactx_serve`, `api_server` and the examples is `std::print`; `std::format` instead of `ostringstream`.

## [0.2.0] — 2026-10-26

Safety and modern C++.

### Changed

- **C++26 mode** (was C++20): needs GCC 15+ or Clang 19+ (Emscripten 6+); the `reactx new` template moves to C++26 too. We use all of C++23 and the implemented parts of C++26.
- CI on GCC 15 (the `gcc:15` container) instead of GCC 14.

### Added

- **Standard library hardening** (`REACTX_HARDENING`, on): bounds checks in `v[i]` / `s[i]` / `front()`.
- **`REACTX_PRE`** — preconditions ready for C++26 contracts (`reactx/contract.hpp`); used in `HttpServer` routes, `router::navigate`, hooks and `mountDom`.
- **`std::expected`**: `json::tryParse`, `FetchResponse::tryJson` / `FetchState::tryJson`, `HttpServer::tryStart`; the CLI reports a broken `reactx.json` instead of crashing.
- **Sanitizers in CI**: ASan + UBSan + LeakSanitizer (fatal on the first error and leak) and ThreadSanitizer; the site is published only after them.
- **Edge-case tests for the reconciler** (`tests/test_reconciler.cpp`, 24 tests): batched updates, setState during render and after unmount, the order of effect cleanups, nested context providers, comparing props in `memo`.
- A documentation page "C++ standard and safety".

## [0.1.0] — 2026-10-26

The first release.

### Added

- **A React-style core in C++20**: `h()` elements, function components, props, children, fragments, keys; a reconciler that reuses nodes and moves them by key; batched updates.
- **Hooks**: `useState`, `useEffect` (deps and cleanup), `useRef`, `useMemo`, `useCallback`; **context** (`createContext`, `provide`, `useContext`) and **`memo<Component>`**.
- **Events with data**: `Callback`, `KeyHandler` (`KeyEvent`), `InputHandler`; bubbling to parents.
- **A C++20 module** `import reactx;` (CMake 3.28+, Ninja).
- **A terminal backend**: ANSI rendering that redraws only changed lines, borders, colors, focus, keyboard input; **Flexbox layout on Yoga**; `TerminalApp`.
- **A browser backend** (WebAssembly, Emscripten): `DomRenderer`, `mountDom`, controlled inputs, event delegation, `data-rx-self` / `data-rx-trap` / `data-rx-link`.
- **`rx::ui` components** with a theme — one code for the terminal and the browser: `button`, `input`, `checkbox`, `tabs`, `modal`, `list` / `listItem`.
- **A router** `rx::router`: routes with parameters and `*`, links without reloading, the History API, the base path from `<base href>`.
- **Data**: `useFetch`, `fetch`, `setLocalFetch` (a "server inside the app"); its own **JSON**, `rx::json`.
- **An HTTP server in C++** `rx::net::HttpServer`: files with the right types, routes with parameters, SPA mode, a URL prefix and a 404 page; the programs `reactx_serve` and `api_server` (a REST API for todos).
- **The `reactx` CLI**: `new` (web or `--terminal`), `dev` (a built-in server, rebuild and live reload of the page), `build`, `add` / `remove` dependencies; `reactx_add_app()` for your own CMake projects.
- **A documentation site** built with ReactX: https://sargisis.github.io/ReactX/, published by GitHub Actions through the `gh-pages` branch.
- The version in one place (`project(ReactX VERSION ...)`) → `reactx/version.hpp`, `rx::version()`; new projects from `reactx new` use ReactX by release tag.

[Unreleased]: https://github.com/sargisis/ReactX/compare/v1.3.0...HEAD
[1.3.0]: https://github.com/sargisis/ReactX/compare/v1.2.0...v1.3.0
[1.2.0]: https://github.com/sargisis/ReactX/compare/v1.1.0...v1.2.0
[1.1.0]: https://github.com/sargisis/ReactX/compare/v1.0.1...v1.1.0
[1.0.1]: https://github.com/sargisis/ReactX/compare/v1.0.0...v1.0.1
[1.0.0]: https://github.com/sargisis/ReactX/compare/v0.21.0...v1.0.0
[0.21.0]: https://github.com/sargisis/ReactX/compare/v0.20.0...v0.21.0
[0.20.0]: https://github.com/sargisis/ReactX/compare/v0.19.0...v0.20.0
[0.19.0]: https://github.com/sargisis/ReactX/compare/v0.18.0...v0.19.0
[0.18.0]: https://github.com/sargisis/ReactX/compare/v0.17.0...v0.18.0
[0.17.0]: https://github.com/sargisis/ReactX/compare/v0.16.0...v0.17.0
[0.16.0]: https://github.com/sargisis/ReactX/compare/v0.15.0...v0.16.0
[0.15.0]: https://github.com/sargisis/ReactX/compare/v0.14.0...v0.15.0
[0.14.0]: https://github.com/sargisis/ReactX/compare/v0.13.0...v0.14.0
[0.13.0]: https://github.com/sargisis/ReactX/compare/v0.12.0...v0.13.0
[0.12.0]: https://github.com/sargisis/ReactX/compare/v0.11.0...v0.12.0
[0.11.0]: https://github.com/sargisis/ReactX/compare/v0.10.0...v0.11.0
[0.10.0]: https://github.com/sargisis/ReactX/compare/v0.9.0...v0.10.0
[0.9.0]: https://github.com/sargisis/ReactX/compare/v0.8.3...v0.9.0
[0.8.3]: https://github.com/sargisis/ReactX/compare/v0.8.2...v0.8.3
[0.8.2]: https://github.com/sargisis/ReactX/compare/v0.8.1...v0.8.2
[0.8.1]: https://github.com/sargisis/ReactX/compare/v0.8.0...v0.8.1
[0.8.0]: https://github.com/sargisis/ReactX/compare/v0.7.0...v0.8.0
[0.7.0]: https://github.com/sargisis/ReactX/compare/v0.6.0...v0.7.0
[0.6.0]: https://github.com/sargisis/ReactX/compare/v0.5.0...v0.6.0
[0.5.0]: https://github.com/sargisis/ReactX/compare/v0.4.0...v0.5.0
[0.4.0]: https://github.com/sargisis/ReactX/compare/v0.3.0...v0.4.0
[0.3.0]: https://github.com/sargisis/ReactX/compare/v0.2.0...v0.3.0
[0.2.0]: https://github.com/sargisis/ReactX/compare/v0.1.0...v0.2.0
[0.1.0]: https://github.com/sargisis/ReactX/releases/tag/v0.1.0
