#pragma once

#include <string>
#include <string_view>

namespace xbreach::worker {

// Lowercase hex SHA-256 digest of `data`.
std::string sha256_hex(std::string_view data);

// Lowercase hex HMAC-SHA256 of `message` keyed with `key`.
std::string hmac_sha256_hex(std::string_view key, std::string_view message);

} // namespace xbreach::worker
