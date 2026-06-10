#include "worker/job_processor.hpp"

#include <spdlog/spdlog.h>

#include <exception>
#include <filesystem>
#include <string>
#include <vector>

#include "domain/leak_record.hpp"
#include "io/file_reader.hpp"
#include "parse/parser.hpp"

namespace xbreach::worker {

JobProcessor::JobProcessor(IJobRepository& repository, ILeakWriter& writer,
                           SnowflakeGenerator& snowflake, ProcessorOptions options)
    : repository_(repository), writer_(writer), snowflake_(snowflake),
      options_(std::move(options)) {}

void JobProcessor::process(const IngestionJob& job) {
    try {
        // Make a re-run idempotent: drop any rows from a previous attempt.
        writer_.delete_job_rows(job.id);

        const std::filesystem::path path =
            std::filesystem::path(options_.data_path) / job.local_path;
        auto reader = open_line_reader(path);

        JobProgress progress;
        std::vector<LeakRecord> batch;
        std::vector<JobError> errors;

        const auto flush_batch = [&]() {
            if (batch.empty()) {
                return;
            }
            writer_.write_batch(batch);
            progress.inserted_lines += batch.size();
            batch.clear();
            repository_.update_progress(job.id, progress);
        };

        std::string line;
        while (reader->next_line(line)) {
            ++progress.total_lines;
            const std::uint64_t line_number = progress.total_lines;

            const ParseOutcome outcome = parse_line(line, line_number, options_.banner_scan_lines);
            switch (outcome.category) {
            case LineCategory::Record: {
                LeakRecord record = normalize(outcome.fields, line, options_.normalize);
                record.id = snowflake_.next_id();
                record.job_id = job.id;
                record.source_id = job.source_id;
                record.breach_id = job.breach_id;
                record.line_number = line_number;
                batch.push_back(std::move(record));
                ++progress.parsed_lines;
                if (batch.size() >= options_.batch_size) {
                    flush_batch();
                }
                break;
            }
            case LineCategory::Rejected:
                ++progress.rejected_lines;
                if (errors.size() < options_.max_job_errors) {
                    errors.push_back(JobError{snowflake_.next_id(), job.id, line_number,
                                              "parse_error", outcome.reject_reason});
                }
                break;
            case LineCategory::Empty:
            case LineCategory::Banner:
                break;
            }

            if (progress.total_lines % options_.progress_flush_every == 0) {
                repository_.update_progress(job.id, progress);
            }
        }

        flush_batch();
        if (!errors.empty()) {
            repository_.record_errors(errors);
        }
        repository_.update_progress(job.id, progress);
        repository_.mark_completed(job.id);
        spdlog::info("job {} completed: total={} parsed={} rejected={} inserted={}", job.id,
                     progress.total_lines, progress.parsed_lines, progress.rejected_lines,
                     progress.inserted_lines);
    } catch (const std::exception& error) {
        spdlog::error("job {} failed: {}", job.id, error.what());
        repository_.mark_failed(job.id, error.what());
    }
}

} // namespace xbreach::worker
