# reactx::i18n

Переводы, множественное число, числа и даты для приложений на [ReactX](https://github.com/sargisis/ReactX) (C++26) — в браузере и в терминале, без ICU и без `std::format` (он добавил бы к `.wasm` ~100 КБ).

In English: [README.md](README.md).

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
    // дальше приложение монтируется как обычно
}

Element Cart(const CartProps& p) {
    auto t = i18n::useTranslation();  // перерисовка при смене языка
    return ui::stack({.children = {
        ui::text({.text = t("hello", {{"name", p.user}})}),
        ui::text({.text = t("cart.items", {{"n", p.count}})}),         // "3 товара"
        ui::text({.text = t.currency(p.total, "EUR")}),                // "1 234,50 €"
        i18n::languagePicker({.label = t("language")}),
    }});
}
```

## Подключение

Библиотека идёт вместе с ReactX (с версии 1.2). Подключите её в `CMakeLists.txt` проекта:

```cmake
target_link_libraries(my_app PRIVATE reactx::i18n)
```

Собираются только подключённые библиотеки, а неиспользуемая ничего не добавляет к `.wasm`.

## Языки

`setup()` вызывается один раз в `main()`. Первый язык выбирается так: `.initial`, сохранённый выбор (`localStorage`, ключ `.storageKey = "lang"`), язык системы (`navigator.language` в браузере, `LANG` в терминале), если он есть в списке, иначе `.fallback` (по умолчанию первый язык). `"ru-RU"` находит `"ru"`.

```cpp
i18n::setLanguage("ru");   // все компоненты с useTranslation() перерисуются; выбор сохраняется
i18n::language();          // "ru"
i18n::languages();         // {code, name} для списка
i18n::languagePicker({.label = "Язык"})  // готовый ui::select
```

В браузере `<html lang>` следует за языком (и `dir="rtl"` для арабского, иврита, фарси, урду).

Сообщение, которого нет в текущем языке, берётся из запасного языка, а если нет и там — показывается сам ключ. `.onMissing = [](auto lang, auto key) { ... }` сообщает о каждом пропущенном ключе один раз.

## Сообщения

Подмножество ICU MessageFormat:

| | |
|---|---|
| `{name}` | аргумент; числа выводятся в формате языка (`1 234,5`) |
| `{n, plural, =0 {нет} one {# файл} few {# файла} other {# файлов}}` | `#` — само число; сначала `=N`, потом категория, потом `other` |
| `{role, select, admin {Админ} other {Пользователь}}` | текстовые аргументы |
| `'{'` `''` | фигурная скобка, апостроф |

Категории — по CLDR: в английском `one` и `other`; в русском и украинском `one` (1, 21), `few` (2–4, 22–24), `many` (5–20, 25), `other` (дроби) — если `many` не написать, используется `other`. `i18n::pluralCategory(n, "ru")` скажет категорию.

Аргументы: `{{"name", user}, {"n", 3}}` — текст или любое число.

`i18n::t(key, args)` работает и вне компонентов (в обработчике, в toast), `i18n::format(message, args)` форматирует готовую строку.

### Сообщения в JSON-файлах

Переводы можно не держать в `.wasm`: положите `public/locales/ru.json` рядом с приложением и загружайте по необходимости.

```json
{ "cart": { "items": "{n, plural, one {# товар} few {# товара} other {# товаров}}" } }
```

```cpp
useEffect([lang] { return i18n::loadMessages(lang, "/locales/" + lang + ".json"); }, deps(lang));
```

`i18n::parseMessages(json)` превращает вложенные объекты в ключи через точку (`cart.items`) и возвращает `std::expected<Messages, std::string>`; `i18n::addMessages(lang, messages)` добавляет их вручную.

## Числа, деньги, даты

Последний параметр каждой функции — язык; `""` (по умолчанию) — текущий. У `Translator` (`useTranslation()`) те же методы: `t.number`, `t.percent`, `t.currency`, `t.date`, `t.time`, `t.relative`.

```cpp
using namespace std::chrono_literals;  // 2026y, 5min, 49h

i18n::formatNumber(1234567.891)                     // en "1,234,567.891"  ru "1 234 567,891"  de "1.234.567,891"
i18n::formatNumber(2.5, {.minDecimals = 2})         // "2,50"
i18n::formatPercent(0.256)                          // en "26%"  ru "26 %"
i18n::formatCurrency(-1234.5, "EUR")                // en "-€1,234.50"  ru "-1 234,50 €"
i18n::formatDate(2026y / 11 / 6, i18n::DateStyle::Long)  // "November 6, 2026"  "6 ноября 2026 г."
i18n::formatTime(14, 5)                             // en "2:05 PM"  ru "14:05"
i18n::formatRelative(-5min)                         // "5 minutes ago"  "5 минут назад"
i18n::formatRelative(49h)                           // "in 2 days"  "через 2 дня"
i18n::monthName(11)  i18n::weekdayName(5, true)     // "ноябрь"  "пт"
```

| `DateStyle` | en | ru |
|---|---|---|
| `Short` | 11/6/2026 | 06.11.2026 |
| `Medium` | Nov 6, 2026 | 6 нояб. 2026 г. |
| `Long` | November 6, 2026 | 6 ноября 2026 г. |
| `Full` | Friday, November 6, 2026 | пятница, 6 ноября 2026 г. |

Символы валют: USD EUR GBP RUB UAH AMD PLN JPY CNY INR KRW (BRL как R$ в португальском); остальные показываются кодом (`CHF 5.00`).

## Встроенные данные языков

`en`, `ru`, `uk`, `hy`, `de`, `fr`, `es`, `it`, `pt` (Бразилия), `pl`: разделители, шаблоны дат, названия месяцев и дней недели, 12/24-часовое время, первый день недели, относительное время. Правила множественного числа есть для них и ещё для `be`, `cs`, `sk`, `ar`, `ja`, `zh`, `ko`, `vi`, `th`, `id`. Остальные языки получают английские форматы; для них можно задать свой `Locale`:

```cpp
static const i18n::Locale dutch{.decimal = ",", .group = ".", .currencyAfter = false, .currencyGap = " ",
                                .dateShort = "{dd}-{MM}-{y}", /* месяцы, дни недели, ... */};
i18n::setup({.languages = {{.code = "nl", .name = "Nederlands", .messages = nl, .locale = &dutch}}});
```

`i18n::locale("ru")` возвращает встроенный — его можно скопировать и поменять.

## Лицензия

MIT или Apache-2.0, как ReactX.
