#include "crypto/hashing.hpp"

#include <openssl/evp.h>
#include <openssl/hmac.h>

#include <array>
#include <stdexcept>

namespace xbreach::worker {

namespace {

std::string to_hex(const unsigned char* data, unsigned int length) {
    static constexpr char kHex[] = "0123456789abcdef";
    std::string out;
    out.resize(static_cast<std::size_t>(length) * 2);
    for (unsigned int i = 0; i < length; ++i) {
        out[2 * i] = kHex[data[i] >> 4];
        out[2 * i + 1] = kHex[data[i] & 0x0F];
    }
    return out;
}

} // namespace

std::string sha256_hex(std::string_view data) {
    std::array<unsigned char, EVP_MAX_MD_SIZE> digest{};
    unsigned int length = 0;
    if (EVP_Digest(data.data(), data.size(), digest.data(), &length, EVP_sha256(), nullptr) != 1) {
        throw std::runtime_error("sha256 computation failed");
    }
    return to_hex(digest.data(), length);
}

std::string hmac_sha256_hex(std::string_view key, std::string_view message) {
    std::array<unsigned char, EVP_MAX_MD_SIZE> digest{};
    unsigned int length = 0;
    const unsigned char* result = HMAC(EVP_sha256(), key.data(), static_cast<int>(key.size()),
                                       reinterpret_cast<const unsigned char*>(message.data()),
                                       message.size(), digest.data(), &length);
    if (result == nullptr) {
        throw std::runtime_error("hmac computation failed");
    }
    return to_hex(digest.data(), length);
}

} // namespace xbreach::worker
