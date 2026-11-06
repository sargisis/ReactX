# reactx::forms — validation rules and schemas, for forms (ui::useForm) and for JSON on a server.
add_library(reactx_forms STATIC src/forms.cpp)
add_library(reactx::forms ALIAS reactx_forms)
target_include_directories(reactx_forms PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(reactx_forms PUBLIC reactx)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(reactx_forms PRIVATE -Wall -Wextra -Wpedantic)
endif()
