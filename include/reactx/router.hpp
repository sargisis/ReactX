#pragma once

// rx::router — several pages in one app, like React Router.
//
//   router::routes({
//       {"/",          h(Home)},
//       {"/todos",     h(TodoList)},
//       {"/todos/:id", h(TodoPage)},   // useParam("id") inside
//       {"*",          h(NotFound)},
//   })
//   router::link({.to = "/todos/7", .label = "Open"})
//   router::navigate("/todos");
//
// In the browser it uses the History API: the address bar changes, Back and
// Forward work, and the page never reloads. Serve the app with a fallback to
// index.html for unknown paths (reactx_serve --spa, or a 404.html copy on
// GitHub Pages). In the terminal the history lives in memory.
//
// Base path: when the page has <base href="/ReactX/">, the app lives under
// /ReactX/ (GitHub Pages). Routes, links and navigate() still use paths
// without it ("/todos/7"); the router adds and removes the base itself.

#include <map>
#include <string>
#include <vector>

#include "reactx/element.hpp"

namespace rx::router {

using Params = std::map<std::string, std::string>;

struct Route {
    std::string path;  // "/todos/:id", "*" = anything
    Element element;
};

// Renders the element of the first route that matches the current path.
Element routes(std::vector<Route> table);

struct LinkProps {
    std::string to{};
    std::string label{};               // or children
    std::vector<Element> children{};
    std::string className{};
    std::string activeClassName = "active";  // added when `to` is the current path
    std::string key{};
};
// Browser: <a href> that navigates without reloading (Ctrl+click still opens a
// new tab). Terminal: focusable text, Enter follows it.
Element link(LinkProps props);

// Go to `path` ("/todos/7?tab=info"). replace: don't add a history entry.
void navigate(const std::string& path, bool replace = false);
void back();
// The current path with the query ("/todos/7?tab=info"); not a hook.
std::string currentPath();
// Clears the history and goes to `path` (tests, terminal apps).
void reset(const std::string& path = "/");

// The base path ("/ReactX", or "" at the root). Read from <base href> in the
// browser; setBase() overrides it.
std::string base();
void setBase(std::string path);
// Full URL path for an app path: href("/todos/7") == "/ReactX/todos/7".
std::string href(const std::string& path);

// ---- hooks ----

// The current path without the query; the component re-renders on navigation.
std::string useLocation();
// Parameters of the matched route: "/todos/:id" at "/todos/7" -> {"id": "7"}.
const Params& useParams();
std::string useParam(const std::string& name);
// A query string parameter: "/search?q=milk" -> useQuery("q") == "milk".
std::string useQuery(const std::string& name);

}  // namespace rx::router
