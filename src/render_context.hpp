#pragma once

#include <cstddef>

namespace rx::detail {

struct Fiber;

// Which component is rendering right now (so hooks know where to store state).
struct RenderContext {
    Fiber* fiber = nullptr;
    std::size_t hookIndex = 0;
};

extern thread_local RenderContext* currentContext;

}  // namespace rx::detail
