#include "snowflake.hpp"

#include <chrono>
#include <stdexcept>
#include <thread>

namespace xbreach::worker {

SnowflakeGenerator::SnowflakeGenerator(int app_id, int node_id)
    : app_id_(app_id), node_id_(node_id) {
    if (app_id < 0 || app_id > kMaxAppId) {
        throw std::out_of_range("app_id must be between 0 and 31");
    }
    if (node_id < 0 || node_id > kMaxNodeId) {
        throw std::out_of_range("node_id must be between 0 and 31");
    }
}

std::uint64_t SnowflakeGenerator::next_id() {
    std::lock_guard<std::mutex> lock(mutex_);

    std::int64_t timestamp_ms = current_millis();

    if (timestamp_ms < last_timestamp_ms_) {
        throw std::runtime_error("system clock moved backwards");
    }

    if (timestamp_ms == last_timestamp_ms_) {
        sequence_ = (sequence_ + 1) & kMaxSequence;
        if (sequence_ == 0) {
            timestamp_ms = wait_next_millis(timestamp_ms);
        }
    } else {
        sequence_ = 0;
    }

    last_timestamp_ms_ = timestamp_ms;

    const std::uint64_t timestamp_component =
        static_cast<std::uint64_t>(timestamp_ms - kSnowflakeEpochMs) << kTimestampShift;
    const std::uint64_t app_component = static_cast<std::uint64_t>(app_id_) << kAppIdShift;
    const std::uint64_t node_component = static_cast<std::uint64_t>(node_id_) << kNodeIdShift;

    return timestamp_component | app_component | node_component |
           static_cast<std::uint64_t>(sequence_);
}

std::int64_t SnowflakeGenerator::current_millis() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

std::int64_t SnowflakeGenerator::wait_next_millis(std::int64_t last_ms) const {
    std::int64_t now = current_millis();
    while (now <= last_ms) {
        std::this_thread::yield();
        now = current_millis();
    }
    return now;
}

std::uint64_t extract_app_id(std::uint64_t snowflake_id) noexcept {
    return (snowflake_id >> kAppIdShift) & static_cast<std::uint64_t>(kMaxAppId);
}

std::uint64_t extract_node_id(std::uint64_t snowflake_id) noexcept {
    return (snowflake_id >> kNodeIdShift) & static_cast<std::uint64_t>(kMaxNodeId);
}

} // namespace xbreach::worker
