// rx-i18n: the language state, the message format, numbers and dates.
// Numbers become text with integer arithmetic and std::to_string: std::format
// and floating-point to_chars would add 100+ KB to every .wasm.
#include "rx/i18n.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "reactx/fetch.hpp"
#include "reactx/json.hpp"
#include "reactx/ui.hpp"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

namespace rx::i18n {

namespace {

#ifdef __EMSCRIPTEN__
EM_JS(char *, rx_i18n_browser_language, (), {
  const l = typeof navigator !=
      = "undefined" && navigator.language ? navigator.language : "";
  return stringToNewUTF8(l);
});
// <html lang dir>; not in a browser (prerendering under Node) it does nothing.
EM_JS(void, rx_i18n_set_document_language, (const char *lang, int rtl), {
  if (typeof document == = "undefined")
    return;
  const html = document.documentElement;
  html.setAttribute("lang", UTF8ToString(lang));
  if (rtl)
    html.setAttribute("dir", "rtl");
  else if (html.getAttribute("dir") == = "rtl")
    html.removeAttribute("dir");
});
#endif

struct LanguageData {
  std::string code;
  std::string name;
  Messages messages;
  const Locale *locale = nullptr;
};

struct State {
  std::vector<LanguageData> languages;
  std::string current;
  std::string fallback;
  std::string storageKey;
  std::function<void(std::string_view, std::string_view)> onMissing;
  std::set<std::string, std::less<>>
      reported; // "lang\nkey" already passed to onMissing
};

State &state() {
  static State s;
  return s;
}

// Bumped on every change of the language or of the messages; components read
// it.
const Store<std::uint64_t> &revision() {
  static const Store<std::uint64_t> store = createStore<std::uint64_t>(0);
  return store;
}

void changed() {
  revision().update([](std::uint64_t v) { return v + 1; });
}

char lower(char c) {
  return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c;
}

// "ru_RU.UTF-8" -> "ru-RU"; "C" and "POSIX" -> "".
std::string normalize(std::string_view lang) {
  std::string out;
  for (char c : lang) {
    if (c == '.' || c == '@')
      break;
    out += c == '_' ? '-' : c;
  }
  if (out == "C" || out == "POSIX")
    return {};
  return out;
}

bool sameCode(std::string_view a, std::string_view b) {
  return a.size() == b.size() && std::ranges::equal(a, b, [](char x, char y) {
           return lower(x) == lower(y);
         });
}

std::string_view base(std::string_view code) {
  return code.substr(0, code.find('-'));
}

// The listed language for a wanted code: the same code, else the same base
// language ("ru-RU" -> "ru", "pt" -> "pt-BR").
LanguageData *find(std::string_view wanted) {
  auto &langs = state().languages;
  const std::string code = normalize(wanted);
  if (code.empty())
    return nullptr;
  for (auto &l : langs)
    if (sameCode(l.code, code))
      return &l;
  for (auto &l : langs)
    if (sameCode(base(l.code), base(code)))
      return &l;
  return nullptr;
}

const Locale &localeOf(std::string_view lang) {
  if (lang.empty())
    lang = state().current;
  for (auto &l : state().languages)
    if (l.locale && sameCode(l.code, lang))
      return *l.locale;
  return locale(lang);
}

[[maybe_unused]] bool rightToLeft(std::string_view code) {
  for (std::string_view rtl : {"ar", "he", "fa", "ur", "yi"})
    if (sameCode(base(code), rtl))
      return true;
  return false;
}

void applyToDocument([[maybe_unused]] const std::string &code) {
#ifdef __EMSCRIPTEN__
  rx_i18n_set_document_language(code.c_str(), rightToLeft(code) ? 1 : 0);
#endif
}

const std::string *lookup(std::string_view lang, std::string_view key) {
  auto &s = state();
  for (auto &l : s.languages)
    if (sameCode(l.code, lang))
      if (auto it = l.messages.find(key); it != l.messages.end())
        return &it->second;
  if (s.onMissing) {
    std::string id = std::string(lang) + "\n" + std::string(key);
    if (!s.reported.contains(id)) {
      s.reported.insert(std::move(id));
      s.onMissing(lang, key);
    }
  }
  if (!sameCode(lang, s.fallback))
    for (auto &l : s.languages)
      if (sameCode(l.code, s.fallback))
        if (auto it = l.messages.find(key); it != l.messages.end())
          return &it->second;
  return nullptr;
}

// ---- numbers ----

std::string digitsOf(std::uint64_t n) { return std::to_string(n); }

// The integer digits and the fraction digits of |value| rounded to maxDecimals.
std::pair<std::string, std::string> split(double value, int maxDecimals,
                                          int minDecimals) {
  maxDecimals = std::clamp(maxDecimals, 0, 15);
  minDecimals = std::clamp(minDecimals, 0, maxDecimals);
  double scale = 1;
  for (int i = 0; i < maxDecimals; ++i)
    scale *= 10;
  while (maxDecimals > 0 &&
         value * scale >= 9e18) { // fewer decimals so it fits in 64 bits
    --maxDecimals;
    scale /= 10;
  }
  std::string whole, fraction;
  if (value * scale < 9e18) {
    const auto scaled = static_cast<std::uint64_t>(std::llround(value * scale));
    const auto unit = static_cast<std::uint64_t>(scale);
    whole = digitsOf(scaled / unit);
    if (maxDecimals > 0) {
      fraction = digitsOf(scaled % unit);
      fraction.insert(
          0, static_cast<std::size_t>(maxDecimals) - fraction.size(), '0');
    }
  } else { // above 9e18: the digits one by one (a double has ~16 exact ones
           // anyway)
    double v = std::floor(value);
    while (v >= 1) {
      const double d = std::fmod(v, 10);
      whole.insert(whole.begin(), static_cast<char>('0' + static_cast<int>(d)));
      v = std::floor(v / 10);
    }
  }
  while (static_cast<int>(fraction.size()) > minDecimals &&
         fraction.back() == '0')
    fraction.pop_back();
  return {whole, fraction};
}

std::string grouped(const std::string &whole, const Locale &l, bool grouping) {
  if (!grouping || l.group.empty() ||
      whole.size() < static_cast<std::size_t>(3 + std::max(l.minGrouping, 1)))
    return whole;
  std::string out;
  const std::size_t first = whole.size() % 3 == 0 ? 3 : whole.size() % 3;
  out += whole.substr(0, first);
  for (std::size_t i = first; i < whole.size(); i += 3)
    out += l.group + whole.substr(i, 3);
  return out;
}

// The number without its sign, and whether it is negative (a -0 isn't).
std::pair<std::string, bool> unsignedNumber(double value, NumberFormat f,
                                            const Locale &l) {
  if (std::isnan(value))
    return {"NaN", false};
  if (std::isinf(value))
    return {"∞", value < 0};
  auto [whole, fraction] =
      split(std::fabs(value), f.maxDecimals, f.minDecimals);
  std::string text = grouped(whole, l, f.grouping);
  if (!fraction.empty())
    text += l.decimal + fraction;
  const bool zero =
      std::ranges::all_of(whole, [](char c) { return c == '0'; }) &&
      std::ranges::all_of(fraction, [](char c) { return c == '0'; });
  return {text, value < 0 && !zero};
}

std::string replaceZero(const std::string &pattern, const std::string &value) {
  std::string out = pattern;
  if (auto at = out.find("{0}"); at != std::string::npos)
    out.replace(at, 3, value);
  return out;
}

// ---- the message format ----

class Formatter {
public:
  Formatter(const Args &args, std::string_view lang)
      : args_(args), lang_(lang) {}

