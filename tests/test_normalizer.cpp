#include <gtest/gtest.h>

#include "parse/normalizer.hpp"

using xbreach::worker::LeakRecord;
using xbreach::worker::normalize;
using xbreach::worker::NormalizeOptions;
using xbreach::worker::ParsedFields;
using xbreach::worker::RecordKind;

TEST(Normalizer, EmailIdentifierLowercased) {
    ParsedFields fields{RecordKind::EmailPassword, "John.Doe@Example.COM", "secret", ""};
    const LeakRecord record = normalize(fields, "John.Doe@Example.COM:secret", {true});
    EXPECT_EQ(record.identifier, "john.doe@example.com");
    EXPECT_EQ(record.password, "secret");
}

TEST(Normalizer, UsernameIdentifierIsPreserved) {
    ParsedFields fields{RecordKind::UserPassword, "admin", "pw", ""};
    const LeakRecord record = normalize(fields, "admin:pw", {true});
    EXPECT_EQ(record.identifier, "admin");
    EXPECT_EQ(record.password, "pw");
}

TEST(Normalizer, UlpExtractsUrlDomain) {
    ParsedFields fields{RecordKind::Ulp, "bob", "pw", "https://Login.Site.com:8080/path"};
    const LeakRecord record = normalize(fields, "n/a", {true});
    EXPECT_EQ(record.url, "https://Login.Site.com:8080/path");
    EXPECT_EQ(record.url_domain, "login.site.com");
    EXPECT_EQ(record.identifier, "bob");
}

TEST(Normalizer, UlpWithEmailIdentityLowercasesIdentifier) {
    ParsedFields fields{RecordKind::Ulp, "user@mail.com", "pw", "https://x.com/"};
    const LeakRecord record = normalize(fields, "n/a", {true});
    EXPECT_EQ(record.identifier, "user@mail.com");
    EXPECT_EQ(record.url_domain, "x.com");
}

TEST(Normalizer, RespectsStorageFlags) {
    ParsedFields fields{RecordKind::EmailPassword, "a@b.com", "pw", ""};

    const LeakRecord no_plaintext = normalize(fields, "a@b.com:pw", {false});
    EXPECT_TRUE(no_plaintext.password.empty());
}
