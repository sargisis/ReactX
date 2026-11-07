// reactx::motion — animation for ReactX apps: things that appear and leave
// smoothly (presence), and numbers that move to a new value (useTween,
// useSpring). In the browser presence uses CSS transitions (and respects
// "reduce motion"); in a terminal it shows and hides at once, while tweens and
// springs step with the terminal's frames.
//
//   rx::motion::presence({.show = open, .kind = rx::motion::Kind::Slide, .children = {panel}})
//   double width = rx::motion::useTween(done ? 100.0 : 0.0, {.duration = std::chrono::milliseconds(400)});
//
// Comes with ReactX (library/motion); link it in CMakeLists.txt:
//   target_link_libraries(my_app PRIVATE reactx::motion)
#pragma once

#include <chrono>
#include <functional>
#include <string>
#include <vector>

#include "reactx/reactx.hpp"

namespace rx::motion {

// ---- easing: progress 0..1 -> eased 0..1 ----

using Easing = double (*)(double);
double linear(double t);
double easeIn(double t);
double easeOut(double t);    // fast, then gently stops: most UI motion
double easeInOut(double t);
double backOut(double t);    // overshoots a little, then settles

// ---- appearing and leaving ----

enum class Kind {
    Fade,      // opacity
    Slide,     // opacity + a short move up (from `distance` px below)
    Scale,     // opacity + growing from 96%
    Collapse,  // the height opens and closes (accordions, details)
};

struct PresenceProps {
    bool show = false;
    Kind kind = Kind::Fade;
    std::chrono::milliseconds duration{200};
    int distance = 8;  // Slide: px
    std::vector<Element> children{};
    std::string key{};
};
// The children while `show` is true, animating in when it turns true and out
// (still shown until the animation ends) when it turns false.
Element presence(PresenceProps props);

// ---- numbers that move ----

struct TweenOptions {
    std::chrono::milliseconds duration{300};
    Easing easing = easeOut;
};
// `target`, but a new target is reached over `duration` instead of at once.
// The component re-renders on every frame while it moves.
double useTween(double target, TweenOptions options = {});

struct SpringOptions {
    double stiffness = 170;  // higher: faster
    double damping = 26;     // lower: bouncier
    double precision = 0.01; // stops when this close and this slow
};
// `target` reached like a spring: it keeps its speed when the target changes
// mid-way (dragging, quick toggles), and may overshoot with low damping.
double useSpring(double target, SpringOptions options = {});

}  // namespace rx::motion
