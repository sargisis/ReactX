#pragma once

#include <concepts>
#include <cstddef>
#include <functional>
#include <memory>
#include <stdexcept>
#include <tuple>
#include <type_traits>
#include <utility>

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

}  // namespace rx
