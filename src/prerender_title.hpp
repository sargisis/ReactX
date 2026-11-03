// The page title while prerendering (src/prerender.cpp): effects don't run
// there, so useTitle (src/utility_hooks.cpp) reports its title as it renders.
#pragma once

#include <optional>
#include <string>

namespace rx::detail {

struct PrerenderTitle {
    bool collecting = false;           // a route is being prerendered
    std::optional<std::string> title;  // the first useTitle of this render: the outermost
};
PrerenderTitle& prerenderTitle();

}  // namespace rx::detail
