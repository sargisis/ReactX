#pragma once

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

}  // namespace rx
