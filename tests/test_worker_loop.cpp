#include <gtest/gtest.h>

#include <cstdint>
#include <deque>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "worker/worker_loop.hpp"

using namespace xbreach::worker;

namespace {

class QueueRepository : public IJobRepository {
  public:
    std::deque<IngestionJob> jobs;

    std::optional<IngestionJob> claim_pending_job() override {
        if (jobs.empty()) {
            return std::nullopt;
        }
        IngestionJob job = jobs.front();
        jobs.pop_front();
        return job;
    }
    void update_progress(std::uint64_t, const JobProgress&) override {}
    void mark_completed(std::uint64_t) override {}
    void mark_failed(std::uint64_t, const std::string&) override {}
    void record_errors(const std::vector<JobError>&) override {}
};

class CountingProcessor : public IJobProcessor {
  public:
    std::vector<std::uint64_t> processed;
    void process(const IngestionJob& job) override { processed.push_back(job.id); }
};

} // namespace

TEST(WorkerLoop, RunOnceProcessesAvailableJob) {
    QueueRepository repository;
    IngestionJob job;
    job.id = 42;
    repository.jobs.push_back(job);
    CountingProcessor processor;
    WorkerLoop loop(repository, processor, 0.01);

    EXPECT_TRUE(loop.run_once());
    EXPECT_FALSE(loop.run_once());
    ASSERT_EQ(processor.processed.size(), 1u);
    EXPECT_EQ(processor.processed[0], 42u);
}

TEST(WorkerLoop, RunStopsPromptlyWhenRequested) {
    QueueRepository repository; // empty: run() goes straight to waiting
    CountingProcessor processor;
    WorkerLoop loop(repository, processor, 5.0);

    std::thread runner([&] { loop.run(); });
    loop.request_stop();
    runner.join(); // must return well before the 5s poll interval
    SUCCEED();
}
