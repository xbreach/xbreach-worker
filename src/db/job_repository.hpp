#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "config.hpp"
#include "domain/ingestion_job.hpp"

namespace pqxx {
class connection;
}

namespace xbreach::worker {

// Drives the Postgres job lifecycle. Abstract so the job processor can be tested
// against a fake.
class IJobRepository {
  public:
    virtual ~IJobRepository() = default;

    // Atomically claims the oldest pending job (FOR UPDATE SKIP LOCKED) and marks
    // it running. Returns nullopt when there is nothing to do.
    virtual std::optional<IngestionJob> claim_pending_job() = 0;

    virtual void update_progress(std::uint64_t job_id, const JobProgress& progress) = 0;
    virtual void mark_completed(std::uint64_t job_id) = 0;
    virtual void mark_failed(std::uint64_t job_id, const std::string& error_message) = 0;
    virtual void record_errors(const std::vector<JobError>& errors) = 0;
};

// Builds a libpq connection string from configuration (values are quoted).
std::string build_pg_conninfo(const Config& config);

// libpqxx-backed implementation talking to the ingest Postgres schema directly.
class PgJobRepository final : public IJobRepository {
  public:
    explicit PgJobRepository(const std::string& conninfo);
    ~PgJobRepository() override;

    std::optional<IngestionJob> claim_pending_job() override;
    void update_progress(std::uint64_t job_id, const JobProgress& progress) override;
    void mark_completed(std::uint64_t job_id) override;
    void mark_failed(std::uint64_t job_id, const std::string& error_message) override;
    void record_errors(const std::vector<JobError>& errors) override;

  private:
    std::unique_ptr<pqxx::connection> connection_;
};

} // namespace xbreach::worker
