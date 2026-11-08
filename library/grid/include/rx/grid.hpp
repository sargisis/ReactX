#pragma once

// reactx::grid — a table of your own rows with the parts every data screen
// writes by hand: sorting by a header click (case doesn't matter, "INC-2"
// before "INC-10", your own order for statuses), a search box, filters, pages
// and selected rows. Drawn with ui::table, so it works in the browser and in a
// terminal. Link reactx::grid.
//
//   struct Incident { std::string id, title, severity; int age = 0; };
//
//   auto grid = rx::grid::useGrid<Incident>(incidents, {
//       .columns = {
//           {.title = "ID", .text = [](const Incident& i) { return i.id; }, .width = 10},
//           {.title = "Title", .text = [](const Incident& i) { return i.title; }},
//           {.title = "Severity", .text = [](const Incident& i) { return i.severity; },
//            .order = {"Critical", "High", "Medium", "Low"}},
//           {.title = "Age", .text = [](const Incident& i) { return std::to_string(i.age); }, .align = ui::Align::End},
//       },
//       .id = [](const Incident& i) { return i.id; },
//       .pageSize = 20,
//   });
//   return grid.view();   // search box, table, "1–20 of 312", pages
//
// Or draw the parts yourself: grid.searchBox(), grid.table(), grid.pager(),
// and grid.rows (the rows on this page, sorted and filtered).

#include <algorithm>
#include <cstddef>
#include <functional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "reactx/reactx.hpp"
#include "reactx/ui.hpp"

namespace rx::grid {

template <class T>
struct Column {
    std::string title{};
    std::function<std::string(const T&)> text{};  // what the cell shows (also what search and sorting use)
    // The cell as any element (a badge, a link); `text` still sorts and searches.
    std::function<Element(const T&)> cell{};
    // Sorting: `order` ranks values ("Critical" before "High"; others go last),
    // `compare` (-1 / 0 / 1) decides it all; neither: the text, the way people read it.
    std::vector<std::string> order{};
    std::function<int(const T&, const T&)> compare{};
    int width = 0;                    // characters; 0 = share the room left
    ui::Align align = ui::Align::Start;
    bool sortable = true;
    bool searchable = true;
};

template <class T>
struct GridOptions {
    std::vector<Column<T>> columns{};
    std::function<std::string(const T&)> id{};      // a row's stable id: keeps rows and selection across sorting
    std::function<bool(const T&)> filter{};         // your own filter (a status select, a checkbox), with the search
    int pageSize = 0;                               // rows per page; 0 = all on one page
    bool search = true;                             // the search box in view()
    bool select = false;                            // a checkbox per row (needs `id`)
    std::function<void(const T&)> onRowClick{};     // a click or Enter on a row
    int sortColumn = -1;                            // the first sort (-1 = the rows' own order)
    bool sortDescending = false;
    std::string searchPlaceholder = "Search...";
    std::string emptyText = "Nothing found";
};

template <class T>
struct Grid {
    std::vector<const T*> rows;  // the rows on this page, filtered and sorted (point into your vector)
    int matching = 0;            // rows that pass the search and the filter
    int total = 0;               // all rows
    int page = 0, pages = 1;     // 0-based page, number of pages
    int sortColumn = -1;
    bool sortDescending = false;
    std::string search;
    std::set<std::string> selected;  // ids of the selected rows

    std::function<void(std::string)> setSearch;
    std::function<void(int)> sortBy;    // like a header click: the same column flips, another starts ascending
    std::function<void(int)> setPage;
    std::function<void(const std::string&)> toggle;  // select / unselect a row by id
    std::function<void(std::set<std::string>)> setSelected;

    Element searchBox() const;
    Element table() const;
    Element pager() const;
    Element view() const;  // search box, table and pager in a column

