#include "db/clickhouse_writer.hpp"

#include <clickhouse/client.h>

#include <string>

namespace xbreach::worker {

namespace {

clickhouse::ClientOptions make_options(const Config& config) {
    return clickhouse::ClientOptions()
        .SetHost(config.clickhouse_host)
        .SetPort(static_cast<unsigned>(config.clickhouse_port))
        .SetUser(config.clickhouse_user)
        .SetPassword(config.clickhouse_password)
        .SetDefaultDatabase(config.clickhouse_db)
        // The worker reuses one connection across a long-running job; reconnect
        // if the server dropped an idle connection, and retry transient sends so
        // a job does not fail with "closed: Resource temporarily unavailable".
        .SetPingBeforeQuery(true)
        .SetSendRetries(3);
}

} // namespace

ClickHouseLeakWriter::ClickHouseLeakWriter(const Config& config)
    : client_(std::make_unique<clickhouse::Client>(make_options(config))) {}

ClickHouseLeakWriter::~ClickHouseLeakWriter() = default;

clickhouse::Client& ClickHouseLeakWriter::client() {
    return *client_;
}

void ClickHouseLeakWriter::write_batch(const std::vector<LeakRecord>& records) {
    if (records.empty()) {
        return;
    }
    using namespace clickhouse;

    auto id = std::make_shared<ColumnUInt64>();
    auto job_id = std::make_shared<ColumnUInt64>();
    auto source_id = std::make_shared<ColumnUInt64>();
    auto breach_id = std::make_shared<ColumnUInt64>();
    auto line_number = std::make_shared<ColumnUInt64>();
    auto record_kind = std::make_shared<ColumnString>();
    auto identifier = std::make_shared<ColumnString>();
    auto password = std::make_shared<ColumnString>();
    auto password_sha256 = std::make_shared<ColumnString>();
    auto email_hmac = std::make_shared<ColumnString>();
    auto url = std::make_shared<ColumnString>();
    auto url_domain = std::make_shared<ColumnString>();
    auto raw_line = std::make_shared<ColumnString>();

    for (const LeakRecord& record : records) {
        id->Append(record.id);
        job_id->Append(record.job_id);
        source_id->Append(record.source_id);
        breach_id->Append(record.breach_id.value_or(0));
        line_number->Append(record.line_number);
        record_kind->Append(std::string(to_string(record.kind)));
        identifier->Append(record.identifier);
        password->Append(record.password);
        password_sha256->Append(record.password_sha256);
        email_hmac->Append(record.email_hmac);
        url->Append(record.url);
        url_domain->Append(record.url_domain);
        raw_line->Append(record.raw_line);
    }

    Block block;
    block.AppendColumn("id", id);
    block.AppendColumn("job_id", job_id);
    block.AppendColumn("source_id", source_id);
    block.AppendColumn("breach_id", breach_id);
    block.AppendColumn("line_number", line_number);
    block.AppendColumn("record_kind", record_kind);
    block.AppendColumn("identifier", identifier);
    block.AppendColumn("password", password);
    block.AppendColumn("password_sha256", password_sha256);
    block.AppendColumn("email_hmac", email_hmac);
    block.AppendColumn("url", url);
    block.AppendColumn("url_domain", url_domain);
    block.AppendColumn("raw_line", raw_line);

    client_->Insert("leak_records", block);
}

void ClickHouseLeakWriter::delete_job_rows(std::uint64_t job_id) {
    client_->Execute("ALTER TABLE leak_records DELETE WHERE job_id = " + std::to_string(job_id));
}

} // namespace xbreach::worker
