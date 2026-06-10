#include "parse/normalizer.hpp"

#include <algorithm>
#include <cctype>

#include "crypto/hashing.hpp"

namespace xbreach::worker {

namespace {

std::string to_lower(std::string_view s) {
    std::string out(s);
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return out;
}

// Extracts the lowercased host from a URL, dropping scheme, port and path.
std::string extract_url_domain(std::string_view url) {
    const std::size_t scheme = url.find("://");
    std::string_view rest = (scheme == std::string_view::npos) ? url : url.substr(scheme + 3);
    const std::size_t end = rest.find_first_of("/:?");
    if (end != std::string_view::npos) {
        rest = rest.substr(0, end);
    }
    return to_lower(rest);
}

} // namespace

LeakRecord normalize(const ParsedFields& fields, std::string_view raw_line,
                     const NormalizeOptions& options) {
    LeakRecord record;
    record.kind = fields.kind;

    if (is_email(fields.identity)) {
        record.email = to_lower(fields.identity);
        const std::size_t at = record.email.find('@');
        if (at != std::string::npos) {
            record.email_domain = record.email.substr(at + 1);
        }
        record.email_hmac = hmac_sha256_hex(options.hmac_email_key, record.email);
    } else {
        record.username = fields.identity;
    }

    if (!fields.url.empty()) {
        record.url = fields.url;
        record.url_domain = extract_url_domain(fields.url);
    }

    record.password_sha256 = sha256_hex(fields.password);
    if (options.store_plaintext_password) {
        record.password = fields.password;
    }
    if (options.store_raw_line) {
        record.raw_line = std::string(raw_line);
    }

    return record;
}

} // namespace xbreach::worker
