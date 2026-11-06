# reactx::icons — line icons: SVG in the browser, one-cell symbols in a terminal.
add_library(reactx_icons STATIC src/icons.cpp)
add_library(reactx::icons ALIAS reactx_icons)
target_include_directories(reactx_icons PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(reactx_icons PUBLIC reactx)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(reactx_icons PRIVATE -Wall -Wextra -Wpedantic)
endif()
