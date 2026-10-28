# Working on ReactX

Instructions for AI coding agents (and people) changing the ReactX framework itself. To write an *app* with ReactX, read [llms.txt](llms.txt) instead.

ReactX is a React-like UI framework in C++26: components, hooks and a reconciler (`src/reconciler.cpp`), with a terminal backend (ANSI + Yoga Flexbox) and a browser backend (WebAssembly + DOM), a router, `useFetch`, JSON, an HTTP server and the `reactx` CLI. The docs and comments in the code are the source of truth; user-facing docs come in English and Russian (README.md / README.ru.md, the site in `examples/site_docs_en.hpp` / `site_docs.hpp`); CHANGELOG.md is in Russian; code and comments in English.

## Toolchains

- Native: **GCC 16+** (`-DCMAKE_CXX_COMPILER=g++-16` if the default `g++` is older), CMake 3.28+, Ninja. CI uses the `gcc:16` image.
- Browser: **Emscripten 6+** (`source ~/emsdk/emsdk_env.sh`, `EMSDK` must be set).
- Yoga is fetched by CMake on the first configure.

```sh
cmake --preset debug -DCMAKE_CXX_COMPILER=g++-16 && cmake --build --preset debug
ctest --preset debug -LE slow                  # quick tests
./build/debug/reactx_cli_tests --e2e           # slow: builds real projects (web too if EMSDK is set)
cmake --preset web && cmake --build --preset web && ctest --test-dir build/web
cmake --preset pages && cmake --build --preset pages   # the website with base /ReactX/
```

## Before you say "done"

Run what CI runs; a green native build alone proved nothing in the past (v0.3.0 shipped a web build that didn't compile and a data race):

1. Native with `-DCMAKE_CXX_FLAGS="-Wall -Wextra -Wpedantic -Werror"`, all tests.
2. ThreadSanitizer: `-fsanitize=thread`, run with `setarch "$(uname -m)" -R ctest ... -LE slow`.
3. ASan + UBSan: `-fsanitize=address,undefined -fno-sanitize-recover=all`.
4. The web build and `ctest --test-dir build/web`.
5. For CLI or template changes: `reactx_cli_tests --e2e` with `EMSDK` set.
6. For anything visible in the browser: open it (`.claude/launch.json` has `web`, `site`, `app`, `pages` previews) and check it, at phone width too.
7. For changes to the core (`reconciler.cpp`, `hooks.cpp`): compare `reactx_bench` (a Release build) before and after; the randomized tests in `test_reconciler.cpp` compare against a fresh render and count backend moves.

## Rules

- **The standard library first.** Use what the compiler's library ships (`std::expected`, `std::format`/`std::print`, `std::ranges`, `std::flat_map`, `std::mdspan`, `std::inplace_vector`, contracts, reflection, ...). Never write a stand-in for a missing std header and never add a third-party "preview" library. If a facility is missing from a toolchain, code built by that toolchain doesn't use it (or uses it under its `__cpp_lib_*` macro with no fallback).
- **Two toolchains.** Everything in the web build (`src/element.cpp`, `hooks.cpp`, `reconciler.cpp`, `router.cpp`, `fetch.cpp`, `json.cpp`, `dom.cpp`, `ui_web.cpp`, all public headers they include) must compile with Emscripten's libc++: no reflection, `std::generator`, `<stacktrace>`, `std::inplace_vector`, `std::text_encoding` there. Native-only code: `terminal*.cpp`, `http.cpp`, `tools/`.
- **Errors.** Run-time failures (input, network, files) return `std::expected`; `REACTX_PRE(cond)` (`reactx/contract.hpp`, becomes a C++26 `contract_assert`) is for programmer errors only. Text to number: `rx::parseNumber` (`reactx/parse.hpp`), never `std::stoi`.
- **Threads.** Component state changes only on the UI thread. Timers go through `detail::setTimer` (browser `setTimeout`, natively `Root::flush()`), never a thread that calls a setter.
- **Props structs** (`h(Card, {.title = "x"})`) are the preferred component API; every field gets a default (`{}`) so designated initializers can skip fields under `-Wextra -Werror`.
- **No breaking changes without a reason**: add, don't rename. While MAJOR is 0, a breaking change means a MINOR bump and a CHANGELOG note.

## Known compiler traps (GCC 16.0.1)

- A `std::inplace_vector` data member in a class reachable from the `reactx` module (`modules/reactx.cppm`) breaks reading the module back ("Bad file data"). Keep such types in `.cpp` files (see `TerminalRenderer::Frame`).
- `-freflection` on any translation unit breaks `import reactx;`. Reflection is opt-in per target: `target_link_libraries(x PRIVATE reactx::reflection)`.
- GCC can't build the module with `-fsanitize=undefined` or `thread`; CMake turns the module off then.

## Layout

```
include/reactx/   public headers (the module re-exports them)
src/              implementation; utility_hooks.cpp = timers, window size, storage
modules/          reactx.cppm — `import reactx;`
tools/            reactx.cpp (the CLI, its project templates are string literals in it), serve.cpp
examples/         demos and the website (site.cpp + site_docs.hpp + web/site_shell.html)
tests/            one executable per area; test_cli.cpp runs the real CLI
web/              HTML shells for the web builds
```

## Changes that need more than code

- Public API or behaviour: README.md and README.ru.md, the docs pages in `examples/site_docs_en.hpp` and `site_docs.hpp`, [llms.txt](llms.txt), and `## [Не выпущено]` in CHANGELOG.md (sections Добавлено / Изменено / Исправлено).
- New source file: add it to **both** source lists in CMakeLists.txt (native and `if(EMSCRIPTEN)`) when the web build needs it.
- CLI templates (`kWebMain`, `kTerminalMain`, ...): new projects pin ReactX to the release tag of the CLI, so a template may only use API that exists in that release.

## Releases

Semantic Versioning, tags `vX.Y.Z`. The version lives only in `project(ReactX VERSION ...)`. A release is one commit "Release vX.Y.Z" (version, CHANGELOG section, README/docs mentions) plus an annotated tag on it; merge it with a merge commit, not a squash. Never move or rewrite a published tag — fix forward with the next version. Don't push or tag without the maintainer's OK.