  void run(std::string_view s, const Arg *hash, std::string &out) const {
    for (std::size_t i = 0; i < s.size();) {
      const char c = s[i];
      if (c == '\'') {
        if (i + 1 < s.size() && s[i + 1] == '\'') { // '' -> '
          out += '\'';
          i += 2;
          continue;
        }
        if (i + 1 < s.size() &&
            (s[i + 1] == '{' || s[i + 1] == '}' || (hash && s[i + 1] == '#'))) {
          const std::size_t end =
              s.find('\'', i + 1); // quoted text up to the next '
          const std::size_t stop =
              end == std::string_view::npos ? s.size() : end;
          out += s.substr(i + 1, stop - i - 1);
          i = stop == s.size() ? stop : stop + 1;
          continue;
        }
        out += c;
        ++i;
      } else if (c == '{') {
        const std::size_t end = closing(s, i);
        if (end == std::string_view::npos) { // unbalanced: the rest as it is
          out += s.substr(i);
          return;
        }
        placeholder(s.substr(i + 1, end - i - 1), hash, out);
        i = end + 1;
      } else if (c == '#' && hash) {
        out += value(*hash);
        ++i;
      } else {
        out += c;
        ++i;
      }
    }
  }

private:
  // The '}' that closes the '{' at `open`, or npos.
  static std::size_t closing(std::string_view s, std::size_t open) {
    int depth = 0;
    bool quoted = false;
    for (std::size_t i = open; i < s.size(); ++i) {
      const char c = s[i];
      if (c == '\'') {
        if (i + 1 < s.size() && s[i + 1] == '\'')
          ++i;
        else if (quoted ||
                 (i + 1 < s.size() && (s[i + 1] == '{' || s[i + 1] == '}')))
          quoted = !quoted;
      } else if (!quoted && c == '{') {
        ++depth;
      } else if (!quoted && c == '}' && --depth == 0) {
        return i;
      }
    }
    return std::string_view::npos;
  }

