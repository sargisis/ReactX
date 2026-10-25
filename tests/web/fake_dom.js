// Tiny DOM for running the DOM backend under Node (no browser needed).
// Enough for ReactX: elements, text nodes, attributes, a few properties,
// insertBefore/removeChild, bubbling events and innerHTML for assertions.
(function () {
  class Node {
    constructor() { this.parentNode = null; this.childNodes = []; }
    insertBefore(child, ref) {
      if (child.parentNode) child.parentNode.removeChild(child);
      const i = ref ? this.childNodes.indexOf(ref) : -1;
      if (i < 0) this.childNodes.push(child); else this.childNodes.splice(i, 0, child);
      child.parentNode = this;
      return child;
    }
    appendChild(child) { return this.insertBefore(child, null); }
    removeChild(child) {
      const i = this.childNodes.indexOf(child);
      if (i >= 0) this.childNodes.splice(i, 1);
      child.parentNode = null;
      return child;
    }
  }
  class Text extends Node {
    constructor(text) { super(); this.nodeValue = text; }
    get outerHTML() { return this.nodeValue; }
  }
  class Element extends Node {
    constructor(tag) {
      super();
      this.tagName = tag.toUpperCase();
      this.attributes = new Map();
      this.style = { cssText: "" };
      this.listeners = {};
      this.value = "";
      this.checked = false;
      this.disabled = false;
      this.selected = false;
    }
    get type() { return this.attributes.get("type") || ""; }
    setAttribute(n, v) { this.attributes.set(n, String(v)); }
    removeAttribute(n) { this.attributes.delete(n); }
    getAttribute(n) { return this.attributes.has(n) ? this.attributes.get(n) : null; }
    addEventListener(type, fn) { (this.listeners[type] ||= []).push(fn); }
    dispatchEvent(e) {
      e.target ||= this;
      for (let n = this; n; n = n.parentNode) for (const fn of (n.listeners?.[e.type] || [])) fn(e);
    }
    get innerHTML() { return this.childNodes.map(c => c.outerHTML).join(""); }
    get outerHTML() {
      const tag = this.tagName.toLowerCase();
      let attrs = "";
      for (const [k, v] of this.attributes) attrs += v === "" ? ` ${k}` : ` ${k}="${v}"`;
      if (this.style.cssText) attrs += ` style="${this.style.cssText}"`;
      if (tag === "input") {
        if (this.value) attrs += ` .value="${this.value}"`;
        if (this.checked) attrs += ` .checked`;
        return `<${tag}${attrs}>`;
      }
      if (this.disabled) attrs += ` .disabled`;
      return `<${tag}${attrs}>${this.innerHTML}</${tag}>`;
    }
    find(pred) {
      if (pred(this)) return this;
      for (const c of this.childNodes) if (c instanceof Element) { const r = c.find(pred); if (r) return r; }
      return null;
    }
  }

  const app = new Element("div");
  app.setAttribute("id", "app");
  globalThis.document = {
    querySelector(sel) { return sel === "#app" ? app : app.find(e => "#" + e.getAttribute("id") === sel); },
    createElement(tag) { return new Element(tag); },
    createTextNode(text) { return new Text(text); },
  };

  // Helpers the C++ tests call through emscripten_run_script.
  const byId = (id) => {
    const el = document.querySelector("#" + id);
    if (!el) throw new Error("no element #" + id);
    return el;
  };
  const event = (type, extra) => Object.assign({ type, preventDefault() { this.defaultPrevented = true; } }, extra);
  globalThis.rxTest = {
    html: () => app.innerHTML,
    click: (id) => byId(id).dispatchEvent(event("click")),
    type: (id, text) => { const el = byId(id); el.value = text; el.dispatchEvent(event("input")); },
    toggle: (id) => { const el = byId(id); el.checked = !el.checked; el.dispatchEvent(event("change")); },
    key: (id, key, shift) => byId(id).dispatchEvent(event("keydown", { key, shiftKey: !!shift })),
    submit: (id) => { const e = event("submit"); byId(id).dispatchEvent(e); return e.defaultPrevented ? "prevented" : "reloaded"; },
    value: (id) => byId(id).value,
  };
})();
