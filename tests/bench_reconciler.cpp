// Timings of the reconciler on big lists, without a real backend: a renderer
// that only counts calls. Not a test (no pass/fail); run it before and after
// a change to the core:
//   ./build/release/reactx_bench            (a Release build gives real numbers)
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <fstream>
#if defined(__GLIBC__)
#include <malloc.h>
#endif
#include <functional>
#include <numeric>
#include <print>
#include <string>
#include <vector>

#include "reactx/reactx.hpp"
#include "reactx/terminal.hpp"

using namespace rx;

namespace {

// Counts what the core asks the backend to do.
class CountingRenderer : public Renderer {
public:
    std::size_t created = 0, inserts = 0, removes = 0, props = 0, texts = 0;
    NodeHandle rootNode() override { return 1; }
    NodeHandle createElement(std::string_view) override { return ++created + 1; }
    NodeHandle createText(std::string_view) override { return ++created + 1; }
    void setText(NodeHandle, std::string_view) override { ++texts; }
    void setProp(NodeHandle, std::string_view, const PropValue&) override { ++props; }
    void removeProp(NodeHandle, std::string_view) override {}
    void insertChild(NodeHandle, NodeHandle, std::size_t) override { ++inserts; }
    void removeChild(NodeHandle, NodeHandle) override { ++removes; }
    void destroyNode(NodeHandle) override {}
    void reset() { inserts = removes = props = texts = 0; }
};

const auto Theme = createContext<std::string>("dark");

struct RowProps {
    int id = 0;
    bool selected = false;
    std::string key{};
};

Element Row(const RowProps& p) {
    auto [hover, setHover] = useState(false);
    const std::string& theme = useContext(Theme);
    return h("li", {{"class", theme + (p.selected ? " selected" : "")}, {"onClick", [setHover] { setHover(true); }}},
             {"row " + std::to_string(p.id) + (hover ? "*" : "")});
}

struct ListProps {
    std::vector<int> ids{};
    int selected = -1;
};

Element List(const ListProps& p) {
    std::vector<Element> rows;
    rows.reserve(p.ids.size());
    for (int id : p.ids) rows.push_back(h(Row, {.id = id, .selected = id == p.selected, .key = std::to_string(id)}));
    return Theme.provide("dark", {h("ul", {}, std::move(rows))});
}

std::vector<int> range(int from, int to) {
    std::vector<int> v(static_cast<std::size_t>(to - from));
    std::iota(v.begin(), v.end(), from);
    return v;
}

// Bytes the program holds on the heap right now (glibc), 0 elsewhere.
std::size_t heapBytes() {
#if defined(__GLIBC__)
    return mallinfo2().uordblks;
#else
    return 0;
#endif
}

// Resident memory in KB (Linux), 0 elsewhere.
long rssKb() {
    std::ifstream statm("/proc/self/statm");
    long pages = 0, resident = 0;
    statm >> pages >> resident;
    return resident * 4;
}

double millis(const std::function<void()>& f) {
    auto start = std::chrono::steady_clock::now();
    f();
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
}

void report(const char* what, double ms, const CountingRenderer& r) {
    std::println("{:<44} {:>9.2f} ms   inserts {:>6}  removes {:>6}  props {:>6}  texts {:>6}", what, ms, r.inserts,
                 r.removes, r.props, r.texts);
}

// A log viewer in a terminal: rows in an overflow="scroll" box, 20 new rows per update.
struct LogRowProps {
    int n = 0;
    std::string key{};
    bool operator==(const LogRowProps& o) const { return n == o.n; }
};

Element LogRow(const LogRowProps& p) {
    return h("box", {{"direction", "row"}, {"gap", 1}}, {h("text", {{"wrap", false}}, {std::to_string(p.n)}),
                                                       h("text", {{"wrap", false}}, {"INFO"}),
                                                       h("text", {{"wrap", false}}, {"GET /api/users 200 12ms"})});
}

struct LogProps {
    int count = 0;
};

Element Log(const LogProps& p) {
    std::vector<Element> rows;
    for (int i = 0; i < p.count; ++i) rows.push_back(h(memo<LogRow>, {.n = i, .key = std::to_string(i)}));
    return h("box", {{"flexGrow", 1}}, {h("text", {}, {"head"}),
                                       h("box", {{"overflow", "scroll"}, {"border", true}}, std::move(rows))});
}

void terminalLog(int n) {
    std::size_t bytes = 0;
    TerminalRenderer r([&](std::string_view s) { bytes += s.size(); });
    r.setSize(100, 30);
    Root root(r);
    root.render(h(Log, {.count = n}));
    double update = 0, repaint = 0;
    for (int k = 1; k <= 10; ++k) {
        update += millis([&] { root.render(h(Log, {.count = n + 20 * k})); });
        repaint += millis([&] { r.repaint(); });
    }
    std::println("terminal, {:>6} log rows in a scroll box: add 20 rows {:>8.2f} ms, repaint {:>8.2f} ms", n,
                 update / 10, repaint / 10);
}

}  // namespace

int main(int argc, char** argv) {
    int n = argc > 1 ? std::atoi(argv[1]) : 10000;
    std::println("reconciler, {} keyed rows (each: useState + useContext + handler)\n", n);

    CountingRenderer r;
    Root root(r);
    auto ids = range(0, n);

    std::size_t heapBefore = heapBytes();
    report("mount", millis([&] { root.render(h(List, {.ids = ids})); }), r);
    std::size_t heapRows = heapBytes() - heapBefore;

    // Each case starts from the plain list `ids` (not timed), then renders `next`.
    auto change = [&](const char* what, std::vector<int> next, int selected = -1) {
        root.render(h(List, {.ids = ids}));
        r.reset();
        report(what, millis([&] { root.render(h(List, {.ids = std::move(next), .selected = selected})); }), r);
    };
    change("render again, nothing changed", ids);
    change("select one row", ids, n / 2);
    auto appended = ids;
    appended.push_back(n);
    change("append one row", appended);
    auto front = ids;
    front.insert(front.begin(), -1);
    change("insert one row at the front", front);
    auto reversed = ids;
    std::ranges::reverse(reversed);
    change("reverse", reversed);
    auto swapped = ids;
    std::swap(swapped[1], swapped[swapped.size() - 2]);
    change("swap two rows", swapped);
    auto moved = ids;
    std::ranges::rotate(moved, moved.begin() + 1);
    change("move the first row to the end", moved);
    std::vector<int> half;
    for (int id : ids)
        if (id % 2 == 0) half.push_back(id);
    change("remove every other row", half);
    change("remove all rows", {});
    root.render(h(List, {.ids = {}}));
    r.reset();
    report("add rows to an empty list", millis([&] { root.render(h(List, {.ids = ids})); }), r);

    // Rows come and go under a provider whose value never changes (a theme):
    // memory must not grow with the number of rows that ever existed.
    int rounds = 50, batch = std::max(1, n / 10);
    long before = rssKb();
    double churn = millis([&] {
        for (int i = 0; i < rounds; ++i) root.render(h(List, {.ids = range(i * batch, (i + 1) * batch)}));
    });
    std::println("\n{} rounds of {} new rows under one provider: {:.2f} ms, memory +{} KB", rounds, batch, churn,
                 rssKb() - before);
    std::println("heap held by the mounted list: {} bytes per row (component + <li> + text)", heapRows / ids.size());
    root.render(h("empty"));

    std::println("");
    for (int rows : {1000, n / 2, n}) terminalLog(rows);
    return 0;
}
