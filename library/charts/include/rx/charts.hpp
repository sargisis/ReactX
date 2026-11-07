// rx-charts: charts for ReactX apps. The same call draws SVG in the browser and
// block and braille characters in a terminal.
//
//   #include "rx/charts.hpp"
//   rx::charts::bar({.labels = {"Mon", "Tue", "Wed"},
//                    .series = {{.name = "Steps", .values = {6200, 8100, 4300}}},
//                    .title = "This week"})
//
// Comes with ReactX (library/charts); link it in CMakeLists.txt:
//   target_link_libraries(my_app PRIVATE reactx::charts)
#pragma once

#include <string>
#include <vector>

#include "reactx/reactx.hpp"

namespace rx::charts {

// One line or one set of bars. color: a palette name ("blue", "green",
// "magenta", "cyan", "yellow", "red"; the browser has a light and a dark shade
// of each) or, in the browser only, any CSS color. Empty: the next palette color.
struct Series {
    std::string name{};
    std::vector<double> values{};  // one per label
    std::string color{};
};

// Bars: columns in the browser, rows of bars in a terminal.
struct BarChartProps {
    std::vector<std::string> labels{};
    std::vector<Series> series{};
    std::string title{};
    std::string unit{};     // after values: "km", " %"
    int height = 0;         // browser: px (default 240); terminal: unused
    int width = 0;          // terminal: cells (default: the width of its box); browser: unused (fills its box)
    std::string key{};
};
Element bar(BarChartProps props);

// Lines through the values, one per series.
struct LineChartProps {
    std::vector<std::string> labels{};
    std::vector<Series> series{};
    std::string title{};
    std::string unit{};
    int height = 0;         // browser: px (default 240)
    int rows = 0;           // terminal: rows (default 10)
    int width = 0;          // terminal: cells (default: the window's width; inside cards, set it)
    bool dots = true;       // browser: a dot on every value
    std::string key{};
};
Element line(LineChartProps props);

// Shares of a whole: a pie (or a ring) in the browser, a split bar and a legend in a terminal.
struct PieChartProps {
    std::vector<std::string> labels{};
    std::vector<double> values{};      // negative values count as 0
    std::vector<std::string> colors{};  // per slice; empty: the palette
    std::string title{};
    std::string unit{};
    bool donut = false;     // browser: a ring with the total in the middle
    int width = 0;          // terminal: cells (default: the width of its box)
    std::string key{};
};
Element pie(PieChartProps props);

// "1.5", "12", "-3.25", "12.3k", "4.1M": short text for a number (no
// std::format: it would add ~100 KB to a .wasm).
std::string shortNumber(double value);

}  // namespace rx::charts
