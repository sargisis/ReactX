# reactx::forms

Проверка данных для приложений на [ReactX](https://github.com/sargisis/ReactX): правила и схемы для форм (с `ui::useForm`) и для JSON от клиента (на сервере — см. `reactx::api`). Одни и те же правила с обеих сторон.

In English: [README.md](README.md).

```cpp
#include "rx/forms.hpp"
using namespace rx;

struct Signup { std::string email, password, confirm; int age = 0; bool terms = false; };

const auto signup = forms::Schema<Signup>()
    .field("email", &Signup::email, {forms::required(), forms::email()})
    .field("password", &Signup::password, {forms::required(), forms::minLength(8)})
    .check("confirm", [](const Signup& s) { return s.confirm == s.password ? "" : "Пароли не совпадают"; })
    .field("age", &Signup::age, {forms::min(18)})
    .field("terms", &Signup::terms, {forms::accepted()});

auto form = ui::useForm<Signup>({.validate = signup.validator(), .onSubmit = send});
ui::input({.value = form.values.email, .onChange = form.set("email", &Signup::email),
           .label = "Email", .error = form.error("email")})
```

## Подключение

Библиотека идёт вместе с ReactX (с версии 1.5). Подключите её в `CMakeLists.txt` проекта:

```cmake
target_link_libraries(my_app PRIVATE reactx::forms)
```

## Правила

| Правило | Ошибка, когда |
|---|---|
| `required()` | текст пустой или из одних пробелов |
| `minLength(n)`, `maxLength(n)` | меньше / больше `n` символов (букв, не байтов: «Аня» — 3) |
| `email()` | не `имя@хост.домен` или есть пробелы |
| `url()` | не `http://` / `https://` с хостом |
| `digits()` | что-то кроме 0–9 |
| `oneOf({"a", "b"})` | не один из вариантов |
| `min(n)`, `max(n)` | число меньше / больше `n` (любой числовой тип) |
| `accepted()` | флажок не отмечен |
| `rule<V>(ok, "сообщение")` | своё: `forms::rule<std::string>([](auto& s) { return !s.contains(' '); }, "Без пробелов")` |

Текстовые правила, кроме `required()`, пропускают пустой текст — необязательное поле остаётся необязательным; добавьте `required()`, чтобы сделать его обязательным. Правила поля проверяются по порядку, первое сообщение — его ошибка. `.check(name, f)` видит всю структуру (два поля, которые должны совпасть); выполняется, только если у `name` ещё нет ошибки.

`schema.validate(value)` даёт `forms::Errors` (поле → сообщение, тот же тип, что `ui::FormErrors`), `schema.valid(value)` — bool, `schema.validator()` — функцию для `ui::useForm`.

### Сообщения

По умолчанию сообщения на английском; `{n}` — число из правила. Поменяйте их один раз, например в `main()`:

```cpp
auto& m = forms::messages();
m.required = "Обязательное поле";
m.minLength = "Не короче {n} символов";
```

Или задайте своё одному правилу: `forms::minLength(8, "Слишком короткий")`.

## JSON

```cpp
const auto newTodo = forms::JsonSchema()
    .text("title", {forms::required(), forms::maxLength(200)})
    .number("priority", {forms::min(1.0), forms::max(5.0)}, /*optional*/ true, /*whole*/ true)
    .boolean("done", {}, true);

auto errors = newTodo.validate(json::parse(body));   // {"title": "Required"}, не тот тип: "Must be text"
if (!errors.empty()) return Response::json(forms::JsonSchema::toJson(errors).dump(), 422);
```

Отсутствующее поле — ошибка, если оно не необязательное; поле не того типа так и называется («Must be a number»); не объект вообще — `{"": "Must be a JSON object"}`.

## Лицензия

MIT или Apache-2.0, как ReactX.
