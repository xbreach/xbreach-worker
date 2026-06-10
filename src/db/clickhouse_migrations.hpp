#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace clickhouse {
class Client;
}

namespace xbreach::worker {

// Splits a SQL script into individual, non-empty trimmed statements on ';'.
std::vector<std::string> split_sql_statements(const std::string& sql);

// Applies any not-yet-applied *.sql files from `migrations_dir` to ClickHouse,
// tracking applied versions in a `schema_migrations` table.
void run_clickhouse_migrations(clickhouse::Client& client,
                               const std::filesystem::path& migrations_dir);

} // namespace xbreach::worker
