#pragma once

#include <cstdint>
#include <mutex>

namespace xbreach::worker {

// Snowflake ID layout, identical to the ingest service so IDs are comparable
// across producers: [timestamp(41) | app_id(5) | node_id(5) | sequence(12)].
inline constexpr std::int64_t kSnowflakeEpochMs = 1'704'067'200'000LL; // 2024-01-01
inline constexpr int kAppIdBits = 5;
inline constexpr int kNodeIdBits = 5;
inline constexpr int kSequenceBits = 12;

inline constexpr int kMaxAppId = (1 << kAppIdBits) - 1;
inline constexpr int kMaxNodeId = (1 << kNodeIdBits) - 1;
inline constexpr int kMaxSequence = (1 << kSequenceBits) - 1;

inline constexpr int kNodeIdShift = kSequenceBits;
inline constexpr int kAppIdShift = kNodeIdShift + kNodeIdBits;
inline constexpr int kTimestampShift = kAppIdShift + kAppIdBits;

// Thread-safe generator of unique, roughly time-ordered 64-bit identifiers.
class SnowflakeGenerator {
  public:
    SnowflakeGenerator(int app_id, int node_id);

    std::uint64_t next_id();

    int app_id() const noexcept { return app_id_; }
    int node_id() const noexcept { return node_id_; }

  private:
    static std::int64_t current_millis();
    std::int64_t wait_next_millis(std::int64_t last_ms) const;

    int app_id_;
    int node_id_;
    std::mutex mutex_;
    std::int64_t last_timestamp_ms_ = -1;
    int sequence_ = 0;
};

std::uint64_t extract_app_id(std::uint64_t snowflake_id) noexcept;
std::uint64_t extract_node_id(std::uint64_t snowflake_id) noexcept;

} // namespace xbreach::worker
