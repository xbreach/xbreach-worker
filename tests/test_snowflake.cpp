#include <gtest/gtest.h>

#include <set>
#include <stdexcept>

#include "snowflake.hpp"

using xbreach::worker::extract_app_id;
using xbreach::worker::extract_node_id;
using xbreach::worker::SnowflakeGenerator;

TEST(Snowflake, RejectsOutOfRangeIds) {
    EXPECT_THROW(SnowflakeGenerator(32, 1), std::out_of_range);
    EXPECT_THROW(SnowflakeGenerator(1, 32), std::out_of_range);
    EXPECT_THROW(SnowflakeGenerator(-1, 1), std::out_of_range);
}

TEST(Snowflake, EncodesAppAndNodeId) {
    SnowflakeGenerator generator(2, 7);
    const std::uint64_t id = generator.next_id();
    EXPECT_EQ(extract_app_id(id), 2u);
    EXPECT_EQ(extract_node_id(id), 7u);
}

TEST(Snowflake, ProducesUniqueIncreasingIds) {
    SnowflakeGenerator generator(2, 1);
    std::set<std::uint64_t> seen;
    std::uint64_t previous = 0;
    for (int i = 0; i < 10'000; ++i) {
        const std::uint64_t id = generator.next_id();
        EXPECT_GT(id, previous);
        EXPECT_TRUE(seen.insert(id).second);
        previous = id;
    }
}
