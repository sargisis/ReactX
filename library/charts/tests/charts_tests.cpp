// rx-charts in a terminal (testing::Screen) and its number text.
#include <string>

#include "reactx/testing.hpp"
#include "rx/charts.hpp"

using namespace rx;

namespace {
int count(const std::string& text, const std::string& part) {
    int n = 0;
    for (auto at = text.find(part); at != std::string::npos; at = text.find(part, at + part.size())) ++n;
    return n;
}
}  // namespace

RX_TEST(short_numbers) {
    CHECK(charts::shortNumber(0) == "0");
    CHECK(charts::shortNumber(12) == "12");
    CHECK(charts::shortNumber(1.5) == "1.5");
    CHECK(charts::shortNumber(-3.25) == "-3.25");
    CHECK(charts::shortNumber(0.004) == "0");
    CHECK(charts::shortNumber(9999) == "9999");
    CHECK(charts::shortNumber(12345) == "12.3k");
    CHECK(charts::shortNumber(4100000) == "4.1M");
    CHECK(charts::shortNumber(250.6) == "251");
}

RX_TEST(bars_are_as_long_as_their_values) {
    testing::Screen s(charts::bar({.labels = {"Mon", "Tue", "Wed"},
                                   .series = {{.name = "Steps", .values = {5000, 10000, 2500}}},
                                   .title = "Steps",
                                   .width = 40}),
                      60, 12);
    auto lines = s.lines();
    CHECK(s.shows("Steps"));
    CHECK(s.shows("Mon") && s.shows("10k") && s.shows("2500"));
    int mon = 0, tue = 0, wed = 0;
    for (const auto& l : lines) {
        if (l.starts_with("Mon")) mon = count(l, "█");
        if (l.starts_with("Tue")) tue = count(l, "█");
        if (l.starts_with("Wed")) wed = count(l, "█");
    }
    CHECK(tue > 0 && mon * 2 >= tue - 1 && mon * 2 <= tue + 1);  // half as long, give or take an eighth
    CHECK(wed * 4 >= tue - 4 && wed * 4 <= tue + 4);
    CHECK(tue <= 40);
}

RX_TEST(bars_with_two_series_have_a_legend) {
    testing::Screen s(charts::bar({.labels = {"Q1", "Q2"},
                                   .series = {{.name = "2025", .values = {3, 4}}, {.name = "2026", .values = {5, 6}}},
                                   .width = 40}),
                      60, 12);
    CHECK(s.shows("■ 2025") && s.shows("■ 2026"));
    CHECK(s.shows("Q1") && s.shows("Q2"));
}

RX_TEST(a_line_is_drawn_in_braille_with_its_scale) {
    testing::Screen s(charts::line({.labels = {"Jan", "Feb", "Mar", "Apr"},
                                    .series = {{.name = "Visits", .values = {10, 40, 25, 80}}},
                                    .unit = "k",
                                    .rows = 6,
                                    .width = 40}),
                      60, 14);
    std::string text = s.text();
    CHECK(text.find("\xE2\xA0") != std::string::npos || text.find("\xE2\xA1") != std::string::npos ||
          text.find("\xE2\xA2") != std::string::npos || text.find("\xE2\xA3") != std::string::npos);  // braille cells
    CHECK(s.shows("80k") && s.shows("0k"));  // the axis: 0 .. 80
    CHECK(s.shows("Jan") && s.shows("Apr"));
}

RX_TEST(pie_shows_shares) {
    testing::Screen s(charts::pie({.labels = {"Rent", "Food", "Fun"}, .values = {50, 30, 20}, .title = "Budget", .width = 40}),
                      60, 10);
    CHECK(s.shows("Budget"));
    CHECK(s.shows("50%") && s.shows("30%") && s.shows("20%"));
    CHECK(count(s.text(), "█") == 40);  // the split bar fills its width
}

RX_TEST(empty_charts_draw_nothing_bad) {
    testing::Screen a(charts::bar({.width = 30}), 40, 6);
    testing::Screen b(charts::line({.width = 30}), 40, 14);
    testing::Screen c(charts::pie({.labels = {"x"}, .values = {0}, .width = 30}), 40, 6);
    CHECK(c.shows("0%"));
}

RX_TEST(width_follows_the_window) {
    testing::Screen s(charts::bar({.labels = {"A"}, .series = {{.values = {1}}}}), 30, 4);
    for (const auto& l : s.lines()) CHECK(count(l, "█") <= 26);
}
