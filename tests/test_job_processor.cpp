#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <vector>

#include "snowflake.hpp"
#include "worker/job_processor.hpp"

using namespace xbreach::worker;

namespace {

class FakeJobRepository : public IJobRepository {
  public:
    std::optional<IngestionJob> claim_pending_job() override { return std::nullopt; }
    void update_progress(std::uint64_t, const JobProgress& progress) override {
        last_progress = progress;
    }
    void mark_completed(std::uint64_t) override { completed = true; }
    void mark_failed(std::uint64_t, const std::string& message) override {
        failed = true;
        failure = message;
    }
    void record_errors(const std::vector<JobError>& items) override {
        errors.insert(errors.end(), items.begin(), items.end());
    }

    JobProgress last_progress;
    bool completed = false;
    bool failed = false;
    std::string failure;
    std::vector<JobError> errors;
};

class FakeLeakWriter : public ILeakWriter {
  public:
    void write_batch(const std::vector<LeakRecord>& items) override {
        records.insert(records.end(), items.begin(), items.end());
        ++batches;
    }
    void delete_job_rows(std::uint64_t) override { ++delete_calls; }

    std::vector<LeakRecord> records;
    int batches = 0;
    int delete_calls = 0;
};

ProcessorOptions test_options(const std::string& data_path) {
    ProcessorOptions options;
    options.data_path = data_path;
    options.batch_size = 1'000;
    options.progress_flush_every = 10'000;
    options.max_job_errors = 1'000;
    options.banner_scan_lines = 50;
    options.normalize = NormalizeOptions{"key", true, false};
    return options;
}

} // namespace

TEST(JobProcessor, ProcessesFileAndMarksCompleted) {
    namespace fs = std::filesystem;
    const fs::path data = fs::temp_directory_path() / "xbw_proc_test";
    fs::create_directories(data / "leak");
    {
        std::ofstream out(data / "leak" / "original.txt", std::ios::binary);
        out << "a@b.com:pw1\n"
            << "admin:s3cret\n"
            << "=====\n"
            << "bob:\n"
            << "\n";
    }

    FakeJobRepository repository;
    FakeLeakWriter writer;
    SnowflakeGenerator snowflake(2, 1);
    JobProcessor processor(repository, writer, snowflake, test_options(data.string()));

    IngestionJob job;
    job.id = 123;
    job.source_id = 7;
    job.local_path = "leak/original.txt";
    processor.process(job);
    fs::remove_all(data);

    EXPECT_TRUE(repository.completed);
    EXPECT_FALSE(repository.failed);
    EXPECT_EQ(repository.last_progress.total_lines, 5u);
    EXPECT_EQ(repository.last_progress.parsed_lines, 2u);
    EXPECT_EQ(repository.last_progress.rejected_lines, 1u);
    EXPECT_EQ(repository.last_progress.inserted_lines, 2u);
    EXPECT_GE(writer.delete_calls, 1);
    ASSERT_EQ(writer.records.size(), 2u);
    EXPECT_EQ(writer.records[0].job_id, 123u);
    EXPECT_EQ(writer.records[0].source_id, 7u);
    ASSERT_EQ(repository.errors.size(), 1u);
    EXPECT_EQ(repository.errors[0].line_number, 4u);
}

TEST(JobProcessor, MarksFailedWhenFileMissing) {
    FakeJobRepository repository;
    FakeLeakWriter writer;
    SnowflakeGenerator snowflake(2, 1);
    JobProcessor processor(repository, writer, snowflake,
                           test_options("definitely_missing_dir_xbw"));

    IngestionJob job;
    job.id = 9;
    job.local_path = "missing/original.txt";
    processor.process(job);

    EXPECT_TRUE(repository.failed);
    EXPECT_FALSE(repository.completed);
}
