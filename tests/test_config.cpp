#include <gtest/gtest.h>

#include <map>
#include <optional>
#include <stdexcept>
#include <string>

#include "config.hpp"

using xbreach::worker::Config;
using xbreach::worker::load_config;

namespace {

xbreach::worker::EnvLookup from_map(std::map<std::string, std::string> values) {
    return [values = std::move(values)](const char* key) -> std::optional<std::string> {
        const auto it = values.find(key);
        if (it == values.end()) {
            return std::nullopt;
        }
        return it->second;
    };
}

} // namespace

TEST(Config, UsesDefaultsWhenUnset) {
    const Config config = load_config(from_map({}));
    EXPECT_EQ(config.postgres_host, "postgres");
    EXPECT_EQ(config.clickhouse_port, 9000);
    EXPECT_EQ(config.app_id, 2);
    EXPECT_EQ(config.clickhouse_batch_size, 50'000u);
    EXPECT_TRUE(config.store_plaintext_password);
    EXPECT_FALSE(config.store_raw_line);
}

TEST(Config, AppliesOverrides) {
    const Config config = load_config(from_map({
        {"XBREACH_POSTGRES_HOST", "db.internal"},
        {"XBREACH_CLICKHOUSE_PORT", "9440"},
        {"XBREACH_APP_ID", "3"},
        {"XBREACH_CLICKHOUSE_BATCH_SIZE", "1000"},
        {"XBREACH_STORE_RAW_LINE", "true"},
        {"XBREACH_STORE_PLAINTEXT_PASSWORD", "false"},
        {"XBREACH_WORKER_POLL_INTERVAL_SECONDS", "0.5"},
        {"XBREACH_HMAC_EMAIL_KEY", "secret"},
    }));
    EXPECT_EQ(config.postgres_host, "db.internal");
    EXPECT_EQ(config.clickhouse_port, 9440);
    EXPECT_EQ(config.app_id, 3);
    EXPECT_EQ(config.clickhouse_batch_size, 1000u);
    EXPECT_TRUE(config.store_raw_line);
    EXPECT_FALSE(config.store_plaintext_password);
    EXPECT_DOUBLE_EQ(config.worker_poll_interval_seconds, 0.5);
    EXPECT_EQ(config.hmac_email_key, "secret");
}

TEST(Config, ParsesBooleanSynonyms) {
    EXPECT_TRUE(load_config(from_map({{"XBREACH_STORE_RAW_LINE", "ON"}})).store_raw_line);
    EXPECT_TRUE(load_config(from_map({{"XBREACH_STORE_RAW_LINE", "1"}})).store_raw_line);
    EXPECT_FALSE(load_config(from_map({{"XBREACH_STORE_PLAINTEXT_PASSWORD", "no"}}))
                     .store_plaintext_password);
}

TEST(Config, ThrowsOnInvalidValues) {
    EXPECT_THROW(load_config(from_map({{"XBREACH_CLICKHOUSE_PORT", "abc"}})), std::runtime_error);
    EXPECT_THROW(load_config(from_map({{"XBREACH_STORE_RAW_LINE", "maybe"}})), std::runtime_error);
    EXPECT_THROW(load_config(from_map({{"XBREACH_MAX_JOB_ERRORS", "-5"}})), std::runtime_error);
}
