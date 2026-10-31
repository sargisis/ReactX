# reactx_add_app(<target> SOURCES <files...> [PUBLIC <dir>] [BASE <path>] [SHELL <file>])
#
# A ReactX program in one line. Native build: a normal executable (terminal
# apps). Web build (emcmake / the "web" preset): <build>/dist/index.html with
# .js and .wasm next to it, the PUBLIC folder (CSS, images) copied in, and
# 404.html = index.html so routes work on static hosts. BASE is the folder the
# site is served from ("/" by default, "/repo/" on GitHub Pages).
#
#   reactx_add_app(my_app SOURCES src/main.cpp PUBLIC public)

function(reactx_add_app target)
  cmake_parse_arguments(APP "" "PUBLIC;BASE;SHELL" "SOURCES" ${ARGN})
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
endfunction()
