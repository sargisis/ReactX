# reactx::motion

Animation for [ReactX](https://github.com/sargisis/ReactX) apps: things that appear and leave smoothly, and numbers that move to a new value. In the browser `presence` uses CSS transitions and respects "reduce motion"; in a terminal it shows and hides at once, while tweens and springs step with the terminal's frames.

На русском: [README.ru.md](README.ru.md).

```cpp
#include "rx/motion.hpp"
using namespace rx;

motion::presence({.show = open, .kind = motion::Kind::Slide, .children = {panel}})

double width = motion::useTween(done ? 100.0 : 0.0, {.duration = std::chrono::milliseconds(400)});
h("div", {{"className", "bar"}, {"style", "width:" + std::to_string(int(width)) + "%"}})

double x = motion::useSpring(dragging ? 1.0 : 0.0, {.stiffness = 220, .damping = 14});
```

## Add it to a project

It comes with ReactX (since 1.5): `target_link_libraries(my_app PRIVATE reactx::motion)`.

## presence

`presence({.show, .kind, .duration = 200ms, .distance = 8, .children})` shows the children while `show` is true; they animate in when it turns true and out (still shown until the end) when it turns false. Kinds: `Fade`, `Slide` (from `distance` px below), `Scale` (from 96%), `Collapse` (the height opens and closes — accordions).

## Values

- `useTween(target, {.duration = 300ms, .easing = motion::easeOut})` — a new target is reached over `duration`.
- `useSpring(target, {.stiffness = 170, .damping = 26, .precision = 0.01})` — like a spring: it keeps its speed when the target changes mid-way, and lower damping bounces.
- Easings: `linear`, `easeIn`, `easeOut`, `easeInOut`, `backOut`, or any `double(double)`.

The component re-renders on each frame while a value moves. Time is measured, not counted in frames, so a terminal (fewer frames) takes as long.

## License

MIT or Apache-2.0, like ReactX.
