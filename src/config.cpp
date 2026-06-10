#include "config.hpp"

#include <algorithm>
#include <cstdlib>
#include <stdexcept>

namespace xbreach::worker {

namespace {

std::string to_lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

void apply_string(const EnvLookup& lookup, const char* key, std::string& target) {
    if (auto value = lookup(key)) {
        target = *value;
    }
}

void apply_int(const EnvLookup& lookup, const char* key, int& target) {
    if (auto value = lookup(key)) {
        try {
            target = std::stoi(*value);
        } catch (const std::exception&) {
            throw std::runtime_error(std::string("invalid integer for ") + key + ": " + *value);
        }
    }
}

void apply_size(const EnvLookup& lookup, const char* key, std::size_t& target) {
    if (auto value = lookup(key)) {
        try {
            const long long parsed = std::stoll(*value);
            if (parsed < 0) {
                throw std::out_of_range("negative");
            }
            target = static_cast<std::size_t>(parsed);
        } catch (const std::exception&) {
            throw std::runtime_error(std::string("invalid size for ") + key + ": " + *value);
        }
    }
}

void apply_double(const EnvLookup& lookup, const char* key, double& target) {
    if (auto value = lookup(key)) {
        try {
            target = std::stod(*value);
        } catch (const std::exception&) {
            throw std::runtime_error(std::string("invalid number for ") + key + ": " + *value);
        }
    }
}

void apply_bool(const EnvLookup& lookup, const char* key, bool& target) {
    if (auto value = lookup(key)) {
        const std::string normalized = to_lower(*value);
        if (normalized == "1" || normalized == "true" || normalized == "yes" ||
            normalized == "on") {
            target = true;
        } else if (normalized == "0" || normalized == "false" || normalized == "no" ||
                   normalized == "off") {
            target = false;
        } else {
            throw std::runtime_error(std::string("invalid boolean for ") + key + ": " + *value);
        }
    }
}

} // namespace

Config load_config(const EnvLookup& lookup) {
    Config config;

    apply_string(lookup, "XBREACH_POSTGRES_HOST", config.postgres_host);
    apply_int(lookup, "XBREACH_POSTGRES_PORT", config.postgres_port);
    apply_string(lookup, "XBREACH_POSTGRES_DB", config.postgres_db);
    apply_string(lookup, "XBREACH_POSTGRES_USER", config.postgres_user);
    apply_string(lookup, "XBREACH_POSTGRES_PASSWORD", config.postgres_password);

    apply_string(lookup, "XBREACH_DATA_PATH", config.data_path);

    apply_string(lookup, "XBREACH_CLICKHOUSE_HOST", config.clickhouse_host);
    apply_int(lookup, "XBREACH_CLICKHOUSE_PORT", config.clickhouse_port);
    apply_string(lookup, "XBREACH_CLICKHOUSE_DB", config.clickhouse_db);
    apply_string(lookup, "XBREACH_CLICKHOUSE_USER", config.clickhouse_user);
    apply_string(lookup, "XBREACH_CLICKHOUSE_PASSWORD", config.clickhouse_password);

    apply_int(lookup, "XBREACH_APP_ID", config.app_id);
    apply_int(lookup, "XBREACH_NODE_ID", config.node_id);

    apply_size(lookup, "XBREACH_CLICKHOUSE_BATCH_SIZE", config.clickhouse_batch_size);
    apply_size(lookup, "XBREACH_PROGRESS_FLUSH_EVERY", config.progress_flush_every);
    apply_size(lookup, "XBREACH_MAX_JOB_ERRORS", config.max_job_errors);
    apply_size(lookup, "XBREACH_BANNER_SCAN_LINES", config.banner_scan_lines);
    apply_bool(lookup, "XBREACH_STORE_RAW_LINE", config.store_raw_line);
    apply_bool(lookup, "XBREACH_STORE_PLAINTEXT_PASSWORD", config.store_plaintext_password);
    apply_double(lookup, "XBREACH_WORKER_POLL_INTERVAL_SECONDS",
                 config.worker_poll_interval_seconds);

    apply_string(lookup, "XBREACH_HMAC_EMAIL_KEY", config.hmac_email_key);
    apply_string(lookup, "XBREACH_LOG_LEVEL", config.log_level);

    return config;
}

Config load_config_from_env() {
    return load_config([](const char* key) -> std::optional<std::string> {
        if (const char* value = std::getenv(key)) {
            return std::string(value);
        }
        return std::nullopt;
    });
}

} // namespace xbreach::worker
