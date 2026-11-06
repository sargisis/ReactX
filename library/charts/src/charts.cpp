// rx-charts: SVG in the browser (__EMSCRIPTEN__), characters in a terminal.
// Numbers become text with shortNumber and std::to_string: std::format would
// add ~100 KB to every .wasm that uses the library.
#include "rx/charts.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace rx::charts {

namespace {

constexpr std::string_view kPalette[] = {"blue", "green", "magenta", "cyan", "yellow", "red"};

bool isPaletteName(std::string_view name) { return std::ranges::find(kPalette, name) != std::end(kPalette); }

// The color of series (or slice) i: its own, else the palette in turn.
std::string colorOf(const std::string& wanted, std::size_t i) {
#ifdef __EMSCRIPTEN__
    if (!wanted.empty()) return wanted;  // any CSS color works in the browser
#else
    if (isPaletteName(wanted)) return wanted;  // a terminal knows the names only
#endif
    return std::string(kPalette[i % std::size(kPalette)]);
}

// "1.25" with at most `decimals` decimals, no trailing zeros; `a` >= 0.
std::string fixed(double a, int decimals) {
    long long scale = decimals >= 2 ? 100 : decimals == 1 ? 10 : 1;
    long long n = std::llround(a * static_cast<double>(scale));
    std::string text = std::to_string(n / scale);
    if (long long part = n % scale; part != 0) {
        std::string digits = std::to_string(part);
        while (static_cast<int>(digits.size()) < decimals) digits.insert(digits.begin(), '0');
        while (digits.back() == '0') digits.pop_back();
        text += "." + digits;
    }
    return text;
}

double maxOf(const std::vector<Series>& series, double start) {
    double m = start;
    for (const auto& s : series)
        for (double v : s.values)
            if (std::isfinite(v)) m = std::max(m, v);
    return m;
}

double minOf(const std::vector<Series>& series, double start) {
    double m = start;
    for (const auto& s : series)
        for (double v : s.values)
            if (std::isfinite(v)) m = std::min(m, v);
    return m;
}

std::size_t pointCount(const std::vector<std::string>& labels, const std::vector<Series>& series) {
    std::size_t n = labels.size();
    for (const auto& s : series) n = std::max(n, s.values.size());
    return n;
}

std::string labelAt(const std::vector<std::string>& labels, std::size_t i) {
    return i < labels.size() ? labels[i] : std::to_string(i + 1);
}

// Axis ends and step at round numbers (0, 2k, 4k, ... rather than 0, 1873, ...).
struct Scale {
    double lo = 0, hi = 1, step = 1;
};

Scale niceScale(double lo, double hi, int ticks = 4) {
    if (!(hi > lo)) hi = lo + 1;
    double raw = (hi - lo) / ticks;
    double magnitude = std::pow(10.0, std::floor(std::log10(raw)));
    double r = raw / magnitude;
    double step = (r <= 1 ? 1 : r <= 2 ? 2 : r <= 5 ? 5 : 10) * magnitude;
    return {std::floor(lo / step) * step, std::ceil(hi / step) * step, step};
}

// Columns a terminal gives this text: wide (CJK, emoji) code points take two.
int cellsOf(std::string_view text) {
    int cells = 0;
    for (std::size_t i = 0; i < text.size();) {
        auto c = static_cast<unsigned char>(text[i]);
        std::uint32_t cp = c;
        std::size_t len = c < 0x80 ? 1 : c < 0xE0 ? 2 : c < 0xF0 ? 3 : 4;
        if (len > 1 && i + len <= text.size()) {
            cp = c & (0xFF >> (len + 1));
            for (std::size_t k = 1; k < len; ++k) cp = (cp << 6) | (static_cast<unsigned char>(text[i + k]) & 0x3F);
        }
        i += len;
        if ((cp >= 0x0300 && cp <= 0x036F) || cp == 0x200D || (cp >= 0xFE00 && cp <= 0xFE0F)) continue;  // marks
        bool wide = (cp >= 0x1100 && cp <= 0x115F) || (cp >= 0x2E80 && cp <= 0xA4CF) || (cp >= 0xAC00 && cp <= 0xD7A3) ||
                    (cp >= 0xF900 && cp <= 0xFAFF) || (cp >= 0xFE30 && cp <= 0xFE4F) || (cp >= 0xFF00 && cp <= 0xFF60) ||
                    (cp >= 0xFFE0 && cp <= 0xFFE6) || (cp >= 0x1F300 && cp <= 0x1FAFF);
        cells += wide ? 2 : 1;
    }
    return cells;
}

[[maybe_unused]] std::string summaryOf(const std::string& title, const std::vector<std::string>& labels, const std::vector<Series>& series,
                      const std::string& unit) {
    std::string text = title.empty() ? std::string("Chart") : title;
    for (const auto& s : series) {
        text += ". " + (s.name.empty() ? std::string("Values") : s.name) + ":";
        for (std::size_t i = 0; i < s.values.size() && i < 24; ++i)
            text += (i ? ", " : " ") + labelAt(labels, i) + " " + shortNumber(s.values[i]) + unit;
    }
    return text;
}

Element withKey(Element e, std::string key) {
    if (!key.empty()) e.key = std::move(key);
    return e;
}

#ifdef __EMSCRIPTEN__
// ---------------------------------------------------------------- browser: SVG

std::string_view paletteHex(std::string_view name) {  // when rx::ui's colors aren't on the page
    if (name == "green") return "#15803d";
    if (name == "magenta") return "#a21caf";
    if (name == "cyan") return "#0e7490";
    if (name == "yellow") return "#a16207";
    if (name == "red") return "#b91c1c";
    return "#2563eb";
}

std::string css(const std::string& color) {
    return isPaletteName(color) ? "var(--rx-c-" + color + ", " + std::string(paletteHex(color)) + ")" : color;
}

std::int64_t px(double v) { return static_cast<std::int64_t>(std::lround(v)); }
std::string pxText(double v) { return std::to_string(px(v)); }

constexpr const char* kMuted = "var(--rx-muted, #6b7280)";
constexpr const char* kGrid = "var(--rx-border, #d1d5db)";

Element svgText(double x, double y, std::string text, const char* anchor, bool muted = false) {
    return h("svg:text", {{"x", px(x)}, {"y", px(y)}, {"text-anchor", anchor}, {"font-size", 12},
                          {"fill", muted ? kMuted : "currentColor"}},
             {std::move(text)});
}

// The viewBox follows the window, so 12px text stays 12px on a phone.
int boxWidth() {
    WindowSize w = useWindowSize();
    return std::clamp(w.width - 32, 280, 720);
}

Element figure(const std::string& title, Element svg) {
    std::vector<Element> parts;
    if (!title.empty())
        parts.push_back(h("figcaption", {{"style", "font-weight: 600; margin-bottom: 4px"}}, {title}));
    parts.push_back(std::move(svg));
    return h("figure", {{"className", "rx-chart"}, {"style", "margin: 0"}}, std::move(parts));
}

Element svgRoot(int w, int hgt, const std::string& label, std::vector<Element> children) {
    return h("svg", {{"viewBox", "0 0 " + std::to_string(w) + " " + std::to_string(hgt)},
                     {"width", "100%"},
                     {"role", "img"},
                     {"aria-label", label},
                     {"style", "display: block; height: auto; overflow: visible"}},
             std::move(children));
}

void legend(std::vector<Element>& out, const std::vector<Series>& series, double left, double y) {
    double x = left;
    for (std::size_t k = 0; k < series.size(); ++k) {
        std::string name = series[k].name.empty() ? "Series " + std::to_string(k + 1) : series[k].name;
        out.push_back(h("rect", {{"x", px(x)}, {"y", px(y - 9)}, {"width", 10}, {"height", 10}, {"rx", 2},
                                 {"fill", css(colorOf(series[k].color, k))}}));
        out.push_back(svgText(x + 14, y, name, "start"));
        x += 14 + 7.0 * cellsOf(name) + 16;
    }
}

// Horizontal grid lines and their values; returns y(v).
struct Plot {
    double left, top, width, height;
    Scale s;
    double y(double v) const { return top + (s.hi - v) / (s.hi - s.lo) * height; }
};

void grid(std::vector<Element>& out, const Plot& p, const std::string& unit) {
    for (double t = p.s.lo; t <= p.s.hi + p.s.step / 2; t += p.s.step) {
        double y = p.y(t);
        out.push_back(h("line", {{"x1", px(p.left)}, {"x2", px(p.left + p.width)}, {"y1", px(y)}, {"y2", px(y)},
                                 {"stroke", kGrid}, {"stroke-width", 1}}));
        out.push_back(svgText(p.left - 6, y + 4, shortNumber(t) + unit, "end", true));
    }
}

void xLabels(std::vector<Element>& out, const std::vector<std::string>& labels, std::size_t n, double y,
             auto&& xOf, double plotWidth) {
    int longest = 1;
    for (std::size_t i = 0; i < n; ++i) longest = std::max(longest, cellsOf(labelAt(labels, i)));
    double room = plotWidth / static_cast<double>(std::max<std::size_t>(n, 1));
    std::size_t every = std::max<std::size_t>(1, static_cast<std::size_t>(std::ceil((longest * 7.0 + 8) / room)));
    for (std::size_t i = 0; i < n; i += every) out.push_back(svgText(xOf(i), y, labelAt(labels, i), "middle", true));
}

Element BarChart(const BarChartProps& p) {
    const int w = boxWidth(), hgt = p.height > 0 ? p.height : 240;
    const std::size_t n = pointCount(p.labels, p.series);
    const double legendH = p.series.size() > 1 ? 22 : 0;
    Plot plot{.left = 52, .top = 8 + legendH, .width = w - 52.0 - 8, .height = hgt - 8 - legendH - 24,
              .s = niceScale(minOf(p.series, 0), maxOf(p.series, 0))};
    std::vector<Element> parts;
    if (legendH > 0) legend(parts, p.series, plot.left, 14);
    grid(parts, plot, p.unit);
    const double group = plot.width / static_cast<double>(std::max<std::size_t>(n, 1));
    const double inner = group * 0.72;
    const double bw = inner / static_cast<double>(std::max<std::size_t>(p.series.size(), 1));
    for (std::size_t k = 0; k < p.series.size(); ++k) {
        const Series& s = p.series[k];
        std::string fill = css(colorOf(s.color, k));
        for (std::size_t i = 0; i < s.values.size() && i < n; ++i) {
            double v = s.values[i];
            if (!std::isfinite(v)) continue;
            double x = plot.left + group * static_cast<double>(i) + (group - inner) / 2 + bw * static_cast<double>(k);
            double y0 = plot.y(std::max(v, 0.0)), y1 = plot.y(std::min(v, 0.0));
            std::string tip = labelAt(p.labels, i) + (s.name.empty() ? "" : " · " + s.name) + ": " + shortNumber(v) + p.unit;
            parts.push_back(h("rect", {{"x", px(x)}, {"y", px(y0)}, {"width", std::max<std::int64_t>(px(bw - 2), 1)},
                                       {"height", std::max<std::int64_t>(px(y1 - y0), 1)}, {"rx", 2}, {"fill", fill}},
                              {h("svg:title", {}, {tip})}));
        }
    }
    parts.push_back(h("line", {{"x1", px(plot.left)}, {"x2", px(plot.left + plot.width)}, {"y1", px(plot.y(0))},
                               {"y2", px(plot.y(0))}, {"stroke", "currentColor"}, {"stroke-opacity", "0.5"}}));
    xLabels(parts, p.labels, n, hgt - 6, [&](std::size_t i) { return plot.left + group * (static_cast<double>(i) + 0.5); },
            plot.width);
    return figure(p.title, svgRoot(w, hgt, summaryOf(p.title, p.labels, p.series, p.unit), std::move(parts)));
}

Element LineChart(const LineChartProps& p) {
    const int w = boxWidth(), hgt = p.height > 0 ? p.height : 240;
    const std::size_t n = pointCount(p.labels, p.series);
    const double legendH = p.series.size() > 1 ? 22 : 0;
    double lo = minOf(p.series, INFINITY), hi = maxOf(p.series, -INFINITY);
    if (!std::isfinite(lo)) lo = 0, hi = 1;
    if (lo > 0 && lo < hi * 0.5) lo = 0;  // close to zero: start the axis there
    Plot plot{.left = 52, .top = 8 + legendH, .width = w - 52.0 - 16, .height = hgt - 8 - legendH - 24,
              .s = niceScale(lo, hi)};
    auto xOf = [&](std::size_t i) {
        return n <= 1 ? plot.left + plot.width / 2 : plot.left + plot.width * static_cast<double>(i) / static_cast<double>(n - 1);
    };
    std::vector<Element> parts;
    if (legendH > 0) legend(parts, p.series, plot.left, 14);
    grid(parts, plot, p.unit);
    for (std::size_t k = 0; k < p.series.size(); ++k) {
        const Series& s = p.series[k];
        std::string stroke = css(colorOf(s.color, k));
        std::string points;
        for (std::size_t i = 0; i < s.values.size() && i < n; ++i)
            if (std::isfinite(s.values[i])) points += pxText(xOf(i)) + "," + pxText(plot.y(s.values[i])) + " ";
        parts.push_back(h("polyline", {{"points", points}, {"fill", "none"}, {"stroke", stroke}, {"stroke-width", 2},
                                       {"stroke-linejoin", "round"}, {"stroke-linecap", "round"}}));
        if (!p.dots) continue;
        for (std::size_t i = 0; i < s.values.size() && i < n; ++i) {
            if (!std::isfinite(s.values[i])) continue;
            std::string tip = labelAt(p.labels, i) + (s.name.empty() ? "" : " · " + s.name) + ": " +
                              shortNumber(s.values[i]) + p.unit;
            parts.push_back(h("circle", {{"cx", px(xOf(i))}, {"cy", px(plot.y(s.values[i]))}, {"r", 3}, {"fill", stroke}},
                              {h("svg:title", {}, {tip})}));
        }
    }
    xLabels(parts, p.labels, n, hgt - 6, xOf, plot.width);
    return figure(p.title, svgRoot(w, hgt, summaryOf(p.title, p.labels, p.series, p.unit), std::move(parts)));
}

Element PieChart(const PieChartProps& p) {
    const int w = boxWidth();
    double total = 0;
    for (double v : p.values) total += std::isfinite(v) ? std::max(v, 0.0) : 0;
    const double d = std::min(200.0, w * 0.45);
    const int hgt = static_cast<int>(std::max(d, 22.0 * static_cast<double>(p.values.size()))) + 8;
    const double cx = d / 2 + 4, cy = hgt / 2.0;
    // Every slice is a circle's dashed stroke: the dash is the slice.
    const double radius = p.donut ? d * 0.38 : d / 4, stroke = p.donut ? d * 0.2 : d / 2;
    const double circumference = 2 * 3.141592653589793 * radius;
    std::vector<Element> parts;
    std::string label = (p.title.empty() ? std::string("Chart") : p.title) + ":";
    double start = 0;
    for (std::size_t i = 0; i < p.values.size(); ++i) {
        double v = std::isfinite(p.values[i]) ? std::max(p.values[i], 0.0) : 0;
        double share = total > 0 ? v / total : 0;
        std::string name = labelAt(p.labels, i);
        std::string fill = css(colorOf(i < p.colors.size() ? p.colors[i] : std::string{}, i));
        std::string pct = fixed(share * 100, 1) + "%";
        label += (i ? ", " : " ") + name + " " + pct;
        double len = share * circumference;
        if (len > 0)
            parts.push_back(h("circle", {{"cx", px(cx)}, {"cy", px(cy)}, {"r", fixed(radius, 1)}, {"fill", "none"}, {"stroke", fill},
                                         {"stroke-width", fixed(stroke, 1)},
                                         {"stroke-dasharray", fixed(len, 1) + " " + fixed(circumference - len, 1)},
                                         {"stroke-dashoffset", start > 0 ? "-" + fixed(start, 1) : std::string("0")},
                                         {"transform", "rotate(-90 " + pxText(cx) + " " + pxText(cy) + ")"}},
                              {h("svg:title", {}, {name + ": " + shortNumber(v) + p.unit + " (" + pct + ")"})}));
        start += len;
        double ly = cy - 11.0 * static_cast<double>(p.values.size()) + 22.0 * static_cast<double>(i) + 15;
        parts.push_back(h("rect", {{"x", px(d + 24)}, {"y", px(ly - 10)}, {"width", 10}, {"height", 10}, {"rx", 2}, {"fill", fill}}));
        parts.push_back(svgText(d + 40, ly, name + "  " + shortNumber(v) + p.unit + " · " + pct, "start"));
    }
    if (p.donut) {
        parts.push_back(h("svg:text", {{"x", px(cx)}, {"y", px(cy + 6)}, {"text-anchor", "middle"}, {"font-size", 18},
                                       {"font-weight", 600}, {"fill", "currentColor"}},
                          {shortNumber(total) + p.unit}));
    }
    return figure(p.title, svgRoot(w, hgt, label, std::move(parts)));
}

#else
// ---------------------------------------------------------------- terminal

constexpr std::string_view kEighths[] = {"", "▏", "▎", "▍", "▌", "▋", "▊", "▉"};

std::string barText(double share, int cells) {  // share 0..1 of `cells`, in eighths
    int eighths = static_cast<int>(std::lround(std::clamp(share, 0.0, 1.0) * cells * 8));
    std::string text;
    for (int i = 0; i < eighths / 8; ++i) text += "█";
    text += kEighths[eighths % 8];
    return text;
}

int widthFor(int wanted) {
    WindowSize win = useWindowSize();
    return wanted > 0 ? wanted : std::max(24, win.width - 4);
}

Element cell(std::string text, int width, std::string color = {}, bool dim = false) {
    return h("box", {{"width", width}, {"flexShrink", 0}},
             {h("text", {{"wrap", false}, {"color", std::move(color)}, {"dim", dim}}, {std::move(text)})});
}

Element row(std::vector<Element> children) { return h("box", {{"direction", "row"}, {"gap", 1}}, std::move(children)); }

Element legendRow(const std::vector<Series>& series) {
    std::vector<Element> items;
    for (std::size_t k = 0; k < series.size(); ++k)
        items.push_back(h("box", {{"direction", "row"}, {"gap", 1}},
                          {h("text", {{"color", colorOf(series[k].color, k)}}, {"■"}),
                           h("text", {{"wrap", false}}, {series[k].name.empty() ? "Series " + std::to_string(k + 1) : series[k].name})}));
    return h("box", {{"direction", "row"}, {"gap", 2}}, std::move(items));
}

Element chartBox(const std::string& title, std::vector<Element> body) {
    if (!title.empty()) body.insert(body.begin(), h("text", {{"bold", true}}, {title}));
    return h("box", {}, std::move(body));
}

Element BarChart(const BarChartProps& p) {
    const int width = widthFor(p.width);
    const std::size_t n = pointCount(p.labels, p.series);
    double top = 0;
    int labelW = 1, valueW = 1;
    for (std::size_t i = 0; i < n; ++i) labelW = std::max(labelW, cellsOf(labelAt(p.labels, i)));
    for (const auto& s : p.series)
        for (double v : s.values) {
            if (!std::isfinite(v)) continue;
            top = std::max(top, std::abs(v));
            valueW = std::max(valueW, cellsOf(shortNumber(v) + p.unit));
        }
    labelW = std::min(labelW, 16);
    const int barW = std::max(4, width - labelW - valueW - 2);
    std::vector<Element> body;
    if (p.series.size() > 1) body.push_back(legendRow(p.series));
    for (std::size_t i = 0; i < n; ++i)
        for (std::size_t k = 0; k < p.series.size(); ++k) {
            const Series& s = p.series[k];
            double v = i < s.values.size() && std::isfinite(s.values[i]) ? s.values[i] : 0;
            body.push_back(row({cell(k == 0 ? labelAt(p.labels, i) : "", labelW),
                                cell(barText(top > 0 ? std::abs(v) / top : 0, barW), barW, colorOf(s.color, k)),
                                cell(shortNumber(v) + p.unit, valueW, {}, true)}));
        }
    return chartBox(p.title, std::move(body));
}

// Braille: a cell holds 2x4 dots, so a line is drawn at twice/four times the cell grid.
constexpr std::uint8_t kDot[4][2] = {{0x01, 0x08}, {0x02, 0x10}, {0x04, 0x20}, {0x40, 0x80}};

std::string braille(std::uint8_t bits) {
    if (bits == 0) return " ";
    std::uint32_t cp = 0x2800u + bits;
    return {static_cast<char>(0xE0 | (cp >> 12)), static_cast<char>(0x80 | ((cp >> 6) & 0x3F)),
            static_cast<char>(0x80 | (cp & 0x3F))};
}

Element LineChart(const LineChartProps& p) {
    const int width = widthFor(p.width);
    const int rows = p.rows > 0 ? p.rows : 10;
    const std::size_t n = pointCount(p.labels, p.series);
    double lo = minOf(p.series, INFINITY), hi = maxOf(p.series, -INFINITY);
    if (!std::isfinite(lo)) lo = 0, hi = 1;
    if (lo > 0 && lo < hi * 0.5) lo = 0;
    Scale s = niceScale(lo, hi);
    std::string topText = shortNumber(s.hi) + p.unit, midText = shortNumber((s.hi + s.lo) / 2) + p.unit,
                lowText = shortNumber(s.lo) + p.unit;
    const int axisW = std::max({cellsOf(topText), cellsOf(midText), cellsOf(lowText)});
    const int cols = std::max(4, width - axisW - 2);
    const int dotsW = cols * 2, dotsH = rows * 4;
    std::vector<std::uint8_t> bits(static_cast<std::size_t>(cols * rows), 0);
    std::vector<int> owner(bits.size(), -1);  // the series a cell shows (its color)
    auto plot = [&](int x, int y, int k) {
        if (x < 0 || y < 0 || x >= dotsW || y >= dotsH) return;
        std::size_t at = static_cast<std::size_t>((y / 4) * cols + x / 2);
        bits[at] |= kDot[y % 4][x % 2];
        owner[at] = k;
    };
    auto xOf = [&](std::size_t i) {
        return n <= 1 ? dotsW / 2 : static_cast<int>(std::lround((dotsW - 1) * static_cast<double>(i) / static_cast<double>(n - 1)));
    };
    auto yOf = [&](double v) { return static_cast<int>(std::lround((s.hi - v) / (s.hi - s.lo) * (dotsH - 1))); };
    for (std::size_t k = 0; k < p.series.size(); ++k) {
        const auto& values = p.series[k].values;
        bool have = false;
        int px0 = 0, py0 = 0;
        for (std::size_t i = 0; i < values.size() && i < n; ++i) {
            if (!std::isfinite(values[i])) {
                have = false;
                continue;
            }
            int x1 = xOf(i), y1 = yOf(values[i]);
            if (!have) plot(x1, y1, static_cast<int>(k));
            else {  // Bresenham from the previous point
                int dx = std::abs(x1 - px0), dy = -std::abs(y1 - py0), sx = px0 < x1 ? 1 : -1, sy = py0 < y1 ? 1 : -1;
                int err = dx + dy, x = px0, y = py0;
                while (true) {
                    plot(x, y, static_cast<int>(k));
                    if (x == x1 && y == y1) break;
                    int e2 = 2 * err;
                    if (e2 >= dy) err += dy, x += sx;
                    if (e2 <= dx) err += dx, y += sy;
                }
            }
            px0 = x1, py0 = y1, have = true;
        }
    }
    std::vector<Element> body;
    if (p.series.size() > 1) body.push_back(legendRow(p.series));
    for (int r = 0; r < rows; ++r) {
        std::string axis = r == 0 ? topText : r == rows - 1 ? lowText : r == rows / 2 ? midText : "";
        std::vector<Element> runs;  // stretches of one color
        int k = -2;
        std::string text;
        auto flush = [&] {
            if (!text.empty())
                runs.push_back(h("text", {{"wrap", false}, {"color", k >= 0 ? colorOf(p.series[static_cast<std::size_t>(k)].color,
                                                                                       static_cast<std::size_t>(k))
                                                                            : std::string{}}},
                                 {text}));
            text.clear();
        };
        for (int c = 0; c < cols; ++c) {
            std::size_t at = static_cast<std::size_t>(r * cols + c);
            if (owner[at] != k) flush(), k = owner[at];
            text += braille(bits[at]);
        }
        flush();
        body.push_back(h("box", {{"direction", "row"}},
                         {h("box", {{"width", axisW + 1}, {"flexShrink", 0}, {"alignItems", "flex-end"}, {"paddingRight", 1}},
                            {h("text", {{"dim", true}, {"wrap", false}}, {axis})}),
                          h("text", {{"dim", true}}, {"│"}), h("box", {{"direction", "row"}}, std::move(runs))}));
    }
    if (n > 0)
        body.push_back(h("box", {{"direction", "row"}, {"paddingLeft", axisW + 2}, {"width", width}},
                         {h("text", {{"dim", true}, {"wrap", false}}, {labelAt(p.labels, 0)}), h("box", {{"flexGrow", 1}}),
                          h("text", {{"dim", true}, {"wrap", false}}, {n > 1 ? labelAt(p.labels, n - 1) : std::string{}})}));
    return chartBox(p.title, std::move(body));
}

Element PieChart(const PieChartProps& p) {
    const int width = widthFor(p.width);
    double total = 0;
    for (double v : p.values) total += std::isfinite(v) ? std::max(v, 0.0) : 0;
    // One bar split by share; the largest remainders get the cells left over.
    std::vector<int> cells(p.values.size(), 0);
    std::vector<std::pair<double, std::size_t>> rest;
    int used = 0;
    for (std::size_t i = 0; i < p.values.size(); ++i) {
        double v = std::isfinite(p.values[i]) ? std::max(p.values[i], 0.0) : 0;
        double exact = total > 0 ? v / total * width : 0;
        cells[i] = static_cast<int>(exact);
        used += cells[i];
        rest.emplace_back(exact - cells[i], i);
    }
    std::ranges::sort(rest, std::greater<>{});
    for (std::size_t j = 0; total > 0 && used < width && j < rest.size(); ++j, ++used) ++cells[rest[j].second];
    std::vector<Element> bar;
    int labelW = 1, valueW = 1;
    for (std::size_t i = 0; i < p.values.size(); ++i) {
        std::string color = colorOf(i < p.colors.size() ? p.colors[i] : std::string{}, i);
        std::string text;
        for (int c = 0; c < cells[i]; ++c) text += "█";
        if (!text.empty()) bar.push_back(h("text", {{"color", color}, {"wrap", false}}, {text}));
        labelW = std::max(labelW, cellsOf(labelAt(p.labels, i)));
        valueW = std::max(valueW, cellsOf(shortNumber(p.values[i]) + p.unit));
    }
    std::vector<Element> body{h("box", {{"direction", "row"}}, std::move(bar))};
    for (std::size_t i = 0; i < p.values.size(); ++i) {
        double v = std::isfinite(p.values[i]) ? std::max(p.values[i], 0.0) : 0;
        std::string color = colorOf(i < p.colors.size() ? p.colors[i] : std::string{}, i);
        body.push_back(row({cell("■", 1, color), cell(labelAt(p.labels, i), std::min(labelW, 20)),
                            cell(shortNumber(v) + p.unit, valueW), cell(fixed(total > 0 ? v / total * 100 : 0, 1) + "%", 6, {}, true)}));
    }
    if (p.donut) body.push_back(h("text", {{"dim", true}}, {"Total " + shortNumber(total) + p.unit}));
    return chartBox(p.title, std::move(body));
}
#endif

}  // namespace

std::string shortNumber(double value) {
    if (!std::isfinite(value)) return "–";
    double a = std::abs(value);
    std::string text = a >= 1e9   ? fixed(a / 1e9, 1) + "B"
                       : a >= 1e6 ? fixed(a / 1e6, 1) + "M"
                       : a >= 1e4 ? fixed(a / 1e3, 1) + "k"
                       : a >= 100 ? fixed(a, 0)
                                  : fixed(a, 2);
    return (value < 0 && text != "0" ? "-" : "") + text;
}

Element bar(BarChartProps props) {
    std::string key = props.key;
    return withKey(h(BarChart, std::move(props)), std::move(key));
}

Element line(LineChartProps props) {
    std::string key = props.key;
    return withKey(h(LineChart, std::move(props)), std::move(key));
}

Element pie(PieChartProps props) {
    std::string key = props.key;
    return withKey(h(PieChart, std::move(props)), std::move(key));
}

}  // namespace rx::charts
