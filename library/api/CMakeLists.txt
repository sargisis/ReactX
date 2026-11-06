# reactx::api — a JSON API on rx::HttpServer (native: servers don't run in the browser).
add_library(reactx_api STATIC src/api.cpp)
add_library(reactx::api ALIAS reactx_api)
target_include_directories(reactx_api PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(reactx_api PUBLIC reactx reactx_net reactx_forms)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(reactx_api PRIVATE -Wall -Wextra -Wpedantic)
endif()