  static std::string_view trim(std::string_view s) {
    while (!s.empty() &&
           (s.front() == ' ' || s.front() == '\n' || s.front() == '\t'))
      s.remove_prefix(1);
    while (!s.empty() &&
           (s.back() == ' ' || s.back() == '\n' || s.back() == '\t'))
      s.remove_suffix(1);
    return s;
  }

  const Arg *arg(std::string_view name) const {
    for (const auto &a : args_)
      if (a.name == name)
        return &a;
    return nullptr;
  }

  std::string value(const Arg &a) const {
    return a.isNumber ? formatNumber(a.number, {}, lang_) : a.text;
  }

  void placeholder(std::string_view body, const Arg *hash,
                   std::string &out) const {
    const std::size_t comma = body.find(',');
    const std::string_view name = trim(body.substr(0, comma));
    const Arg *a = arg(name);
    if (comma == std::string_view::npos) {
      if (a)
        out += value(*a);
      else
        out += "{" + std::string(body) + "}"; // shows what is missing
      return;
    }
    std::string_view rest = body.substr(comma + 1);
    const std::size_t comma2 = rest.find(',');
    const std::string_view kind = trim(rest.substr(0, comma2));
    const std::string_view options = comma2 == std::string_view::npos
                                         ? std::string_view{}
                                         : rest.substr(comma2 + 1);
    if (kind == "number") {
      if (a)
        out += value(*a);
      return;
    }
    if (kind != "plural" && kind != "select") {
      out += "{" + std::string(body) + "}";
      return;
    }
    const bool plural = kind == "plural";
    std::string exact, category;
    if (plural && a && a->isNumber) {
      exact = "=" + split(std::fabs(a->number), 3, 0).first;
      if (a->number < 0)
        exact.insert(1, "-");
      if (a->number != std::floor(a->number))
        exact.clear(); // =N matches whole numbers only
      category = pluralCategory(a->number, lang_);
    } else if (a) {
      category = a->text;
    }
    std::optional<std::string_view> chosen, other;
    std::size_t i = 0;
    while (i < options.size()) {
      while (i < options.size() &&
             (options[i] == ' ' || options[i] == '\n' || options[i] == '\t'))
        ++i;
      const std::size_t brace = options.find('{', i);
      if (brace == std::string_view::npos)
        break;
      const std::string_view selector = trim(options.substr(i, brace - i));
      const std::size_t end = closing(options, brace);
      if (end == std::string_view::npos)
        break;
      const std::string_view message =
          options.substr(brace + 1, end - brace - 1);
      if (selector == "other")
        other = message;
      if (!exact.empty() && selector == exact)
        chosen = message; // =N wins over the category
      else if (!chosen && !category.empty() && selector == category)
        chosen = message;
      i = end + 1;
    }
    // Russian "many" written as "other" is the usual short form.
    if (const auto m = chosen ? chosen : other)
      run(*m, plural ? a : hash, out);
  }

