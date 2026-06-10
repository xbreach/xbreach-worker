#include <gtest/gtest.h>

#include "db/clickhouse_migrations.hpp"

using xbreach::worker::split_sql_statements;

TEST(ClickHouseMigrations, SplitsAndTrimsStatements) {
    const auto statements =
        split_sql_statements("  CREATE TABLE a (x UInt64);\n\nINSERT INTO a;  ");
    ASSERT_EQ(statements.size(), 2u);
    EXPECT_EQ(statements[0], "CREATE TABLE a (x UInt64)");
    EXPECT_EQ(statements[1], "INSERT INTO a");
}

TEST(ClickHouseMigrations, IgnoresEmptyStatements) {
    EXPECT_TRUE(split_sql_statements("   ;;  \n ;").empty());
    EXPECT_TRUE(split_sql_statements("").empty());
}

TEST(ClickHouseMigrations, HandlesStatementWithoutTrailingSemicolon) {
    const auto statements = split_sql_statements("SELECT 1");
    ASSERT_EQ(statements.size(), 1u);
    EXPECT_EQ(statements[0], "SELECT 1");
}
