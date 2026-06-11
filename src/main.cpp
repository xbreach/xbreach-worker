#include <spdlog/spdlog.h>

#include <csignal>
#include <cstdlib>
#include <exception>
#include <string>

#include "config.hpp"
#include "db/clickhouse_migrations.hpp"
#include "db/clickhouse_writer.hpp"
#include "db/job_repository.hpp"
#include "snowflake.hpp"
#include "version.hpp"
#include "worker/job_processor.hpp"
#include "worker/worker_loop.hpp"

namespace {

xbreach::worker::WorkerLoop* g_loop = nullptr;

void handle_signal(int /*signal*/) {
    if (g_loop != nullptr) {
        g_loop->request_stop();
    }
}

std::string migrations_path() {
    if (const char* value = std::getenv("XBREACH_CLICKHOUSE_MIGRATIONS_PATH")) {
        return value;
    }
    return "migrations_clickhouse";
}

} // namespace

int main() {
    using namespace xbreach::worker;

    try {
        const Config config = load_config_from_env();
        spdlog::info("xbreach-worker {} starting", version());

        PgJobRepository repository(build_pg_conninfo(config));
        ClickHouseLeakWriter writer(config);
        run_clickhouse_migrations(writer.client(), migrations_path());

        SnowflakeGenerator snowflake(config.app_id, config.node_id);

        ProcessorOptions options;
        options.data_path = config.data_path;
        options.normalize = NormalizeOptions{config.store_plaintext_password};
        options.batch_size = config.clickhouse_batch_size;
        options.progress_flush_every = config.progress_flush_every;
        options.max_job_errors = config.max_job_errors;
        options.banner_scan_lines = config.banner_scan_lines;

        JobProcessor processor(repository, writer, snowflake, options);
        WorkerLoop loop(repository, processor, config.worker_poll_interval_seconds);

        g_loop = &loop;
        std::signal(SIGINT, handle_signal);
        std::signal(SIGTERM, handle_signal);

        spdlog::info("worker ready; polling for jobs");
        loop.run();
        spdlog::info("worker stopped");
        return 0;
    } catch (const std::exception& error) {
        spdlog::critical("worker fatal error: {}", error.what());
        return 1;
    }
}
