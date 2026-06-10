#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace xbreach::worker {

// A claimed ingestion job, with just the fields the worker needs to process it.
struct IngestionJob {
    std::uint64_t id = 0;
    std::uint64_t source_id = 0;
    std::optional<std::uint64_t> breach_id;
    std::string local_path; // relative to the configured data_path
};

// Progress counters periodically flushed back to Postgres.
struct JobProgress {
    std::uint64_t total_lines = 0;
    std::uint64_t parsed_lines = 0;
    std::uint64_t rejected_lines = 0;
    std::uint64_t inserted_lines = 0;
};

// A per-line ingestion error to persist into ingestion_job_errors.
struct JobError {
    std::uint64_t id = 0; // snowflake, assigned by the caller
    std::uint64_t job_id = 0;
    std::optional<std::uint64_t> line_number;
    std::string error_type;
    std::string error_message;
};

} // namespace xbreach::worker
