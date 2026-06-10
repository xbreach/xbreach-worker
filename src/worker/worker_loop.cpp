#include "worker/worker_loop.hpp"

#include <chrono>
#include <optional>

namespace xbreach::worker {

WorkerLoop::WorkerLoop(IJobRepository& repository, IJobProcessor& processor,
                       double poll_interval_seconds)
    : repository_(repository), processor_(processor),
      poll_interval_seconds_(poll_interval_seconds) {}

bool WorkerLoop::run_once() {
    std::optional<IngestionJob> job = repository_.claim_pending_job();
    if (!job) {
        return false;
    }
    processor_.process(*job);
    return true;
}

void WorkerLoop::run() {
    while (!stop_requested_.load()) {
        if (run_once()) {
            continue;
        }
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait_for(lock, std::chrono::duration<double>(poll_interval_seconds_),
                     [this] { return stop_requested_.load(); });
    }
}

void WorkerLoop::request_stop() {
    stop_requested_.store(true);
    std::lock_guard<std::mutex> lock(mutex_);
    cv_.notify_all();
}

} // namespace xbreach::worker
