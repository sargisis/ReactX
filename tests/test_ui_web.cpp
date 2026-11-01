// rx::ui in the browser: WebAssembly under Node with tests/web/fake_dom.js.
#include <emscripten.h>

#include <chrono>
#include <iostream>
#include <string>
#include <vector>

#include "reactx/dom.hpp"
#include "reactx/reactx.hpp"
#include "reactx/ui.hpp"

using namespace rx;

namespace {

int failures = 0;

#define CHECK(cond)                                                                   \
    do {                                                                              \
        if (!(cond)) {                                                                \
            ++failures;                                                               \
            std::cerr << "  FAIL " << __FILE__ << ":" << __LINE__ << ": " #cond "\n"; \
        }                                                                             \
    } while (0)

std::string js(const std::string& code) { return emscripten_run_script_string(code.c_str()); }
std::string html() { return js("rxTest.html()"); }
bool has(const std::string& part) { return html().find(part) != std::string::npos; }
std::string focused() { return js("rxTest.focused()"); }

int clicks = 0;
std::string submitted;

Element BigList(const Props&) {
    auto list = ui::useVirtualList({.count = 10000, .rowHeight = 20, .height = 200});
    std::vector<Element> rows;
    for (int i = list.first; i < list.last; ++i) rows.push_back(h("span", {}, {"row " + std::to_string(i)}));
    return list.view(std::move(rows));
}

Element App(const Props&) {
    auto [name, setName] = useState(std::string{});
    auto [agree, setAgree] = useState(false);
    auto [open, setOpen] = useState(false);
    auto [picked, setPicked] = useState(std::string{});
    auto [accent, setAccent] = useState(std::string("cyan"));

    ui::Theme theme;
    theme.accent = accent;

    std::vector<Element> fruit;
    for (std::string f : {"apple", "pear"})
        fruit.push_back(ui::listItem({.label = f, .detail = std::to_string(f.size()), .selected = picked == f,
                                      .onSelect = [setPicked, f] { setPicked(f); }, .key = f}));

    return ui::themeProvider(theme, {
        ui::button({.label = "Save", .onClick = [] { ++clicks; }, .variant = ui::Variant::Primary}),
        ui::button({.label = "Locked", .onClick = [] { ++clicks; }, .disabled = true}),
        ui::button({.label = "Magenta", .onClick = [setAccent] { setAccent(std::string("magenta")); }}),
        ui::input({.value = name,
                   .onChange = [setName](const std::string& v) { setName(v); },
                   .onSubmit = [](const std::string& v) { submitted = v; },
                   .placeholder = "your name",
                   .label = "Name"}),
        ui::checkbox({.label = "I agree", .checked = agree, .onToggle = [setAgree, agree] { setAgree(!agree); }}),
        ui::tabs({.tabs = {{"One", {h("p", {}, {"first page"})}}, {"Two", {h("p", {}, {"second page"})}}}}),
        ui::list({.title = "Fruit", .items = std::move(fruit)}),
        ui::list({.title = "Empty", .emptyText = "no fruit"}),
        h("p", {}, {"name=" + name + " agree=" + (agree ? "yes" : "no") + " picked=" + picked}),
        ui::button({.label = "Open", .onClick = [setOpen] { setOpen(true); }}),
        ui::modal({.title = "Sure?", .open = open, .onClose = [setOpen] { setOpen(false); }, .width = 30,
                   .children = {
                       h("p", {}, {"Delete it?"}),
                       ui::button({.label = "Yes", .onClick = [setOpen] { setOpen(false); }, .variant = ui::Variant::Danger}),
                       ui::button({.label = "No", .onClick = [setOpen] { setOpen(false); }}),
                   }}),
    });
}

// The newer atoms: card, stack, text, select, tooltip, confirm, toasts.
Element Atoms(const Props&) {
    auto [size, setSize] = useState(std::string("m"));
    auto [asking, setAsking] = useState(true);
    auto [result, setResult] = useState(std::string("none"));
    return ui::stack({.children = {
        ui::card({.title = "Stats", .children = {
                      ui::stack({.direction = ui::Direction::Row, .gap = 2, .justify = ui::Justify::Between,
                                 .children = {ui::text({.text = "one"}), ui::text({.text = "two", .muted = true})}}),
                  }}),
        ui::select({.label = "Size", .value = size, .options = {{"", "Any"}, {"s", "Small"}, {"m", "Medium"}, {"l"}},
                    .onChange = [setSize](const std::string& v) { setSize(v); }}),
        ui::tooltip({.text = "Saves the file", .children = {h("button", {{"id", "save"}}, {"Save"})}}),
        h("button", {{"id", "notify"}, {"onClick", [] {
             ui::toast({.text = "Saved!", .variant = ui::Variant::Success, .duration = std::chrono::milliseconds(0)});
         }}}, {"notify"}),
        h("p", {}, {"size=" + size + " result=" + result}),
        ui::confirm({.title = "Delete?", .message = "It can't be undone.", .open = asking, .confirmLabel = "Delete",
                     .danger = true,
                     .onConfirm = [setAsking, setResult] { setResult("deleted"); setAsking(false); },
                     .onCancel = [setAsking, setResult] { setResult("kept"); setAsking(false); }}),
        h(BigList),
        ui::toaster(),
    }});
}

}  // namespace

