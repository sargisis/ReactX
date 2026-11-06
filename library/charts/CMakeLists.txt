# reactx::charts — bar, line and pie charts: SVG in the browser, characters in a terminal.
add_library(reactx_charts STATIC src/charts.cpp)
add_library(reactx::charts ALIAS reactx_charts)
target_include_directories(reactx_charts PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(reactx_charts PUBLIC reactx)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(reactx_charts PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(REACTX_BUILD_TESTS AND NOT EMSCRIPTEN)
  add_executable(reactx_charts_tests tests/charts_tests.cpp)
  target_link_libraries(reactx_charts_tests PRIVATE reactx_charts reactx_testing)
  add_test(NAME reactx_charts_tests COMMAND reactx_charts_tests)
endif()
