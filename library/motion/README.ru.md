# reactx::motion

Анимации для приложений на [ReactX](https://github.com/sargisis/ReactX): плавное появление и исчезновение и числа, которые плавно идут к новому значению. В браузере `presence` использует CSS-переходы и учитывает «уменьшение движения»; в терминале показывает и скрывает сразу, а tween и spring шагают с кадрами терминала.

In English: [README.md](README.md).

```cpp
#include "rx/motion.hpp"
using namespace rx;

motion::presence({.show = open, .kind = motion::Kind::Slide, .children = {panel}})

double width = motion::useTween(done ? 100.0 : 0.0, {.duration = std::chrono::milliseconds(400)});
h("div", {{"className", "bar"}, {"style", "width:" + std::to_string(int(width)) + "%"}})

double x = motion::useSpring(dragging ? 1.0 : 0.0, {.stiffness = 220, .damping = 14});
```

## Подключение

Идёт вместе с ReactX (с версии 1.5): `target_link_libraries(my_app PRIVATE reactx::motion)`.

## presence

`presence({.show, .kind, .duration = 200ms, .distance = 8, .children})` показывает детей, пока `show` истинно; они анимируются при появлении и при исчезновении (видны до конца анимации). Виды: `Fade`, `Slide` (снизу на `distance` px), `Scale` (с 96%), `Collapse` (высота открывается и закрывается — аккордеоны).

## Значения

- `useTween(target, {.duration = 300ms, .easing = motion::easeOut})` — новая цель достигается за `duration`.
- `useSpring(target, {.stiffness = 170, .damping = 26, .precision = 0.01})` — как пружина: сохраняет скорость, если цель меняется на полпути; меньше damping — больше отскок.
- Сглаживание: `linear`, `easeIn`, `easeOut`, `easeInOut`, `backOut` или любая `double(double)`.

Пока значение движется, компонент перерисовывается каждый кадр. Время измеряется, а не считается кадрами, поэтому в терминале (кадров меньше) анимация длится столько же.

## Лицензия

MIT или Apache-2.0, как ReactX.
