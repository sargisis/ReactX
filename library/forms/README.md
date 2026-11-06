# reactx::forms

Validation for [ReactX](https://github.com/sargisis/ReactX) apps: rules and schemas for forms (with `ui::useForm`) and for JSON from a client (on a server, see `reactx::api`). The same rules on both sides.

На русском: [README.ru.md](README.ru.md).

```cpp
#include "rx/forms.hpp"
using namespace rx;

struct Signup { std::string email, password, confirm; int age = 0; bool terms = false; };

const auto signup = forms::Schema<Signup>()
    .field("email", &Signup::email, {forms::required(), forms::email()})
    .field("password", &Signup::password, {forms::required(), forms::minLength(8)})
    .check("confirm", [](const Signup& s) { return s.confirm == s.password ? "" : "The passwords differ"; })
    .field("age", &Signup::age, {forms::min(18)})
    .field("terms", &Signup::terms, {forms::accepted()});

auto form = ui::useForm<Signup>({.validate = signup.validator(), .onSubmit = send});
ui::input({.value = form.values.email, .onChange = form.set("email", &Signup::email),
           .label = "Email", .error = form.error("email")})
```

## Add it to a project

It comes with ReactX (since 1.5). Link it in the project's `CMakeLists.txt`:

```cmake
target_link_libraries(my_app PRIVATE reactx::forms)
```

## Rules

| Rule | Fails when |
|---|---|
| `required()` | the text is empty or only spaces |
| `minLength(n)`, `maxLength(n)` | fewer / more than `n` characters (letters, not bytes: "Аня" is 3) |
| `email()` | not `name@host.domain`, or has spaces |
| `url()` | not `http://` / `https://` with a host |
| `digits()` | anything but 0–9 |
| `oneOf({"a", "b"})` | not one of the choices |
| `min(n)`, `max(n)` | a number below / above `n` (any number type) |
| `accepted()` | a checkbox that isn't ticked |
| `rule<V>(ok, "message")` | your own: `forms::rule<std::string>([](auto& s) { return !s.contains(' '); }, "No spaces")` |

Text rules other than `required()` pass empty text, so an optional field stays optional; add `required()` to make it mandatory. The rules of a field run in order and the first message is its error. `.check(name, f)` sees the whole struct (two fields that must match); it runs only when `name` has no error yet.

`schema.validate(value)` gives `forms::Errors` (field → message, the same type as `ui::FormErrors`), `schema.valid(value)` a bool, `schema.validator()` the function for `ui::useForm`.

### Messages

The default messages are English; `{n}` is the rule's number. Change them once, e.g. in `main()`:

```cpp
auto& m = forms::messages();
m.required = "Обязательное поле";
m.minLength = "Не короче {n} символов";
```

Or give one rule its own: `forms::minLength(8, "Too short")`.

## JSON

```cpp
const auto newTodo = forms::JsonSchema()
    .text("title", {forms::required(), forms::maxLength(200)})
    .number("priority", {forms::min(1.0), forms::max(5.0)}, /*optional*/ true, /*whole*/ true)
    .boolean("done", {}, true);

auto errors = newTodo.validate(json::parse(body));   // {"title": "Required"}, a wrong type: "Must be text"
if (!errors.empty()) return Response::json(forms::JsonSchema::toJson(errors).dump(), 422);
```

A missing field is an error unless it is optional; a field of the wrong type says so ("Must be a number"); not an object at all gives `{"": "Must be a JSON object"}`.

## License

MIT or Apache-2.0, like ReactX.
