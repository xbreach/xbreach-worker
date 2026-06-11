#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "domain/record_kind.hpp"

namespace xbreach::worker {

// A single normalized leak record, mirroring the ClickHouse `leak_records`
// table. Identity/content fields are filled by the normalizer; id/job context
// and ingestion timestamp are filled by the job processor / writer.
struct LeakRecord {
    std::uint64_t id = 0;
    std::uint64_t job_id = 0;
    std::uint64_t source_id = 0;
    std::optional<std::uint64_t> breach_id;
    std::uint64_t line_number = 0;
    RecordKind kind = RecordKind::UserPassword;

    std::string identifier; // login identity: email or username
    std::string password;   // empty when plaintext storage is disabled
    std::string url;
    std::string url_domain;
};

} // namespace xbreach::worker
