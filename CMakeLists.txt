cmake_minimum_required(VERSION 3.21)
project(xbreach-worker VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(XBREACH_WORKER_BUILD_TESTS "Build unit tests" ON)

find_package(spdlog CONFIG REQUIRED)

# Core library shared by the executable and the tests. Feature PRs add their
# translation units (config, snowflake, io, parse, crypto, db, worker) here.
add_library(xbreach_worker_lib
  src/version.cpp
)
target_include_directories(xbreach_worker_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(xbreach_worker_lib PUBLIC spdlog::spdlog)

add_executable(xbreach-worker src/main.cpp)
target_link_libraries(xbreach-worker PRIVATE xbreach_worker_lib)

if(XBREACH_WORKER_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
