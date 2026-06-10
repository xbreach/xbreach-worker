#pragma once

#include <cstddef>
#include <string>

#include "db/clickhouse_writer.hpp"
#include "db/job_repository.hpp"
#include "domain/ingestion_job.hpp"
#include "parse/normalizer.hpp"
#include "snowflake.hpp"

namespace xbreach::worker {

// Abstract processor so the worker loop can be tested against a fake.
class IJobProcessor {
  public:
    virtual ~IJobProcessor() = default;
    virtual void process(const IngestionJob& job) = 0;
};

// Tunables for processing a single job.
struct ProcessorOptions {
    std::string data_path = "/data/xbreach";
    NormalizeOptions normalize;
    std::size_t batch_size = 50'000;
    std::size_t progress_flush_every = 10'000;
    std::size_t max_job_errors = 1'000;
    std::size_t banner_scan_lines = 50;
};

// Reads the raw file for a job, parses/normalizes each line, batch-writes the
// records to ClickHouse, records rejected lines and progress in Postgres, and
// marks the job completed (or failed on error).
class JobProcessor final : public IJobProcessor {
  public:
    JobProcessor(IJobRepository& repository, ILeakWriter& writer, SnowflakeGenerator& snowflake,
                 ProcessorOptions options);

    void process(const IngestionJob& job) override;

  private:
    IJobRepository& repository_;
    ILeakWriter& writer_;
    SnowflakeGenerator& snowflake_;
    ProcessorOptions options_;
};

} // namespace xbreach::worker
