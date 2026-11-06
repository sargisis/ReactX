# reactx::config — settings from arguments, environment variables and a JSON file (native).
add_library(reactx_config STATIC src/config.cpp)
add_library(reactx::config ALIAS reactx_config)
target_include_directories(reactx_config PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(reactx_config PUBLIC reactx)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(reactx_config PRIVATE -Wall -Wextra -Wpedantic)
endif()
