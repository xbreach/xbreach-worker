#include <gtest/gtest.h>

#include "parse/parser.hpp"

using xbreach::worker::LineCategory;
using xbreach::worker::parse_line;
using xbreach::worker::RecordKind;

namespace {

constexpr std::size_t kScan = 50;

// Helper: parse a line well past the banner window so unrecognized lines are
// treated as rejections rather than header noise.
xbreach::worker::ParseOutcome parse_data(std::string_view line) {
    return parse_line(line, /*line_number=*/1'000, kScan);
}

} // namespace

TEST(Parser, EmailPassword) {
    const auto outcome = parse_data("john.doe@example.com:hunter2");
    ASSERT_EQ(outcome.category, LineCategory::Record);
    EXPECT_EQ(outcome.fields.kind, RecordKind::EmailPassword);
    EXPECT_EQ(outcome.fields.identity, "john.doe@example.com");
    EXPECT_EQ(outcome.fields.password, "hunter2");
}

TEST(Parser, UsernamePassword) {
    const auto outcome = parse_data("admin:s3cret");
    ASSERT_EQ(outcome.category, LineCategory::Record);
    EXPECT_EQ(outcome.fields.kind, RecordKind::UserPassword);
    EXPECT_EQ(outcome.fields.identity, "admin");
    EXPECT_EQ(outcome.fields.password, "s3cret");
}

TEST(Parser, PasswordKeepsEmbeddedColon) {
    const auto outcome = parse_data("user@mail.com:pa:ss:word");
    ASSERT_EQ(outcome.category, LineCategory::Record);
    EXPECT_EQ(outcome.fields.identity, "user@mail.com");
    EXPECT_EQ(outcome.fields.password, "pa:ss:word");
}

TEST(Parser, SemicolonAndPipeDelimiters) {
    const auto semicolon = parse_data("user@mail.com;pw1");
    ASSERT_EQ(semicolon.category, LineCategory::Record);
    EXPECT_EQ(semicolon.fields.password, "pw1");

    const auto pipe = parse_data("bob|pw2");
    ASSERT_EQ(pipe.category, LineCategory::Record);
    EXPECT_EQ(pipe.fields.kind, RecordKind::UserPassword);
    EXPECT_EQ(pipe.fields.password, "pw2");
}

TEST(Parser, UlpUrlUserPassword) {
    const auto outcome = parse_data("https://site.com/login:user@mail.com:secret");
    ASSERT_EQ(outcome.category, LineCategory::Record);
    EXPECT_EQ(outcome.fields.kind, RecordKind::Ulp);
    EXPECT_EQ(outcome.fields.url, "https://site.com/login");
    EXPECT_EQ(outcome.fields.identity, "user@mail.com");
    EXPECT_EQ(outcome.fields.password, "secret");
}

TEST(Parser, UlpWithPortKeepsUrl) {
    const auto outcome = parse_data("https://site.com:8080/path:bob:pw");
    ASSERT_EQ(outcome.category, LineCategory::Record);
    EXPECT_EQ(outcome.fields.kind, RecordKind::Ulp);
    EXPECT_EQ(outcome.fields.url, "https://site.com:8080/path");
    EXPECT_EQ(outcome.fields.identity, "bob");
    EXPECT_EQ(outcome.fields.password, "pw");
}

TEST(Parser, SchemelessUlp) {
    const auto outcome = parse_data("facebook.com/dyi:bob:secret");
    ASSERT_EQ(outcome.category, LineCategory::Record);
    EXPECT_EQ(outcome.fields.kind, RecordKind::Ulp);
    EXPECT_EQ(outcome.fields.url, "facebook.com/dyi");
    EXPECT_EQ(outcome.fields.identity, "bob");
    EXPECT_EQ(outcome.fields.password, "secret");
}

TEST(Parser, SchemelessUlpWithEmailUser) {
    const auto outcome = parse_data("intermatia.com/datos_usuario.php:user@mail.com:pw");
    ASSERT_EQ(outcome.category, LineCategory::Record);
    EXPECT_EQ(outcome.fields.kind, RecordKind::Ulp);
    EXPECT_EQ(outcome.fields.url, "intermatia.com/datos_usuario.php");
    EXPECT_EQ(outcome.fields.identity, "user@mail.com");
    EXPECT_EQ(outcome.fields.password, "pw");
}

TEST(Parser, UrlWithoutCredentialsIsRejected) {
    const auto outcome = parse_data("https://site.com");
    EXPECT_EQ(outcome.category, LineCategory::Rejected);
}

TEST(Parser, SpaceSeparatedOnlyWhenEmail) {
    const auto good = parse_data("user@mail.com mypassword");
    ASSERT_EQ(good.category, LineCategory::Record);
    EXPECT_EQ(good.fields.identity, "user@mail.com");
    EXPECT_EQ(good.fields.password, "mypassword");

    // Two plain words separated by a space are not a credential pair.
    const auto bad = parse_data("hello world");
    EXPECT_EQ(bad.category, LineCategory::Rejected);
}

TEST(Parser, EmptyLineIsEmpty) {
    EXPECT_EQ(parse_data("   ").category, LineCategory::Empty);
    EXPECT_EQ(parse_data("").category, LineCategory::Empty);
}

TEST(Parser, SeparatorArtIsBanner) {
    EXPECT_EQ(parse_data("==================").category, LineCategory::Banner);
    EXPECT_EQ(parse_data("----- * -----").category, LineCategory::Banner);
}

TEST(Parser, PromoLineIsBanner) {
    EXPECT_EQ(parse_data("Join our channel t.me/leakchannel").category, LineCategory::Banner);
    EXPECT_EQ(parse_data("Database leaked by SomeCrew").category, LineCategory::Banner);
}

TEST(Parser, HeaderTextInWindowIsBannerButRejectedOutside) {
    // A non-record line within the scan window is tolerated as header noise.
    EXPECT_EQ(parse_line("MY AWESOME DATABASE DUMP", 3, kScan).category, LineCategory::Banner);
    // The same shape deep in the file is a rejected record.
    EXPECT_EQ(parse_line("MY AWESOME DATABASE DUMP", 5'000, kScan).category,
              LineCategory::Rejected);
}

TEST(Parser, EmptyPasswordIsRejected) {
    EXPECT_EQ(parse_data("user@mail.com:").category, LineCategory::Rejected);
    EXPECT_EQ(parse_data("admin:").category, LineCategory::Rejected);
}
