#pragma once

#include <atomic>
#include <condition_variable>
#include <mutex>

#include "db/job_repository.hpp"
#include "worker/job_processor.hpp"

namespace xbreach::worker {

// Polls Postgres for pending jobs and dispatches them to the processor, with a
// cooperative, signal-friendly shutdown.
class WorkerLoop {
  public:
    WorkerLoop(IJobRepository& repository, IJobProcessor& processor, double poll_interval_seconds);

    // Claims and processes a single job. Returns false when no job was available.
    bool run_once();

    // Runs until request_stop() is called, sleeping between empty polls.
    void run();

    // Asks run() to stop and wakes it from any pending sleep. Signal-safe enough
    // for typical use from a handler that only sets the flag and notifies.
    void request_stop();

  private:
    IJobRepository& repository_;
    IJobProcessor& processor_;
    double poll_interval_seconds_;
    std::atomic<bool> stop_requested_{false};
    std::mutex mutex_;
    std::condition_variable cv_;
};

} // namespace xbreach::worker
