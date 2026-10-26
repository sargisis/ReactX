cmake_minimum_required(VERSION 3.20...3.31)
# The ReactX version — the only place it is written (Semantic Versioning:
# MAJOR.MINOR.PATCH; release tags are vMAJOR.MINOR.PATCH, see CHANGELOG.md).
project(ReactX VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Inside another project (FetchContent / add_subdirectory, e.g. a project made
# by `reactx new`) only the libraries are built.
if(NOT DEFINED PROJECT_IS_TOP_LEVEL)  # CMake < 3.21
  string(COMPARE EQUAL "${CMAKE_SOURCE_DIR}" "${PROJECT_SOURCE_DIR}" PROJECT_IS_TOP_LEVEL)
endif()
option(REACTX_BUILD_EXAMPLES "Build the ReactX examples and the website" ${PROJECT_IS_TOP_LEVEL})
option(REACTX_BUILD_TESTS "Build the ReactX tests" ${PROJECT_IS_TOP_LEVEL})
option(REACTX_BUILD_TOOLS "Build reactx_serve, api_server and the reactx CLI" ${PROJECT_IS_TOP_LEVEL})
set(REACTX_DIR ${PROJECT_SOURCE_DIR} CACHE INTERNAL "ReactX source directory")

# reactx/version.hpp (REACTX_VERSION, rx::version()) for the CLI and for apps.
configure_file(${PROJECT_SOURCE_DIR}/cmake/version.hpp.in ${PROJECT_BINARY_DIR}/generated/reactx/version.hpp @ONLY)

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
# GCC (checked with 15.2) fails compiling modules with -fsanitize=undefined
# (internal compiler error) and -fsanitize=thread (TU-local entity errors).
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_FLAGS MATCHES "sanitize=[a-z,]*(undefined|thread)")
  set(REACTX_MODULES_DEFAULT OFF)
endif()
option(REACTX_MODULES "Build the reactx C++20 module (import reactx;)" ${REACTX_MODULES_DEFAULT})

if(EMSCRIPTEN)
  # ---- Web build (emcmake cmake ...): core + DOM backend ----
  add_library(reactx
    src/element.cpp
    src/hooks.cpp
    src/reconciler.cpp
    src/root.cpp
    src/text_renderer.cpp
    src/json.cpp
    src/router.cpp
    src/fetch.cpp
    src/dom.cpp
    src/ui_web.cpp
    src/ui_helpers.cpp
  )
  target_include_directories(reactx PUBLIC include ${PROJECT_BINARY_DIR}/generated)
  target_compile_options(reactx PRIVATE -Wall -Wextra -Wpedantic)
  # Native WebAssembly exceptions (all current browsers and Node support them);
  # PUBLIC, so every program that links reactx uses the same model.
  target_compile_options(reactx PUBLIC -fwasm-exceptions)
  target_link_options(reactx PUBLIC -fwasm-exceptions)
  add_library(reactx::reactx ALIAS reactx)
  include(${PROJECT_SOURCE_DIR}/cmake/ReactXApp.cmake)
  # Functions and helpers the page's JavaScript calls (see src/dom.cpp).
  target_link_options(reactx INTERFACE
    "SHELL:-sEXPORTED_FUNCTIONS=_main,_rx_dom_event"
    "SHELL:-sEXPORTED_RUNTIME_METHODS=ccall"
    "SHELL:-sDEFAULT_LIBRARY_FUNCS_TO_INCLUDE=$stringToNewUTF8"
    "SHELL:-sALLOW_MEMORY_GROWTH=1")
  message(STATUS "ReactX: web build (Emscripten) - DOM backend")
  if(NOT REACTX_BUILD_EXAMPLES AND NOT REACTX_BUILD_TESTS)
    return()
  endif()

  if(REACTX_BUILD_EXAMPLES)
  function(reactx_web_example name)
    add_executable(${name} examples/${name}.cpp)
    target_link_libraries(${name} PRIVATE reactx)
    set_target_properties(${name} PROPERTIES SUFFIX ".html")
    target_link_options(${name} PRIVATE "SHELL:--shell-file ${PROJECT_SOURCE_DIR}/web/shell.html")
    set_property(TARGET ${name} APPEND PROPERTY LINK_DEPENDS ${PROJECT_SOURCE_DIR}/web/shell.html)
  endfunction()
  reactx_web_example(web_counter)
  reactx_web_example(web_todo)
  reactx_web_example(web_ui_demo)

  # Pages with a router use web/app_shell.html; @REACTX_WEB_BASE@ becomes
  # <base href>: "/" for web_app, REACTX_WEB_BASE for the website.
  set(REACTX_WEB_BASE "/" CACHE STRING "Base path of the website (<base href>), e.g. /ReactX/ on GitHub Pages")
  set(REACTX_SITE_BASE ${REACTX_WEB_BASE})
  set(REACTX_WEB_BASE "/")
  configure_file(${PROJECT_SOURCE_DIR}/web/app_shell.html ${PROJECT_BINARY_DIR}/app_shell_root.html @ONLY)
  set(REACTX_WEB_BASE ${REACTX_SITE_BASE})
  configure_file(${PROJECT_SOURCE_DIR}/web/app_shell.html ${PROJECT_BINARY_DIR}/app_shell_site.html @ONLY)

  function(reactx_web_page target source outdir shell)
    add_executable(${target} ${source})
    target_link_libraries(${target} PRIVATE reactx)
    set_target_properties(${target} PROPERTIES OUTPUT_NAME index SUFFIX ".html"
                                               RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/${outdir})
    target_link_options(${target} PRIVATE "SHELL:--shell-file ${shell}")
    set_property(TARGET ${target} APPEND PROPERTY LINK_DEPENDS ${shell})
  endfunction()

  # Full-stack todo example: build/web/app/index.html, served with its REST API
  # by api_server (native build).
  reactx_web_page(web_app examples/web_app.cpp app ${PROJECT_BINARY_DIR}/app_shell_root.html)

  # The ReactX website (docs + demos). build/<preset>/site is everything for a
  # static host: the site is the start page and also 404.html, so direct links
  # like /ReactX/docs/hooks open it (GitHub Pages has no other fallback).
  reactx_web_page(website examples/site.cpp site_app ${PROJECT_BINARY_DIR}/app_shell_site.html)
  add_custom_target(site ALL
    COMMAND ${CMAKE_COMMAND} -E rm -rf ${PROJECT_BINARY_DIR}/site
    COMMAND ${CMAKE_COMMAND} -E make_directory ${PROJECT_BINARY_DIR}/site
    COMMAND ${CMAKE_COMMAND} -E copy ${PROJECT_BINARY_DIR}/site_app/index.html ${PROJECT_BINARY_DIR}/site_app/index.js
            ${PROJECT_BINARY_DIR}/site_app/index.wasm ${PROJECT_BINARY_DIR}/site/
    COMMAND ${CMAKE_COMMAND} -E copy ${PROJECT_BINARY_DIR}/site_app/index.html ${PROJECT_BINARY_DIR}/site/404.html
    COMMAND ${CMAKE_COMMAND} -E copy
            ${PROJECT_BINARY_DIR}/web_counter.html ${PROJECT_BINARY_DIR}/web_counter.js ${PROJECT_BINARY_DIR}/web_counter.wasm
            ${PROJECT_BINARY_DIR}/web_ui_demo.html ${PROJECT_BINARY_DIR}/web_ui_demo.js ${PROJECT_BINARY_DIR}/web_ui_demo.wasm
            ${PROJECT_BINARY_DIR}/site/
    COMMAND ${CMAKE_COMMAND} -E touch ${PROJECT_BINARY_DIR}/site/.nojekyll
    DEPENDS website web_counter web_ui_demo
    COMMENT "Assembling the website in ${PROJECT_BINARY_DIR}/site")
  # Start page with links to the demos (http://localhost:8090/).
  configure_file(${PROJECT_SOURCE_DIR}/web/index.html ${PROJECT_BINARY_DIR}/index.html COPYONLY)
  endif()  # REACTX_BUILD_EXAMPLES

  if(REACTX_BUILD_TESTS)
  # Tests run under Node with a tiny fake DOM (tests/web/fake_dom.js).
  enable_testing()
  add_executable(reactx_dom_tests tests/test_dom.cpp)
  target_link_libraries(reactx_dom_tests PRIVATE reactx)
  target_link_options(reactx_dom_tests PRIVATE
    "SHELL:--pre-js ${PROJECT_SOURCE_DIR}/tests/web/fake_dom.js" "SHELL:-sENVIRONMENT=node")
  set_property(TARGET reactx_dom_tests APPEND PROPERTY LINK_DEPENDS ${PROJECT_SOURCE_DIR}/tests/web/fake_dom.js)
  add_test(NAME reactx_dom_tests COMMAND ${CMAKE_CROSSCOMPILING_EMULATOR} $<TARGET_FILE:reactx_dom_tests>)

  add_executable(reactx_web_app_tests tests/test_web_app.cpp)
  target_link_libraries(reactx_web_app_tests PRIVATE reactx)
  target_link_options(reactx_web_app_tests PRIVATE
    "SHELL:--pre-js ${PROJECT_SOURCE_DIR}/tests/web/fake_dom.js" "SHELL:-sENVIRONMENT=node")
  set_property(TARGET reactx_web_app_tests APPEND PROPERTY LINK_DEPENDS ${PROJECT_SOURCE_DIR}/tests/web/fake_dom.js)
  add_test(NAME reactx_web_app_tests COMMAND ${CMAKE_CROSSCOMPILING_EMULATOR} $<TARGET_FILE:reactx_web_app_tests>)

  add_executable(reactx_web_base_tests tests/test_web_base.cpp)
  target_link_libraries(reactx_web_base_tests PRIVATE reactx)
  target_link_options(reactx_web_base_tests PRIVATE
    "SHELL:--pre-js ${PROJECT_SOURCE_DIR}/tests/web/fake_dom.js" "SHELL:-sENVIRONMENT=node")
  set_property(TARGET reactx_web_base_tests APPEND PROPERTY LINK_DEPENDS ${PROJECT_SOURCE_DIR}/tests/web/fake_dom.js)
  add_test(NAME reactx_web_base_tests COMMAND ${CMAKE_CROSSCOMPILING_EMULATOR} $<TARGET_FILE:reactx_web_base_tests>)

  add_executable(reactx_ui_web_tests tests/test_ui_web.cpp)
  target_link_libraries(reactx_ui_web_tests PRIVATE reactx)
  target_link_options(reactx_ui_web_tests PRIVATE
    "SHELL:--pre-js ${PROJECT_SOURCE_DIR}/tests/web/fake_dom.js" "SHELL:-sENVIRONMENT=node")
  set_property(TARGET reactx_ui_web_tests APPEND PROPERTY LINK_DEPENDS ${PROJECT_SOURCE_DIR}/tests/web/fake_dom.js)
  add_test(NAME reactx_ui_web_tests COMMAND ${CMAKE_CROSSCOMPILING_EMULATOR} $<TARGET_FILE:reactx_ui_web_tests>)
  endif()  # REACTX_BUILD_TESTS
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
  src/json.cpp
  src/router.cpp
  src/fetch.cpp
  src/terminal_renderer.cpp
  src/terminal.cpp
  src/ui_terminal.cpp
  src/ui_helpers.cpp
)
target_include_directories(reactx PUBLIC include ${PROJECT_BINARY_DIR}/generated)
target_link_libraries(reactx PRIVATE ${REACTX_YOGA_TARGET})
add_library(reactx::reactx ALIAS reactx)
include(${PROJECT_SOURCE_DIR}/cmake/ReactXApp.cmake)
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
target_include_directories(reactx_net PUBLIC include ${PROJECT_BINARY_DIR}/generated)
target_link_libraries(reactx_net PUBLIC Threads::Threads)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(reactx_net PRIVATE -Wall -Wextra -Wpedantic)
endif()
add_library(reactx::net ALIAS reactx_net)

