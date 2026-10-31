# reactx_add_app(<target> SOURCES <files...> [PUBLIC <dir>] [BASE <path>] [SHELL <file>])
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

function(reactx_add_app target)
  cmake_parse_arguments(APP "" "PUBLIC;BASE;SHELL" "SOURCES;PRERENDER" ${ARGN})
  if(NOT APP_SOURCES)
    message(FATAL_ERROR "reactx_add_app(${target}): SOURCES is required")
  endif()
  add_executable(${target} ${APP_SOURCES})
  target_link_libraries(${target} PRIVATE reactx::reactx)
  if(NOT EMSCRIPTEN)
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

  set(steps COMMAND ${CMAKE_COMMAND} -E copy ${dist}/index.html ${dist}/404.html)
  if(APP_PUBLIC)
    get_filename_component(public ${APP_PUBLIC} ABSOLUTE BASE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
    list(APPEND steps COMMAND ${CMAKE_COMMAND} -E copy_directory ${public} ${dist})
  endif()
  add_custom_command(TARGET ${target} POST_BUILD ${steps} VERBATIM)

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
