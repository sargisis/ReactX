#pragma once

#include <chrono>
#include <cstdint>
#include <format>
#include <concepts>
#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <stdexcept>
#include <tuple>
#include <type_traits>
#include <utility>

#include "reactx/element.hpp"
#include "reactx/parse.hpp"

// The value returned by useMemo lives in the hook, not in the temporaries
// passed to it; tell GCC so it doesn't warn about a dangling reference.
#if defined(__has_cpp_attribute)
#if __has_cpp_attribute(gnu::no_dangling)
#define REACTX_NO_DANGLING [[gnu::no_dangling]]
#endif
#endif
#ifndef REACTX_NO_DANGLING
#define REACTX_NO_DANGLING
#endif

namespace rx {

using Cleanup = std::function<void()>;

namespace detail {

struct Fiber;

struct HookBase {
    virtual ~HookBase() = default;
};

struct HookSlot {
    HookBase* hook;
    std::weak_ptr<Fiber> fiber;
    std::size_t index;
};

// Hook at the current position of the component being rendered.
// `make` creates it on the first render.
HookSlot nextHook(const std::function<std::unique_ptr<HookBase>()>& make);
HookBase* hookAt(Fiber& fiber, std::size_t index);
void scheduleUpdate(Fiber& fiber);

template <class H>
H& checkedHook(HookBase* hook) {
    auto* h = dynamic_cast<H*>(hook);
    if (!h) throw std::logic_error("ReactX: hook order changed between renders");
    return *h;
}

template <class T>
struct StateHook : HookBase {
    T value;
    explicit StateHook(T v) : value(std::move(v)) {}
};

template <class T>
struct RefHook : HookBase {
    T value;
    explicit RefHook(T v) : value(std::move(v)) {}
};

struct DepsBase {
    virtual ~DepsBase() = default;
    virtual bool equals(const DepsBase& other) const = 0;
};

template <class Tuple>
struct Deps : DepsBase {
    Tuple values;
    explicit Deps(Tuple t) : values(std::move(t)) {}
    bool equals(const DepsBase& other) const override {
        auto* o = dynamic_cast<const Deps*>(&other);
        return o && o->values == values;
    }
};

using EffectFn = std::function<Cleanup()>;

struct EffectHook : HookBase {
    std::unique_ptr<DepsBase> deps;
    EffectFn pending;  // effect waiting to run after commit
    Cleanup cleanup;   // returned by the last run
};

// deps == nullptr means "run after every render".
void useEffectImpl(EffectFn effect, std::unique_ptr<DepsBase> deps);

template <class T>
struct MemoHook : HookBase {
    std::optional<T> value;
    std::unique_ptr<DepsBase> deps;
};

template <class Sig>
struct CallbackHook : HookBase {
    std::shared_ptr<std::function<Sig>> fn;
    std::unique_ptr<DepsBase> deps;
};

// Handler signature for a callable: void(), void(const KeyEvent&) or
// void(const std::string&).
template <class F>
constexpr auto handlerSignature() {
    if constexpr (std::is_invocable_v<F&>) return std::type_identity<void()>{};
    else if constexpr (std::is_invocable_v<F&, const KeyEvent&>)
        return std::type_identity<void(const KeyEvent&)>{};
    else if constexpr (std::is_invocable_v<F&, const std::string&>)
        return std::type_identity<void(const std::string&)>{};
    else if constexpr (std::is_invocable_v<F&, const ScrollEvent&>)
        return std::type_identity<void(const ScrollEvent&)>{};
    else static_assert(sizeof(F) == 0, "useCallback: not a ReactX event handler");
}

template <class F>
using HandlerSig = typename decltype(handlerSignature<std::decay_t<F>>())::type;

// Value of the nearest provider of context `id` above the rendering
// component, or nullptr. Subscribes the component to that provider.
const ContextValueBase* readContext(const void* id);

template <class F>
EffectFn wrapEffect(F&& f) {
    using R = std::invoke_result_t<std::decay_t<F>&>;
    if constexpr (std::is_void_v<R>) {
        return [f = std::forward<F>(f)]() mutable -> Cleanup {
            f();
            return {};
        };
    } else {
        return [f = std::forward<F>(f)]() mutable -> Cleanup { return Cleanup(f()); };
    }
}

}  // namespace detail

// State setter returned by useState. Accepts a new value or an updater
// function: setCount(5) or setCount([](int c) { return c + 1; }).
// Calls after the component unmounted are ignored.
template <class T>
class Setter {
public:
    Setter(std::weak_ptr<detail::Fiber> fiber, std::size_t index)
        : fiber_(std::move(fiber)), index_(index) {}

