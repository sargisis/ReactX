# reactx::log — structured logs (text or JSON lines), thread-safe.
add_library(reactx_log STATIC src/log.cpp)
add_library(reactx::log ALIAS reactx_log)
target_include_directories(reactx_log PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(reactx_log PUBLIC reactx)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(reactx_log PRIVATE -Wall -Wextra -Wpedantic)
endif()
