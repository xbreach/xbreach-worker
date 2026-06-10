#pragma once

#include <string>
#include <string_view>

#include "domain/leak_record.hpp"
#include "parse/parser.hpp"

namespace xbreach::worker {

// Controls how parsed fields are turned into a stored record.
struct NormalizeOptions {
    std::string hmac_email_key;
    bool store_plaintext_password = true;
    bool store_raw_line = false;
};

// Turns parsed fields into a normalized LeakRecord: lowercases the email and
// derives its domain, extracts the URL host, computes password_sha256 and (when
// an email is present) the keyed email_hmac, and honors the storage flags. The
// id/job context fields are left for the caller to populate.
LeakRecord normalize(const ParsedFields& fields, std::string_view raw_line,
                     const NormalizeOptions& options);

} // namespace xbreach::worker
