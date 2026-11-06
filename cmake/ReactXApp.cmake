# reactx_add_app(<target> SOURCES <files...> [PUBLIC <dir>] [BASE <path>] [SHELL <file>])
#
# Natively, tests/*.cpp next to CMakeLists.txt become <target>_tests (with the
# SOURCES except main.cpp, and reactx::testing): `reactx test` builds and runs it.
#
# A ReactX program in one line. Native build: a normal executable (terminal
# apps). Web build (emcmake / the "web" preset): <build>/dist/index.html with
# .js and .wasm next to it, the PUBLIC folder (CSS, images) copied in, and
# 404.html = index.html so routes work on static hosts. BASE is the folder the
# site is served from ("/" by default, "/repo/" on GitHub Pages).
#
#   reactx_add_app(my_app SOURCES src/main.cpp PUBLIC public)
#
# PRERENDER <routes...> (or "web": {"prerender": ["/", "/docs/faq"]} in the
# project's reactx.json): after the build the same program runs under Node and
# mountDom writes each route's HTML into dist/index.html, dist/docs/faq.html, ...

# ccache for a target of the app, when ReactX found it (REACTX_CCACHE) and the
# project hasn't chosen a compiler launcher of its own.
function(_reactx_use_ccache target)
  get_target_property(launcher ${target} CXX_COMPILER_LAUNCHER)
  if(REACTX_CCACHE AND REACTX_CCACHE_PROGRAM AND NOT launcher)
    set_target_properties(${target} PROPERTIES CXX_COMPILER_LAUNCHER ${REACTX_CCACHE_PROGRAM}
                                               C_COMPILER_LAUNCHER ${REACTX_CCACHE_PROGRAM})
  endif()
endfunction()

