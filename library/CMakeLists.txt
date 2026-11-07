# Libraries that come with ReactX (charts, i18n, query, markdown, icons, forms, log, motion, dnd, calendar; api, auth and config for servers). Link the ones an app uses:
#   target_link_libraries(my_app PRIVATE reactx::charts reactx::i18n)
# In an app's build only the linked ones are compiled (EXCLUDE_FROM_ALL in the
# root CMakeLists.txt), and an unused library adds nothing to a .wasm.
if(REACTX_BUILD_TESTS AND NOT EMSCRIPTEN)
  enable_testing()  # so ctest finds the tests in the library folders
endif()
add_subdirectory(charts)
add_subdirectory(i18n)
add_subdirectory(query)
add_subdirectory(markdown)
add_subdirectory(icons)
add_subdirectory(forms)
add_subdirectory(log)
add_subdirectory(motion)
add_subdirectory(dnd)
add_subdirectory(calendar)
# Server libraries: native only.
if(NOT EMSCRIPTEN)
  add_subdirectory(api)
  add_subdirectory(auth)
  add_subdirectory(config)
endif()
