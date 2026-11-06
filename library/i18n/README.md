# reactx::i18n

Translations, plurals, numbers and dates for [ReactX](https://github.com/sargisis/ReactX) apps — in the browser and in a terminal, with no ICU and no `std::format` (it would add ~100 KB to a `.wasm`).

На русском: [README.ru.md](README.ru.md).

```cpp
#include "rx/i18n.hpp"
using namespace rx;

int main() {
    i18n::setup({.languages = {
        {.code = "en", .name = "English", .messages = {
            {"cart.items", "{n, plural, =0 {Your cart is empty} one {# item} other {# items}}"},
            {"hello", "Hello, {name}!"}}},
        {.code = "ru", .name = "Русский", .messages = {
            {"cart.items", "{n, plural, =0 {Корзина пуста} one {# товар} few {# товара} other {# товаров}}"},
            {"hello", "Привет, {name}!"}}},
    }});
    // mount the app as usual
}

Element Cart(const CartProps& p) {
    auto t = i18n::useTranslation();  // re-renders when the language changes
    return ui::stack({.children = {
        ui::text({.text = t("hello", {{"name", p.user}})}),
        ui::text({.text = t("cart.items", {{"n", p.count}})}),         // "3 товара"
        ui::text({.text = t.currency(p.total, "EUR")}),                // "1 234,50 €"
        i18n::languagePicker({.label = t("language")}),
    }});
}
```

## Add it to a project

It comes with ReactX (since 1.2). Link it in the project's `CMakeLists.txt`:

```cmake
target_link_libraries(my_app PRIVATE reactx::i18n)
```

Only linked libraries are compiled, and an unused one adds nothing to a `.wasm`.

## Languages

`setup()` takes the languages, once, in `main()`. The first language is chosen in this order: `.initial`, the saved choice (`localStorage`, key `.storageKey = "lang"`), the system language (`navigator.language` in the browser, `LANG` in a terminal) if it is listed, else `.fallback` (the first language). `"ru-RU"` finds `"ru"`.

```cpp
i18n::setLanguage("ru");   // every component with useTranslation() re-renders; the choice is saved
i18n::language();          // "ru"
i18n::languages();         // {code, name} for a picker
i18n::languagePicker({.label = "Language"})  // a ready ui::select
```

In the browser `<html lang>` follows the language (and `dir="rtl"` for Arabic, Hebrew, Persian, Urdu).

A message missing in the current language comes from the fallback language, then the key itself is shown. `.onMissing = [](auto lang, auto key) { ... }` hears about each missing key once.

## Messages

A subset of ICU MessageFormat:

| | |
|---|---|
| `{name}` | an argument; numbers come out in the language's format (`1 234,5`) |
| `{n, plural, =0 {none} one {# file} few {# files} other {# files}}` | `#` is the number; `=N` first, then the plural category, then `other` |
| `{role, select, admin {Admin} other {User}}` | text arguments |
| `'{'` `''` | a literal brace, a literal apostrophe |

Plural categories follow CLDR: English has `one` and `other`; Russian and Ukrainian `one` (1, 21), `few` (2–4, 22–24), `many` (5–20, 25), `other` (fractions) — leave out `many` and `other` is used. `i18n::pluralCategory(n, "ru")` tells you the category.

Arguments: `{{"name", user}, {"n", 3}}` — text or any number.

`i18n::t(key, args)` works outside components (in a handler, a toast), and `i18n::format(message, args)` formats a message you already have.

### Messages in JSON files

Keep the translations out of the `.wasm`: put `public/locales/ru.json` next to the app and load it when needed.

```json
{ "cart": { "items": "{n, plural, one {# товар} few {# товара} other {# товаров}}" } }
```

```cpp
useEffect([lang] { return i18n::loadMessages(lang, "/locales/" + lang + ".json"); }, deps(lang));
```

`i18n::parseMessages(json)` turns nested objects into dotted keys (`cart.items`) and returns `std::expected<Messages, std::string>`; `i18n::addMessages(lang, messages)` adds them by hand.

## Numbers, money, dates

Every function takes the language last; `""` (the default) is the current one. A `Translator` (`useTranslation()`) has the same as methods: `t.number`, `t.percent`, `t.currency`, `t.date`, `t.time`, `t.relative`.

```cpp
using namespace std::chrono_literals;  // 2026y, 5min, 49h

i18n::formatNumber(1234567.891)                     // en "1,234,567.891"  ru "1 234 567,891"  de "1.234.567,891"
i18n::formatNumber(2.5, {.minDecimals = 2})         // "2.50"
i18n::formatPercent(0.256)                          // en "26%"  ru "26 %"
i18n::formatCurrency(-1234.5, "EUR")                // en "-€1,234.50"  ru "-1 234,50 €"
i18n::formatDate(2026y / 11 / 6, i18n::DateStyle::Long)  // "November 6, 2026"  "6 ноября 2026 г."
i18n::formatTime(14, 5)                             // en "2:05 PM"  ru "14:05"
i18n::formatRelative(-5min)                         // "5 minutes ago"  "5 минут назад"
i18n::formatRelative(49h)                           // "in 2 days"  "через 2 дня"
i18n::monthName(11)  i18n::weekdayName(5, true)     // "November"  "Fri"
```

| `DateStyle` | en | ru |
|---|---|---|
| `Short` | 11/6/2026 | 06.11.2026 |
| `Medium` | Nov 6, 2026 | 6 нояб. 2026 г. |
| `Long` | November 6, 2026 | 6 ноября 2026 г. |
| `Full` | Friday, November 6, 2026 | пятница, 6 ноября 2026 г. |

Currency symbols: USD EUR GBP RUB UAH AMD PLN JPY CNY INR KRW (BRL as R$ in Portuguese); other codes are shown as the code (`CHF 5.00`).

## Built-in locale data

`en`, `ru`, `uk`, `hy`, `de`, `fr`, `es`, `it`, `pt` (Brazil), `pl`: separators, date patterns, month and weekday names, 12/24-hour time, the first day of the week, relative time. Plural rules cover these and also `be`, `cs`, `sk`, `ar`, `ja`, `zh`, `ko`, `vi`, `th`, `id`. Any other language gets English formats; give it a `Locale` of your own:

```cpp
static const i18n::Locale dutch{.decimal = ",", .group = ".", .currencyAfter = false, .currencyGap = " ",
                                .dateShort = "{dd}-{MM}-{y}", /* months, weekdays, ... */};
i18n::setup({.languages = {{.code = "nl", .name = "Nederlands", .messages = nl, .locale = &dutch}}});
```

`i18n::locale("ru")` gives a built-in one, to copy and change.

## License

MIT or Apache-2.0, like ReactX.
