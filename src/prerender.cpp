// Prerendering for apps that list routes in reactx.json ("web": {"prerender": [...]}).
// Only the Node build of an app links this file (reactx::prerender, see
// cmake/ReactXApp.cmake); it hooks into mountDom, which then writes the HTML
// of each route instead of mounting.
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "reactx/dom.hpp"
#include "reactx/html.hpp"
#include "reactx/router.hpp"
#include "prerender_title.hpp"

namespace rx {

namespace {

std::optional<std::string> readText(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return std::nullopt;
    return std::string(std::istreambuf_iterator<char>(in), {});
}

// "/" -> index.html, "/docs/faq" -> docs/faq.html (GitHub Pages, Netlify and
// HttpServer::serveDirectory serve /docs/faq from docs/faq.html).
std::string escapeHtml(std::string_view text) {
    std::string out;
    for (char c : text) {
        if (c == '&') out += "&amp;";
        else if (c == '<') out += "&lt;";
        else if (c == '>') out += "&gt;";
        else out += c;
    }
    return out;
}

std::filesystem::path pageFile(std::string route) {
    while (route.size() > 1 && route.back() == '/') route.pop_back();
    if (route == "/" || route.empty()) return "index.html";
    return std::filesystem::path(route.substr(1) + ".html");
}

int prerender(const Element& app, const std::string& args) {
    std::vector<std::string> parts;
    for (std::size_t from = 0;;) {
        auto nl = args.find('\n', from);
        parts.push_back(args.substr(from, nl - from));
        if (nl == std::string::npos) break;
        from = nl + 1;
    }
    if (parts.size() < 3) {
        std::fputs("reactx prerender: usage: --reactx-prerender <page.html> <out dir> <route>...\n", stderr);
        return 2;
    }
    auto page = readText(parts[0]);
    if (!page) {
        std::fputs(("reactx prerender: can't read " + parts[0] + "\n").c_str(), stderr);
        return 1;
    }
    // The built page is minified: <main id=app></main>, <base href=/repo/ >.
    // Shells from before v0.21 have a <div> there.
    std::size_t at = std::string::npos, len = 0;
    std::string_view tag;  // kept: the prerendered page has the same container
    for (std::string_view empty : {"<main id=app></main>", "<main id=\"app\"></main>", "<div id=app></div>",
                                   "<div id=\"app\"></div>"}) {
        if (at = page->find(empty); at != std::string::npos) {
            len = empty.size();
            tag = empty.starts_with("<main") ? "main" : "div";
            break;
        }
    }
    if (at == std::string::npos) {
        std::fputs("reactx prerender: no empty <main id=\"app\"></main> (or <div>) in the page\n", stderr);
        return 1;
    }
    if (auto b = page->find("<base href="); b != std::string::npos) {
        b += std::string_view("<base href=").size();
        if ((*page)[b] == '"') ++b;
        router::setBase(page->substr(b, page->find_first_of("\" >", b) - b));
    }
    for (std::size_t i = 2; i < parts.size(); ++i) {
        const std::string& route = parts[i];
        if (route.empty() || route.front() != '/') continue;
        router::reset(route);
        detail::prerenderTitle() = {.collecting = true, .title = std::nullopt};
        std::string html = renderToString(app);
        auto title = std::exchange(detail::prerenderTitle(), {}).title;
        std::string out = *page;
        out.replace(at, len, "<" + std::string(tag) + " id=app>" + html + "</" + std::string(tag) + ">");
        // useTitle's title, so each page has its own (tabs, search results).
        if (auto open = out.find("<title>"); title && open != std::string::npos && open < at)
            if (auto close = out.find("</title>", open); close != std::string::npos)
                out.replace(open + 7, close - open - 7, escapeHtml(*title));
        if (auto head = out.find("</head>"); head != std::string::npos && !detail::prerenderHead().empty() &&
                                              out.find("id=\"rx-ui-styles\"") == std::string::npos)
            out.insert(head, detail::prerenderHead());
        auto file = std::filesystem::path(parts[1]) / pageFile(route);
        std::error_code ec;
        std::filesystem::create_directories(file.parent_path(), ec);
        std::ofstream f(file, std::ios::binary);
        if (!(f << out) || !f.flush()) {
            std::fputs(("reactx prerender: can't write " + file.string() + "\n").c_str(), stderr);
            return 1;
        }
        std::fputs(("reactx: prerendered " + route + " -> " + pageFile(route).string() + "\n").c_str(), stdout);
    }
    return 0;
}

// Registers itself before main() runs.
const bool registered = (detail::prerenderer() = &prerender, true);

}  // namespace

}  // namespace rx
