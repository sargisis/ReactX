# reactx::log

Structured logs for [ReactX](https://github.com/sargisis/ReactX) servers and tools: a level, a message and `key=value` fields — text for people or JSON lines for log collectors (Loki, Elastic, CloudWatch). Safe from any thread.

На русском: [README.ru.md](README.ru.md).

```cpp
#include "rx/log.hpp"

rx::log::info("signed in", {{"user", 42}, {"ip", request.remote}});
// 2026-11-06T14:05:07Z INFO  signed in user=42 ip=10.0.0.7

rx::log::setup({.level = rx::log::levelFromEnv(), .json = true});
rx::log::warn("disk almost full", {{"free_gb", 3.5}});
// {"free_gb":3.5,"level":"warn","msg":"disk almost full","time":"2026-11-06T14:05:07Z"}
```

## Add it to a project

It comes with ReactX (since 1.5): `target_link_libraries(my_server PRIVATE reactx::log)`.

## API

| | |
|---|---|
| `debug / info / warn / error(message, {{"key", value}, ...})` | a value is text, a number or a bool |
| `setup({.level, .json, .time, .sink})` | `.level` drops lower ones (Info by default), `.json` one object per line, `.time` the UTC time first, `.sink` your own output (default stderr) |
| `levelFromEnv(fallback)` | the level `LOG_LEVEL=debug/info/warn/error/off` names |
| `enabled(level)` | skip building costly fields when the level is off |

Text values with spaces or quotes are quoted (`note="two words"`). To log every API request, see `api::log(sink)` in [`reactx::api`](../api/README.md): `api.use(rx::api::log([](const std::string& l) { rx::log::info(l); }))`.

## License

MIT or Apache-2.0, like ReactX.
