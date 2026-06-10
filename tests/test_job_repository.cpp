#include <gtest/gtest.h>

#include "config.hpp"
#include "db/job_repository.hpp"

using xbreach::worker::build_pg_conninfo;
using xbreach::worker::Config;

// PgJobRepository itself needs a live Postgres and is exercised by the
// end-to-end docker-compose verification; here we only test the pure conninfo
// builder.
TEST(JobRepository, BuildsQuotedConninfoFromDefaults) {
    Config config;
    EXPECT_EQ(build_pg_conninfo(config),
              "host='postgres' port=5432 dbname='xbreach' user='xbreach' password='xbreach'");
}

TEST(JobRepository, EscapesSpecialCharactersInValues) {
    Config config;
    config.postgres_password = "a'b\\c";
    const std::string conninfo = build_pg_conninfo(config);
    EXPECT_NE(conninfo.find("password='a\\'b\\\\c'"), std::string::npos);
}
