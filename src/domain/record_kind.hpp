#pragma once

#include <string_view>

namespace xbreach::worker {

// Shape of a parsed credential record. The string form matches the ClickHouse
// `record_kind` column values.
enum class RecordKind { EmailPassword, UserPassword, Ulp };

inline std::string_view to_string(RecordKind kind) {
    switch (kind) {
    case RecordKind::EmailPassword:
        return "email_password";
    case RecordKind::UserPassword:
        return "user_password";
    case RecordKind::Ulp:
        return "ulp";
    }
    return "unknown";
}

} // namespace xbreach::worker
