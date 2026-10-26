// DOM backend: the C++ side keeps node ids, the JavaScript side (EM_JS below)
// keeps a Map from id to DOM node and forwards browser events back to C++.
#include "reactx/dom.hpp"

#include "reactx/contract.hpp"
#include "reactx/parse.hpp"

#include <emscripten.h>

#include <memory>
#include <ranges>
#include <string_view>
#include <string>
#include <variant>
#include <vector>

namespace {

// ---- JavaScript side ----

EM_JS(void, rx_js_init, (), {
    if (Module.rxNodes) return;
    Module.rxNodes = new Map();
    Module.rxSetProp = function(el, name, kind, str, num) {
        if (name === "className") name = "class";
        if (name === "htmlFor") name = "for";
        if (name === "style") { el.style.cssText = kind === 3 ? str : ""; return; }
        if (name === "value") {
            const v = kind === 3 ? str : kind === 2 ? String(num) : "";
            el.__rxValue = v;
            if (el.value !== v) el.value = v;
            return;
        }
        if (name === "checked" || name === "disabled" || name === "selected") { el[name] = kind === 1 && num !== 0; return; }
        if (kind === 1) {  // boolean attribute
            if (num !== 0) el.setAttribute(name, ""); else el.removeAttribute(name);
            return;
        }
        el.setAttribute(name, kind === 3 ? str : kind === 2 ? String(num) : "");
    };
});

EM_JS(void, rx_js_root, (double id, const char* selector), {
    const sel = UTF8ToString(selector);
    const el = document.querySelector(sel);
    if (!el) throw new Error("ReactX: no element matches " + sel);
    el.__rxId = id;
    Module.rxNodes.set(id, el);
});

// SVG elements need the SVG namespace. These tags are SVG-only; "svg:text",
// "svg:a", "svg:title", "svg:style" spell the ones HTML also has.
EM_JS(void, rx_js_create_element, (double id, const char* tag), {
    if (!Module.rxSvgTags) Module.rxSvgTags = new Set(["svg", "g", "path", "circle", "ellipse", "line", "polyline",
        "polygon", "rect", "defs", "use", "symbol", "marker", "pattern", "mask", "clipPath", "linearGradient",
        "radialGradient", "stop", "tspan", "textPath", "foreignObject", "filter", "feGaussianBlur", "feOffset",
        "feBlend", "feColorMatrix", "feDropShadow", "animate", "animateTransform"]);
    const t = UTF8ToString(tag);
    const ns = "http://www.w3.org/2000/svg";
    const el = t.startsWith("svg:") ? document.createElementNS(ns, t.slice(4))
             : Module.rxSvgTags.has(t) ? document.createElementNS(ns, t)
             : document.createElement(t);
    el.__rxId = id;
    Module.rxNodes.set(id, el);
});

EM_JS(void, rx_js_create_text, (double id, const char* text), {
    Module.rxNodes.set(id, document.createTextNode(UTF8ToString(text)));
});

EM_JS(void, rx_js_set_text, (double id, const char* text), {
    Module.rxNodes.get(id).nodeValue = UTF8ToString(text);
});

EM_JS(void, rx_js_set_prop, (double id, const char* name, int kind, const char* str, double num), {
    Module.rxSetProp(Module.rxNodes.get(id), UTF8ToString(name), kind, kind === 3 ? UTF8ToString(str) : "", num);
});

EM_JS(void, rx_js_remove_prop, (double id, const char* name), {
    const el = Module.rxNodes.get(id);
    const n = UTF8ToString(name);
    if (n === "style") el.style.cssText = "";
    else if (n === "value") { delete el.__rxValue; el.value = ""; }
    else if (n === "checked" || n === "disabled" || n === "selected") el[n] = false;
    else el.removeAttribute(n === "className" ? "class" : n === "htmlFor" ? "for" : n);
});

// Focusable elements inside `el`, in tab order (enough for dialogs).
EM_JS(void, rx_js_init_focus, (), {
    if (Module.rxFocusables) return;
    Module.rxFocusables = function(el) {
        return Array.from(el.querySelectorAll("button, input, select, textarea, a[href], [tabindex]"))
            .filter(function(n) { return !n.disabled && n.getAttribute("tabindex") !== "-1"; });
    };
});

EM_JS(void, rx_js_insert, (double parentId, double childId, int index), {
    const parent = Module.rxNodes.get(parentId);
    const child = Module.rxNodes.get(childId);
    const fresh = !child.parentNode;
    if (child.parentNode === parent) parent.removeChild(child);
    parent.insertBefore(child, parent.childNodes[index] || null);
    // data-rx-trap (dialogs): move focus inside when it appears, remember where it was.
    if (fresh && child.getAttribute && child.getAttribute("data-rx-trap") !== null) {
        child.__rxPrevFocus = document.activeElement;
        const focusFirst = function() {
            const first = Module.rxFocusables(child)[0];
            if (first) first.focus();
        };
        // Already on the page: focus now; otherwise once its parent is attached.
        if (child.isConnected) focusFirst(); else queueMicrotask(focusFirst);
    }
});

EM_JS(void, rx_js_remove, (double parentId, double childId), {
    const parent = Module.rxNodes.get(parentId);
    const child = Module.rxNodes.get(childId);
    if (child.parentNode === parent) parent.removeChild(child);
    // A dialog closed: give focus back to where it was.
    const prev = child.__rxPrevFocus;
    if (prev && prev.focus && prev.isConnected !== false) prev.focus();
});

EM_JS(void, rx_js_destroy, (double id), {
    Module.rxNodes.delete(id);
});

// One delegated listener per event type on the container.
EM_JS(void, rx_js_listen, (int app, double rootId), {
    const root = Module.rxNodes.get(rootId);
    const keyName = function(k) {
        return { ArrowUp: "Up", ArrowDown: "Down", ArrowLeft: "Left", ArrowRight: "Right", Esc: "Escape" }[k] || k;
    };
    // Like React: the handler of the target runs, then those of all its
    // ancestors. An element with data-rx-self only gets clicks on itself
    // (a dialog backdrop must not close on clicks inside the dialog).
    const send = function(e, name, text) {
        const path = [];
        for (let el = e.target; el; el = el.parentNode) {
            const selfOnly = name === "onClick" && el !== e.target && el.getAttribute("data-rx-self") !== null;
            if (el.__rxId !== undefined && !selfOnly) path.push(el.__rxId);
            if (el === root) break;
        }
        let handled = false;
        for (const id of path) {
            if (!Module.rxNodes.has(id)) continue;  // removed by an earlier handler
            handled = Module.ccall("rx_dom_event", "number",
                ["number", "number", "string", "string", "number", "number", "number"],
                [app, id, name, text, e.ctrlKey ? 1 : 0, e.altKey ? 1 : 0, e.shiftKey ? 1 : 0]) !== 0 || handled;
        }
        return handled;
    };
    // Keep controlled inputs in sync with the value from C++ state.
    const resync = function(el) {
        if (el && el.__rxValue !== undefined && el.value !== el.__rxValue) el.value = el.__rxValue;
    };
    root.addEventListener("click", function(e) {
        // Router links: stay on the page (unless the user wants a new tab).
        for (let el = e.target; el && el !== root; el = el.parentNode) {
            if (el.getAttribute && el.getAttribute("data-rx-link") !== null) {
                if (!e.ctrlKey && !e.metaKey && !e.shiftKey && !e.button) e.preventDefault();
                else return;  // the browser opens it; don't navigate here too
                break;
            }
        }
        send(e, "onClick", "");
    });
    root.addEventListener("input", function(e) { send(e, "onInput", e.target.value ?? ""); resync(e.target); });
    root.addEventListener("change", function(e) {
        const t = e.target;
        send(e, "onChange", t.type === "checkbox" || t.type === "radio" ? (t.checked ? "true" : "false") : (t.value ?? ""));
    });
    // No preventDefault: typing must keep working even if a parent has onKey.
    root.addEventListener("keydown", function(e) {
        if (e.isComposing) return;
        if (e.key === "Tab") {  // keep Tab inside the last open dialog
            const traps = root.querySelectorAll("[data-rx-trap]");
            const trap = traps[traps.length - 1];
            if (trap) {
                const items = Module.rxFocusables(trap);
                const i = items.indexOf(document.activeElement);
                if (items.length && (i < 0 || (e.shiftKey && i === 0) || (!e.shiftKey && i === items.length - 1))) {
                    e.preventDefault();
                    items[e.shiftKey ? items.length - 1 : 0].focus();
                }
            }
        }
        send(e, "onKey", keyName(e.key));
    });
    root.addEventListener("submit", function(e) { e.preventDefault(); send(e, "onSubmit", ""); });
    // Focus: focusin / focusout bubble (like React's onFocus / onBlur).
    root.addEventListener("focusin", function(e) { send(e, "onFocus", ""); });
    root.addEventListener("focusout", function(e) { send(e, "onBlur", ""); });
    // Scroll doesn't bubble: only the scrolled element's onScroll runs (like React).
    // Captured on the root, so scrolling inside any element of the app is seen.
    root.addEventListener("scroll", function(e) {
        const t = e.target;
        if (!t || t.__rxId === undefined || !Module.rxNodes.has(t.__rxId)) return;
        const pos = [t.scrollTop, t.scrollLeft, t.scrollHeight, t.scrollWidth, t.clientHeight, t.clientWidth]
            .map(function(v) { return Math.round(v || 0); }).join(",");
        Module.ccall("rx_dom_event", "number", ["number", "number", "string", "string", "number", "number", "number"],
                     [app, t.__rxId, "onScroll", pos, 0, 0, 0]);
    }, {capture: true, passive: true});
});

// ---- C++ side ----

rx::NodeHandle nextId = 1;

double js(rx::NodeHandle h) { return static_cast<double>(h); }

std::vector<rx::Root*>& apps() {
    static std::vector<rx::Root*> list;
    return list;
}

struct DomApp {
    rx::DomRenderer renderer;
    rx::Root root;
    explicit DomApp(const std::string& container) : renderer(container), root(renderer) {}
};

}  // namespace

