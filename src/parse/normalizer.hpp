#pragma once

#include <string>
#include <string_view>

#include "domain/leak_record.hpp"
#include "parse/parser.hpp"

namespace xbreach::worker {

// Controls how parsed fields are turned into a stored record.
struct NormalizeOptions {
    bool store_plaintext_password = true;
};

// Turns parsed fields into a normalized LeakRecord: lowercases email identifiers,
// extracts the URL host, and honors the plaintext password storage flag. The
// id/job context fields are left for the caller to populate.
LeakRecord normalize(const ParsedFields& fields, std::string_view raw_line,
                     const NormalizeOptions& options);

} // namespace xbreach::worker
