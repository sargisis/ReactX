# reactx::log

Структурные логи для серверов и утилит на [ReactX](https://github.com/sargisis/ReactX): уровень, сообщение и поля `key=value` — текст для людей или строки JSON для систем сбора логов (Loki, Elastic, CloudWatch). Безопасно из любого потока.

In English: [README.md](README.md).

```cpp
#include "rx/log.hpp"

rx::log::info("signed in", {{"user", 42}, {"ip", request.remote}});
// 2026-11-06T14:05:07Z INFO  signed in user=42 ip=10.0.0.7

rx::log::setup({.level = rx::log::levelFromEnv(), .json = true});
rx::log::warn("disk almost full", {{"free_gb", 3.5}});
// {"free_gb":3.5,"level":"warn","msg":"disk almost full","time":"2026-11-06T14:05:07Z"}
```

## Подключение

Идёт вместе с ReactX (с версии 1.5): `target_link_libraries(my_server PRIVATE reactx::log)`.

## API

| | |
|---|---|
| `debug / info / warn / error(сообщение, {{"key", значение}, ...})` | значение — текст, число или bool |
| `setup({.level, .json, .time, .sink})` | `.level` отбрасывает уровни ниже (по умолчанию Info), `.json` — объект на строку, `.time` — время UTC первым, `.sink` — свой вывод (по умолчанию stderr) |
| `levelFromEnv(fallback)` | уровень из `LOG_LEVEL=debug/info/warn/error/off` |
| `enabled(level)` | не собирать дорогие поля, когда уровень выключен |

Текст с пробелами или кавычками берётся в кавычки (`note="two words"`). Чтобы логировать каждый запрос API, см. `api::log(sink)` в [`reactx::api`](../api/README.ru.md): `api.use(rx::api::log([](const std::string& l) { rx::log::info(l); }))`.

## Лицензия

MIT или Apache-2.0, как ReactX.