namespace {

// "top,left,scrollHeight,scrollWidth,clientHeight,clientWidth" from the page.
rx::ScrollEvent scrollEvent(std::string_view text) {
    int values[6] = {};
    std::size_t i = 0;
    for (auto part : std::views::split(text, ',')) {
        if (i == 6) break;
        values[i++] = rx::parseNumber<int>(std::string_view(part.begin(), part.end())).value_or(0);
    }
    return {.top = values[0], .left = values[1], .scrollHeight = values[2], .scrollWidth = values[3],
            .clientHeight = values[4], .clientWidth = values[5]};
}

}  // namespace

// Called from JavaScript for every DOM event. Returns 1 if a handler ran.
extern "C" EMSCRIPTEN_KEEPALIVE int rx_dom_event(int app, double node, const char* event, const char* text, int ctrl,
                                                  int alt, int shift) {
    if (app < 0 || static_cast<std::size_t>(app) >= apps().size()) return 0;
    rx::Root& root = *apps()[static_cast<std::size_t>(app)];
    auto handle = static_cast<rx::NodeHandle>(node);
    std::string name = event;
    rx::EventData data;
    if (name == "onKey")
        data = rx::KeyEvent{.key = text, .ctrl = ctrl != 0, .alt = alt != 0, .shift = shift != 0};
    else if (name == "onInput" || name == "onChange")
        data = std::string(text);
    else if (name == "onScroll")
        data = scrollEvent(text);
    return root.dispatch(handle, name, data) ? 1 : 0;
}

