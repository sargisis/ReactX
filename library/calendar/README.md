# reactx::calendar

A month calendar and a date picker for [ReactX](https://github.com/sargisis/ReactX) apps, in the browser and in a terminal, in the language of [`reactx::i18n`](../i18n/README.md): month and weekday names, the first day of the week, the date format.

На русском: [README.ru.md](README.ru.md).

```cpp
#include "rx/calendar.hpp"
using namespace rx;

auto [due, setDue] = useState(calendar::Date{});           // no date yet
calendar::datePicker({.value = due, .onChange = [setDue](calendar::Date d) { setDue(d); },
                      .label = "Due", .min = calendar::today()})

calendar::calendar({.value = day, .onChange = setDayFn,
                    .disabled = [](calendar::Date d) {      // no weekends
                        auto wd = std::chrono::weekday(std::chrono::sys_days(d));
                        return wd == std::chrono::Saturday || wd == std::chrono::Sunday;
                    }})
```

## Add it to a project

It comes with ReactX (since 1.5) and uses `reactx::i18n`: `target_link_libraries(my_app PRIVATE reactx::calendar)`.

## API

- `calendar::Date` is `std::chrono::year_month_day`; an invalid one (the default) means "none".
- `calendar::today()` — today in the user's time zone.
- `calendar({.value, .onChange, .min, .max, .disabled, .lang})` — a month grid with ‹ ›; days outside `min`/`max` or `disabled` can't be picked. `.lang = ""` uses i18n's current language.
- `datePicker({.value, .onChange, .label, .placeholder = "—", .min, .max, .disabled, .lang})` — a field with the date in i18n's Medium format ("Nov 6, 2026", "6 нояб. 2026 г.") that opens the calendar; picking closes it.

Keyboard: arrows move a day / a week, PageUp / PageDown a month, Home / End the start / end of the week, Enter picks. In the browser the whole month is one Tab stop (the marked day is `aria-activedescendant`); in a terminal the marked day is in brackets `[14]`, today has a dot.

## License

MIT or Apache-2.0, like ReactX.
