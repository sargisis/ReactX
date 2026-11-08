# reactx::jobs

Background work for [ReactX](https://github.com/sargisis/ReactX) servers: send the email after the response went out, retry a webhook that failed, clean up every night. Jobs run on worker threads; a job that throws is tried again later.

На русском: [README.ru.md](README.ru.md).

```cpp
#include "rx/jobs.hpp"
using namespace rx;
using namespace std::chrono_literals;

jobs::Queue jobs({
    .workers = 4,
    .onFailed = [](const jobs::Failure& f) { std::println(stderr, "{} failed: {}", f.name, f.error); },
});

jobs.add("welcome email", [to] { sendMail(to, "Welcome!"); });                 // now; 3 tries
jobs.add("webhook", [=] { post(url, body); }, {.attempts = 5, .delay = 2s});   // 2 s, 4 s, 8 s, 16 s
jobs.after(10min, "reminder", [] { remind(); });
auto health = jobs.every(30s, "health check", [] { check(); });
jobs.cron("0 3 * * *", "nightly cleanup", [] { cleanup(); });                   // 03:00 local time
jobs.cancel(health);
```

A job fails by throwing (any exception; `std::exception::what()` is the reason). One-off jobs (`add`, `after`) are tried `attempts` times with a growing delay (`delay`, ×`backoff`, at most `maxDelay`), then `onFailed` is called. A repeating job (`every`, `cron`) that fails is reported and still runs next time.

## Add it to a project

It comes with ReactX (since 1.7), for native builds (a server). Link it in the project's `CMakeLists.txt`:

```cmake
target_link_libraries(my_server PRIVATE reactx::jobs)
```

## The queue

| Call | |
|---|---|
| `add(name, work, retry)` | as soon as a worker is free |
| `after(delay, name, work, retry)` | once, after `delay` |
| `every(period, name, work)` | every `period` (first after one period) |
| `cron(spec, name, work)` | on a schedule; `std::expected` (a spec it can't read is an error) |
| `cancel(id)` | `false` if it is gone already; a running job finishes |
| `stats()` | `waiting`, `running`, `done`, `failed` |
| `wait()` | until every one-off job (with its retries) has finished |
| `stop()` | running jobs finish, waiting ones are dropped (the destructor does it too) |

`Options`: `workers` (0 = one per CPU) and `onFailed`. `Retry`: `attempts` (3), `delay` (1 s), `backoff` (2), `maxDelay` (10 min).

## Cron

`"minute hour day month weekday"` in local time; each field is a number, `*`, a list `1,15`, a range `1-5` or a step `*/10` (`5/15` = from 5, every 15). Sunday is 0 or 7. When both the day and the weekday are set, either one matches (like cron).

| Spec | |
|---|---|
| `0 3 * * *` | 03:00 every day |
| `*/5 9-18 * * 1-5` | every 5 minutes, 9:00–18:55, Monday to Friday |
| `0 0 1 * *` | midnight on the 1st of each month |
| `30 12 * * 0` | Sundays at 12:30 |

`rx::jobs::nextCron(spec, from)` gives the next time a spec matches.

Jobs run on worker threads, not the UI thread: guard shared data with a mutex, and don't call a component's setter from a job. With `reactx::db` a job can read and write the store directly (it is thread-safe).
