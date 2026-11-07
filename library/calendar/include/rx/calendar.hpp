// reactx::calendar — a month calendar and a date picker for ReactX apps, in the
// browser and in a terminal, in the language of reactx::i18n (month and
// weekday names, the first day of the week, the date format).
//
//   auto [day, setDay] = useState(rx::calendar::today());
//   rx::calendar::datePicker({.value = day, .onChange = [setDay](auto d) { setDay(d); }, .label = "Due"})
//
// Keyboard: arrows move a day / a week, PageUp / PageDown a month, Home / End
// the start / end of the week, Enter picks.
//
// Comes with ReactX (library/calendar); link it in CMakeLists.txt:
//   target_link_libraries(my_app PRIVATE reactx::calendar)
#pragma once

#include <chrono>
#include <functional>
#include <string>

#include "reactx/reactx.hpp"

namespace rx::calendar {

using Date = std::chrono::year_month_day;

// Today where the user is (the browser's or the system's time zone).
Date today();

struct CalendarProps {
    Date value{};  // the picked day; an invalid date (the default) = none
    std::function<void(Date)> onChange{};
    Date min{}, max{};  // days outside can't be picked; invalid = no limit
    std::function<bool(Date)> disabled{};  // more days that can't be picked (weekends, holidays)
    std::string lang{};  // "" = reactx::i18n's current language
    std::string key{};
};
// A month grid with ‹ › to change the month; it opens on the picked day's
// month (or today's).
Element calendar(CalendarProps props);

struct DatePickerProps {
    Date value{};
    std::function<void(Date)> onChange{};
    std::string label{};
    std::string placeholder = "—";  // shown with no date
    Date min{}, max{};
    std::function<bool(Date)> disabled{};
    std::string lang{};
    std::string key{};
};
// A field showing the date (i18n Medium format: "Nov 6, 2026", "6 нояб. 2026 г.")
// that opens a calendar below it; picking a day closes it.
Element datePicker(DatePickerProps props);

}  // namespace rx::calendar
