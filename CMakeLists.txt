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
# Web build: headers for now (the module also exports the terminal API).
if(EMSCRIPTEN)
  set(REACTX_MODULES_DEFAULT OFF)
endif()
# GCC (checked with 15.2) crashes compiling modules with -fsanitize=undefined.
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_FLAGS MATCHES "sanitize=[a-z,]*undefined")
  set(REACTX_MODULES_DEFAULT OFF)
endif()
option(REACTX_MODULES "Build the reactx C++20 module (import reactx;)" ${REACTX_MODULES_DEFAULT})

if(EMSCRIPTEN)
  # ---- Web build (emcmake cmake ...): core + DOM backend ----
  # Native WebAssembly exceptions (all current browsers and Node support them).
  add_compile_options(-fwasm-exceptions)
  add_link_options(-fwasm-exceptions)
  add_library(reactx
    src/element.cpp
    src/hooks.cpp
    src/reconciler.cpp
    src/root.cpp
    src/text_renderer.cpp
    src/dom.cpp
  )
  target_include_directories(reactx PUBLIC include)
  target_compile_options(reactx PRIVATE -Wall -Wextra -Wpedantic)
  # Functions and helpers the page's JavaScript calls (see src/dom.cpp).
  target_link_options(reactx INTERFACE
    "SHELL:-sEXPORTED_FUNCTIONS=_main,_rx_dom_event"
    "SHELL:-sEXPORTED_RUNTIME_METHODS=ccall"
    "SHELL:-sALLOW_MEMORY_GROWTH=1")
  message(STATUS "ReactX: web build (Emscripten) - DOM backend")

  function(reactx_web_example name)
    add_executable(${name} examples/${name}.cpp)
    target_link_libraries(${name} PRIVATE reactx)
    set_target_properties(${name} PROPERTIES SUFFIX ".html")
    target_link_options(${name} PRIVATE "SHELL:--shell-file ${CMAKE_SOURCE_DIR}/web/shell.html")
    set_property(TARGET ${name} APPEND PROPERTY LINK_DEPENDS ${CMAKE_SOURCE_DIR}/web/shell.html)
  endfunction()
  reactx_web_example(web_counter)
  reactx_web_example(web_todo)
  # Start page with links to the demos (http://localhost:8090/).
  configure_file(${CMAKE_SOURCE_DIR}/web/index.html ${CMAKE_BINARY_DIR}/index.html COPYONLY)

  # Tests run under Node with a tiny fake DOM (tests/web/fake_dom.js).
  enable_testing()
  add_executable(reactx_dom_tests tests/test_dom.cpp)
  target_link_libraries(reactx_dom_tests PRIVATE reactx)
  target_link_options(reactx_dom_tests PRIVATE
    "SHELL:--pre-js ${CMAKE_SOURCE_DIR}/tests/web/fake_dom.js" "SHELL:-sENVIRONMENT=node")
  set_property(TARGET reactx_dom_tests APPEND PROPERTY LINK_DEPENDS ${CMAKE_SOURCE_DIR}/tests/web/fake_dom.js)
  add_test(NAME reactx_dom_tests COMMAND ${CMAKE_CROSSCOMPILING_EMULATOR} $<TARGET_FILE:reactx_dom_tests>)
  return()
endif()

# ---- Yoga (Flexbox layout for the terminal backend) ----
# Uses an installed Yoga if CMake can find it, otherwise downloads v3.2.1.
# Offline: -DFETCHCONTENT_SOURCE_DIR_YOGA=/path/to/yoga
find_package(yoga CONFIG QUIET)
if(TARGET yoga::yogacore)
  set(REACTX_YOGA_TARGET yoga::yogacore)
  message(STATUS "ReactX: using installed Yoga")
else()
  include(FetchContent)
  FetchContent_Declare(yoga
    URL https://github.com/facebook/yoga/archive/refs/tags/v3.2.1.tar.gz
    URL_HASH SHA256=86b399ac31fd820d8ffa823c3fae31bb690b6fc45301b2a8a966c09b5a088b55
    SOURCE_SUBDIR yoga  # only the library, not Yoga's own tests
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
  )
  FetchContent_MakeAvailable(yoga)
  set(REACTX_YOGA_TARGET yogacore)
  message(STATUS "ReactX: using Yoga v3.2.1 (FetchContent)")
endif()

add_library(reactx
  src/element.cpp
  src/hooks.cpp
  src/reconciler.cpp
  src/root.cpp
  src/text_renderer.cpp
  src/terminal_renderer.cpp
  src/terminal.cpp
  src/ui.cpp
)
target_include_directories(reactx PUBLIC include)
target_link_libraries(reactx PRIVATE ${REACTX_YOGA_TARGET})
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

# ---- HTTP server (development server for the web build, API routes) ----
find_package(Threads REQUIRED)
add_library(reactx_net src/http.cpp)
target_include_directories(reactx_net PUBLIC include)
target_link_libraries(reactx_net PUBLIC Threads::Threads)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(reactx_net PRIVATE -Wall -Wextra -Wpedantic)
endif()
add_executable(reactx_serve tools/serve.cpp)
target_link_libraries(reactx_serve PRIVATE reactx_net)

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
reactx_example(ui_demo)

enable_testing()
add_executable(reactx_tests tests/test_core.cpp)
target_link_libraries(reactx_tests PRIVATE reactx)
add_test(NAME reactx_tests COMMAND reactx_tests)

add_executable(reactx_terminal_tests tests/test_terminal.cpp)
target_link_libraries(reactx_terminal_tests PRIVATE reactx)
add_test(NAME reactx_terminal_tests COMMAND reactx_terminal_tests)

add_executable(reactx_http_tests tests/test_http.cpp)
target_link_libraries(reactx_http_tests PRIVATE reactx_net)
add_test(NAME reactx_http_tests COMMAND reactx_http_tests)

add_executable(reactx_ui_tests tests/test_ui.cpp)
target_link_libraries(reactx_ui_tests PRIVATE reactx)
add_test(NAME reactx_ui_tests COMMAND reactx_ui_tests)

if(REACTX_MODULES)
  add_executable(reactx_module_tests tests/test_module.cpp)
  target_link_libraries(reactx_module_tests PRIVATE reactx)
  add_test(NAME reactx_module_tests COMMAND reactx_module_tests)
endif()