    // ---- inside ----
    GridOptions<T> options;
};

// ---- helpers (grid.cpp), usable on their own ----

// -1 / 0 / 1 the way people sort: case doesn't matter, numbers inside text
// compare as numbers ("INC-2" < "INC-10", "file9" < "file10").
int compareNatural(std::string_view a, std::string_view b);
bool containsIgnoringCase(std::string_view text, std::string_view part);
// Place of `value` in `order` (order.size() if it isn't there).
std::size_t rank(const std::vector<std::string>& order, std::string_view value);

namespace detail {
Element searchBox(const std::string& value, std::function<void(std::string)> onChange, const std::string& placeholder);
Element pager(int page, int pages, int first, int last, int matching, std::function<void(int)> setPage);
}  // namespace detail

// ---- the hook ----

template <class T>
Grid<T> useGrid(const std::vector<T>& rows, GridOptions<T> options) {
    struct SortState {
        int column = -1;
        bool descending = false;
        bool operator==(const SortState&) const = default;
    };
    auto [sort, setSort] = useState(SortState{options.sortColumn, options.sortDescending});
    auto [search, setSearchState] = useState(std::string{});
    auto [page, setPageState] = useState(0);
    auto [selected, setSelectedState] = useState(std::set<std::string>{});

    Grid<T> g;
    g.total = static_cast<int>(rows.size());
    g.search = search;
    g.sortColumn = sort.column;
    g.sortDescending = sort.descending;
    g.selected = selected;

    // Search and filter.
    std::vector<const T*> matching;
    matching.reserve(rows.size());
    for (const T& row : rows) {
        if (options.filter && !options.filter(row)) continue;
        if (!search.empty()) {
            bool found = false;
            for (const auto& c : options.columns)
                if (c.searchable && c.text && containsIgnoringCase(c.text(row), search)) {
                    found = true;
                    break;
                }
            if (!found) continue;
        }
        matching.push_back(&row);
    }

    // Sort (stable: equal rows keep your order). Texts are made once per row, not per comparison.
    if (sort.column >= 0 && sort.column < static_cast<int>(options.columns.size())) {
        const Column<T>& c = options.columns[static_cast<std::size_t>(sort.column)];
        if (c.compare) {
            std::ranges::stable_sort(matching, [&](const T* a, const T* b) {
                int r = c.compare(*a, *b);
                return sort.descending ? r > 0 : r < 0;
            });
        } else if (c.text) {
            std::vector<std::pair<std::string, const T*>> keyed;
            keyed.reserve(matching.size());
            for (const T* row : matching) keyed.emplace_back(c.text(*row), row);
            std::ranges::stable_sort(keyed, [&](const auto& a, const auto& b) {
                int r = 0;
                if (!c.order.empty()) {
                    std::size_t ra = rank(c.order, a.first), rb = rank(c.order, b.first);
                    r = ra < rb ? -1 : rb < ra ? 1 : 0;
                }
                if (r == 0) r = compareNatural(a.first, b.first);
                return sort.descending ? r > 0 : r < 0;
            });
            for (std::size_t i = 0; i < keyed.size(); ++i) matching[i] = keyed[i].second;
        }
    }

    // Page.
    g.matching = static_cast<int>(matching.size());
    const int size = options.pageSize > 0 ? options.pageSize : std::max(g.matching, 1);
    g.pages = std::max(1, (g.matching + size - 1) / size);
    g.page = std::clamp(page, 0, g.pages - 1);
    const auto first = static_cast<std::size_t>(g.page * size);
    const auto last = std::min(matching.size(), first + static_cast<std::size_t>(size));
    if (first < last) g.rows.assign(matching.begin() + static_cast<std::ptrdiff_t>(first),
                                    matching.begin() + static_cast<std::ptrdiff_t>(last));

    // A new search or sort starts at the first page.
    g.setSearch = [setSearchState, setPageState](std::string text) {
        setSearchState(std::move(text));
        setPageState(0);
    };
    g.sortBy = [setSort, setPageState](int column) {
        setSort([column](const SortState& s) {
            return s.column == column ? SortState{column, !s.descending} : SortState{column, false};
        });
        setPageState(0);
    };
    g.setPage = [setPageState](int p) { setPageState(p); };
    g.toggle = [setSelectedState](const std::string& id) {
        setSelectedState([id](const std::set<std::string>& old) {
            auto next = old;
            if (!next.erase(id)) next.insert(id);
            return next;
        });
    };
    g.setSelected = [setSelectedState](std::set<std::string> ids) { setSelectedState(std::move(ids)); };
    g.options = std::move(options);
    return g;
}

// ---- drawing ----

template <class T>
Element Grid<T>::searchBox() const {
    return detail::searchBox(search, setSearch, options.searchPlaceholder);
}

template <class T>
Element Grid<T>::table() const {
    std::vector<ui::TableColumn> columns;
    const bool checks = options.select && options.id;
    if (checks) columns.push_back({.title = "", .width = 4});
    for (const auto& c : options.columns)
        columns.push_back({.title = c.title, .width = c.width, .align = c.align, .sortable = c.sortable});

    std::vector<ui::TableRow> out;
    out.reserve(rows.size());
    for (const T* row : rows) {
        ui::TableRow r;
        const std::string id = options.id ? options.id(*row) : std::string{};
        if (checks) {
            r.cells.push_back(ui::checkbox({.checked = selected.contains(id), .onToggle = [toggle = toggle, id] { toggle(id); }}));
        }
        for (const auto& c : options.columns) {
            if (c.cell) r.cells.push_back(c.cell(*row));
            else r.cells.push_back(Element(c.text ? c.text(*row) : std::string{}));
        }
        r.key = id;
        r.selected = !id.empty() && selected.contains(id);
        if (options.onRowClick) r.onSelect = [click = options.onRowClick, row] { click(*row); };
        else if (checks) r.onSelect = [toggle = toggle, id] { toggle(id); };
        out.push_back(std::move(r));
    }
    const int offset = checks ? 1 : 0;
    return ui::table({
        .columns = std::move(columns),
        .rows = std::move(out),
        .sortColumn = sortColumn >= 0 ? sortColumn + offset : -1,
        .sortDescending = sortDescending,
        .onSort = [sortBy = sortBy, offset](int column) {
            if (column >= offset) sortBy(column - offset);
        },
        .emptyText = options.emptyText,
    });
}

template <class T>
Element Grid<T>::pager() const {
    const int size = options.pageSize > 0 ? options.pageSize : std::max(matching, 1);
    const int first = matching == 0 ? 0 : page * size + 1;
    const int last = std::min(matching, (page + 1) * size);
    return detail::pager(page, pages, first, last, matching, setPage);
}

template <class T>
Element Grid<T>::view() const {
    std::vector<Element> parts;
    if (options.search) parts.push_back(searchBox());
    parts.push_back(table());
    parts.push_back(pager());
    return ui::stack({.direction = ui::Direction::Column, .gap = 1, .children = std::move(parts)});
}

}  // namespace rx::grid
