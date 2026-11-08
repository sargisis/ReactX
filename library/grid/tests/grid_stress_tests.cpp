// Stress tests of reactx::grid: random tables checked against a plain
// reference, the natural order on random and huge inputs, 20,000 rows, and a
// user clicking and typing fast. `ctest -L stress`.
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <random>
#include <string>
#include <vector>

#include "reactx/testing.hpp"
#include "rx/grid.hpp"

using namespace rx;

namespace {

struct Row {
    int index = 0;  // the place in the input: shows whether sorting kept equal rows in order
    std::string name, status, num;
};

std::mt19937 rng(20261108);

std::string randomText(std::size_t maxLen, std::string_view alphabet = "aAbBzZ09 -_.") {
    std::uniform_int_distribution<std::size_t> len(0, maxLen), pick(0, alphabet.size() - 1);
    std::string s(len(rng), ' ');
    for (char& c : s) c = alphabet[pick(rng)];
    return s;
}

std::vector<Row> randomRows(std::size_t n) {
    static const char* statuses[] = {"Open", "Closed", "Waiting", "open", "Blocked"};
    std::uniform_int_distribution<int> st(0, 4), num(0, 3000);
    std::vector<Row> rows;
    for (std::size_t i = 0; i < n; ++i)
        rows.push_back({static_cast<int>(i), randomText(6), statuses[st(rng)], "N-" + std::to_string(num(rng))});
    return rows;
}

std::vector<grid::Column<Row>> columns() {
    return {
        {.title = "Name", .text = [](const Row& r) { return r.name; }},
        {.title = "Status", .text = [](const Row& r) { return r.status; }, .order = {"Open", "Waiting", "Blocked"}},
        {.title = "Num", .text = [](const Row& r) { return r.num; }},
        {.title = "Index", .text = [](const Row& r) { return std::to_string(r.index); }, .searchable = false},
    };
}

// What useGrid should give, written the plain way.
std::vector<int> reference(const std::vector<Row>& rows, const std::vector<grid::Column<Row>>& cols, const std::string& search,
                           const std::function<bool(const Row&)>& filter, int sortColumn, bool descending, int pageSize, int page,
                           int* matchingOut, int* pagesOut) {
    std::vector<const Row*> m;
    for (const Row& r : rows) {
        if (filter && !filter(r)) continue;
        bool ok = search.empty();
        for (const auto& c : cols)
            if (!ok && c.searchable && grid::containsIgnoringCase(c.text(r), search)) ok = true;
        if (ok) m.push_back(&r);
    }
    if (sortColumn >= 0) {
        const auto& c = cols[static_cast<std::size_t>(sortColumn)];
        std::stable_sort(m.begin(), m.end(), [&](const Row* a, const Row* b) {
            std::string ta = c.text(*a), tb = c.text(*b);
            int r = 0;
            if (!c.order.empty()) {
                auto ra = grid::rank(c.order, ta), rb = grid::rank(c.order, tb);
                r = ra < rb ? -1 : rb < ra ? 1 : 0;
            }
            if (r == 0) r = grid::compareNatural(ta, tb);
            return descending ? r > 0 : r < 0;
        });
    }
    const int matching = static_cast<int>(m.size());
    const int size = pageSize > 0 ? pageSize : std::max(matching, 1);
    const int pages = std::max(1, (matching + size - 1) / size);
    page = std::clamp(page, 0, pages - 1);
    std::vector<int> out;
    for (int i = page * size; i < std::min(matching, (page + 1) * size); ++i) out.push_back(m[static_cast<std::size_t>(i)]->index);
    *matchingOut = matching;
    *pagesOut = pages;
    return out;
}

// A component that only runs the hook and hands the result out.
std::vector<Row>* probeRows = nullptr;
grid::GridOptions<Row> probeOptions;
grid::Grid<Row> last;

Element Probe(const Props&) {
    last = grid::useGrid<Row>(*probeRows, probeOptions);
    return last.view();
}

std::vector<int> indexes(const grid::Grid<Row>& g) {
    std::vector<int> out;
    for (const Row* r : g.rows) out.push_back(r->index);
    return out;
}

int sign(int x) { return (x > 0) - (x < 0); }

}  // namespace

RX_TEST(natural_order_is_a_strict_weak_order_on_random_text) {
    std::vector<std::string> v;
    for (int i = 0; i < 1500; ++i) v.push_back(randomText(8, "aAb0129 -"));
    for (std::size_t i = 0; i < v.size(); ++i)
        for (std::size_t j = 0; j < v.size(); j += 7) {
            CHECK(sign(grid::compareNatural(v[i], v[j])) == -sign(grid::compareNatural(v[j], v[i])));
            if (v[i] == v[j]) CHECK(grid::compareNatural(v[i], v[j]) == 0);
        }
    std::ranges::sort(v, [](const std::string& a, const std::string& b) { return grid::compareNatural(a, b) < 0; });
    int bad = 0;  // after sorting, nothing later may compare smaller than something earlier (transitivity)
    for (std::size_t i = 0; i < v.size(); ++i)
        for (std::size_t j = i + 1; j < v.size(); ++j) bad += grid::compareNatural(v[i], v[j]) > 0;
    CHECK(bad == 0);
}

