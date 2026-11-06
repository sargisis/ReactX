// rx-i18n: translations, plurals, numbers and dates for ReactX apps, in the
// browser and in a terminal.
//
//   rx::i18n::setup({.languages = {
//       {.code = "en", .name = "English", .messages = {{"cart", "{n, plural, one {# item} other {# items}}"}}},
//       {.code = "ru", .name = "Русский", .messages = {{"cart", "{n, plural, one {# товар} few {# товара} other {# товаров}}"}}},
//   }});
//
//   Element Cart() {
//       auto t = rx::i18n::useTranslation();   // re-renders when the language changes
//       return text(t("cart", {{"n", 3}}));     // "3 items" / "3 товара"
//   }
//
// Comes with ReactX (library/i18n); link it in CMakeLists.txt:
//   target_link_libraries(my_app PRIVATE reactx::i18n)
#pragma once

#include <chrono>
#include <concepts>
#include <cstdint>
#include <expected>
#include <functional>
#include <initializer_list>
#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "reactx/fetch.hpp"
#include "reactx/reactx.hpp"

namespace rx::i18n {

// key -> message. Keys are any text; dotted keys ("cart.empty") are the
// convention, and parseMessages turns nested JSON objects into them.
using Messages = std::map<std::string, std::string, std::less<>>;

// How numbers, dates and plurals look in a language. The built-in data
// (locale()) covers en, ru, uk, hy, de, fr, es, it, pt, pl; any other language
// gets English rules unless its Language gives a Locale of its own.
struct Locale {
    std::string decimal = ".";       // 1.5 / 1,5
    std::string group = ",";         // 1,000 / 1 000 (no-break space) / 1.000
    int minGrouping = 1;             // 2: four-digit numbers stay "1234" (es, pl)
    std::string percent = "{0}%";    // "{0} %" in ru, de, fr
    bool currencyAfter = false;      // "$5.00" / "5,00 €"
    std::string currencyGap{};       // between the number and the symbol
    // Dates: {d} {dd} {M} {MM} {MMM} {MMMM} {y} {EEEE}.
    std::string dateShort = "{M}/{d}/{y}";
    std::string dateMedium = "{MMM} {d}, {y}";
    std::string dateLong = "{MMMM} {d}, {y}";
    std::string dateFull = "{EEEE}, {MMMM} {d}, {y}";
    std::vector<std::string> months{};       // 12, in a date ("ноября")
    std::vector<std::string> monthsShort{};  // 12
    std::vector<std::string> monthsAlone{};  // 12, on their own ("ноябрь"); empty: months
    std::vector<std::string> weekdays{};     // 7, Sunday first
    std::vector<std::string> weekdaysShort{};
    int firstWeekday = 1;            // 0 = Sunday, 1 = Monday
    bool hour12 = false;             // "2:05 PM" / "14:05"
    std::string am = "AM", pm = "PM";
    // Relative time: plural messages with {n} for each unit, then the
    // direction around the result.
    std::string relSeconds = "{n, plural, one {# second} other {# seconds}}";
    std::string relMinutes = "{n, plural, one {# minute} other {# minutes}}";
    std::string relHours = "{n, plural, one {# hour} other {# hours}}";
    std::string relDays = "{n, plural, one {# day} other {# days}}";
    std::string relWeeks = "{n, plural, one {# week} other {# weeks}}";
    std::string relMonths = "{n, plural, one {# month} other {# months}}";
    std::string relYears = "{n, plural, one {# year} other {# years}}";
    std::string past = "{0} ago";
    std::string future = "in {0}";
    std::string now = "now";
};

// The built-in Locale of a language: "ru", "ru-RU" and "ru_RU.UTF-8" all give
// the Russian one; an unknown language gives English.
const Locale& locale(std::string_view lang);

// The plural category of `n` in a language: "zero", "one", "two", "few",
// "many" or "other" (the CLDR rules; numbers with a fraction are "other" in
// most languages).
std::string_view pluralCategory(double n, std::string_view lang);

struct Language {
    std::string code{};     // "en", "ru", "pt-BR"
    std::string name{};     // in that language, for a picker: "English", "Русский"
    Messages messages{};
    const Locale* locale = nullptr;  // nullptr: the built-in one for `code`
};

struct Options {
    std::vector<Language> languages{};
    // Where a missing message comes from; "": the first language.
    std::string fallback{};
    // The first language: "" = the saved choice, else the system language if
    // it is listed, else `fallback`.
    std::string initial{};
    // localStorage key that remembers the choice; "" = don't remember.
    std::string storageKey = "lang";
    // A message missing in the current language (and maybe in the fallback
    // too, then the key itself is shown). Once per language and key.
    std::function<void(std::string_view lang, std::string_view key)> onMissing{};
};

// Call once in main(), before mounting the app.
void setup(Options options);

// Adds messages (or a whole language) after setup(), e.g. fetched JSON; the
// components that show text re-render.
void addMessages(std::string_view lang, Messages messages);

// The current language code, and switching it: every component that uses
// useTranslation() or useLanguage() re-renders; the choice is saved
// (Options::storageKey) and in the browser <html lang> (and dir="rtl" for
// ar, he, fa, ur) follows it. An unknown code is ignored.
const std::string& language();
void setLanguage(std::string_view code);

struct LanguageInfo {
    std::string code{};
    std::string name{};
};
// The languages from setup() and addMessages(), in that order.
std::vector<LanguageInfo> languages();

// The language of the browser (navigator.language) or of the terminal (LC_ALL,
// LC_MESSAGES, LANG): "ru-RU", "en-US"; "" if unknown.
std::string systemLanguage();

// ---- messages ----

// A value for a {name} in a message: text or a number.
struct Arg {
    std::string name{};
    std::string text{};
    double number = 0;
    bool isNumber = false;

