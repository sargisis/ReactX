# reactx::auth — password hashes, sessions, CSRF for servers (native).
add_library(reactx_auth STATIC src/crypto.cpp src/sessions.cpp)
add_library(reactx::auth ALIAS reactx_auth)
target_include_directories(reactx_auth PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(reactx_auth PUBLIC reactx reactx_net reactx_api)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(reactx_auth PRIVATE -Wall -Wextra -Wpedantic)
endif()
# The PBKDF2 loop runs 600,000 rounds per password: optimized even in Debug
# builds, or a sign-in on a dev server would take seconds.
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  set_source_files_properties(src/crypto.cpp PROPERTIES COMPILE_OPTIONS "-O2")
endif()
