# reactx::motion — presence animations (CSS in the browser) and tweened / spring values.
add_library(reactx_motion STATIC src/motion.cpp)
add_library(reactx::motion ALIAS reactx_motion)
target_include_directories(reactx_motion PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(reactx_motion PUBLIC reactx)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(reactx_motion PRIVATE -Wall -Wextra -Wpedantic)
endif()