  const Args &args_;
  std::string_view lang_;
};

std::string pad2(unsigned v) { return (v < 10 ? "0" : "") + std::to_string(v); }

const std::string &at(const std::vector<std::string> &list, std::size_t i,
                      const std::vector<std::string> &english) {
  if (i < list.size())
    return list[i];
  static const std::string none;
  return i < english.size() ? english[i] : none;
}

const Locale &englishLocale() { return locale("en"); }

std::string currencySymbol(std::string_view code, std::string_view lang) {
  struct Symbol {
    std::string_view code, symbol;
  };
  static constexpr Symbol symbols[] = {
      {"USD", "$"}, {"EUR", "€"}, {"GBP", "£"},  {"RUB", "₽"},
      {"UAH", "₴"}, {"AMD", "֏"}, {"PLN", "zł"}, {"JPY", "¥"},
      {"CNY", "¥"}, {"INR", "₹"}, {"KRW", "₩"},
  };
  if (code == "BRL" && sameCode(base(lang), "pt"))
    return "R$";
  for (const auto &s : symbols)
    if (sameCode(s.code, code))
      return std::string(s.symbol);
  std::string upper(code);
  for (char &c : upper)
    c = c >= 'a' && c <= 'z' ? static_cast<char>(c - 'a' + 'A') : c;
  return upper;
}

std::string langOrCurrent(std::string_view lang) {
  return lang.empty() ? state().current : std::string(lang);
}

} // namespace

// ---- the language ----

void setup(Options options) {
  auto &s = state();
  s.languages.clear();
  s.reported.clear();
  for (auto &l : options.languages)
    s.languages.push_back({.code = std::move(l.code),
                           .name = std::move(l.name),
                           .messages = std::move(l.messages),
                           .locale = l.locale});
  s.storageKey = std::move(options.storageKey);
  s.onMissing = std::move(options.onMissing);
  const LanguageData *fallback = find(options.fallback);
  if (!fallback && !s.languages.empty())
    fallback = &s.languages.front();
  s.fallback = fallback ? fallback->code : "en";

  const LanguageData *first = find(options.initial);
  if (!first && !s.storageKey.empty())
    if (auto saved = storageRead(s.storageKey); saved && *saved)
      first = find(**saved);
  if (!first)
    first = find(systemLanguage());
  s.current = first ? first->code : s.fallback;
  applyToDocument(s.current);
  changed();
}

void addMessages(std::string_view lang, Messages messages) {
  auto &s = state();
  LanguageData *target = nullptr;
  for (auto &l : s.languages)
    if (sameCode(l.code, lang))
      target = &l;
  if (!target) {
    s.languages.push_back(
        {.code = std::string(lang), .name = std::string(lang), .messages = {}});
    target = &s.languages.back();
    if (s.current.empty())
      s.current = target->code;
  }
  for (auto &[key, text] : messages)
    target->messages.insert_or_assign(key, std::move(text));
  std::erase_if(s.reported, [&](const std::string &id) {
    return id.starts_with(target->code + "\n");
  });
  changed();
}

const std::string &language() { return state().current; }

void setLanguage(std::string_view code) {
  auto &s = state();
  const LanguageData *l = find(code);
  if (!l || l->code == s.current)
    return;
  s.current = l->code;
  if (!s.storageKey.empty())
    (void)storageWrite(s.storageKey, s.current); // not saved: still switched
  applyToDocument(s.current);
  changed();
}

std::vector<LanguageInfo> languages() {
  std::vector<LanguageInfo> out;
  for (const auto &l : state().languages)
    out.push_back({.code = l.code, .name = l.name.empty() ? l.code : l.name});
  return out;
}

std::string systemLanguage() {
#ifdef __EMSCRIPTEN__
  char *raw = rx_i18n_browser_language();
  std::string lang = raw ? raw : "";
  std::free(raw);
  return normalize(lang);
#else
  for (const char *name : {"LC_ALL", "LC_MESSAGES", "LANG"})
    if (const char *v = std::getenv(name); v && *v) {
      std::string lang = normalize(v);
      if (!lang.empty())
        return lang;
    }
  return {};
#endif
}

// ---- messages ----

std::string format(std::string_view message, const Args &args,
                   std::string_view lang) {
  const std::string code = langOrCurrent(lang);
  std::string out;
  Formatter(args, code).run(message, nullptr, out);
  return out;
}

std::string t(std::string_view key, const Args &args) {
  return Translator(state().current)(key, args);
}

std::string Translator::operator()(std::string_view key,
                                   const Args &args) const {
  const std::string *message = lookup(lang_, key);
  if (!message)
    return std::string(key);
  return i18n::format(*message, args, lang_);
}

std::expected<Messages, std::string> parseMessages(std::string_view text) {
  auto parsed = json::tryParse(text);
  if (!parsed)
    return std::unexpected("invalid JSON: " +
                           std::string(parsed.error().what()));
  if (!parsed->isObject())
    return std::unexpected(std::string("the messages must be a JSON object"));
  Messages out;
  std::string error;
  auto walk = [&](auto &self, const json::Object &object,
                  const std::string &prefix) -> void {
    for (const auto &[name, value] : object) {
      const std::string key = prefix.empty() ? name : prefix + "." + name;
      if (value.isObject())
        self(self, value.asObject(), key);
      else if (value.isString())
        out.insert_or_assign(key, value.asString());
      else if (error.empty())
        error = "\"" + key + "\" is not a string";
    }
  };
  walk(walk, parsed->asObject(), "");
  if (!error.empty())
    return std::unexpected(error);
  return out;
}

FetchHandle loadMessages(std::string lang, std::string url,
                         std::function<void(const std::string &)> done) {
  return fetch(
      {.url = std::move(url)},
      [lang = std::move(lang), done = std::move(done)](const FetchResponse &r) {
        std::string error;
        if (!r.ok()) {
          error = r.errorText();
        } else if (auto messages = parseMessages(r.body)) {
          addMessages(lang, std::move(*messages));
        } else {
          error = messages.error();
        }
        if (done)
          done(error);
      });
}

// ---- numbers and dates ----

std::string formatNumber(double value, NumberFormat format,
                         std::string_view lang) {
  auto [text, negative] = unsignedNumber(value, format, localeOf(lang));
  return negative ? "-" + text : text;
}

std::string formatPercent(double fraction, int maxDecimals,
                          std::string_view lang) {
  const Locale &l = localeOf(lang);
  auto [text, negative] =
      unsignedNumber(fraction * 100, {.maxDecimals = maxDecimals}, l);
  return (negative ? "-" : "") + replaceZero(l.percent, text);
}

std::string formatCurrency(double amount, std::string_view currency,
                           int decimals, std::string_view lang) {
  const std::string code = langOrCurrent(lang);
  const Locale &l = localeOf(code);
  if (decimals < 0)
    decimals = sameCode(currency, "JPY") || sameCode(currency, "KRW") ? 0 : 2;
  auto [text, negative] = unsignedNumber(
      amount, {.maxDecimals = decimals, .minDecimals = decimals}, l);
  const std::string symbol = currencySymbol(currency, code);
  // A code ("CHF") always gets a gap: "CHF 5.00".
  const std::string gap = l.currencyGap.empty() && symbol.size() == 3 &&
                                  symbol[0] >= 'A' && symbol[0] <= 'Z'
                              ? " "
                              : l.currencyGap;
  const std::string body =
      l.currencyAfter ? text + gap + symbol : symbol + gap + text;
  return negative ? "-" + body : body;
}

std::string formatDate(std::chrono::year_month_day date, DateStyle style,
                       std::string_view lang) {
  if (!date.ok())
    return {};
  const Locale &l = localeOf(lang);
  const Locale &en = englishLocale();
  const std::string &pattern = style == DateStyle::Short    ? l.dateShort
                               : style == DateStyle::Medium ? l.dateMedium
                               : style == DateStyle::Long   ? l.dateLong
                                                            : l.dateFull;
  const unsigned d = static_cast<unsigned>(date.day()),
                 m = static_cast<unsigned>(date.month());
  const unsigned wd =
      std::chrono::weekday(std::chrono::sys_days(date)).c_encoding();
  std::string out;
  for (std::size_t i = 0; i < pattern.size();) {
    const std::size_t end =
        pattern[i] == '{' ? pattern.find('}', i) : std::string::npos;
    if (end == std::string::npos) {
      out += pattern[i++];
      continue;
    }
    const std::string_view token =
        std::string_view(pattern).substr(i + 1, end - i - 1);
    if (token == "d")
      out += std::to_string(d);
    else if (token == "dd")
      out += pad2(d);
    else if (token == "M")
      out += std::to_string(m);
    else if (token == "MM")
      out += pad2(m);
    else if (token == "MMM")
      out += at(l.monthsShort, m - 1, en.monthsShort);
    else if (token == "MMMM")
      out += at(l.months, m - 1, en.months);
    else if (token == "y")
      out += std::to_string(static_cast<int>(date.year()));
    else if (token == "EEEE")
      out += at(l.weekdays, wd, en.weekdays);
    else if (token == "EEE")
      out += at(l.weekdaysShort, wd, en.weekdaysShort);
    else
      out += pattern.substr(i, end - i + 1);
    i = end + 1;
  }
  return out;
}

std::string formatTime(int hour, int minute, int seconds,
                       std::string_view lang) {
  const Locale &l = localeOf(lang);
  hour = ((hour % 24) + 24) % 24;
  const auto mm = pad2(static_cast<unsigned>(std::clamp(minute, 0, 59)));
  const std::string ss =
      seconds >= 0 ? ":" + pad2(static_cast<unsigned>(std::min(seconds, 59)))
                   : "";
  if (l.hour12)
    return std::to_string(hour % 12 == 0 ? 12 : hour % 12) + ":" + mm + ss +
           " " + (hour < 12 ? l.am : l.pm);
  return pad2(static_cast<unsigned>(hour)) + ":" + mm + ss;
}

std::string formatRelative(std::chrono::seconds offset, std::string_view lang) {
  const std::string code = langOrCurrent(lang);
  const Locale &l = localeOf(code);
  const long long total = offset.count();
  const long long s = total < 0 ? -total : total;
  if (s < 5)
    return l.now;
  constexpr long long minute = 60, hour = 3600, day = 86400;
  const std::string *unit = nullptr;
  long long n = 0;
  if (s < minute)
    unit = &l.relSeconds, n = s;
  else if (s < hour)
    unit = &l.relMinutes, n = s / minute;
  else if (s < day)
    unit = &l.relHours, n = s / hour;
  else if (s < 7 * day)
    unit = &l.relDays, n = s / day;
  else if (s < 30 * day)
    unit = &l.relWeeks, n = s / (7 * day);
  else if (s < 365 * day)
    unit = &l.relMonths, n = std::max(1LL, s / (30 * day));
  else
    unit = &l.relYears, n = s / (365 * day);
  const std::string amount = i18n::format(*unit, {{"n", n}}, code);
  return replaceZero(total < 0 ? l.past : l.future, amount);
}

std::string monthName(unsigned month, bool shortName, std::string_view lang) {
  if (month < 1 || month > 12)
    return {};
  const Locale &l = localeOf(lang);
  const Locale &en = englishLocale();
  if (shortName)
    return at(l.monthsShort, month - 1, en.monthsShort);
  return at(l.monthsAlone.empty() ? l.months : l.monthsAlone, month - 1,
            en.months);
}

std::string weekdayName(unsigned weekday, bool shortName,
                        std::string_view lang) {
  if (weekday > 6)
    return {};
  const Locale &l = localeOf(lang);
  const Locale &en = englishLocale();
  return shortName ? at(l.weekdaysShort, weekday, en.weekdaysShort)
                   : at(l.weekdays, weekday, en.weekdays);
}

// ---- in components ----

Translator useTranslation() {
  (void)useStore(revision());
  return Translator(state().current);
}

std::string useLanguage() {
  (void)useStore(revision());
  return state().current;
}

namespace {

Element LanguagePicker(const LanguagePickerProps &p) {
  const std::string current = useLanguage();
  std::vector<ui::Option> options;
  for (auto &l : languages())
    options.push_back({.value = l.code, .label = l.name});
  return ui::select(
      {.label = p.label,
       .value = current,
       .options = std::move(options),
       .onChange = [](const std::string &code) { setLanguage(code); },
       .labelAbove = p.labelAbove});
}

} // namespace

Element languagePicker(LanguagePickerProps props) {
  return h(LanguagePicker, std::move(props));
}

} // namespace rx::i18n
