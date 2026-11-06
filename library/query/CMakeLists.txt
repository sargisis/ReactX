# reactx::query — server data with a shared cache (like React Query / SWR).
add_library(reactx_query STATIC src/query.cpp)
add_library(reactx::query ALIAS reactx_query)
target_include_directories(reactx_query PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(reactx_query PUBLIC reactx)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(reactx_query PRIVATE -Wall -Wextra -Wpedantic)
endif()
