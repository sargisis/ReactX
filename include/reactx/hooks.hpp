#pragma once

#include <chrono>
#include <concepts>
#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <stdexcept>
#include <thread>
#include <tuple>
#include <type_traits>
#include <utility>

#include "reactx/element.hpp"

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

// ---- Practical C++20 / C++26 utility hooks ----

// Delays updating the returned value until `delayMs` milliseconds have passed
// without new changes. Safe against unmounting and race conditions.
template <class T>
    requires std::copy_constructible<T>
T useDebounce(const T& value, std::chrono::milliseconds delay) {
    auto [debounced, setDebounced] = useState(value);
    useEffect([value, delay, setDebounced] {
        auto alive = std::make_shared<bool>(true);
#if defined(__EMSCRIPTEN__)
        // Web: set a browser timeout
        detail::setTimeoutJs(static_cast<int>(delay.count()), [alive, value, setDebounced] {
            if (*alive) setDebounced(value);
        });
#else
        // Native/terminal: use std::jthread with cancel/stop_token
        std::jthread worker([alive, value, delay, setDebounced](std::stop_token st) {
            std::this_thread::sleep_for(delay);
            if (!st.stop_requested() && *alive) setDebounced(value);
        });
        worker.detach();
#endif
        return [alive] { *alive = false; };
    }, deps(value, delay.count()));
    return debounced;
}

struct WindowSize {
    int width = 0;
    int height = 0;
    bool operator==(const WindowSize&) const = default;
};

// Returns current viewport/terminal dimensions and updates when resized.
WindowSize useWindowSize();

namespace detail {
#if defined(__EMSCRIPTEN__)
void setTimeoutJs(int delayMs, std::function<void()> cb);
#endif
std::string storageGet(std::string_view key);
void storageSet(std::string_view key, std::string_view value);
}  // namespace detail

// Persistent state hook: synchronizes value with localStorage (in the browser)
// or an in-memory/process-level store (in terminal/CLI).
// Works with std::string, primitives, or any type convertible to/from string.
template <class T>
std::pair<T, Setter<T>> useLocalStorage(std::string key, T initialValue) {
    T initial = initialValue;
    std::string saved = detail::storageGet(key);
    if (!saved.empty()) {
        if constexpr (std::same_as<T, std::string>) {
            initial = saved;
        } else if constexpr (std::integral<T> || std::floating_point<T>) {
            try {
                if constexpr (std::same_as<T, bool>) initial = (saved == "true" || saved == "1");
                else if constexpr (std::integral<T>) initial = static_cast<T>(std::stoll(saved));
                else initial = static_cast<T>(std::stod(saved));
            } catch (...) {}
        }
    }

    auto [state, setState] = useState(std::move(initial));

    useEffect([key, state] {
        if constexpr (std::same_as<T, std::string>) {
            detail::storageSet(key, state);
        } else if constexpr (std::same_as<T, bool>) {
            detail::storageSet(key, state ? "true" : "false");
        } else if constexpr (std::integral<T> || std::floating_point<T>) {
            detail::storageSet(key, std::to_string(state));
        }
    }, deps(key, state));

    return {state, setState};
}

inline std::pair<std::string, Setter<std::string>> useLocalStorage(std::string key, const char* initial) {
    return useLocalStorage<std::string>(std::move(key), std::string(initial));
}

}  // namespace rx