RX_TEST(natural_order_on_huge_and_odd_numbers) {
    const std::string big(5000, '9'), bigger = "1" + std::string(5000, '0');
    CHECK(grid::compareNatural("x" + big, "x" + bigger) < 0);
    CHECK(grid::compareNatural("x" + std::string(3000, '0') + "7", "x8") < 0);  // leading zeros don't count
    CHECK(grid::compareNatural("INC-2", "INC-10") < 0);
    CHECK(grid::compareNatural("inc-2", "INC-2") == 0);
    CHECK(grid::compareNatural("", "") == 0);
    CHECK(grid::compareNatural("", "0") < 0);
    CHECK(grid::compareNatural("a", "a0") < 0);
    CHECK(grid::containsIgnoringCase(std::string(100000, 'a') + "NeEdLe", "needle"));
    CHECK(!grid::containsIgnoringCase("abc", "abcd"));
}

RX_TEST(random_tables_match_a_plain_reference) {
    for (int round = 0; round < 250; ++round) {
        std::uniform_int_distribution<int> nRows(0, 120), col(-1, 3), pageSize(0, 12), flip(0, 1), page(-2, 15);
        std::vector<Row> rows = randomRows(static_cast<std::size_t>(nRows(rng)));
        const auto cols = columns();
        const std::string search = flip(rng) ? randomText(2, "aAb0- ") : std::string{};
        std::function<bool(const Row&)> filter;
        if (flip(rng)) filter = [](const Row& r) { return r.index % 3 != 0; };
        const int sortColumn = col(rng), size = pageSize(rng), wantPage = page(rng);
        const bool desc = flip(rng);

        probeRows = &rows;
        probeOptions = {.columns = cols,
                        .id = [](const Row& r) { return std::to_string(r.index); },
                        .filter = filter,
                        .pageSize = size,
                        .sortColumn = sortColumn,
                        .sortDescending = desc};
        testing::Screen s(h(Probe), 100, 40);
        last.setSearch(search);
        s.settle();
        last.setPage(wantPage);
        s.settle();
        int matching = 0, pages = 0;
        const auto want = reference(rows, cols, search, filter, sortColumn, desc, size, wantPage, &matching, &pages);
        CHECK(indexes(last) == want);
        CHECK(last.matching == matching);
        CHECK(last.pages == pages);
        CHECK(last.total == static_cast<int>(rows.size()));
        if (indexes(last) != want) {
            std::printf("  round %d: rows %zu search \"%s\" sort %d%s page size %d page %d\n", round, rows.size(),
                        search.c_str(), sortColumn, desc ? " desc" : "", size, wantPage);
            break;
        }
    }
}

RX_TEST(twenty_thousand_rows_sort_and_page) {
    std::vector<Row> rows = randomRows(20000);
    probeRows = &rows;
    probeOptions = {.columns = columns(), .id = [](const Row& r) { return std::to_string(r.index); }, .pageSize = 50};
    testing::Screen s(h(Probe), 120, 70);
    for (int c = 0; c < 4; ++c)
        for (int twice = 0; twice < 2; ++twice) {  // ascending, then descending
            auto t0 = std::chrono::steady_clock::now();
            last.sortBy(c);
            s.settle();
            const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
            std::printf("  20,000 rows, sort by column %d %s: %.1f ms\n", c, twice ? "desc" : "asc", ms);
            int matching = 0, pages = 0;
            CHECK(indexes(last) == reference(rows, probeOptions.columns, "", {}, c, twice == 1, 50, 0, &matching, &pages));
            CHECK(last.pages == 400);
        }
    last.setPage(399);
    s.settle();
    CHECK(last.rows.size() == 50);
    CHECK(s.shows("19951–20000 of 20000"));
}

RX_TEST(fast_clicks_and_typing_keep_the_selection) {
    std::vector<Row> rows = randomRows(300);
    probeRows = &rows;
    probeOptions = {.columns = columns(), .id = [](const Row& r) { return std::to_string(r.index); }, .pageSize = 10, .select = true};
    testing::Screen s(h(Probe), 120, 30);
    for (int i = 0; i < 10; ++i) last.toggle(std::to_string(i * 7));
    s.settle();
    CHECK(last.selected.size() == 10);
    for (int i = 0; i < 200; ++i) {
        last.sortBy(i % 4);
        if (i % 5 == 0) last.setPage(i % 30);
        s.settle();
    }
    CHECK(last.selected.size() == 10);  // sorting and paging never drop the selection
    // The search box has the focus (the first field): type into it.
    for (int i = 0; i < 60; ++i) s.type(i % 3 ? "a" : "0");
    for (int i = 0; i < 60; ++i) s.press("Backspace");
    s.settle();
    CHECK(last.search.empty());
    CHECK(last.matching == 300);
    CHECK(last.page == 0);  // a new search starts at the first page
    for (int i = 0; i < 40; ++i) s.click("Next ›");
    CHECK(last.page == last.pages - 1);
    CHECK(last.selected.size() == 10);
}
