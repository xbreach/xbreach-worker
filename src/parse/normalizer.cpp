#include "parse/normalizer.hpp"

#include <algorithm>
#include <cctype>

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
    (void)raw_line;

    LeakRecord record;
    record.kind = fields.kind;

    if (is_email(fields.identity)) {
        record.identifier = to_lower(fields.identity);
    } else {
        record.identifier = fields.identity;
    }

    if (!fields.url.empty()) {
        record.url = fields.url;
        record.url_domain = extract_url_domain(fields.url);
    }

    if (options.store_plaintext_password) {
        record.password = fields.password;
    }

    return record;
}

} // namespace xbreach::worker
