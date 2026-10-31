# Changelog

All notable changes to ReactX. Versions follow [Semantic Versioning](https://semver.org): `MAJOR.MINOR.PATCH`, git tags are `vMAJOR.MINOR.PATCH`. While `MAJOR = 0` the API may change: incompatible changes bump `MINOR`, fixes bump `PATCH`.

На русском: [CHANGELOG.ru.md](CHANGELOG.ru.md).

## [Unreleased]

### Added

- **`rx::renderToString(element)`** (`reactx/html.hpp`) — the HTML of components without a browser, to prerender a page at build time or serve it from C++. Props become attributes the way the DOM backend sets them (`className` → `class`, `true` → a present attribute, the `value` of a `<textarea>` / `<select>`), text is escaped, `javascript:` URLs are blocked; effects don't run, like in React. `HtmlRenderer` is the backend behind it.
- **The site's home page is prerendered**: `index.html` holds its HTML in English and Russian, so the page shows before `index.wasm` arrives (on a throttled phone LCP went from ~1.2 s to ~0.3 s, the same as the first paint).

### Changed

- `mountDom` replaces what the container showed (a loading text, prerendered HTML) instead of adding the app after it.
- The router works without a page (under Node): the path is `/` until `reset` or `setBase` say otherwise.

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

[Unreleased]: https://github.com/sargisis/ReactX/compare/v0.8.2...HEAD
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
