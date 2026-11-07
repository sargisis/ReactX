# reactx::calendar

Календарь месяца и выбор даты для приложений на [ReactX](https://github.com/sargisis/ReactX), в браузере и в терминале, на языке [`reactx::i18n`](../i18n/README.ru.md): названия месяцев и дней недели, первый день недели, формат даты.

In English: [README.md](README.md).

```cpp
#include "rx/calendar.hpp"
using namespace rx;

auto [due, setDue] = useState(calendar::Date{});           // даты пока нет
calendar::datePicker({.value = due, .onChange = [setDue](calendar::Date d) { setDue(d); },
                      .label = "Срок", .min = calendar::today()})

calendar::calendar({.value = day, .onChange = setDayFn,
                    .disabled = [](calendar::Date d) {      // без выходных
                        auto wd = std::chrono::weekday(std::chrono::sys_days(d));
                        return wd == std::chrono::Saturday || wd == std::chrono::Sunday;
                    }})
```

## Подключение

Идёт вместе с ReactX (с версии 1.5) и использует `reactx::i18n`: `target_link_libraries(my_app PRIVATE reactx::calendar)`.

## API

- `calendar::Date` — это `std::chrono::year_month_day`; недействительная дата (по умолчанию) значит «нет».
- `calendar::today()` — сегодня в часовом поясе пользователя.
- `calendar({.value, .onChange, .min, .max, .disabled, .lang})` — сетка месяца с ‹ ›; дни вне `min`/`max` или `disabled` выбрать нельзя. `.lang = ""` — текущий язык i18n.
- `datePicker({.value, .onChange, .label, .placeholder = "—", .min, .max, .disabled, .lang})` — поле с датой в формате i18n Medium («Nov 6, 2026», «6 нояб. 2026 г.»), открывающее календарь; выбор закрывает его.

Клавиатура: стрелки — день / неделя, PageUp / PageDown — месяц, Home / End — начало / конец недели, Enter — выбрать. В браузере весь месяц — одна остановка Tab (отмеченный день — `aria-activedescendant`); в терминале отмеченный день в скобках `[14]`, у сегодняшнего точка.

## Лицензия

MIT или Apache-2.0, как ReactX.
