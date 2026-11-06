# reactx::i18n — translations, plurals, numbers and dates.
add_library(reactx_i18n STATIC src/i18n.cpp src/locales.cpp src/load.cpp src/picker.cpp)
add_library(reactx::i18n ALIAS reactx_i18n)
target_include_directories(reactx_i18n PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(reactx_i18n PUBLIC reactx)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(reactx_i18n PRIVATE -Wall -Wextra -Wpedantic)
endif()
