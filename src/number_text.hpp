#pragma once

// The shortest text that reads back as the same double ("0.1", "1e+21").
//
// Natively this is std::to_chars. In the browser build it is the browser's own
// Number -> String (ECMAScript also gives the shortest round-trip form):
// libc++'s floating-point to_chars brings ~110 KB of tables into every
// WebAssembly app. Defined in element.cpp.

#include <string>

namespace rx::detail {
std::string numberText(double x);
}
