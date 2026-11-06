# reactx::markdown — Markdown as elements: HTML in the browser, colors and indents in a terminal.
add_library(reactx_markdown STATIC src/parse.cpp src/view.cpp)
add_library(reactx::markdown ALIAS reactx_markdown)
target_include_directories(reactx_markdown PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(reactx_markdown PUBLIC reactx)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(reactx_markdown PRIVATE -Wall -Wextra -Wpedantic)
endif()
