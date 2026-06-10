#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>

namespace xbreach::worker {

// Runtime configuration, loaded from `XBREACH_*` environment variables.
struct Config {
    // Postgres (job lifecycle).
    std::string postgres_host = "postgres";
    int postgres_port = 5432;
    std::string postgres_db = "xbreach";
    std::string postgres_user = "xbreach";
    std::string postgres_password = "xbreach";

    // Local storage where ingest wrote the raw files.
    std::string data_path = "/data/xbreach";

    // ClickHouse (native protocol).
    std::string clickhouse_host = "clickhouse";
    int clickhouse_port = 9000;
    std::string clickhouse_db = "xbreach";
    std::string clickhouse_user = "xbreach";
    std::string clickhouse_password = "xbreach";

    // Snowflake identity (app_id=2 distinguishes the worker from ingest=1).
    int app_id = 2;
    int node_id = 1;

    // Processing tuning.
    std::size_t clickhouse_batch_size = 50'000;
    std::size_t progress_flush_every = 10'000;
    std::size_t max_job_errors = 1'000;
    std::size_t banner_scan_lines = 50;
    bool store_raw_line = false;
    bool store_plaintext_password = true;
    double worker_poll_interval_seconds = 2.0;

    // HMAC key for email lookup hashing. Must be stable across the deployment.
    std::string hmac_email_key = "change-me-hmac-key-min-32-bytes-long-please";

    std::string log_level = "INFO";
};

// Looks up an environment variable; returns nullopt when unset.
using EnvLookup = std::function<std::optional<std::string>(const char*)>;

// Builds a Config using the given lookup (injectable for tests). Throws
// std::runtime_error if a present value cannot be parsed.
Config load_config(const EnvLookup& lookup);

// Builds a Config from the process environment via std::getenv.
Config load_config_from_env();

} // namespace xbreach::worker
