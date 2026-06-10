#include "db/job_repository.hpp"

#include <pqxx/pqxx>

namespace xbreach::worker {

namespace {

std::string quote_value(std::string_view value) {
    std::string out = "'";
    for (const char c : value) {
        if (c == '\'' || c == '\\') {
            out += '\\';
        }
        out += c;
    }
    out += '\'';
    return out;
}

} // namespace

std::string build_pg_conninfo(const Config& config) {
    return "host=" + quote_value(config.postgres_host) +
           " port=" + std::to_string(config.postgres_port) +
           " dbname=" + quote_value(config.postgres_db) +
           " user=" + quote_value(config.postgres_user) +
           " password=" + quote_value(config.postgres_password);
}

PgJobRepository::PgJobRepository(const std::string& conninfo)
    : connection_(std::make_unique<pqxx::connection>(conninfo)) {}

PgJobRepository::~PgJobRepository() = default;

std::optional<IngestionJob> PgJobRepository::claim_pending_job() {
    pqxx::work txn{*connection_};
    const pqxx::result rows =
        txn.exec("SELECT id, source_id, breach_id, local_path FROM ingestion_jobs "
                 "WHERE status = 'pending' ORDER BY created_at ASC LIMIT 1 FOR UPDATE SKIP LOCKED");
    if (rows.empty()) {
        txn.commit();
        return std::nullopt;
    }

    const auto row = rows[0];
    IngestionJob job;
    job.id = row["id"].as<std::uint64_t>();
    job.source_id = row["source_id"].as<std::uint64_t>();
    if (!row["breach_id"].is_null()) {
        job.breach_id = row["breach_id"].as<std::uint64_t>();
    }
    job.local_path = row["local_path"].as<std::string>();

    txn.exec("UPDATE ingestion_jobs SET status = 'running', started_at = now(), "
             "error_message = NULL, updated_at = now() WHERE id = $1",
             pqxx::params{job.id});
    txn.commit();
    return job;
}

void PgJobRepository::update_progress(std::uint64_t job_id, const JobProgress& progress) {
    pqxx::work txn{*connection_};
    txn.exec("UPDATE ingestion_jobs SET total_lines = $2, parsed_lines = $3, "
             "rejected_lines = $4, inserted_lines = $5, updated_at = now() WHERE id = $1",
             pqxx::params{job_id, progress.total_lines, progress.parsed_lines,
                          progress.rejected_lines, progress.inserted_lines});
    txn.commit();
}

void PgJobRepository::mark_completed(std::uint64_t job_id) {
    pqxx::work txn{*connection_};
    txn.exec("UPDATE ingestion_jobs SET status = 'completed', finished_at = now(), "
             "error_message = NULL, updated_at = now() WHERE id = $1",
             pqxx::params{job_id});
    txn.commit();
}

void PgJobRepository::mark_failed(std::uint64_t job_id, const std::string& error_message) {
    pqxx::work txn{*connection_};
    txn.exec("UPDATE ingestion_jobs SET status = 'failed', finished_at = now(), "
             "error_message = $2, updated_at = now() WHERE id = $1",
             pqxx::params{job_id, error_message});
    txn.commit();
}

void PgJobRepository::record_errors(const std::vector<JobError>& errors) {
    if (errors.empty()) {
        return;
    }
    pqxx::work txn{*connection_};
    for (const JobError& error : errors) {
        txn.exec("INSERT INTO ingestion_job_errors "
                 "(id, job_id, line_number, error_type, error_message) "
                 "VALUES ($1, $2, $3, $4, $5)",
                 pqxx::params{error.id, error.job_id, error.line_number, error.error_type,
                              error.error_message});
    }
    txn.commit();
}

} // namespace xbreach::worker
