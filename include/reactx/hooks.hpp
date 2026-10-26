#pragma once

#include <chrono>
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

}  // namespace rx
