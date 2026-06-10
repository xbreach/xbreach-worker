find_package(GTest CONFIG REQUIRED)

add_executable(xbreach_worker_tests
  test_smoke.cpp
  test_snowflake.cpp
  test_config.cpp
  test_file_reader.cpp
  test_parser.cpp
  test_hashing.cpp
  test_normalizer.cpp
  test_job_repository.cpp
  test_clickhouse_migrations.cpp
)
target_link_libraries(xbreach_worker_tests
  PRIVATE
    xbreach_worker_lib
    GTest::gtest
    GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(xbreach_worker_tests)
