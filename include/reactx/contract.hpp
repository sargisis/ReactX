#pragma once

// Preconditions, ready for C++26 contracts.
//
//   REACTX_PRE(width > 0);
//
// With a compiler that implements contracts (__cpp_contracts) this is
// contract_assert; until then a failed check prints what failed and where,
// then stops the program — like a contract violation with the "enforce"
// semantic. Use it for programming errors (a bug in the caller), not for
// things that can fail at run time (bad input, network): those return
// std::expected.

#include <cstdio>
#include <cstdlib>
#include <source_location>

namespace rx::detail {

[[noreturn]] inline void contractViolation(const char* condition, std::source_location where) {
    std::fprintf(stderr, "ReactX: precondition failed: %s\n  at %s:%u in %s\n", condition, where.file_name(),
                 static_cast<unsigned>(where.line()), where.function_name());
    std::fflush(stderr);
    std::abort();
}

}  // namespace rx::detail

#if defined(__cpp_contracts)
#define REACTX_PRE(...) contract_assert(__VA_ARGS__)
#else
#define REACTX_PRE(...) \
    ((__VA_ARGS__) ? static_cast<void>(0) : ::rx::detail::contractViolation(#__VA_ARGS__, std::source_location::current()))
#endif
