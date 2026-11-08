# reactx::jobs — background jobs for servers (native): a queue on worker threads, retries, timers and cron schedules.
add_library(reactx_jobs STATIC src/jobs.cpp)
add_library(reactx::jobs ALIAS reactx_jobs)
target_include_directories(reactx_jobs PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(reactx_jobs PUBLIC reactx)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(reactx_jobs PRIVATE -Wall -Wextra -Wpedantic)
endif()
