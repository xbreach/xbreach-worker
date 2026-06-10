#include <gtest/gtest.h>

#include "crypto/hashing.hpp"
#include "parse/normalizer.hpp"

using xbreach::worker::hmac_sha256_hex;
using xbreach::worker::LeakRecord;
using xbreach::worker::normalize;
using xbreach::worker::NormalizeOptions;
using xbreach::worker::ParsedFields;
using xbreach::worker::RecordKind;
using xbreach::worker::sha256_hex;

TEST(Normalizer, EmailIdentifierLowercasedWithHmac) {
    ParsedFields fields{RecordKind::EmailPassword, "John.Doe@Example.COM", "secret", ""};
    const LeakRecord record =
        normalize(fields, "John.Doe@Example.COM:secret", {"key", true, false});
    EXPECT_EQ(record.identifier, "john.doe@example.com");
    EXPECT_EQ(record.email_hmac, hmac_sha256_hex("key", "john.doe@example.com"));
    EXPECT_EQ(record.password, "secret");
    EXPECT_EQ(record.password_sha256, sha256_hex("secret"));
    EXPECT_TRUE(record.raw_line.empty());
}

TEST(Normalizer, UsernameIdentifierHasNoEmailHmac) {
    ParsedFields fields{RecordKind::UserPassword, "admin", "pw", ""};
    const LeakRecord record = normalize(fields, "admin:pw", {"key", true, false});
    EXPECT_EQ(record.identifier, "admin");
    EXPECT_TRUE(record.email_hmac.empty());
    EXPECT_EQ(record.password_sha256, sha256_hex("pw"));
}

TEST(Normalizer, UlpExtractsUrlDomain) {
    ParsedFields fields{RecordKind::Ulp, "bob", "pw", "https://Login.Site.com:8080/path"};
    const LeakRecord record = normalize(fields, "n/a", {"key", true, false});
    EXPECT_EQ(record.url, "https://Login.Site.com:8080/path");
    EXPECT_EQ(record.url_domain, "login.site.com");
    EXPECT_EQ(record.identifier, "bob");
}

TEST(Normalizer, UlpWithEmailIdentityComputesHmac) {
    ParsedFields fields{RecordKind::Ulp, "user@mail.com", "pw", "https://x.com/"};
    const LeakRecord record = normalize(fields, "n/a", {"key", true, false});
    EXPECT_EQ(record.identifier, "user@mail.com");
    EXPECT_FALSE(record.email_hmac.empty());
    EXPECT_EQ(record.url_domain, "x.com");
}

TEST(Normalizer, RespectsStorageFlags) {
    ParsedFields fields{RecordKind::EmailPassword, "a@b.com", "pw", ""};

    const LeakRecord no_plaintext = normalize(fields, "a@b.com:pw", {"key", false, false});
    EXPECT_TRUE(no_plaintext.password.empty());
    EXPECT_EQ(no_plaintext.password_sha256, sha256_hex("pw"));

    const LeakRecord with_raw = normalize(fields, "a@b.com:pw", {"key", true, true});
    EXPECT_EQ(with_raw.raw_line, "a@b.com:pw");
}
