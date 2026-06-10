#include "db/clickhouse_migrations.hpp"

#include <clickhouse/client.h>

#include <algorithm>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace xbreach::worker {

namespace {

std::string trim(const std::string& s) {
    const auto is_space = [](unsigned char c) { return std::isspace(c) != 0; };
    std::size_t begin = 0;
    while (begin < s.size() && is_space(s[begin])) {
        ++begin;
    }
    std::size_t end = s.size();
    while (end > begin && is_space(s[end - 1])) {
        --end;
    }
    return s.substr(begin, end - begin);
}

std::string read_file(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        throw std::runtime_error("failed to read migration: " + path.string());
    }
    std::ostringstream buffer;
    buffer << stream.rdbuf();
    return buffer.str();
}

bool migration_applied(clickhouse::Client& client, const std::string& version) {
    bool applied = false;
    client.Select("SELECT count() FROM schema_migrations WHERE version = '" + version + "'",
                  [&](const clickhouse::Block& block) {
                      if (block.GetRowCount() == 0) {
                          return;
                      }
                      const auto count = block[0]->As<clickhouse::ColumnUInt64>()->At(0);
                      if (count > 0) {
                          applied = true;
                      }
                  });
    return applied;
}

} // namespace

std::vector<std::string> split_sql_statements(const std::string& sql) {
    std::vector<std::string> statements;
    std::string current;
    for (const char c : sql) {
        if (c == ';') {
            const std::string trimmed = trim(current);
            if (!trimmed.empty()) {
                statements.push_back(trimmed);
            }
            current.clear();
        } else {
            current += c;
        }
    }
    const std::string trimmed = trim(current);
    if (!trimmed.empty()) {
        statements.push_back(trimmed);
    }
    return statements;
}

void run_clickhouse_migrations(clickhouse::Client& client,
                               const std::filesystem::path& migrations_dir) {
    client.Execute("CREATE TABLE IF NOT EXISTS schema_migrations "
                   "(version String, applied_at DateTime64(3, 'UTC') DEFAULT now64(3)) "
                   "ENGINE = MergeTree ORDER BY version");

    std::vector<std::filesystem::path> files;
    for (const auto& entry : std::filesystem::directory_iterator(migrations_dir)) {
        if (entry.is_regular_file() && entry.path().extension() == ".sql") {
            files.push_back(entry.path());
        }
    }
    std::sort(files.begin(), files.end());

    for (const auto& file : files) {
        const std::string version = file.stem().string();
        if (migration_applied(client, version)) {
            continue;
        }
        for (const std::string& statement : split_sql_statements(read_file(file))) {
            client.Execute(statement);
        }
        client.Execute("INSERT INTO schema_migrations (version) VALUES ('" + version + "')");
    }
}

} // namespace xbreach::worker
