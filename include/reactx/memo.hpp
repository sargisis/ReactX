#pragma once

#include "reactx/element.hpp"

namespace rx {

namespace detail {

// Remembers which component functions are memoized.
Component registerMemo(Component component);
bool isMemo(Component component);

template <Component C>
Element memoWrapper(const Props& props) {
    return C(props);
}

}  // namespace detail

// Memoized component: skips re-rendering when its parent re-renders with
// equal props. Its own state changes and contexts it reads still re-render it.
//
//   h(memo<TodoItem>, {{"title", t.title}, {"onToggle", onToggle}})
//
// Props are compared one by one: values with ==, handlers only if they come
// from the same useCallback. Elements with children are always re-rendered.
template <Component C>
inline const Component memo = detail::registerMemo(&detail::memoWrapper<C>);

}  // namespace rx
