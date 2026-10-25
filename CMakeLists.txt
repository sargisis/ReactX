cmake_minimum_required(VERSION 3.20...3.31)
project(ReactX VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# C++20 modules (`import reactx;`) need CMake 3.28+, the Ninja or Visual Studio
# generator (not Makefiles) and GCC 14+ / Clang 17+ / MSVC 17.4+.
# Otherwise the library still works through the headers.
set(REACTX_MODULES_DEFAULT OFF)
if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.28 AND
   CMAKE_GENERATOR MATCHES "Ninja|Visual Studio" AND
   ((CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 14) OR
    (CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 17) OR
    (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC" AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 19.34)))
  set(REACTX_MODULES_DEFAULT ON)
endif()
# GCC (checked with 15.2) crashes compiling modules with -fsanitize=undefined.
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_FLAGS MATCHES "sanitize=[a-z,]*undefined")
  set(REACTX_MODULES_DEFAULT OFF)
endif()
option(REACTX_MODULES "Build the reactx C++20 module (import reactx;)" ${REACTX_MODULES_DEFAULT})

add_library(reactx
  src/element.cpp
  src/hooks.cpp
  src/reconciler.cpp
  src/root.cpp
  src/text_renderer.cpp
  src/terminal_renderer.cpp
  src/terminal.cpp
)
target_include_directories(reactx PUBLIC include)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(reactx PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(REACTX_MODULES)
  target_sources(reactx PUBLIC FILE_SET CXX_MODULES FILES modules/reactx.cppm)
  target_compile_features(reactx PUBLIC cxx_std_20)
  message(STATUS "ReactX: C++20 module enabled (import reactx;)")
else()
  message(STATUS "ReactX: C++20 module disabled, using headers (needs CMake 3.28+ and -G Ninja)")
endif()

function(reactx_example name)
  add_executable(${name} examples/${name}.cpp)
  target_link_libraries(${name} PRIVATE reactx)
  if(REACTX_MODULES)
    target_compile_definitions(${name} PRIVATE REACTX_USE_MODULES)
  endif()
endfunction()
reactx_example(counter)
reactx_example(todo)
reactx_example(todo_tui)

enable_testing()
add_executable(reactx_tests tests/test_core.cpp)
target_link_libraries(reactx_tests PRIVATE reactx)
add_test(NAME reactx_tests COMMAND reactx_tests)

add_executable(reactx_terminal_tests tests/test_terminal.cpp)
target_link_libraries(reactx_terminal_tests PRIVATE reactx)
add_test(NAME reactx_terminal_tests COMMAND reactx_terminal_tests)

if(REACTX_MODULES)
  add_executable(reactx_module_tests tests/test_module.cpp)
  target_link_libraries(reactx_module_tests PRIVATE reactx)
  add_test(NAME reactx_module_tests COMMAND reactx_module_tests)
endif()
