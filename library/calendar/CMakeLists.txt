# reactx::calendar — a month calendar and a date picker, in the language of reactx::i18n.
add_library(reactx_calendar STATIC src/calendar.cpp)
add_library(reactx::calendar ALIAS reactx_calendar)
target_include_directories(reactx_calendar PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(reactx_calendar PUBLIC reactx reactx_i18n)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(reactx_calendar PRIVATE -Wall -Wextra -Wpedantic)
endif()
