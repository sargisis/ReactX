// reactx::icons — icons for ReactX apps: a line-drawn SVG in the browser (it
// takes the text color and size), a one-cell symbol in a terminal.
//
//   rx::icons::icon({.name = "search"})
//   ui::button({.label = "Delete", ...}) next to rx::icons::icon({.name = "trash", .color = "red"})
//
// Comes with ReactX (library/icons); link it in CMakeLists.txt:
//   target_link_libraries(my_app PRIVATE reactx::icons)
#pragma once

#include <string>
#include <vector>

#include "reactx/reactx.hpp"

namespace rx::icons {

struct IconProps {
    std::string name{};   // "check", "search", ...: see names()
    int size = 0;         // browser: px; 0 = the font size (1em)
    std::string label{};  // what it means for screen readers when there is no text next to it ("Delete")
    std::string color{};  // "" = the text color; a palette name ("red") or, in the browser, any CSS color
    bool spin = false;    // turns forever: a loading indicator ("loader", "refresh")
    std::string key{};
};
// An unknown name shows "?" (and a dashed box in the browser), so a typo is visible.
Element icon(IconProps props);

// The names of all icons, built in and added.
std::vector<std::string> names();

// Your own icon: `path` is SVG path data on a 24x24 grid, drawn as a 2 px line
// with round ends (fill none); `symbol` is what a terminal shows (one cell).
// The same name replaces a built-in icon.
void add(std::string name, std::string path, std::string symbol);

}  // namespace rx::icons