if(REACTX_BUILD_TOOLS)
  add_executable(reactx_serve tools/serve.cpp)
  target_link_libraries(reactx_serve PRIVATE reactx_net)

  # The CLI: reactx new / dev / build / add (tools/reactx.cpp).
  add_executable(reactx_cli tools/reactx.cpp)
  target_link_libraries(reactx_cli PRIVATE reactx reactx_net)
  set_target_properties(reactx_cli PROPERTIES OUTPUT_NAME reactx)
  target_compile_definitions(reactx_cli PRIVATE REACTX_SOURCE_DIR="${PROJECT_SOURCE_DIR}")

  # Site + REST API in one process (examples/web_app.cpp is its front end).
  add_executable(api_server examples/api_server.cpp)
  target_link_libraries(api_server PRIVATE reactx reactx_net)

  # cmake --install <build> --prefix ~/.local  ->  ~/.local/bin/reactx
  install(TARGETS reactx_cli reactx_serve RUNTIME DESTINATION bin)
endif()

if(NOT REACTX_BUILD_EXAMPLES AND NOT REACTX_BUILD_TESTS)
  return()
endif()

function(reactx_example name)
  add_executable(${name} examples/${name}.cpp)
  target_link_libraries(${name} PRIVATE reactx)
  if(REACTX_MODULES)
    target_compile_definitions(${name} PRIVATE REACTX_USE_MODULES)
  endif()
