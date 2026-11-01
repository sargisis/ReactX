// main() for test programs that link reactx::testing and don't have their own.
// It lives in its own file of the static library, so the linker takes it only
// when nothing else defines main.
#include "reactx/testing.hpp"

int main(int argc, char** argv) { return rx::testing::runTests(argc, argv); }
