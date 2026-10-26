#pragma once

#include <concepts>
#include <cstdint>

#include "reactx/element.hpp"

namespace rx {

namespace detail {

// Remembers which component functions are memoized.
Component registerMemo(Component component);
bool isMemo(Component component);
// The same for typed components (by function address).
void registerTypedMemo(std::uintptr_t component);
bool isTypedMemo(std::uintptr_t component);

template <Component C>
Element memoWrapper(const Props& props) {
    return C(props);
}

template <class>
struct PropsOf;
template <class P>
struct PropsOf<Element (*)(const P&)> {
    using type = P;
};

template <auto C, class P>
Element typedMemoWrapper(const P& props) {
    return C(props);
}

template <auto C>
auto makeMemo() {
    if constexpr (std::same_as<decltype(C), Component>) {
        return registerMemo(&memoWrapper<C>);
    } else {
        using P = typename PropsOf<decltype(C)>::type;
        static_assert(std::equality_comparable<P>,
                      "memo<C>: the props struct needs operator== (bool operator==(const P&) const = default;)");
        TypedComponent<P> wrapper = &typedMemoWrapper<C, P>;
        registerTypedMemo(addressOf(wrapper));
        return wrapper;
    }
}

}  // namespace detail

// Memoized component: skips re-rendering when its parent re-renders with
// equal props. Its own state changes and contexts it reads still re-render it.
//
//   h(memo<TodoItem>, {{"title", t.title}, {"onToggle", onToggle}})
//   h(memo<Counter>, {.start = 5})      // typed: the structs are compared with ==
//
// Props are compared one by one: values with ==, handlers only if they come
// from the same useCallback. Elements with children are always re-rendered.
template <auto C>
inline const auto memo = detail::makeMemo<C>();

}  // namespace rx
