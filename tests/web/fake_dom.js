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
    constructor(tag, ns) {
      super();
      this.tagName = tag.toUpperCase();
      this.namespaceURI = ns || "http://www.w3.org/1999/xhtml";
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
      for (let n = this; n && !e.stopped; n = n.parentNode) for (const fn of (n.listeners?.[e.type] || [])) fn(e);
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
    // Selectors: "tag", "[attr]", "tag[attr]", comma-separated.
    querySelectorAll(selector) {
      const tests = selector.split(",").map(part => {
        const m = part.trim().match(/^([a-z0-9]*)(?:\[([a-z0-9-]+)\])?$/);
        if (!m) throw new Error("fake DOM: unsupported selector " + part);
        return el => (!m[1] || el.tagName === m[1].toUpperCase()) && (!m[2] || el.getAttribute(m[2]) !== null);
      });
      const out = [];
      const walk = el => {
        for (const c of el.childNodes) if (c instanceof Element) {
          if (tests.some(t => t(c))) out.push(c);
          walk(c);
        }
      };
      walk(this);
      return out;
    }
    focus() { document.activeElement = this; }
    contains(other) { for (let n = other; n; n = n.parentNode) if (n === this) return true; return false; }
    get isConnected() { for (let n = this; n; n = n.parentNode) if (n === app || n === head) return true; return false; }
    set textContent(text) {
      for (const c of this.childNodes) c.parentNode = null;
      this.childNodes = text === "" ? [] : [new Text(text)];  // like the DOM: "" leaves no node
    }
    get textContent() { return this.childNodes.map(c => c instanceof Text ? c.nodeValue : c.textContent).join(""); }
    find(pred) {
      if (pred(this)) return this;
      for (const c of this.childNodes) if (c instanceof Element) { const r = c.find(pred); if (r) return r; }
      return null;
    }
  }

  const app = new Element("div");
  app.setAttribute("id", "app");
  const head = new Element("head");
  globalThis.document = {
    head,
    activeElement: null,
    getElementById(id) { return head.find(e => e.id === id || e.getAttribute("id") === id) || app.find(e => e.getAttribute("id") === id); },
    querySelector(sel) {
      if (sel === "base[href]") return head.find(e => e.tagName === "BASE" && e.getAttribute("href") !== null);
      return sel === "#app" ? app : app.find(e => "#" + e.getAttribute("id") === sel);
    },
    createElement(tag) { return new Element(tag); },
    createElementNS(ns, tag) { return new Element(tag, ns); },
    createTextNode(text) { return new Text(text); },
  };

  // Browser history and fetch, enough for rx::router and rx::useFetch.
  const windowListeners = {};
  globalThis.window = {
    addEventListener(type, fn) { (windowListeners[type] ||= []).push(fn); },
    scrollTo() {},
  };
  globalThis.location = { pathname: "/", search: "" };
  const entries = [{ pathname: "/", search: "" }];
  let current = 0;
  const setLocation = (url) => {
    const q = url.indexOf("?");
    location.pathname = q < 0 ? url : url.slice(0, q);
    location.search = q < 0 ? "" : url.slice(q);
  };
  globalThis.history = {
    pushState(_s, _t, url) { entries.splice(current + 1); entries.push(url); current++; setLocation(url); },
    replaceState(_s, _t, url) { entries[current] = url; setLocation(url); },
    back() {
      if (current === 0) return;
      current--;
      setLocation(typeof entries[current] === "string" ? entries[current] : "/");
      setTimeout(() => (windowListeners.popstate || []).forEach(fn => fn({ type: "popstate" })), 0);  // async, like browsers
    },
  };
  // fetch: answers come from rxTest.mock(); every request is recorded.
  const mocks = {};
  const requests = [];
  // A mock may wait `delay` ms and send `headers`; an AbortSignal rejects it.
  globalThis.fetch = (url, opts) => {
    const method = (opts && opts.method) || "GET";
    const sent = Object.entries((opts && opts.headers) || {}).map(([n, v]) => " [" + n + ": " + v + "]").join("");
    requests.push(method + " " + url + (opts && opts.body ? " " + opts.body : "") + sent);
    const m = mocks[method + " " + url];
    const signal = opts && opts.signal;
    return new Promise((resolve, reject) => {
      const timer = setTimeout(() => {
        if (!m) return reject(new Error("no route " + method + " " + url));
        const hs = new Map(Object.entries(m.headers || {}));
        resolve({ status: m.status, headers: hs, text: () => Promise.resolve(m.body) });
      }, (m && m.delay) || 0);
      if (signal) signal.addEventListener("abort", () => { clearTimeout(timer); aborted.push(method + " " + url); reject(new Error("aborted")); });
    });
  };
  const aborted = [];
  // localStorage; rxTest.storageFull(true) makes setItem throw like a full quota.
  const store = new Map();
  let full = false;
  globalThis.localStorage = {
    getItem: (k) => (store.has(k) ? store.get(k) : null),
    setItem: (k, v) => {
      if (full) { const e = new Error("the quota has been exceeded"); e.name = "QuotaExceededError"; throw e; }
      store.set(k, String(v));
    },
    removeItem: (k) => store.delete(k),
  };

  // Helpers the C++ tests call through emscripten_run_script.
  const byId = (id) => {
    const el = document.querySelector("#" + id);
    if (!el) throw new Error("no element #" + id);
    return el;
  };
  const event = (type, extra) => Object.assign({
    type, preventDefault() { this.defaultPrevented = true; }, stopPropagation() { this.stopped = true; } }, extra);
  const all = (pred) => {
    const out = [];
    const walk = (el) => { for (const c of el.childNodes) if (c instanceof Element) { if (pred(c)) out.push(c); walk(c); } };
    walk(app);
    return out;
  };
  const byClass = (cls) => all(e => (e.getAttribute("class") || "").split(" ").includes(cls));
  const nth = (list, i, what) => { const el = list[i || 0]; if (!el) throw new Error("not found: " + what); return el; };
  const byText = (text) => nth(all(e => e.textContent === text).reverse(), 0, "text " + text);  // deepest match
  globalThis.rxTest = {
    // Like GitHub Pages: <base href="/ReactX/"> and the page opened at `path`.
    setBase: (href, path) => {
      const b = new Element("base");
      b.setAttribute("href", href);
      head.appendChild(b);
      setLocation(path);
      entries[0] = path;
    },
    mock: (method, url, status, body, extra) => { mocks[method + " " + url] = Object.assign({ status, body }, extra || {}); },
    aborted: () => aborted.join("\n"),
    storageFull: (on) => { full = on; },
    requests: () => requests.join("\n"),
    path: () => location.pathname + location.search,
    back: () => history.back(),
    clickPrevented: (id) => { const e = event("click", { button: 0 }); byId(id).dispatchEvent(e); return e.defaultPrevented ? "prevented" : "followed"; },
    ctrlClick: (id) => { const e = event("click", { button: 0, ctrlKey: true }); byId(id).dispatchEvent(e); return e.defaultPrevented ? "prevented" : "followed"; },
    clickText: (text) => byText(text).dispatchEvent(event("click")),
    clickClass: (cls, i) => nth(byClass(cls), i, cls).dispatchEvent(event("click")),
    countClass: (cls) => String(byClass(cls).length),
    typeClass: (cls, text, i) => { const el = nth(byClass(cls), i, cls); el.value = text; el.dispatchEvent(event("input")); },
    keyClass: (cls, key, i) => nth(byClass(cls), i, cls).dispatchEvent(event("keydown", { key })),
    toggleIn: (cls, i) => { const el = nth(byClass(cls), i, cls).querySelectorAll("input")[0]; el.checked = !el.checked; el.dispatchEvent(event("change")); },
    focusText: (text) => byText(text).focus(),
    keyFocused: (key, shift) => { const e = event("keydown", { key, shiftKey: !!shift }); document.activeElement.dispatchEvent(e); return e.defaultPrevented ? "prevented" : ""; },
    html: () => app.innerHTML,
    click: (id) => byId(id).dispatchEvent(event("click")),
    type: (id, text) => { const el = byId(id); el.value = text; el.dispatchEvent(event("input")); },
    toggle: (id) => { const el = byId(id); el.checked = !el.checked; el.dispatchEvent(event("change")); },
    key: (id, key, shift) => byId(id).dispatchEvent(event("keydown", { key, shiftKey: !!shift })),
    submit: (id) => { const e = event("submit"); byId(id).dispatchEvent(e); return e.defaultPrevented ? "prevented" : "reloaded"; },
    value: (id) => byId(id).value,
    // A key press the way a browser delivers it: the window's capture
    // listeners first (useKeyboard), then the element and its ancestors.
    // id "" = nothing focused (the event targets the page itself).
    pressKey: (id, key, ctrl, shift) => {
      const target = id ? byId(id) : { tagName: "BODY" };
      const e = event("keydown", { key, ctrlKey: !!ctrl, shiftKey: !!shift, target });
      for (const fn of (windowListeners.keydown || [])) fn(e);
      if (!e.stopped && id) target.dispatchEvent(e);
      return e.defaultPrevented ? "consumed" : "passed";
    },
    scroll: (id, top, scrollHeight, clientHeight) => {
      const el = byId(id);
      Object.assign(el, { scrollTop: top, scrollLeft: 0, scrollHeight, scrollWidth: 300, clientHeight, clientWidth: 300 });
      el.dispatchEvent(event("scroll"));
    },
    focusIn: (id) => byId(id).dispatchEvent(event("focusin")),
    focusOut: (id) => byId(id).dispatchEvent(event("focusout")),
    ns: (id) => byId(id).namespaceURI,
    tag: (id) => byId(id).tagName,
    focused: () => document.activeElement ? (document.activeElement.textContent || document.activeElement.tagName) : "",
  };
})();
