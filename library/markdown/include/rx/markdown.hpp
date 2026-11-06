// reactx::markdown — Markdown as ReactX elements: HTML in the browser (h1-h6,
// p, lists, code, quotes, tables, links, images), colors and indents in a
// terminal. CommonMark basics plus GitHub tables, task lists, strikethrough
// and bare links. Raw HTML is shown as text, never run, and `javascript:`
// links are dropped, so text from users is safe to show.
//
//   rx::markdown::view({.text = "# Hello\n\nSome **bold** text and `code`."})
//
// Comes with ReactX (library/markdown); link it in CMakeLists.txt:
//   target_link_libraries(my_app PRIVATE reactx::markdown)
#pragma once

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "reactx/reactx.hpp"

namespace rx::markdown {

// ---- the parsed document (for your own rendering, a table of contents, ...) ----

struct Inline {
    enum class Kind { Text, Emphasis, Strong, Strike, Code, Link, Image, LineBreak };
    Kind kind = Kind::Text;
    std::string text{};             // Text, Code; the alt text of an Image
    std::string url{};              // Link, Image ("" if the link was unsafe)
    std::vector<Inline> children{};  // Emphasis, Strong, Strike, Link
};

struct Block {
    enum class Kind { Paragraph, Heading, Code, Quote, List, Rule, Table };
    Kind kind = Kind::Paragraph;
    int level = 0;                    // Heading: 1-6
    std::string id{};                 // Heading: "getting-started", for #links
    std::vector<Inline> inlines{};    // Paragraph, Heading
    std::string code{};               // Code: the text
    std::string language{};           // Code: after ```
    std::vector<Block> children{};    // Quote: the blocks inside
    bool ordered = false;             // List
    int start = 1;                    // List: the first number
    // List: one entry per item, each a list of blocks; checked: a task item
    // ("- [x] done": true, "- [ ] todo": false).
    std::vector<std::vector<Block>> items{};
    std::vector<std::optional<bool>> checked{};
    // Table: the header row, then the body rows; align per column: 'l', 'c', 'r' or 0.
    std::vector<std::vector<std::vector<Inline>>> rows{};
    std::vector<char> align{};
};

using Document = std::vector<Block>;

Document parse(std::string_view markdown);

// The text without the markup, e.g. for a preview line or search:
// "**Hi** [there](/x)" -> "Hi there".
std::string plainText(std::string_view markdown);

// ---- showing it ----

struct MarkdownProps {
    std::string text{};
    // Browser: how links are made. Default: <a href> (other sites open in a new
    // tab). In an app with rx::router pass
    //   .link = [](std::string to, std::vector<Element> label) {
    //       return to.starts_with('/') ? router::link({.to = to, .children = std::move(label)})
    //                                  : rx::markdown::anchor(to, std::move(label)); }
    // so links inside the app don't reload the page.
    std::function<Element(std::string url, std::vector<Element> label)> link{};
    // Terminal: show "(url)" after a link whose text isn't its address.
    bool showUrls = true;
    std::string key{};
};
Element view(MarkdownProps props);

// The default browser link: <a href>, with target="_blank" for other sites.
Element anchor(std::string url, std::vector<Element> label);

}  // namespace rx::markdown
