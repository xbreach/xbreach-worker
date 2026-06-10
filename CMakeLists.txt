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
find_package(ZLIB REQUIRED)
find_package(zstd CONFIG REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(libpqxx CONFIG REQUIRED)
# clickhouse-cpp does not ship a CMake config in vcpkg, so locate the static
# library directly and link its dependencies (abseil, cityhash, lz4) explicitly.
find_library(CLICKHOUSE_CPP_LIB NAMES clickhouse-cpp-lib REQUIRED)
find_package(absl CONFIG REQUIRED)
find_package(cityhash CONFIG REQUIRED)
find_package(lz4 CONFIG REQUIRED)

# Core library shared by the executable and the tests. Feature PRs add their
# translation units (config, snowflake, io, parse, crypto, db, worker) here.
add_library(xbreach_worker_lib
  src/version.cpp
  src/config.cpp
  src/snowflake.cpp
  src/io/file_reader.cpp
  src/parse/parser.cpp
  src/parse/normalizer.cpp
  src/crypto/hashing.cpp
  src/db/job_repository.cpp
  src/db/clickhouse_migrations.cpp
  src/db/clickhouse_writer.cpp
)
target_include_directories(xbreach_worker_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(xbreach_worker_lib
  PUBLIC
    spdlog::spdlog
    ZLIB::ZLIB
    zstd::libzstd
    OpenSSL::Crypto
    libpqxx::pqxx
    ${CLICKHOUSE_CPP_LIB}
    cityhash
    lz4::lz4
    absl::int128
)

add_executable(xbreach-worker src/main.cpp)
target_link_libraries(xbreach-worker PRIVATE xbreach_worker_lib)

if(XBREACH_WORKER_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