    Arg(std::string_view n, std::string_view value) : name(n), text(value) {}
    Arg(std::string_view n, const char* value) : name(n), text(value) {}
    Arg(std::string_view n, const std::string& value) : name(n), text(value) {}
    template <class T>
        requires(std::integral<T> || std::floating_point<T>) && (!std::same_as<T, bool>)
    Arg(std::string_view n, T value) : name(n), number(static_cast<double>(value)), isNumber(true) {}
};
using Args = std::vector<Arg>;

// The message for `key` in the current language with the arguments put in.
// Message syntax (a subset of ICU MessageFormat):
//   {name}                                        the argument (numbers in the language's format)
//   {n, plural, =0 {none} one {# file} other {# files}}   # = n; exact =N first, then the category, then other
//   {role, select, admin {Admin} other {User}}
//   '{'  ''                                       a literal brace, a literal apostrophe
// A missing key: the fallback language, then the key itself.
std::string t(std::string_view key, const Args& args = {});

// The same pattern rules for a message you already have.
std::string format(std::string_view message, const Args& args = {}, std::string_view lang = {});

// Messages from JSON: {"cart": {"empty": "Empty", "items": "..."}} gives
// "cart.empty" and "cart.items". Every leaf must be a string.
std::expected<Messages, std::string> parseMessages(std::string_view json);

// Fetches a JSON file of messages (e.g. "/locales/ru.json" in public/) and
// adds it to `lang`; `done` gets "" or the error. The returned handle cancels
// it (an effect's cleanup).
FetchHandle loadMessages(std::string lang, std::string url, std::function<void(const std::string& error)> done = {});

// ---- numbers and dates (lang "" = the current language) ----

struct NumberFormat {
    int maxDecimals = 3;   // rounded to this many, trailing zeros dropped
    int minDecimals = 0;   // but at least this many: 2 for money
    bool grouping = true;  // 1,000
};
std::string formatNumber(double value, NumberFormat format = {}, std::string_view lang = {});
// 0.25 -> "25%" / "25 %".
std::string formatPercent(double fraction, int maxDecimals = 0, std::string_view lang = {});
// An ISO code: "USD" -> "$1,234.50" / "1 234,50 $". Known symbols: USD EUR GBP
// RUB UAH AMD PLN JPY CNY INR; others show the code. decimals -1: 0 for JPY
// and KRW, else 2.
std::string formatCurrency(double amount, std::string_view currency, int decimals = -1, std::string_view lang = {});

enum class DateStyle { Short, Medium, Long, Full };
// 2026-11-06: "11/6/2026" "Nov 6, 2026" "November 6, 2026" "Friday, November 6, 2026";
// ru: "06.11.2026" "6 нояб. 2026 г." "6 ноября 2026 г." "пятница, 6 ноября 2026 г.".
std::string formatDate(std::chrono::year_month_day date, DateStyle style = DateStyle::Medium, std::string_view lang = {});
// "14:05" or "2:05 PM" (Locale::hour12); seconds >= 0 adds ":07".
std::string formatTime(int hour, int minute, int seconds = -1, std::string_view lang = {});
// -300 s -> "5 minutes ago", 7200 s -> "in 2 hours", under 5 s -> "now".
// The largest whole unit is picked for you (seconds .. years).
std::string formatRelative(std::chrono::seconds offset, std::string_view lang = {});
// Names for calendars and pickers: month 1..12, weekday 0..6 (Sunday = 0).
std::string monthName(unsigned month, bool shortName = false, std::string_view lang = {});
std::string weekdayName(unsigned weekday, bool shortName = false, std::string_view lang = {});

// ---- in components ----

// All of the above in one language, captured when the component rendered.
class Translator {
public:
    explicit Translator(std::string lang) : lang_(std::move(lang)) {}
    std::string operator()(std::string_view key, const Args& args = {}) const;
    const std::string& language() const { return lang_; }
    std::string number(double value, NumberFormat format = {}) const { return formatNumber(value, format, lang_); }
    std::string percent(double fraction, int maxDecimals = 0) const { return formatPercent(fraction, maxDecimals, lang_); }
    std::string currency(double amount, std::string_view code, int decimals = -1) const {
        return formatCurrency(amount, code, decimals, lang_);
    }
    std::string date(std::chrono::year_month_day d, DateStyle style = DateStyle::Medium) const {
        return formatDate(d, style, lang_);
    }
    std::string time(int hour, int minute, int seconds = -1) const { return formatTime(hour, minute, seconds, lang_); }
    std::string relative(std::chrono::seconds offset) const { return formatRelative(offset, lang_); }

private:
    std::string lang_;
};

// The translator for the current language; the component re-renders when the
// language changes or messages are added.
Translator useTranslation();
// Just the current language code, with the same re-render.
std::string useLanguage();

// A ready language picker (rx::ui::select over languages()).
struct LanguagePickerProps {
    std::string label{};
    bool labelAbove = false;
    std::string key{};
};
Element languagePicker(LanguagePickerProps props = {});

}  // namespace rx::i18n
