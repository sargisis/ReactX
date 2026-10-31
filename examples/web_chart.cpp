// A JavaScript library driven by C++ state: Chart.js draws, ReactX owns the data.
//
// The <canvas> gets a ref; an effect creates the chart once and destroys it
// when the component goes away; another effect hands the chart new data
// whenever the state changes. Chart.js comes from a CDN (see CMakeLists.txt).
#include <emscripten/val.h>

#include <string>
#include <vector>

#include "reactx/dom.hpp"
#include "reactx/js.hpp"
#include "reactx/json.hpp"
#include "reactx/reactx.hpp"

using namespace rx;
using emscripten::val;

namespace {

const std::vector<std::string> kDays = {"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};

json::Value numbers(const std::vector<int>& xs) {
    json::Value out = json::array();
    for (int x : xs) out.push(x);
    return out;
}

json::Value labels(std::size_t n) {
    json::Value out = json::array();
    for (std::size_t i = 0; i < n; ++i) out.push(kDays[i % kDays.size()]);
    return out;
}

// Chart.js options: plain JSON, turned into a JS object by rx::jsValue.
json::Value chartConfig(const std::string& type) {
    return json::object({
        {"type", type},
        {"data", json::object({
            {"labels", json::array()},
            {"datasets", json::array({json::object({{"label", "commits"}, {"data", json::array()},
                                                    {"borderColor", "#4f8cff"}, {"backgroundColor", "rgba(79,140,255,0.35)"},
                                                    {"tension", 0.3}, {"fill", true}})})},
        })},
        {"options", json::object({{"maintainAspectRatio", false},  // fill the 260px box
                                  {"animation", json::object({{"duration", 250}})},
                                  {"plugins", json::object({{"legend", json::object({{"display", false}})}})}})},
    });
}

struct ChartProps {
    std::vector<int> values{};
    std::string type = "line";
    bool operator==(const ChartProps&) const = default;
};

// The Chart.js part: a canvas, the chart object kept in a ref.
Element Chart(const ChartProps& p) {
    auto canvas = useNodeRef();
    auto& chart = useRef(val::undefined());

    // Create the chart when the canvas is on the page (again if the type changes).
    useEffect([canvas, &chart, type = p.type] {
        chart = val::global("Chart").new_(domElement(canvas), jsValue(chartConfig(type)));
        return [&chart] {
            chart.call<void>("destroy");
            chart = val::undefined();
        };
    }, deps(p.type));

    // New data from C++ state -> the chart.
    useEffect([&chart, values = p.values, type = p.type] {
        if (chart.isUndefined()) return;
        val data = chart["data"];
        data.set("labels", jsValue(labels(values.size())));
        data["datasets"][0].set("data", jsValue(numbers(values)));
        chart.call<void>("update");
    }, deps(p.values, p.type));

    return h("div", {{"style", "position: relative; height: 260px"}},
             {h("canvas", {{"ref", canvas}, {"role", "img"}, {"aria-label", "commits per day"}})});
}

Element App(const Props&) {
    auto [values, setValues] = useState(std::vector<int>{3, 7, 4, 9, 6, 2, 5});
    auto [type, setType] = useState(std::string("line"));
    auto [seed, setSeed] = useState(1u);

    int total = 0;
    for (int v : values) total += v;

    auto shuffle = [values, seed, setValues, setSeed] {
        std::vector<int> next = values;
        unsigned s = seed;
        for (int& v : next) {
            s = s * 1103515245u + 12345u;  // a tiny LCG: no <random> in the .wasm
            v = static_cast<int>((s >> 16) % 12) + 1;
        }
        setValues(next);
        setSeed(s);
    };

    return h("div", {{"className", "card demo"}}, {
        h("h1", {}, {"Chart.js + ReactX"}),
        h(Chart, {.values = values, .type = type}),
        h("p", {{"className", "hint"}, {"id", "total"}}, {std::to_string(values.size()) + " days, " + std::to_string(total) + " commits"}),
        h("div", {{"className", "row"}}, {
            h("button", {{"id", "add"}, {"className", "primary grow"},
                         {"onClick", [values, seed, setValues] {
                              auto next = values;
                              next.push_back(static_cast<int>((seed * 7 + next.size() * 5) % 10) + 1);
                              setValues(next);
                          }}}, {"+ day"}),
            h("button", {{"id", "shuffle"}, {"onClick", shuffle}}, {"shuffle"}),
            h("button", {{"id", "type"}, {"onClick", [type, setType] { setType(std::string(type == "line" ? "bar" : "line")); }}},
              {type == "line" ? "bars" : "line"}),
        }),
        h("p", {{"className", "hint"}}, {"The state lives in C++; Chart.js only draws it (useNodeRef + rx::domElement)."}),
    });
}

}  // namespace

int main() { mountDom(h(App)); }