    template <class U>
        requires std::convertible_to<U, T> || std::is_invocable_r_v<T, U, const T&>
    void operator()(U&& next) const {
        auto fiber = fiber_.lock();
        if (!fiber) return;
        auto& hook = static_cast<detail::StateHook<T>&>(*detail::hookAt(*fiber, index_));
        T value = [&]() -> T {
            if constexpr (std::is_invocable_r_v<T, U, const T&>)
                return std::invoke(std::forward<U>(next), std::as_const(hook.value));
            else
                return T(std::forward<U>(next));
        }();
        if constexpr (std::equality_comparable<T>) {
            if (value == hook.value) return;  // nothing changed: skip re-render
        }
        hook.value = std::move(value);
        detail::scheduleUpdate(*fiber);
    }

private:
    std::weak_ptr<detail::Fiber> fiber_;
    std::size_t index_;
};

template <class T>
std::pair<T, Setter<T>> useState(T initial) {
    auto slot = detail::nextHook(
        [&] { return std::make_unique<detail::StateHook<T>>(std::move(initial)); });
    auto& hook = detail::checkedHook<detail::StateHook<T>>(slot.hook);
    return {hook.value, Setter<T>(slot.fiber, slot.index)};
}

inline std::pair<std::string, Setter<std::string>> useState(const char* initial) {
    return useState<std::string>(initial);
}

// Mutable value that survives re-renders and does not trigger them.
template <class T>
T& useRef(T initial = T{}) {
    auto slot = detail::nextHook(
        [&] { return std::make_unique<detail::RefHook<T>>(std::move(initial)); });
    return detail::checkedHook<detail::RefHook<T>>(slot.hook).value;
}

// Dependency list for useEffect: rx::deps(a, b). rx::deps() means "run once".
template <class... D>
std::tuple<std::decay_t<D>...> deps(D&&... values) {
    return {std::forward<D>(values)...};
}

// Runs after every render. The effect may return a cleanup function.
template <class F>
void useEffect(F&& effect) {
    detail::useEffectImpl(detail::wrapEffect(std::forward<F>(effect)), nullptr);
}

// Runs after the first render and whenever a dependency changes.
template <class F, class... D>
void useEffect(F&& effect, std::tuple<D...> dependencies) {
    detail::useEffectImpl(
        detail::wrapEffect(std::forward<F>(effect)),
        std::make_unique<detail::Deps<std::tuple<D...>>>(std::move(dependencies)));
}

// Computes a value on the first render and again only when a dependency
// changes: const auto& sorted = useMemo([&] { return sort(items); }, deps(items));
template <class F, class... D>
REACTX_NO_DANGLING const std::decay_t<std::invoke_result_t<F&>>& useMemo(F&& compute, std::tuple<D...> dependencies) {
    using T = std::decay_t<std::invoke_result_t<F&>>;
    auto slot = detail::nextHook([] { return std::make_unique<detail::MemoHook<T>>(); });
    auto& hook = detail::checkedHook<detail::MemoHook<T>>(slot.hook);
    auto next = std::make_unique<detail::Deps<std::tuple<D...>>>(std::move(dependencies));
    if (!hook.value || !hook.deps || !hook.deps->equals(*next)) {
        hook.value.emplace(std::invoke(compute));
        hook.deps = std::move(next);
    }
    return *hook.value;
}

// Event handler that keeps the same identity until a dependency changes, so
// memo() components that receive it are not re-rendered:
//   auto onToggle = useCallback([setDone] { setDone(true); }, deps());
// Returns Callback, KeyHandler or InputHandler depending on the lambda.
template <class F, class... D>
std::function<detail::HandlerSig<F>> useCallback(F&& handler, std::tuple<D...> dependencies) {
    using Sig = detail::HandlerSig<F>;
    auto slot = detail::nextHook([] { return std::make_unique<detail::CallbackHook<Sig>>(); });
    auto& hook = detail::checkedHook<detail::CallbackHook<Sig>>(slot.hook);
    auto next = std::make_unique<detail::Deps<std::tuple<D...>>>(std::move(dependencies));
    if (!hook.fn || !hook.deps || !hook.deps->equals(*next)) {
        hook.fn = std::make_shared<std::function<Sig>>(std::forward<F>(handler));
        hook.deps = std::move(next);
    }
    return detail::StableFn<Sig>{hook.fn};
}

// ---- utility hooks: useDebounce, useWindowSize, useLocalStorage ----

namespace detail {

// Runs `fn` after `delay` on the UI thread — never on another thread, so it
// may set state. Browser: setTimeout. Native: the first Root::flush() after
// the deadline (TerminalApp flushes every 100 ms). Returns the cancel
// function, ready to be an effect's cleanup.
Cleanup setTimer(std::chrono::milliseconds delay, std::function<void()> fn);
// Native: runs the timers that are due (called by Root::flush()).
void runDueTimers();

std::string storageGet(std::string_view key);
void storageSet(std::string_view key, std::string_view value);

}  // namespace detail

// The value, but only after it stopped changing for `delay` — for search
// boxes and other input that shouldn't trigger work on every key press:
//   std::string query = useDebounce(text, 300ms);
template <class T>
    requires std::copy_constructible<T> && std::equality_comparable<T>
T useDebounce(const T& value, std::chrono::milliseconds delay) {
    auto [debounced, setDebounced] = useState(value);
    // A new value cancels the pending timer (the effect's cleanup) and starts
    // a new one; unmounting cancels it too.
    useEffect([value, delay, setDebounced] { return detail::setTimer(delay, [value, setDebounced] { setDebounced(value); }); },
              deps(value, delay.count()));
    return debounced;
}

struct WindowSize {
    int width = 0;
    int height = 0;
    bool operator==(const WindowSize&) const = default;
};

namespace detail {
// The current size: the browser window (CSS pixels) or the terminal (cells).
WindowSize currentWindowSize();
// Calls `fn` with the new size after every resize; returns the unsubscribe
// function. Browser: the window's resize event; native: TerminalApp reports
// terminal resizes (see notifyResize).
Cleanup onResize(std::function<void(WindowSize)> fn);
void notifyResize(WindowSize size);
}  // namespace detail

// The size of the browser window or of the terminal; the component
// re-renders when it changes.
WindowSize useWindowSize();

// State that survives a reload: kept in localStorage in the browser and in
// memory for the life of the process natively.
//   auto [theme, setTheme] = useLocalStorage("theme", std::string("dark"));
// Supports std::string, bool and numbers.
template <class T>
std::pair<T, Setter<T>> useLocalStorage(std::string key, T initialValue) {
    static_assert(std::same_as<T, std::string> || std::same_as<T, bool> || std::integral<T> || std::floating_point<T>,
                  "useLocalStorage: the value must be std::string, bool or a number");
    T initial = std::move(initialValue);
    // Read the stored value once, on the first render.
    bool& loaded = useRef(false);
    if (!loaded) {
        loaded = true;
        if (std::string saved = detail::storageGet(key); !saved.empty()) {
            if constexpr (std::same_as<T, std::string>) initial = std::move(saved);
            else if constexpr (std::same_as<T, bool>) initial = saved == "true";
            else if (auto n = parseNumber<T>(saved)) initial = *n;  // a broken value keeps the default
        }
    }
    auto [state, setState] = useState(std::move(initial));
    useEffect(
        [key, state] {
            if constexpr (std::same_as<T, std::string>) detail::storageSet(key, state);
            else if constexpr (std::same_as<T, bool>) detail::storageSet(key, state ? "true" : "false");
            else detail::storageSet(key, std::format("{}", state));
        },
        deps(key, state));
    return {state, setState};
}

inline std::pair<std::string, Setter<std::string>> useLocalStorage(std::string key, const char* initial) {
    return useLocalStorage<std::string>(std::move(key), std::string(initial));
}

// ---- useReducer ----

namespace detail {
// The action type of a reducer: the second parameter of State(const State&, Action).
template <class F>
struct ReducerTraits : ReducerTraits<decltype(&F::operator())> {};
template <class C, class R, class S, class A>
struct ReducerTraits<R (C::*)(S, A) const> {
    using Action = std::decay_t<A>;
};
template <class C, class R, class S, class A>
struct ReducerTraits<R (C::*)(S, A)> {
    using Action = std::decay_t<A>;
};
template <class R, class S, class A>
struct ReducerTraits<R (*)(S, A)> {
    using Action = std::decay_t<A>;
};
}  // namespace detail

// Sends an action to a reducer (from useReducer). Calls after unmounting
// are ignored, like setters.
template <class State, class Action>
class Dispatch {
public:
    Dispatch(Setter<State> set, std::shared_ptr<std::function<State(const State&, const Action&)>> reducer)
        : set_(std::move(set)), reducer_(std::move(reducer)) {}
    void operator()(Action action) const {
        set_([reducer = reducer_, action = std::move(action)](const State& s) { return (*reducer)(s, action); });
    }

private:
    Setter<State> set_;
    std::shared_ptr<std::function<State(const State&, const Action&)>> reducer_;
};

// State changed by actions, for state with several related fields:
//   enum class Op { Add, Undo };
//   auto [s, dispatch] = useReducer([](const Doc& d, Op op) { ...; return next; }, Doc{});
//   dispatch(Op::Undo);
// The reducer is a function (State, Action) -> State without side effects; the
// newest one is used, so it may capture props.
template <class Reducer, class State>
auto useReducer(Reducer reducer, State initial) {
    using Action = typename detail::ReducerTraits<std::decay_t<Reducer>>::Action;
    using Fn = std::function<State(const State&, const Action&)>;
    auto [state, set] = useState(std::move(initial));
    // One shared reducer per component, replaced on every render: the dispatch
    // function stays valid and always uses the newest reducer.
    auto& holder = useRef(std::shared_ptr<Fn>{});
    if (!holder) holder = std::make_shared<Fn>();
    *holder = Fn(std::move(reducer));
    return std::pair<State, Dispatch<State, Action>>{state, Dispatch<State, Action>(set, holder)};
}

// ---- timers ----

// Calls `callback` once, `delay` after the component mounts (or after `delay`
// changes). std::nullopt switches it off; unmounting cancels it. The newest
// callback runs, so it may use current state:
//   useTimeout([setToast] { setToast(std::nullopt); }, toast ? std::optional(5s) : std::nullopt);
template <class F>
    requires std::invocable<F&>
void useTimeout(F callback, std::optional<std::chrono::milliseconds> delay) {
    auto& latest = useRef(std::shared_ptr<std::function<void()>>{});
    if (!latest) latest = std::make_shared<std::function<void()>>();
    *latest = std::function<void()>(std::move(callback));
    std::int64_t ms = delay ? delay->count() : -1;
    useEffect(
        [ms, latest = latest]() -> Cleanup {
            if (ms < 0) return {};
            return detail::setTimer(std::chrono::milliseconds(ms), [latest] { (*latest)(); });
        },
        deps(ms));
}

// Calls `callback` every `period` while mounted (std::nullopt pauses it).
// Natively the period is at least TerminalApp's 100 ms flush.
template <class F>
    requires std::invocable<F&>
void useInterval(F callback, std::optional<std::chrono::milliseconds> period) {
    auto& latest = useRef(std::shared_ptr<std::function<void()>>{});
    if (!latest) latest = std::make_shared<std::function<void()>>();
    *latest = std::function<void()>(std::move(callback));
    std::int64_t ms = period ? period->count() : -1;
    useEffect(
        [ms, latest = latest]() -> Cleanup {
            if (ms < 0) return {};
            // Each tick schedules the next one; `cancel` always holds the pending one.
            auto cancel = std::make_shared<Cleanup>();
            auto tick = std::make_shared<std::function<void()>>();
            std::weak_ptr<std::function<void()>> weakTick = tick;
            *tick = [latest, cancel, weakTick, ms] {
                if (auto self = weakTick.lock()) *cancel = detail::setTimer(std::chrono::milliseconds(ms), *self);
                (*latest)();
            };
            *cancel = detail::setTimer(std::chrono::milliseconds(ms), *tick);
            return [cancel, tick] { (*cancel)(); };  // keeps `tick` alive while mounted
        },
        deps(ms));
}

// ---- keyboard shortcuts for the whole app ----

namespace detail {
// Every key press, wherever the focus is (see useKeyboard). Returns the
// unsubscribe function.
Cleanup onGlobalKey(std::function<bool(const KeyEvent&)> handler);
// Called by the backends for each key press, before the focused element gets
// it. `editing`: the focus is in a text field — then keys that edit text are
// not reported. Returns true if a handler consumed the key.
bool dispatchGlobalKey(const KeyEvent& key, bool editing);
}  // namespace detail

// Keyboard shortcuts for the whole app (Ctrl+Z, "/", Escape), wherever the
// focus is — even on no element at all. The handler runs before the focused
// element's onKey; return true to consume the key (the browser's default
// action and the element's handlers are skipped), or nothing / false to let it
// through. While the user types in a text field, keys that edit text
// (letters, digits, Backspace, Delete, Left/Right, Home/End without Ctrl/Alt)
// don't reach it, so "/" or "n" shortcuts never steal typing.
//   useKeyboard([&](const KeyEvent& k) { if (k.ctrl && k.key == "z") { undo(); return true; } return false; });
template <class F>
    requires std::invocable<F&, const KeyEvent&>
void useKeyboard(F handler) {
    auto& latest = useRef(std::shared_ptr<std::function<bool(const KeyEvent&)>>{});
    if (!latest) latest = std::make_shared<std::function<bool(const KeyEvent&)>>();
    *latest = [h = std::move(handler)](const KeyEvent& k) mutable -> bool {
        if constexpr (std::is_void_v<std::invoke_result_t<F&, const KeyEvent&>>) {
            h(k);
            return false;
        } else {
            return static_cast<bool>(h(k));
        }
    };
    useEffect([latest = latest] { return detail::onGlobalKey([latest](const KeyEvent& k) { return (*latest)(k); }); }, deps());
}

}  // namespace rx
