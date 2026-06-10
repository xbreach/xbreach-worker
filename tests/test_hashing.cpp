#include <gtest/gtest.h>

#include "crypto/hashing.hpp"

using xbreach::worker::hmac_sha256_hex;
using xbreach::worker::sha256_hex;

TEST(Hashing, Sha256KnownVectors) {
    EXPECT_EQ(sha256_hex(""), "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    EXPECT_EQ(sha256_hex("abc"),
              "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
}

TEST(Hashing, HmacSha256Rfc4231Case2) {
    // RFC 4231, test case 2.
    EXPECT_EQ(hmac_sha256_hex("Jefe", "what do ya want for nothing?"),
              "5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843");
}

TEST(Hashing, HmacIsStableAndKeyDependent) {
    EXPECT_EQ(hmac_sha256_hex("key", "message"), hmac_sha256_hex("key", "message"));
    EXPECT_NE(hmac_sha256_hex("key1", "message"), hmac_sha256_hex("key2", "message"));
}