endfunction()
if(REACTX_BUILD_EXAMPLES)
  reactx_example(counter)
  reactx_example(todo)
  reactx_example(todo_tui)
  reactx_example(ui_demo)
endif()

if(NOT REACTX_BUILD_TESTS)
  return()
endif()
enable_testing()
add_executable(reactx_tests tests/test_core.cpp)
target_link_libraries(reactx_tests PRIVATE reactx)
add_test(NAME reactx_tests COMMAND reactx_tests)

add_executable(reactx_terminal_tests tests/test_terminal.cpp)
target_link_libraries(reactx_terminal_tests PRIVATE reactx)
add_test(NAME reactx_terminal_tests COMMAND reactx_terminal_tests)

add_executable(reactx_router_tests tests/test_router.cpp)
target_link_libraries(reactx_router_tests PRIVATE reactx)
add_test(NAME reactx_router_tests COMMAND reactx_router_tests)

add_executable(reactx_json_tests tests/test_json.cpp)
target_link_libraries(reactx_json_tests PRIVATE reactx)
add_test(NAME reactx_json_tests COMMAND reactx_json_tests)

add_executable(reactx_todo_api_tests tests/test_todo_api.cpp)
target_link_libraries(reactx_todo_api_tests PRIVATE reactx reactx_net)
add_test(NAME reactx_todo_api_tests COMMAND reactx_todo_api_tests)

