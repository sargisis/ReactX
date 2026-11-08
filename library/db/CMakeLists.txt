# reactx::db — an embedded store of JSON documents for servers (native): one file, no other libraries.
add_library(reactx_db STATIC src/db.cpp)
add_library(reactx::db ALIAS reactx_db)
target_include_directories(reactx_db PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(reactx_db PUBLIC reactx)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(reactx_db PRIVATE -Wall -Wextra -Wpedantic)
endif()
