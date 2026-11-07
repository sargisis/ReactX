# reactx::dnd — drag and drop to reorder lists (mouse in both backends, keyboard everywhere).
add_library(reactx_dnd STATIC src/dnd.cpp)
add_library(reactx::dnd ALIAS reactx_dnd)
target_include_directories(reactx_dnd PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(reactx_dnd PUBLIC reactx)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(reactx_dnd PRIVATE -Wall -Wextra -Wpedantic)
endif()