add_executable(reactx_http_tests tests/test_http.cpp)
target_link_libraries(reactx_http_tests PRIVATE reactx_net)
add_test(NAME reactx_http_tests COMMAND reactx_http_tests)

if(REACTX_BUILD_TOOLS)
  # The reactx CLI as a real program. The end-to-end run (new -> build -> dev)
  # compiles projects and downloads Yoga: label "slow" (ctest -LE slow skips it).
  add_executable(reactx_cli_tests tests/test_cli.cpp)
  target_link_libraries(reactx_cli_tests PRIVATE reactx)
  target_compile_definitions(reactx_cli_tests PRIVATE REACTX_CLI="$<TARGET_FILE:reactx_cli>"
                                                      REACTX_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
  add_dependencies(reactx_cli_tests reactx_cli)
  add_test(NAME reactx_cli_tests COMMAND reactx_cli_tests)
  add_test(NAME reactx_cli_e2e_tests COMMAND reactx_cli_tests --e2e)
  set_tests_properties(reactx_cli_e2e_tests PROPERTIES LABELS slow TIMEOUT 1500)
endif()

add_executable(reactx_ui_tests tests/test_ui.cpp)
target_link_libraries(reactx_ui_tests PRIVATE reactx)
add_test(NAME reactx_ui_tests COMMAND reactx_ui_tests)

if(REACTX_MODULES)
  add_executable(reactx_module_tests tests/test_module.cpp)
  target_link_libraries(reactx_module_tests PRIVATE reactx)
  add_test(NAME reactx_module_tests COMMAND reactx_module_tests)
endif()
