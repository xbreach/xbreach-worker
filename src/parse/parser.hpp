#pragma once

#include <cstddef>
#include <string>
#include <string_view>

#include "domain/record_kind.hpp"

namespace xbreach::worker {

// How a raw line was classified.
enum class LineCategory {
    Record,   // a credential record was extracted
    Empty,    // blank line; ignore (neither parsed nor rejected)
    Banner,   // header/promo/separator noise; ignore
    Rejected, // looked like data but could not be parsed
};

// Raw fields extracted from a record line, before normalization.
struct ParsedFields {
    RecordKind kind = RecordKind::UserPassword;
    std::string identity; // email or username
    std::string password;
    std::string url; // populated for ULP records only
};

struct ParseOutcome {
    LineCategory category = LineCategory::Rejected;
    ParsedFields fields;       // valid when category == Record
    std::string reject_reason; // set when category == Rejected
};

// Classifies and parses a single raw line. `line_number` is 1-based; lines at
// or before `banner_scan_lines` are treated leniently (unrecognized lines are
// considered header/banner noise rather than rejected records).
ParseOutcome parse_line(std::string_view raw, std::size_t line_number,
                        std::size_t banner_scan_lines);

// Detects whether a token is a syntactically plausible email address.
bool is_email(std::string_view token);

} // namespace xbreach::worker
