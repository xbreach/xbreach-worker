#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "config.hpp"
#include "domain/leak_record.hpp"

namespace clickhouse {
class Client;
}

namespace xbreach::worker {

// Writes normalized records to ClickHouse. Abstract so the job processor can be
// tested against a fake.
class ILeakWriter {
  public:
    virtual ~ILeakWriter() = default;

    virtual void write_batch(const std::vector<LeakRecord>& records) = 0;

    // Removes any rows previously written for a job (used before re-processing).
    virtual void delete_job_rows(std::uint64_t job_id) = 0;
};

// clickhouse-cpp-backed writer using the native protocol.
class ClickHouseLeakWriter final : public ILeakWriter {
  public:
    explicit ClickHouseLeakWriter(const Config& config);
    ~ClickHouseLeakWriter() override;

    void write_batch(const std::vector<LeakRecord>& records) override;
    void delete_job_rows(std::uint64_t job_id) override;

    // Exposes the underlying client so migrations can run on the same connection.
    clickhouse::Client& client();

  private:
    std::unique_ptr<clickhouse::Client> client_;
};

} // namespace xbreach::worker
