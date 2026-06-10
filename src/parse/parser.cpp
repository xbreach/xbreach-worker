#include "parse/parser.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <optional>

namespace xbreach::worker {

namespace {

std::string_view trim(std::string_view s) {
    const auto is_space = [](char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; };
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

std::string to_lower(std::string_view s) {
    std::string out(s);
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return out;
}

bool has_alphanumeric(std::string_view s) {
    return std::any_of(s.begin(), s.end(), [](unsigned char c) { return std::isalnum(c) != 0; });
}

bool has_promo_keyword(std::string_view s) {
    static constexpr std::array<std::string_view, 9> kKeywords = {
        "t.me/",     "telegram", "discord.gg", "leaked by", "cracked by",
        "combolist", "made by",  "sold by",    "free combo"};
    const std::string lowered = to_lower(s);
    return std::any_of(kKeywords.begin(), kKeywords.end(), [&](std::string_view keyword) {
        return lowered.find(keyword) != std::string::npos;
    });
}

// A line is treated as ULP (url:user:password) when it has an explicit scheme
// or its first ':'-separated field looks like a host/path (contains '/'), which
// covers the very common scheme-less stealer-log form "host.com/path:user:pass".
bool looks_like_ulp(std::string_view s) {
    const std::size_t scheme = s.find("://");
    if (scheme != std::string_view::npos && scheme > 0 && scheme <= 10) {
        return true;
    }
    const std::size_t first_colon = s.find(':');
    const std::string_view head =
        (first_colon == std::string_view::npos) ? s : s.substr(0, first_colon);
    return head.find('/') != std::string_view::npos;
}

char pick_delimiter(std::string_view s) {
    for (const char candidate : {':', ';', '|', '\t'}) {
        if (s.find(candidate) != std::string_view::npos) {
            return candidate;
        }
    }
    return '\0';
}

enum class TryResult { Ok, NotARecord, Malformed };

struct TryOutcome {
    TryResult result = TryResult::NotARecord;
    ParsedFields fields;
    std::string reason;
};

TryOutcome not_a_record() {
    return {TryResult::NotARecord, {}, ""};
}

TryOutcome malformed(std::string reason) {
    return {TryResult::Malformed, {}, std::move(reason)};
}

TryOutcome ok(ParsedFields fields) {
    return {TryResult::Ok, std::move(fields), ""};
}

TryOutcome parse_ulp(std::string_view line) {
    // ULP is "<url>:<user>:<password>"; the URL itself contains ':' (scheme,
    // port), so split the two credentials off the right-hand side.
    const std::size_t last = line.rfind(':');
    if (last == std::string_view::npos) {
        return malformed("ulp missing credentials");
    }
    const std::string_view rest = line.substr(0, last);
    const std::size_t prev = rest.rfind(':');
    if (prev == std::string_view::npos) {
        return malformed("ulp missing credentials");
    }

    ParsedFields fields;
    fields.kind = RecordKind::Ulp;
    fields.url = std::string(rest.substr(0, prev));
    fields.identity = std::string(rest.substr(prev + 1));
    fields.password = std::string(line.substr(last + 1));

    // Accept either an explicit scheme or a host/path form (contains '/'); this
    // also guards against over-splitting a scheme-only line such as
    // "https://site.com:pass" whose url part would be just "https".
    if (fields.url.find("://") == std::string::npos && fields.url.find('/') == std::string::npos) {
        return malformed("ulp malformed url");
    }
    if (fields.identity.empty()) {
        return malformed("ulp empty username");
    }
    if (fields.password.empty()) {
        return malformed("ulp empty password");
    }
    return ok(std::move(fields));
}

TryOutcome try_parse_record(std::string_view line) {
    if (looks_like_ulp(line)) {
        return parse_ulp(line);
    }

    const char delimiter = pick_delimiter(line);
    if (delimiter != '\0') {
        // Identity (email/username) never contains the delimiter, so split on
        // the first occurrence and keep the remainder as the password.
        const std::size_t pos = line.find(delimiter);
        const std::string_view identity = line.substr(0, pos);
        const std::string_view password = line.substr(pos + 1);
        if (identity.empty()) {
            return malformed("empty identity");
        }
        if (password.empty()) {
            return malformed("empty password");
        }
        ParsedFields fields;
        fields.kind = is_email(identity) ? RecordKind::EmailPassword : RecordKind::UserPassword;
        fields.identity = std::string(identity);
        fields.password = std::string(password);
        return ok(std::move(fields));
    }

    // Space is only trusted as a separator when the first token is an email,
    // e.g. "user@host.com somepassword" — otherwise it is too ambiguous.
    const std::size_t space = line.find(' ');
    if (space != std::string_view::npos) {
        const std::string_view identity = line.substr(0, space);
        if (is_email(identity)) {
            const std::string_view password = trim(line.substr(space + 1));
            if (password.empty()) {
                return malformed("empty password");
            }
            ParsedFields fields;
            fields.kind = RecordKind::EmailPassword;
            fields.identity = std::string(identity);
            fields.password = std::string(password);
            return ok(std::move(fields));
        }
    }

    return not_a_record();
}

} // namespace

bool is_email(std::string_view token) {
    const std::size_t at = token.find('@');
    if (at == std::string_view::npos || at == 0 || at + 1 >= token.size()) {
        return false;
    }
    if (token.find('@', at + 1) != std::string_view::npos) {
        return false; // more than one '@'
    }
    const std::size_t dot = token.find('.', at + 1);
    if (dot == std::string_view::npos || dot + 1 >= token.size()) {
        return false; // need a dot after '@' that is not the last character
    }
    for (const char c : token) {
        if (c == ' ' || c == '\t' || c == ':' || c == ';' || c == '|') {
            return false;
        }
    }
    return true;
}

ParseOutcome parse_line(std::string_view raw, std::size_t line_number,
                        std::size_t banner_scan_lines) {
    const std::string_view line = trim(raw);
    if (line.empty()) {
        return {LineCategory::Empty, {}, ""};
    }
    if (!has_alphanumeric(line) || has_promo_keyword(line)) {
        return {LineCategory::Banner, {}, ""};
    }

    const TryOutcome outcome = try_parse_record(line);
    if (outcome.result == TryResult::Ok) {
        return {LineCategory::Record, outcome.fields, ""};
    }
    if (outcome.result == TryResult::Malformed) {
        return {LineCategory::Rejected, {}, outcome.reason};
    }

    // Unrecognized: lenient inside the banner window, otherwise a rejection.
    if (line_number <= banner_scan_lines) {
        return {LineCategory::Banner, {}, ""};
    }
    return {LineCategory::Rejected, {}, "unrecognized line format"};
}

} // namespace xbreach::worker
