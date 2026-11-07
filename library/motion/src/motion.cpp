// reactx::motion: presence (CSS transitions in the browser) and values that
// move over time on rx::detail::setTimer (browser setTimeout, natively the
// terminal's frames). Time is measured, not counted in frames, so a slower
// terminal takes as long, in fewer steps.
#include "rx/motion.hpp"

#include <algorithm>
#include <cmath>
#include <memory>
#include <string>
#include <utility>
#include <chrono>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>

#include "reactx/dom.hpp"
#endif

namespace rx::motion {

namespace {

using Clock = std::chrono::steady_clock;
constexpr std::chrono::milliseconds kFrame{16};

// A function that runs every frame until it returns false or the cleanup is called.
Cleanup everyFrame(std::function<bool()> step) {
    struct Loop {
        bool alive = true;
        Cleanup timer;
        std::function<void()> tick;
    };
    auto loop = std::make_shared<Loop>();
    std::weak_ptr<Loop> weak = loop;
    loop->tick = [weak, step = std::move(step)] {
        auto l = weak.lock();
        if (!l || !l->alive) return;
        if (step() && l->alive) l->timer = rx::detail::setTimer(kFrame, l->tick);
    };
    loop->timer = rx::detail::setTimer(kFrame, loop->tick);
    return [loop] {
        loop->alive = false;
        if (loop->timer) loop->timer();
    };
}

#ifdef __EMSCRIPTEN__
constexpr const char* kCss = R"css(
.rx-motion { transition: opacity var(--rx-motion-duration, 200ms) ease-out,
  transform var(--rx-motion-duration, 200ms) cubic-bezier(.2, .8, .2, 1),
  grid-template-rows var(--rx-motion-duration, 200ms) ease-out; }
.rx-motion--from { opacity: 0; }
.rx-motion--slide.rx-motion--from { transform: translateY(var(--rx-motion-distance, 8px)); }
.rx-motion--scale.rx-motion--from { transform: scale(.96); }
.rx-motion--collapse { display: grid; grid-template-rows: 1fr; }
.rx-motion--collapse.rx-motion--from { grid-template-rows: 0fr; }
.rx-motion__inner { min-height: 0; overflow: hidden; }
@media (prefers-reduced-motion: reduce) { .rx-motion { transition-duration: 1ms; } }
)css";

// clang-format off  (JavaScript: a formatter would split === into == =)
EM_JS(int, rx_motion_install_styles, (const char* css), {
    if (typeof document === "undefined") return 0;
    if (document.getElementById("rx-motion-styles")) return 1;
    const style = document.createElement("style");
    style.id = "rx-motion-styles";
    style.textContent = UTF8ToString(css);
    document.head.prepend(style);
    return 1;
});
// clang-format on

void ensureStyles() {
    static bool done = false;
    if (done) return;
    done = true;
    if (!rx_motion_install_styles(kCss))
        rx::detail::addPrerenderHead(std::string("<style id=\"rx-motion-styles\">") + kCss + "</style>");
}

const char* kindClass(Kind k) {
    switch (k) {
        case Kind::Fade: return "rx-motion--fade";
        case Kind::Slide: return "rx-motion--slide";
        case Kind::Scale: return "rx-motion--scale";
        case Kind::Collapse: return "rx-motion--collapse";
    }
    return "";
}
#endif

Element Presence(const PresenceProps& p) {
    auto [mounted, setMounted] = useState(p.show);
    auto [from, setFrom] = useState(p.show);  // at the hidden style: entering starts there
    rx::useEffect(
        [show = p.show, ms = p.duration.count(), setMounted, setFrom]() -> Cleanup {
            if (show) {
                setMounted(true);
                setFrom(true);
                // The next frame: the browser has drawn the hidden style, so the change animates.
                return rx::detail::setTimer(std::chrono::milliseconds(20), [setFrom] { setFrom(false); });
            }
            setFrom(true);
            return rx::detail::setTimer(std::chrono::milliseconds(ms), [setMounted] { setMounted(false); });
        },
        deps(p.show));
#ifdef __EMSCRIPTEN__
    if (!mounted && !p.show) return fragment({});
    ensureStyles();
    std::string className = std::string("rx-motion ") + kindClass(p.kind) + (from ? " rx-motion--from" : "");
    std::string style = "--rx-motion-duration:" + std::to_string(p.duration.count()) + "ms;--rx-motion-distance:" +
                        std::to_string(p.distance) + "px";
    if (!p.show) className += " rx-motion--leaving";
    std::vector<Element> inner = p.children;
    if (p.kind == Kind::Collapse) inner = {h("div", {{"className", "rx-motion__inner"}}, std::move(inner))};
    return h("div", {{"className", className}, {"style", style}, {"aria-hidden", p.show ? "false" : "true"}}, std::move(inner));
#else
    (void)mounted;
    (void)from;
    if (!p.show) return fragment({});  // a terminal has no transitions: gone at once
    return h("box", {}, p.children);
#endif
}

}  // namespace

double linear(double t) { return t; }
double easeIn(double t) { return t * t * t; }
double easeOut(double t) {
    const double u = 1 - t;
    return 1 - u * u * u;
}
double easeInOut(double t) { return t < 0.5 ? 4 * t * t * t : 1 - std::pow(-2 * t + 2, 3) / 2; }
double backOut(double t) {
    constexpr double c1 = 1.70158, c3 = c1 + 1;
    const double u = t - 1;
    return 1 + c3 * u * u * u + c1 * u * u;
}

Element presence(PresenceProps props) {
    std::string key = std::move(props.key);
    Element e = h(Presence, std::move(props));
    if (!key.empty()) e.key = std::move(key);
    return e;
}

double useTween(double target, TweenOptions options) {
    auto [value, setValue] = useState(target);
    rx::useEffect(
        [target, from = value, duration = options.duration, easing = options.easing, setValue]() -> Cleanup {
            if (from == target || duration.count() <= 0) {
                setValue(target);
                return {};
            }
            const auto start = Clock::now();
            return everyFrame([=] {
                const double t = std::min(1.0, std::chrono::duration<double, std::milli>(Clock::now() - start).count() /
                                                   static_cast<double>(duration.count()));
                setValue(t >= 1 ? target : from + (target - from) * (easing ? easing(t) : t));
                return t < 1;
            });
        },
        deps(target));
    return value;
}

double useSpring(double target, SpringOptions options) {
    struct State {
        double x = 0, v = 0;
        Clock::time_point last{};
    };
    auto [value, setValue] = useState(target);
    auto& state = useRef(std::make_shared<State>(State{target, 0, {}}));
    rx::useEffect(
        [target, s = state, o = options, setValue]() -> Cleanup {
            s->last = Clock::now();
            return everyFrame([=] {
                const auto now = Clock::now();
                double dt = std::chrono::duration<double>(now - s->last).count();
                s->last = now;
                dt = std::min(dt, 0.1);  // after a pause, don't jump
                // Small steps keep the integration stable at low frame rates.
                for (double left = dt; left > 0; left -= 0.004) {
                    const double h = std::min(left, 0.004);
                    const double a = -o.stiffness * (s->x - target) - o.damping * s->v;
                    s->v += a * h;
                    s->x += s->v * h;
                }
                const bool settled = std::fabs(s->x - target) < o.precision && std::fabs(s->v) < o.precision;
                if (settled) {
                    s->x = target;
                    s->v = 0;
                }
                setValue(s->x);
                return !settled;
            });
        },
        deps(target));
    return value;
}

}  // namespace rx::motion
