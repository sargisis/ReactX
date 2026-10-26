Use the C++23/C++26 standard library (GCC 16, contracts, reflection) and typed props

## What

ReactX now uses everything the compiler's standard library ships from C++23/C++26 — and nothing hand-rolled or third-party in place of what it doesn't. Native builds need **GCC 16**; code shared with the web build (Emscripten libc++, which has no C++26 library yet) uses only what both have.

| Where | What |
|---|---|
| everywhere | `std::expected` (+ `and_then` / `transform_error`), `std::flat_map` (props, HTTP headers), `std::flat_set`, `std::format`, `std::print`, `std::from_chars` |
| terminal (GCC 16) | `std::inplace_vector<char, 4>` per screen cell, `std::mdspan` screen, `std::text_encoding` → Unicode or ASCII borders |
| CLI (GCC 16) | `std::generator` file watcher, `reactx.json` read as a typed `Manifest` with reflection |
| preconditions | C++26 contracts (`-fcontracts`): `REACTX_PRE` → `contract_assert`; else `<stacktrace>` + `<debugging>` where present |
| JSON (GCC 16) | C++26 reflection: `rx::json::to(struct)` / `rx::json::from<T>(json)` with typed errors (`field "dependencies": "fmt": missing field "rev"`) |

Not used because no toolchain has them yet: `<hive>`, `<simd>`, `<linalg>`, `<rcu>`, `<hazard_pointer>`.

### Typed props

A component can take its own props struct instead of `rx::Props`:

```cpp
struct CounterProps {
    int start = 0;
    bool operator==(const CounterProps&) const = default;  // only needed for memo
};

Element Counter(const CounterProps& p);

h(Counter, {.start = 5})   // .strat = 5 is a compile error
```

- Plain C++20 designated initializers, so it works natively and in the browser.
- A `key` field becomes the element key; a `children` field (`std::vector<Element>`) receives the children: `h(Card, {.title = "Hi"}, {...})`. Passing children to a component without that field doesn't compile.
- `memo<C>` on a typed component compares the structs with `operator==` (a `static_assert` says what to add if it's missing).
- `Props` components keep working and mix with typed ones. The examples (`counter`, `todo`, `todo_tui`, `web_counter`, `web_todo`) now use structs.

## Why

- `std::stoi` / `std::stoull` threw or misread input: `--port abc` crashed, `Content-Length: -5` was read as a huge number (now 400). `rx::parseNumber` / `rx::parsePort` return `std::expected`.
- `net::parseRequestHead` now says why a request is bad; `net::bodyLength` gives 400 / 413.
- A dependency without `rev` in `reactx.json` used to fail later inside CMake; the CLI now reports it right away.

## Notes for the reviewer

- **API changes (0.x, so MINOR):** `Props::values()` and `Request::headers` are `std::flat_map`; `Props::has/find/get` take `std::string_view`; `parseRequestHead` returns `std::expected<Request, HttpError>`.
- **GCC 16 module bugs (16.0.1 20260322, local):** a `std::inplace_vector` member in a class exported by `import reactx;` breaks reading the module, so the cell types live in `terminal_renderer.cpp` (`TerminalRenderer::Frame`). `-freflection` on any TU breaks `import reactx;`, so reflection is opt-in per target: `target_link_libraries(app PRIVATE reactx::reflection)` (used by the CLI and JSON tests). CI uses `gcc:16` (16.2), which may behave differently.
- `reactx` builds native projects with the compiler it was built with (or `CXX`), since the system `g++` may be older.
- **Typed props internals:** `Element` and the fiber carry a `shared_ptr<const detail::TypedProps>` (the struct + the function). Component identity is the function address (`std::bit_cast` to `uintptr_t`), so two components with same-shaped structs are still different types. Give every props field a default (`{}`), otherwise `-Wextra` (and CI's `-Werror`) flags omitted fields.
- **`reactx new` templates stay on `Props`** for now: new projects pin ReactX to the `v0.2.0` tag, which doesn't have typed props. Switch them when `v0.3.0` is tagged.
- Doubles in props now print in shortest exact form (`std::format`), e.g. `3.14159265` instead of `3.14159`.

## Tested locally

GCC 16.0.1:
- first commit: all tests (11) including the slow CLI end-to-end run, `-Werror`, ASan + UBSan + LeakSanitizer, ThreadSanitizer;
- typed props: all tests (11, with 8 new ones in `test_reconciler.cpp`), `-Werror` with ASan + UBSan + LeakSanitizer. ThreadSanitizer and the slow CLI end-to-end run were not repeated after this commit — CI covers them.

Emscripten 6.0.13: web tests (5) and the site build; `web_counter` and `web_todo` checked by hand in a browser (count, toggle, remove; no console errors).

🤖 Generated with [Claude Code](https://claude.com/claude-code)
