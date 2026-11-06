# reactx::markdown

Markdown as [ReactX](https://github.com/sargisis/ReactX) elements: semantic HTML in the browser, colors and indents in a terminal. CommonMark basics plus GitHub tables, task lists, strikethrough and bare links.

На русском: [README.ru.md](README.ru.md).

```cpp
#include "rx/markdown.hpp"

rx::markdown::view({.text = "# Hello\n\nSome **bold** text, `code` and a [link](https://example.com)."})
```

## Add it to a project

It comes with ReactX (since 1.3). Link it in the project's `CMakeLists.txt`:

```cmake
target_link_libraries(my_app PRIVATE reactx::markdown)
```

## What it shows

| Markdown | Browser | Terminal |
|---|---|---|
| `# Title` … `###### Title`, `Title` + `===` | `<h1 id="title">` … `<h6>` | bold; h1 cyan underlined, h2 blue |
| `**bold**`, `*em*`, `~~gone~~`, `` `code` `` | `<strong>`, `<em>`, `<del>`, `<code>` | bold, underlined, dim, yellow |
| `[text](url)`, `<https://…>`, bare `https://…` | `<a href>` (other sites in a new tab) | underlined blue, then `(url)` |
| `![alt](src)` | `<img loading="lazy">` | `[alt]` |
| `- item`, `1. item`, nested, `- [x] task` | `<ul>`, `<ol start>`, checkboxes | `•`, `1.`, `[x]` |
| `> quote` | `<blockquote>` | `┃` and dim |
| ```` ```lang ```` / indented code | `<pre><code class="language-lang">` | green, indented |
| tables with `:--`, `:-:`, `--:` | `<table>` with alignment | aligned columns |
| `---` | `<hr>` | a line |
| two spaces or `\` at the end of a line | `<br>` | a new line |

**Safe for text from users:** raw HTML is shown as text and never run, and links other than `http`, `https`, `mailto`, `tel` and relative ones (`javascript:` …) become plain text.

In the browser the output sits in `<div class="rx-md">` with a little CSS (code blocks, quotes, tables) that any rule of your own overrides; it follows the text color, so it works in light and dark themes.

## Links inside the app

By default a link is `<a href>`, so `/docs` reloads the page. In an app with `rx::router`, make internal links router links:

```cpp
rx::markdown::view({.text = text, .link = [](std::string to, std::vector<Element> label) {
    return to.starts_with('/') ? router::link({.to = to, .children = std::move(label)})
                               : rx::markdown::anchor(to, std::move(label));
}})
```

`.showUrls = false` hides the `(url)` after links in a terminal.

## The parsed document

```cpp
auto doc = rx::markdown::parse(text);   // std::vector<Block>
for (auto& b : doc)
    if (b.kind == rx::markdown::Block::Kind::Heading) toc.push_back({b.id, b.level});   // a table of contents
rx::markdown::plainText("**Hi** [there](/x)");   // "Hi there": previews, search
```

`Block` (paragraph, heading with `level` and `id`, code with `language`, quote, list with `items` and `checked`, rule, table with `rows` and `align`) and `Inline` (text, emphasis, strong, strike, code, link, image, line break) are plain structs for your own rendering.

## License

MIT or Apache-2.0, like ReactX.
