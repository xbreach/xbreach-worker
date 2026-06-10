#include "parse/parser.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <string>
#include <vector>

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

// Heuristic, allocation-free host/URL detector (this runs on every line). A
// token looks like a URL/host when it has a path ('/') or is a bare domain:
// labels of [A-Za-z0-9-] separated by dots with an alphabetic TLD (2-24 chars).
bool looks_like_host(std::string_view token) {
    if (token.empty() || token.find('@') != std::string_view::npos ||
        token.find(' ') != std::string_view::npos || token.find('\t') != std::string_view::npos) {
        return false;
    }
    if (token.find('/') != std::string_view::npos) {
        return true;
    }
    const std::size_t dot = token.rfind('.');
    if (dot == std::string_view::npos || dot == 0 || dot + 1 >= token.size()) {
        return false;
    }
    const std::string_view tld = token.substr(dot + 1);
    if (tld.size() < 2 || tld.size() > 24) {
        return false;
    }
    for (const char c : tld) {
        if (std::isalpha(static_cast<unsigned char>(c)) == 0) {
            return false;
        }
    }
    for (const char c : token.substr(0, dot)) {
        if (std::isalnum(static_cast<unsigned char>(c)) == 0 && c != '-' && c != '.') {
            return false;
        }
    }
    return true;
}

// A token continues a scheme-less URL when it is a path segment ('/') or a
// plausible port (all digits, <= 65535). Used to keep "host:port/path" together.
bool is_url_continuation(std::string_view token) {
    if (token.find('/') != std::string_view::npos) {
        return true;
    }
    if (token.empty() || token.size() > 5) {
        return false;
    }
    int value = 0;
    for (const char c : token) {
        if (std::isdigit(static_cast<unsigned char>(c)) == 0) {
            return false;
        }
        value = value * 10 + (c - '0');
    }
    return value <= 65535;
}

char pick_delimiter(std::string_view s) {
    for (const char candidate : {':', ';', '|', '\t'}) {
        if (s.find(candidate) != std::string_view::npos) {
            return candidate;
        }
    }
    return '\0';
}