function(reactx_add_app target)
  cmake_parse_arguments(APP "" "PUBLIC;BASE;SHELL" "SOURCES;PRERENDER" ${ARGN})
  if(NOT APP_SOURCES)
    message(FATAL_ERROR "reactx_add_app(${target}): SOURCES is required")
  endif()
  add_executable(${target} ${APP_SOURCES})
  target_link_libraries(${target} PRIVATE reactx::reactx)
  _reactx_use_ccache(${target})
  if(NOT EMSCRIPTEN)
    # Tests (`reactx test`): tests/*.cpp with the app's sources except main.cpp,
    # linked with reactx::testing (Screen, RX_TEST and a main). What the app
    # links later (libraries, include folders, defines) follows it.
    file(GLOB app_tests CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp)
    if(app_tests AND TARGET reactx::testing)
      set(tested ${APP_SOURCES})
      list(FILTER tested EXCLUDE REGEX "(^|/)main\\.cpp$")
      add_executable(${target}_tests EXCLUDE_FROM_ALL ${app_tests} ${tested})
      _reactx_use_ccache(${target}_tests)
      target_link_libraries(${target}_tests PRIVATE reactx::testing $<TARGET_PROPERTY:${target},LINK_LIBRARIES>)
      target_include_directories(${target}_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
                                 $<TARGET_PROPERTY:${target},INCLUDE_DIRECTORIES>)
      target_compile_definitions(${target}_tests PRIVATE $<TARGET_PROPERTY:${target},COMPILE_DEFINITIONS>)
      target_compile_options(${target}_tests PRIVATE $<TARGET_PROPERTY:${target},COMPILE_OPTIONS>)
    endif()
    # A web project's main() calls mountDom, which exists only in the browser
    # build: natively the app is built only when asked for (--target, or
    # `reactx build --native`), so tests and helper programs build on their own.
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/reactx.json)
      file(READ ${CMAKE_CURRENT_SOURCE_DIR}/reactx.json manifest)
      string(JSON kind ERROR_VARIABLE no_kind GET "${manifest}" template)
      if(NOT no_kind AND kind STREQUAL "web")
        set_target_properties(${target} PROPERTIES EXCLUDE_FROM_ALL TRUE)
      endif()
    endif()
    return()
  endif()

  if(NOT APP_BASE)
    set(APP_BASE "/")
  endif()
  if(NOT APP_SHELL)
    set(APP_SHELL ${REACTX_DIR}/web/project_shell.html)
  endif()
  set(REACTX_APP_BASE ${APP_BASE})
  set(REACTX_APP_TITLE ${target})
  set(shell ${CMAKE_CURRENT_BINARY_DIR}/${target}_shell.html)
  configure_file(${APP_SHELL} ${shell} @ONLY)

  set(dist ${CMAKE_BINARY_DIR}/dist)
  set_target_properties(${target} PROPERTIES OUTPUT_NAME index SUFFIX ".html" RUNTIME_OUTPUT_DIRECTORY ${dist})
  target_link_options(${target} PRIVATE "SHELL:--shell-file ${shell}")
  # What each source file costs in the .wasm; `reactx build` reads it to warn
  # about std::format and printed doubles (~100 KB each in the browser).
  target_link_options(${target} PRIVATE "-Wl,-Map=${CMAKE_BINARY_DIR}/${target}.link.map")
  set_property(TARGET ${target} APPEND PROPERTY LINK_DEPENDS ${shell})

  add_custom_command(TARGET ${target} POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy ${dist}/index.html ${dist}/404.html VERBATIM)
  if(APP_PUBLIC)
    # The PUBLIC folder is copied whenever one of its files changes, not only
    # when the program is linked again (a CSS edit alone reaches dist too).
    get_filename_component(public ${APP_PUBLIC} ABSOLUTE BASE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
    file(GLOB_RECURSE public_files CONFIGURE_DEPENDS ${public}/*)
    set(stamp ${CMAKE_CURRENT_BINARY_DIR}/${target}_public.stamp)
    add_custom_command(OUTPUT ${stamp}
      COMMAND ${CMAKE_COMMAND} -E copy_directory ${public} ${dist}
      COMMAND ${CMAKE_COMMAND} -E touch ${stamp}
      DEPENDS ${public_files} ${target}   # and after every link: dist may have been emptied
      COMMENT "Copying ${APP_PUBLIC} into dist"
      VERBATIM)
    add_custom_target(${target}_public ALL DEPENDS ${stamp})
  endif()

  # Routes to prerender: the argument, else reactx.json next to CMakeLists.txt.
  set(routes ${APP_PRERENDER})
  if(NOT routes AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/reactx.json)
    file(READ ${CMAKE_CURRENT_SOURCE_DIR}/reactx.json manifest)
    string(JSON count ERROR_VARIABLE no_routes LENGTH "${manifest}" web prerender)
    if(NOT no_routes AND count GREATER 0)
      math(EXPR last "${count} - 1")
      foreach(i RANGE ${last})
        string(JSON route GET "${manifest}" web prerender ${i})
        list(APPEND routes ${route})
      endforeach()
    endif()
  endif()
  if(routes)
    # The same sources, built for Node; what the app adds later
    # (target_link_libraries, include folders, defines) follows it.
    add_executable(${target}_prerender ${APP_SOURCES})
    _reactx_use_ccache(${target}_prerender)
    target_link_libraries(${target}_prerender PRIVATE reactx::reactx reactx::prerender
                          $<TARGET_PROPERTY:${target},LINK_LIBRARIES>)
    target_include_directories(${target}_prerender PRIVATE $<TARGET_PROPERTY:${target},INCLUDE_DIRECTORIES>)
    target_compile_definitions(${target}_prerender PRIVATE $<TARGET_PROPERTY:${target},COMPILE_DEFINITIONS>)
    target_compile_options(${target}_prerender PRIVATE $<TARGET_PROPERTY:${target},COMPILE_OPTIONS>)
    target_link_options(${target}_prerender PRIVATE "SHELL:-sENVIRONMENT=node" "SHELL:-sNODERAWFS=1")
    set_target_properties(${target}_prerender PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/prerender)
    # 404.html is the page as built (GitHub Pages' fallback for other routes);
    # 200.html is the same for hosts that rewrite unknown paths (surge.sh,
    # HttpServer's spaFallback, a Netlify/Vercel rewrite to /200.html), since
    # index.html now shows the home page. Fill copies of it.
    add_custom_target(${target}_prerendered ALL
      COMMAND ${CMAKE_COMMAND} -E copy ${dist}/404.html ${dist}/200.html
      COMMAND ${CMAKE_CROSSCOMPILING_EMULATOR} $<TARGET_FILE:${target}_prerender>
              --reactx-prerender ${dist}/404.html ${dist} ${routes}
      VERBATIM)
    add_dependencies(${target}_prerendered ${target} ${target}_prerender)
  endif()
endfunction()