namespace rx {

DomRenderer::DomRenderer(std::string container) : root_(nextId++) {
    rx_js_init();
    rx_js_init_focus();
    rx_js_root(js(root_), container.c_str());
}

NodeHandle DomRenderer::createElement(std::string_view type) {
    NodeHandle h = nextId++;
    rx_js_create_element(js(h), std::string(type).c_str());
    return h;
}

NodeHandle DomRenderer::createText(std::string_view text) {
    NodeHandle h = nextId++;
    rx_js_create_text(js(h), std::string(text).c_str());
    return h;
}

void DomRenderer::setText(NodeHandle node, std::string_view text) {
    rx_js_set_text(js(node), std::string(text).c_str());
}

void DomRenderer::setProp(NodeHandle node, std::string_view name, const PropValue& value) {
    std::string n(name);
    if (const auto* b = std::get_if<bool>(&value)) rx_js_set_prop(js(node), n.c_str(), 1, "", *b ? 1 : 0);
    else if (const auto* i = std::get_if<std::int64_t>(&value))
        rx_js_set_prop(js(node), n.c_str(), 2, "", static_cast<double>(*i));
    else if (const auto* d = std::get_if<double>(&value)) rx_js_set_prop(js(node), n.c_str(), 2, "", *d);
    else if (const auto* s = std::get_if<std::string>(&value)) rx_js_set_prop(js(node), n.c_str(), 3, s->c_str(), 0);
    else rx_js_remove_prop(js(node), n.c_str());
}

void DomRenderer::removeProp(NodeHandle node, std::string_view name) {
    rx_js_remove_prop(js(node), std::string(name).c_str());
}

void DomRenderer::insertChild(NodeHandle parent, NodeHandle child, std::size_t index) {
    rx_js_insert(js(parent), js(child), static_cast<int>(index));
}

void DomRenderer::removeChild(NodeHandle parent, NodeHandle child) { rx_js_remove(js(parent), js(child)); }

void DomRenderer::destroyNode(NodeHandle node) { rx_js_destroy(js(node)); }

void flushDom() {
    for (Root* root : apps()) root->flush();
}

Root& mountDom(Element element, const std::string& container) {
    REACTX_PRE(!container.empty());
    // Lives for the whole page: the runtime keeps running after main().
    auto* app = new DomApp(container);
    int index = static_cast<int>(apps().size());
    apps().push_back(&app->root);
    rx_js_listen(index, js(app->renderer.rootNode()));
    app->root.render(std::move(element));
    return app->root;
}

}  // namespace rx