std::vector<std::string_view> split(std::string_view s, char delim) {
    std::vector<std::string_view> tokens;
    std::size_t start = 0;
    for (std::size_t i = 0; i <= s.size(); ++i) {
        if (i == s.size() || s[i] == delim) {
            tokens.push_back(s.substr(start, i - start));
            start = i + 1;
        }
    }
    return tokens;
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

ParsedFields make_credential(std::string_view identity, std::string_view password) {
    ParsedFields fields;
    fields.kind = is_email(identity) ? RecordKind::EmailPassword : RecordKind::UserPassword;
    fields.identity = std::string(identity);
    fields.password = std::string(password);
    return fields;
}

ParsedFields make_ulp(std::string_view url, std::string_view identity, std::string_view password) {
    ParsedFields fields;
    fields.kind = RecordKind::Ulp;
    fields.url = std::string(url);
    fields.identity = std::string(identity);
    fields.password = std::string(password);
    return fields;
}

// Offset of a token (a view into `line`) from the start of the line.
std::size_t offset_in(std::string_view line, std::string_view token) {
    return static_cast<std::size_t>(token.data() - line.data());
}

// Classifies a scheme-less, already-tokenized line. Recognizes url:user:pass and
// user:pass:url (where the url is a bare host/domain) as ULP, otherwise treats
// the line as identity:password (the password keeps any remaining delimiters).
TryOutcome classify_tokens(std::string_view line, const std::vector<std::string_view>& tokens,
                           char delim) {
    const std::size_t n = tokens.size();
    if (n < 2) {
        return not_a_record();
    }
    if (n == 2) {
        if (tokens[0].empty()) {
            return malformed("empty identity");
        }
        if (tokens[1].empty()) {
            return malformed("empty password");
        }
        return ok(make_credential(tokens[0], tokens[1]));
    }

    // n >= 3: look for the URL/host at the front or the back.
    if (looks_like_host(tokens[0]) && !is_email(tokens[0])) {
        // The URL may span several tokens when it has a port and/or path
        // (host:port/path); extend over those before the credentials begin.
        std::size_t user_index = 1;
        while (user_index < n - 2 && is_url_continuation(tokens[user_index])) {
            ++user_index;
        }
        const std::string_view url = line.substr(0, offset_in(line, tokens[user_index]) - 1);
        const std::string_view user = tokens[user_index];
        const std::string_view password = line.substr(offset_in(line, tokens[user_index + 1]));
        if (user.empty() || password.empty()) {
            return malformed("ulp empty credentials");
        }
        return ok(make_ulp(url, user, password));
    }
    if (looks_like_host(tokens.back()) && !is_email(tokens.back())) {
        const std::string_view user = tokens[0];
        const std::size_t last_offset = offset_in(line, tokens[n - 1]);
        const std::size_t pass_offset = offset_in(line, tokens[1]);
        const std::string_view password = line.substr(pass_offset, (last_offset - 1) - pass_offset);
        if (user.empty() || password.empty()) {
            return malformed("ulp empty credentials");
        }
        return ok(make_ulp(tokens[n - 1], user, password));
    }

    if (tokens[0].empty()) {
        return malformed("empty identity");
    }
    const std::string_view password = line.substr(offset_in(line, tokens[1]));
    (void)delim;
    return ok(make_credential(tokens[0], password));
}

// Splits a url-first ULP ("<url>:<user>:<password>") from the right, since the
// URL may itself contain ':' (scheme, port).
TryOutcome parse_ulp_url_first(std::string_view line) {
    const std::size_t last = line.rfind(':');
    if (last == std::string_view::npos) {
        return malformed("ulp missing credentials");
    }
    const std::string_view rest = line.substr(0, last);
    const std::size_t prev = rest.rfind(':');
    if (prev == std::string_view::npos) {
        return malformed("ulp missing credentials");
    }
    const std::string_view url = rest.substr(0, prev);
    const std::string_view user = rest.substr(prev + 1);
    const std::string_view password = line.substr(last + 1);
    if (url.find("://") == std::string_view::npos && url.find('/') == std::string_view::npos) {
        return malformed("ulp malformed url");
    }
    if (user.empty()) {
        return malformed("ulp empty username");
    }
    if (password.empty()) {
        return malformed("ulp empty password");
    }
    return ok(make_ulp(url, user, password));
}

// Handles colon-delimited lines, accounting for URLs that embed ':' via a
// scheme. A scheme at the start means url:user:pass; a scheme later means
// user:pass:url; no scheme means a clean tokenization.
TryOutcome parse_colon_line(std::string_view line) {
    const std::size_t scheme = line.find("://");
    if (scheme != std::string_view::npos) {
        std::size_t name_start = scheme;
        while (name_start > 0 &&
               std::isalpha(static_cast<unsigned char>(line[name_start - 1])) != 0) {
            --name_start;
        }
        if (name_start == 0) {
            return parse_ulp_url_first(line);
        }
        const std::string_view url = line.substr(name_start);
        std::string_view creds = line.substr(0, name_start);
        while (!creds.empty() && creds.back() == ':') {
            creds.remove_suffix(1);
        }
        const std::size_t split_at = creds.find(':');
        if (split_at == std::string_view::npos) {
            return malformed("ulp missing credentials");
        }
        const std::string_view user = creds.substr(0, split_at);
        const std::string_view password = creds.substr(split_at + 1);
        if (user.empty() || password.empty()) {
            return malformed("ulp empty credentials");
        }
        return ok(make_ulp(url, user, password));
    }

    const std::vector<std::string_view> tokens = split(line, ':');
    return classify_tokens(line, tokens, ':');
}

// Some stealer exports label fields, e.g. "<url>:Password:...:<password>" (the
// real username is absent). A ":password:"/":username:" with a trailing colon
// marks a label, whereas a normal "user:password" ends with the password and
// has no trailing colon. When matched, take the URL before the label and the
// password from the last field; the identifier is unknown.
TryOutcome try_labeled_ulp(std::string_view line) {
    const std::string lowered = to_lower(line);
    std::size_t pos = lowered.find(":password:");
    if (pos == std::string::npos) {
        pos = lowered.find(":username:");
    }
    if (pos == std::string::npos) {
        return not_a_record();
    }
    const std::string_view url = line.substr(0, pos);
    if (url.find("://") == std::string_view::npos && url.find('/') == std::string_view::npos) {
        return not_a_record(); // not a URL: let normal parsing handle it
    }
    const std::string_view rest = line.substr(pos + 10); // skip ":password:"/":username:"
    const std::size_t last_colon = rest.rfind(':');
    const std::string_view password =
        trim((last_colon == std::string_view::npos) ? rest : rest.substr(last_colon + 1));
    if (password.empty()) {
        return malformed("labeled line empty password");
    }
    return ok(make_ulp(url, "", password));
}

TryOutcome try_parse_record(std::string_view line) {
    if (const TryOutcome labeled = try_labeled_ulp(line); labeled.result != TryResult::NotARecord) {
        return labeled;
    }

    const char delimiter = pick_delimiter(line);
    if (delimiter == ':') {
        return parse_colon_line(line);
    }
    if (delimiter != '\0') {
        const std::vector<std::string_view> tokens = split(line, delimiter);
        return classify_tokens(line, tokens, delimiter);
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
            return ok(make_credential(identity, password));
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
