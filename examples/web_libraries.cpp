// The libraries that come with ReactX, live: reactx::query, markdown, charts,
// icons and i18n on one page (web_libraries.html, linked from the site's
// /demos). A page of its own, so the site's .wasm doesn't carry them. The
// language follows the site (localStorage "rx-lang").
#include <emscripten.h>

#include <chrono>
#include <cstdlib>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "reactx/dom.hpp"
#include "reactx/fetch.hpp"
#include "reactx/json.hpp"
#include "reactx/parse.hpp"
#include "reactx/reactx.hpp"
#include "reactx/ui.hpp"
#include "rx/charts.hpp"
#include "rx/i18n.hpp"
#include "rx/icons.hpp"
#include "rx/markdown.hpp"
#include "rx/query.hpp"

using namespace rx;

namespace {

bool useEnglish() { return i18n::useLanguage() != "ru"; }

std::string tr(bool en, std::string_view english, std::string_view russian) { return std::string(en ? english : russian); }

#include "site_libs.hpp"

constexpr const char* kCss = R"css(
#app { max-width: 1040px; }
body { place-items: start center; }
.top { display: flex; justify-content: space-between; align-items: center; gap: 12px; flex-wrap: wrap; margin-bottom: 8px; }
.top a { color: var(--muted); }
.top h1 { margin: 0; overflow-wrap: anywhere; }
@media (max-width: 560px) { .top h1 { font-size: 1.2rem; } }
.live-pane, .lib-split > *, .lib-grid > * { min-width: 0; }
.lead { color: var(--muted); margin: 0 0 18px; }
.live { background: var(--card); border: 1px solid var(--border); border-radius: 14px; padding: 8px 20px 20px; }
@media (max-width: 560px) { .live { padding: 4px 12px 14px; } }
.live-pane { display: flex; flex-direction: column; gap: 12px; min-height: 380px; }
.lib-grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(340px, 1fr)); gap: 18px; align-items: start; }
.lib-split { display: grid; grid-template-columns: repeat(auto-fit, minmax(260px, 1fr)); gap: 18px; align-items: start; }
.lib-preview { border: 1px solid var(--border); border-radius: 10px; padding: 14px 18px; background: var(--bg); min-width: 0; }
.lib-card { border: 1px solid var(--border); border-radius: 10px; padding: 12px 16px; display: flex; flex-direction: column; gap: 8px; }
.lib-icons { display: grid; grid-template-columns: repeat(auto-fill, minmax(92px, 1fr)); gap: 6px; }
.lib-icon { display: flex; flex-direction: column; align-items: center; gap: 6px; padding: 10px 4px; border-radius: 8px; }
.lib-icon:hover { background: var(--bg); }
.lib-icon small, .lib-row small { font: 11px ui-monospace, Menlo, Consolas, monospace; color: var(--muted); }
.lib-row { display: flex; justify-content: space-between; gap: 12px; padding: 7px 0; border-bottom: 1px solid var(--border); flex-wrap: wrap; }
)css";

// "#icons" in the address opens that tab.
EM_JS(char*, libs_js_hash, (), { return stringToNewUTF8((location.hash || "").slice(1)); });

int initialTab() {
    char* raw = libs_js_hash();
    std::string hash = raw;
    std::free(raw);
    for (int i = 0; i < static_cast<int>(std::size(kLibraryTabs)); ++i)
        if (hash == kLibraryTabs[i]) return i;
    return 0;
}

Element Page(const Props&) {
    bool en = useEnglish();
    useTitle(tr(en, "Libraries — ReactX", "Библиотеки — ReactX"));
    return h("div", {}, {
        h("style", {}, {kCss}),
        h("div", {{"className", "top"}}, {
            h("h1", {}, {tr(en, "Libraries that come with ReactX", "Библиотеки, которые идут с ReactX")}),
            h("a", {{"href", "./demos"}}, {tr(en, "← All demos", "← Все демо")}),
        }),
        h("p", {{"className", "lead"}}, {tr(en, "Live: C++ compiled to WebAssembly. The same code runs in a terminal.",
                                            "Вживую: C++, скомпилированный в WebAssembly. Тот же код работает в терминале.")}),
        h(LibraryDemos, LibraryDemosProps{.initial = initialTab()}),
        ui::toaster(),
    });
}

}  // namespace

int main() {
    i18n::setup({.languages = {{.code = "en", .name = "English"}, {.code = "ru", .name = "Русский"}}, .storageKey = "rx-lang"});
    mountDom(h(Page));
}
