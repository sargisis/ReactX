#include "rx/grid.hpp"

#include <cctype>

namespace rx::grid {

namespace {
char lower(char c) { return static_cast<char>(std::tolower(static_cast<unsigned char>(c))); }
bool digit(char c) { return c >= '0' && c <= '9'; }
}  // namespace

int compareNatural(std::string_view a, std::string_view b) {
    std::size_t i = 0, j = 0;
    while (i < a.size() && j < b.size()) {
        if (digit(a[i]) && digit(b[j])) {
            // Numbers: skip leading zeros, the longer one is bigger, then digit by digit.
            std::size_t si = i, sj = j;
            while (si < a.size() && a[si] == '0') ++si;
            while (sj < b.size() && b[sj] == '0') ++sj;
            std::size_t ei = si, ej = sj;
            while (ei < a.size() && digit(a[ei])) ++ei;
            while (ej < b.size() && digit(b[ej])) ++ej;
            if (ei - si != ej - sj) return ei - si < ej - sj ? -1 : 1;
            for (std::size_t k = 0; k < ei - si; ++k)
                if (a[si + k] != b[sj + k]) return a[si + k] < b[sj + k] ? -1 : 1;
            i = ei;
            j = ej;
            continue;
        }
        const char x = lower(a[i]), y = lower(b[j]);
        if (x != y) return x < y ? -1 : 1;
        ++i;
        ++j;
    }
    if (i < a.size()) return 1;
    if (j < b.size()) return -1;
    return 0;
}

bool containsIgnoringCase(std::string_view text, std::string_view part) {
    if (part.empty()) return true;
    if (part.size() > text.size()) return false;
    for (std::size_t i = 0; i + part.size() <= text.size(); ++i) {
        std::size_t k = 0;
        while (k < part.size() && lower(text[i + k]) == lower(part[k])) ++k;
        if (k == part.size()) return true;
    }
    return false;
}

std::size_t rank(const std::vector<std::string>& order, std::string_view value) {
    for (std::size_t i = 0; i < order.size(); ++i)
        if (order[i] == value) return i;
    return order.size();
}

namespace detail {

Element searchBox(const std::string& value, std::function<void(std::string)> onChange, const std::string& placeholder) {
    return ui::input({
        .value = value,
        .onChange = [onChange = std::move(onChange)](const std::string& text) { onChange(text); },
        .placeholder = placeholder,
    });
}

Element pager(int page, int pages, int first, int last, int matching, std::function<void(int)> setPage) {
    // "1–20 of 312" (no std::format: it would add ~100 KB to a .wasm).
    std::string range = matching == 0 ? std::string("0 rows")
                                      : std::to_string(first) + "–" + std::to_string(last) + " of " + std::to_string(matching);
    std::vector<Element> parts{ui::text({.text = std::move(range), .muted = true})};
    if (pages > 1) {
        parts.push_back(ui::button({.label = "‹ Prev",
                                    .onClick = [setPage, page] { setPage(page - 1); },
                                    .disabled = page == 0}));
        parts.push_back(ui::text({.text = std::to_string(page + 1) + " / " + std::to_string(pages)}));
        parts.push_back(ui::button({.label = "Next ›",
                                    .onClick = [setPage, page] { setPage(page + 1); },
                                    .disabled = page + 1 >= pages}));
    }
    return ui::stack({.direction = ui::Direction::Row, .gap = 1, .align = ui::Align::Center, .children = std::move(parts)});
}

}  // namespace detail
}  // namespace rx::grid
