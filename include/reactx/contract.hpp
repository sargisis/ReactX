#pragma once

// Preconditions, ready for C++26 contracts.
//
//   REACTX_PRE(width > 0);
//
// With a compiler that implements contracts (__cpp_contracts) this is
// contract_assert (GCC 16 with -fcontracts, which ReactX turns on). Otherwise
// a failed check prints what failed and where, the call stack (C++23
// <stacktrace>) and stops in the debugger (C++26 <debugging>) where the
// standard library has those, then stops the program — like a contract
// violation with the "enforce" semantic. Use it for programming errors (a bug
// in the caller), not for things that can fail at run time (bad input,
// network): those return std::expected.

#include <version>

#include <cstdio>
#include <cstdlib>
#include <print>  // not used here; kept so code that relied on it still builds
#include <source_location>
#include <string>

// std::stacktrace needs libstdc++exp with GCC; CMake defines
// REACTX_HAS_STACKTRACE when it links it (see CMakeLists.txt).
#if defined(REACTX_HAS_STACKTRACE) && defined(__cpp_lib_stacktrace)
#include <stacktrace>
#define REACTX_STD_STACKTRACE 1
#else
#define REACTX_STD_STACKTRACE 0
#endif

// C++26 <debugging>: stop in the debugger on a violation (GCC 16+; not in
// every library yet — where it is missing, nothing happens).
#if defined(__cpp_lib_debugging)
#include <debugging>
#endif

namespace rx::detail {

[[noreturn]] inline void contractViolation(const char* condition, std::source_location where) {
    // fputs, not std::println: std::format would add ~100 KB to every
    // WebAssembly app for a message that is printed at most once.
    const std::string line = std::to_string(where.line());
    for (const char* part : {"ReactX: precondition failed: ", condition, "\n  at ", where.file_name(), ":",
                             line.c_str(), " in ", where.function_name(), "\n"})
        std::fputs(part, stderr);
#if REACTX_STD_STACKTRACE
    std::println(stderr, "{}", std::stacktrace::current(1));  // skip this function
#endif
    std::fflush(stderr);
#if defined(__cpp_lib_debugging)
    std::breakpoint_if_debugging();
#endif
    std::abort();
}

}  // namespace rx::detail

#if defined(__cpp_contracts)
#define REACTX_PRE(...) contract_assert(__VA_ARGS__)
#else
#define REACTX_PRE(...) \
    ((__VA_ARGS__) ? static_cast<void>(0) : ::rx::detail::contractViolation(#__VA_ARGS__, std::source_location::current()))
#endif