int main() {
    mountDom(h(App));

    std::cout << "stylesheet is added once\n";
    CHECK(js("String(document.head.querySelectorAll('style').length)") == "1");
    CHECK(js("String(document.getElementById('rx-ui-styles').textContent.includes('.rx-btn--primary'))") == "true");

    std::cout << "theme becomes CSS variables\n";
    CHECK(has("<div class=\"rx-theme\" style=\"--rx-accent: #0891b2; --rx-danger: #dc2626; --rx-success: #16a34a; "
              "--rx-warning: #ca8a04; --rx-muted: #6b7280;\">"));
    js("rxTest.clickText('Magenta')");
    CHECK(has("--rx-accent: #c026d3"));

    std::cout << "buttons\n";
    CHECK(has("<button class=\"rx-btn rx-btn--primary\" type=\"button\">Save</button>"));
    CHECK(has("<button class=\"rx-btn rx-btn--default\" type=\"button\" .disabled>Locked</button>"));
    clicks = 0;
    js("rxTest.clickText('Save')");
    js("rxTest.clickText('Locked')");  // disabled: no handler
    CHECK(clicks == 1);

    std::cout << "input: change, submit, escape\n";
    CHECK(has("<label class=\"rx-field rx-field--input\"><span class=\"rx-field__label\">Name</span><input class=\"rx-input\""));
    CHECK(js("String(document.getElementById('rx-ui-styles').textContent.includes('.rx-field--above'))") == "true");
    js("rxTest.typeClass('rx-input', 'Ann')");
    CHECK(has("name=Ann "));
    js("rxTest.keyClass('rx-input', 'Enter')");
    CHECK(submitted == "Ann");
    js("rxTest.keyClass('rx-input', 'Escape')");
    CHECK(has("name= "));

    std::cout << "checkbox\n";
    js("rxTest.toggleIn('rx-check')");
    CHECK(has("agree=yes"));
    CHECK(has("<input type=\"checkbox\" .checked>"));

    std::cout << "tabs\n";
    CHECK(has("<button aria-selected=\"true\" class=\"rx-tab rx-tab--active\" role=\"tab\" type=\"button\">One</button>"));
    CHECK(has("first page") && !has("second page"));
    js("rxTest.clickText('Two')");
    CHECK(has("second page") && !has("first page"));
    CHECK(has("<button aria-selected=\"true\" class=\"rx-tab rx-tab--active\" role=\"tab\" type=\"button\">Two</button>"));

    std::cout << "lists\n";
    CHECK(has("<div class=\"rx-list__title\">Fruit</div><ul class=\"rx-list__items\">"));
    CHECK(has("<p class=\"rx-list__empty\">no fruit</p>"));
    CHECK(js("String(globalThis.__rxScrolledTo)") == "undefined");  // no scrolling while the page is built
    js("rxTest.clickText('pear')");
    CHECK(has("picked=pear"));
    CHECK(js("rxTest.countClass('rx-item--selected')") == "1");
    CHECK(js("globalThis.__rxScrolledTo.textContent").starts_with("pear"));  // the newly selected item scrolled into view
    CHECK(!has("scrollIntoView") && !has("scrollintoview"));  // not an attribute

    std::cout << "modal: opens, takes focus, traps Tab\n";
    CHECK(js("rxTest.countClass('rx-modal')") == "0");
    js("rxTest.focusText('Open')");
    js("rxTest.clickText('Open')");
    CHECK(js("rxTest.countClass('rx-modal')") == "1");
    CHECK(has("role=\"dialog\" style=\"--rx-modal-width: 36ch\""));
    CHECK(focused() == "Yes");  // first button inside
    CHECK(js("rxTest.focusText('No'); rxTest.keyFocused('Tab')") == "prevented");
    CHECK(focused() == "Yes");  // wrapped around
    CHECK(js("rxTest.keyFocused('Tab', true)") == "prevented");
    CHECK(focused() == "No");

    std::cout << "modal: clicks inside keep it open, backdrop and Escape close it\n";
    js("rxTest.clickText('Delete it?')");
    CHECK(js("rxTest.countClass('rx-modal')") == "1");
    js("rxTest.clickClass('rx-modal')");
    CHECK(js("rxTest.countClass('rx-modal')") == "0");
    CHECK(focused() == "Open");  // focus came back
    js("rxTest.clickText('Open')");
    js("rxTest.keyFocused('Escape')");  // from the focused button, bubbles to the modal
    CHECK(js("rxTest.countClass('rx-modal')") == "0");

    std::cout << "card, stack, text\n";
    mountDom(h(Atoms));  // replaces the app above
    CHECK(has("<section aria-label=\"Stats\" class=\"rx-card\"><h3 class=\"rx-card__title\">Stats</h3>"));
    CHECK(has("flex-direction: row; gap: 16px; align-items: stretch; justify-content: space-between"));
    CHECK(has("<p class=\"rx-text\">one</p><p class=\"rx-text rx-text--muted\">two</p>"));

    std::cout << "virtual list: only the rows on screen exist\n";
    {
        std::string page = html();
        std::size_t rows = 0;
        for (auto at = page.find("rx-vlist__row"); at != std::string::npos; at = page.find("rx-vlist__row", at + 1)) ++rows;
        CHECK(rows > 5 && rows < 60);
        CHECK(has("<span>row 0</span>") && !has("<span>row 9999</span>"));
        CHECK(has("class=\"rx-vlist\" style=\"height: 200px\""));
    }

    std::cout << "select\n";

    CHECK(has("<option value>Any</option><option value=\"s\">Small</option><option value=\"m\">Medium</option>"
              "<option value=\"l\">l</option>"));  // the label falls back to the value
    // value="" must be written: otherwise a browser takes the text ("Any") as the value.
    CHECK(js("document.querySelector('#app').querySelectorAll('option')[0].value") == "");
    CHECK(js("document.querySelector('#app').querySelectorAll('select')[0].value") == "m");
    js("rxTest.changeClass('rx-select', 'l')");
    CHECK(has("size=l"));
    js("rxTest.changeClass('rx-select', '')");
    CHECK(has("size= result"));

    std::cout << "tooltip\n";
    CHECK(!has("Saves the file"));
    js("rxTest.hover('', 'save')");
    CHECK(has("role=\"tooltip\"") && has("Saves the file"));
    js("rxTest.hover('save', '')");
    CHECK(!has("Saves the file"));

    std::cout << "confirm\n";
    CHECK(has("It can't be undone.") && has("rx-btn rx-btn--danger"));
    js("rxTest.clickText('Delete')");
    CHECK(has("result=deleted") && !has("It can't be undone."));

    std::cout << "toasts\n";
    CHECK(has("<div aria-live=\"polite\" class=\"rx-toaster\" role=\"status\"></div>"));
    js("rxTest.click('notify')");
    CHECK(has("rx-toast rx-toast--success") && has("Saved!"));
    js("rxTest.clickClass('rx-toast')");
    CHECK(!has("Saved!"));

    std::cout << (failures ? "FAILED: " + std::to_string(failures) : std::string("OK, all tests passed")) << '\n';
    return failures ? 1 : 0;
}
