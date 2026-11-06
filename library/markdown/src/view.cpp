// Document -> elements. Browser: semantic HTML (a little CSS once, easy to
// override). Terminal: a text node keeps one style, so a paragraph is a
// wrapping row of words, each with its own style.
#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "rx/markdown.hpp"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>

#include "reactx/dom.hpp"
#endif

namespace rx::markdown {

namespace {

Element withKey(Element e, std::string key) {
    if (!key.empty()) e.key = std::move(key);
    return e;
}

[[maybe_unused]] std::string plain(const std::vector<Inline>& in) {
    std::string out;
    for (const auto& i : in) {
        if (i.kind == Inline::Kind::Text || i.kind == Inline::Kind::Code || i.kind == Inline::Kind::Image) out += i.text;
        else if (i.kind == Inline::Kind::LineBreak) out += ' ';
        else out += plain(i.children);
    }
    return out;
}

#ifdef __EMSCRIPTEN__
// ---------------------------------------------------------------- browser

// :where() keeps these at zero specificity: any rule of the app wins.
constexpr const char* kCss = R"css(
:where(.rx-md) { line-height: 1.6; overflow-wrap: anywhere; }
:where(.rx-md) > :first-child { margin-top: 0; }
:where(.rx-md) > :last-child { margin-bottom: 0; }
:where(.rx-md) :where(code) { font-family: ui-monospace, SFMono-Regular, Menlo, Consolas, monospace; font-size: .9em;
  background: color-mix(in srgb, currentColor 10%, transparent); padding: .1em .35em; border-radius: 4px; }
:where(.rx-md) :where(pre) { padding: 12px 14px; border-radius: 8px; overflow-x: auto; line-height: 1.45;
  background: color-mix(in srgb, currentColor 8%, transparent); }
:where(.rx-md) :where(pre code) { background: none; padding: 0; font-size: .875em; }
:where(.rx-md) :where(blockquote) { margin-left: 0; padding-left: 14px; opacity: .85;
  border-left: 3px solid color-mix(in srgb, currentColor 30%, transparent); }
:where(.rx-md) :where(table) { border-collapse: collapse; display: block; overflow-x: auto; max-width: 100%; }
:where(.rx-md) :where(th, td) { border: 1px solid color-mix(in srgb, currentColor 20%, transparent); padding: 6px 10px; }
:where(.rx-md) :where(img) { max-width: 100%; height: auto; }
:where(.rx-md) :where(hr) { border: none; border-top: 1px solid color-mix(in srgb, currentColor 20%, transparent); }
:where(.rx-md) :where(li.rx-md-task) { list-style: none; }
:where(.rx-md) :where(li.rx-md-task > input) { margin: 0 .5em 0 -1.4em; }
)css";

EM_JS(int, rx_md_install_styles, (const char* css), {
    if (typeof document === "undefined") return 0;
    if (document.getElementById("rx-md-styles")) return 1;
    const style = document.createElement("style");
    style.id = "rx-md-styles";
    style.textContent = UTF8ToString(css);
    document.head.prepend(style);  // first, so the app's own CSS wins ties
    return 1;
});

void ensureStyles() {
    static bool done = false;
    if (done) return;
    done = true;
    if (!rx_md_install_styles(kCss))  // prerendering under Node
        rx::detail::addPrerenderHead(std::string("<style id=\"rx-md-styles\">") + kCss + "</style>");
}

std::vector<Element> inlines(const std::vector<Inline>& in, const MarkdownProps& p);

Element inlineElement(const Inline& i, const MarkdownProps& p) {
    switch (i.kind) {
        case Inline::Kind::Text: return Element(i.text);
        case Inline::Kind::Emphasis: return h("em", {}, inlines(i.children, p));
        case Inline::Kind::Strong: return h("strong", {}, inlines(i.children, p));
        case Inline::Kind::Strike: return h("del", {}, inlines(i.children, p));
        case Inline::Kind::Code: return h("code", {}, {i.text});
        case Inline::Kind::LineBreak: return h("br");
        case Inline::Kind::Image:
            if (i.url.empty()) return Element(i.text);
            return h("img", {{"src", i.url}, {"alt", i.text}, {"loading", "lazy"}});
        case Inline::Kind::Link: {
            auto label = inlines(i.children, p);
            if (i.url.empty()) return h("span", {}, std::move(label));  // an unsafe link: text only
            return p.link ? p.link(i.url, std::move(label)) : anchor(i.url, std::move(label));
        }
    }
    return Element("");
}

std::vector<Element> inlines(const std::vector<Inline>& in, const MarkdownProps& p) {
    std::vector<Element> out;
    out.reserve(in.size());
    for (const auto& i : in) out.push_back(inlineElement(i, p));
    return out;
}

std::vector<Element> blocks(const Document& doc, const MarkdownProps& p, bool tight = false);

Element blockElement(const Block& b, const MarkdownProps& p, bool tight) {
    switch (b.kind) {
        case Block::Kind::Paragraph:
            if (tight) return fragment(inlines(b.inlines, p));  // a tight list item: no <p> margins
            return h("p", {}, inlines(b.inlines, p));
        case Block::Kind::Heading:
            return h("h" + std::to_string(b.level), {{"id", b.id}}, inlines(b.inlines, p));
        case Block::Kind::Code: {
            Element code = h("code", {}, {b.code});
            if (!b.language.empty()) code.props.set("className", "language-" + b.language);
            return h("pre", {}, {std::move(code)});
        }
        case Block::Kind::Quote: return h("blockquote", {}, blocks(b.children, p));
        case Block::Kind::Rule: return h("hr");
        case Block::Kind::List: {
            std::vector<Element> items;
            for (std::size_t k = 0; k < b.items.size(); ++k) {
                const bool itemTight = b.items[k].size() == 1 && b.items[k][0].kind == Block::Kind::Paragraph;
                std::vector<Element> content;
                if (b.checked[k])
                    content.push_back(h("input", {{"type", "checkbox"}, {"checked", *b.checked[k]}, {"disabled", true}}));
                for (auto& e : blocks(b.items[k], p, itemTight)) content.push_back(std::move(e));
                Element li = h("li", {}, std::move(content));
                if (b.checked[k]) li.props.set("className", std::string("rx-md-task"));
                items.push_back(std::move(li));
            }
            if (!b.ordered) return h("ul", {}, std::move(items));
            Element ol = h("ol", {}, std::move(items));
            if (b.start != 1) ol.props.set("start", static_cast<long>(b.start));
            return ol;
        }
        case Block::Kind::Table: {
            auto cell = [&](const char* tag, const std::vector<Inline>& in, std::size_t col) {
                Element e = h(tag, {}, inlines(in, p));
                const char a = col < b.align.size() ? b.align[col] : 0;
                if (a) e.props.set("style", std::string(a == 'c' ? "text-align:center" : a == 'r' ? "text-align:right" : "text-align:left"));
                return e;
            };
            std::vector<Element> head, body;
            for (std::size_t c = 0; c < b.rows[0].size(); ++c) head.push_back(cell("th", b.rows[0][c], c));
            for (std::size_t r = 1; r < b.rows.size(); ++r) {
                std::vector<Element> tds;
                for (std::size_t c = 0; c < b.rows[r].size(); ++c) tds.push_back(cell("td", b.rows[r][c], c));
                body.push_back(h("tr", {}, std::move(tds)));
            }
            return h("table", {}, {h("thead", {}, {h("tr", {}, std::move(head))}), h("tbody", {}, std::move(body))});
        }
    }
    return Element("");
}

std::vector<Element> blocks(const Document& doc, const MarkdownProps& p, bool tight) {
    std::vector<Element> out;
    out.reserve(doc.size());
    for (const auto& b : doc) out.push_back(blockElement(b, p, tight));
    return out;
}

#else
// ---------------------------------------------------------------- terminal

struct Style {
    bool bold = false;
    bool underline = false;
    bool dim = false;
    std::string color{};
};

struct Run {
    std::string text;
    Style style;
    bool lineBreak = false;
};

void runs(const std::vector<Inline>& in, Style s, const MarkdownProps& p, std::vector<Run>& out) {
    for (const auto& i : in) {
        switch (i.kind) {
            case Inline::Kind::Text: out.push_back({i.text, s}); break;
            case Inline::Kind::LineBreak: out.push_back({"", s, true}); break;
            case Inline::Kind::Code: {
                Style c = s;
                c.color = "yellow";
                out.push_back({i.text, c});
                break;
            }
            case Inline::Kind::Image: {
                Style d = s;
                d.dim = true;
                out.push_back({"[" + (i.text.empty() ? std::string("image") : i.text) + "]", d});
                break;
            }
            case Inline::Kind::Emphasis: {  // most terminals have no italics: underline
                Style e = s;
                e.underline = true;
                runs(i.children, e, p, out);
                break;
            }
            case Inline::Kind::Strong: {
                Style b = s;
                b.bold = true;
                runs(i.children, b, p, out);
                break;
            }
            case Inline::Kind::Strike: {
                Style d = s;
                d.dim = true;
                runs(i.children, d, p, out);
                break;
            }
            case Inline::Kind::Link: {
                Style l = s;
                l.underline = true;
                l.color = "blue";
                runs(i.children, l, p, out);
                if (p.showUrls && !i.url.empty() && plain(i.children) != i.url) {
                    Style d = s;
                    d.dim = true;
                    out.push_back({" (" + i.url + ")", d});
                }
                break;
            }
        }
    }
}

Element word(std::string text, const Style& s) {
    Element e = h("text", {{"wrap", false}}, {std::move(text)});
    if (s.bold) e.props.set("bold", true);
    if (s.underline) e.props.set("underline", true);
    if (s.dim) e.props.set("dim", true);
    if (!s.color.empty()) e.props.set("color", s.color);
    return e;
}

// The runs as lines (at hard breaks) of wrapping words.
Element paragraph(const std::vector<Inline>& in, Style base, const MarkdownProps& p) {
    std::vector<Run> rs;
    runs(in, base, p, rs);
    std::vector<Element> lines;
    std::vector<Element> words;
    auto endLine = [&] {
        lines.push_back(h("box", {{"direction", "row"}, {"flexWrap", "wrap"}}, std::move(words)));
        words = {};
    };
    for (auto& r : rs) {
        if (r.lineBreak) {
            endLine();
            continue;
        }
        // Each word keeps its trailing space, so styled neighbours stay apart.
        std::size_t start = 0;
        while (start < r.text.size()) {
            auto space = r.text.find(' ', start);
            std::size_t end = space == std::string::npos ? r.text.size() : space + 1;
            std::string w = r.text.substr(start, end - start);
            if (!(w == " " && words.empty())) words.push_back(word(std::move(w), r.style));
            start = end;
        }
    }
    endLine();
    if (lines.size() == 1) return std::move(lines.front());
    return h("box", {}, std::move(lines));
}

std::vector<Element> blocks(const Document& doc, const MarkdownProps& p, int gap = 1);

std::size_t cells(std::string_view s) {  // display width, roughly: one cell per code point
    std::size_t n = 0;
    for (char c : s)
        if ((static_cast<unsigned char>(c) & 0xC0) != 0x80) ++n;
    return n;
}

Element blockElement(const Block& b, const MarkdownProps& p) {
    switch (b.kind) {
        case Block::Kind::Paragraph: return paragraph(b.inlines, {}, p);
        case Block::Kind::Heading: {
            Style s{.bold = true, .underline = b.level == 1};
            if (b.level <= 2) s.color = b.level == 1 ? "cyan" : "blue";
            return paragraph(b.inlines, s, p);
        }
        case Block::Kind::Code: {
            std::vector<Element> lines;
            std::size_t start = 0;
            while (start <= b.code.size()) {
                auto nl = b.code.find('\n', start);
                if (nl == std::string::npos) nl = b.code.size();
                std::string line = b.code.substr(start, nl - start);
                lines.push_back(h("text", {{"wrap", false}, {"color", "green"}}, {line.empty() ? std::string(" ") : line}));
                start = nl + 1;
            }
            return h("box", {{"paddingLeft", 2}, {"overflow", "hidden"}}, std::move(lines));
        }
        case Block::Kind::Quote:
            return h("box", {{"direction", "row"}},
                     {h("text", {{"wrap", false}, {"dim", true}}, {"┃ "}),
                      h("box", {{"flexGrow", 1}, {"flexShrink", 1}, {"dim", true}}, blocks(b.children, p))});
        case Block::Kind::Rule:
            return h("box", {{"height", 1}, {"overflow", "hidden"}},
                     {h("text", {{"wrap", false}, {"dim", true}}, {[] { std::string s; for (int k = 0; k < 300; ++k) s += "─"; return s; }()})});
        case Block::Kind::List: {
            std::vector<Element> items;
            for (std::size_t k = 0; k < b.items.size(); ++k) {
                std::string mark = b.ordered ? std::to_string(b.start + static_cast<int>(k)) + ". " : "• ";
                if (b.checked[k]) mark = *b.checked[k] ? "[x] " : "[ ] ";
                items.push_back(h("box", {{"direction", "row"}},
                                  {h("text", {{"wrap", false}, {"color", b.checked[k] ? "" : "gray"}}, {mark}),
                                   h("box", {{"flexGrow", 1}, {"flexShrink", 1}}, blocks(b.items[k], p, 0))}));
            }
            return h("box", {}, std::move(items));
        }
        case Block::Kind::Table: {
            std::vector<std::size_t> width(b.align.size(), 0);
            std::vector<std::vector<std::string>> text;
            for (const auto& row : b.rows) {
                auto& t = text.emplace_back();
                for (std::size_t c = 0; c < row.size(); ++c) {
                    t.push_back(plain(row[c]));
                    width[c] = std::max(width[c], cells(t.back()));
                }
            }
            auto pad = [&](const std::string& s, std::size_t c) {
                const std::size_t room = width[c] - std::min(width[c], cells(s));
                const char a = b.align[c];
                const std::size_t left = a == 'r' ? room : a == 'c' ? room / 2 : 0;
                return std::string(left, ' ') + s + std::string(room - left, ' ');
            };
            std::vector<Element> rows;
            for (std::size_t r = 0; r < text.size(); ++r) {
                std::string line;
                for (std::size_t c = 0; c < text[r].size(); ++c) line += (c ? " │ " : "") + pad(text[r][c], c);
                rows.push_back(h("text", {{"wrap", false}, {"bold", r == 0}}, {line}));
                if (r == 0) {
                    std::string sep;
                    for (std::size_t c = 0; c < width.size(); ++c) {
                        if (c) sep += "─┼─";
                        for (std::size_t k = 0; k < width[c]; ++k) sep += "─";
                    }
                    rows.push_back(h("text", {{"wrap", false}, {"dim", true}}, {sep}));
                }
            }
            return h("box", {{"overflow", "hidden"}}, std::move(rows));
        }
    }
    return Element("");
}

std::vector<Element> blocks(const Document& doc, const MarkdownProps& p, int gap) {
    std::vector<Element> out;
    out.reserve(doc.size());
    for (const auto& b : doc) out.push_back(blockElement(b, p));
    if (gap == 0 || out.size() < 2) return out;
    return {h("box", {{"gap", gap}}, std::move(out))};
}
#endif

}  // namespace

Element anchor(std::string url, std::vector<Element> label) {
    const bool external = url.starts_with("http://") || url.starts_with("https://");
    Element a = h("a", {{"href", std::move(url)}}, std::move(label));
    if (external) {
        a.props.set("target", std::string("_blank"));
        a.props.set("rel", std::string("noopener noreferrer"));
    }
    return a;
}

Element view(MarkdownProps props) {
    Document doc = parse(props.text);
#ifdef __EMSCRIPTEN__
    ensureStyles();
    return withKey(h("div", {{"className", "rx-md"}}, blocks(doc, props)), std::move(props.key));
#else
    return withKey(h("box", {}, blocks(doc, props)), std::move(props.key));
#endif
}

}  // namespace rx::markdown
