#include "security/ed25519.hpp"

#include <openssl/evp.h>
#include <openssl/sha.h>

#include <array>
#include <iomanip>
#include <memory>
#include <sstream>

namespace easydesk::security {
namespace {

struct KeyDeleter {
    void operator()(EVP_PKEY* key) const { EVP_PKEY_free(key); }
};

struct ContextDeleter {
    void operator()(EVP_MD_CTX* context) const { EVP_MD_CTX_free(context); }
};

}  // namespace

std::string sha256Fingerprint(std::span<const std::uint8_t> publicKey) {
    std::array<unsigned char, SHA256_DIGEST_LENGTH> digest{};
    SHA256(publicKey.data(), publicKey.size(), digest.data());

    std::ostringstream output;
    output << std::hex << std::setfill('0');
    for (const unsigned char value : digest) {
        output << std::setw(2) << static_cast<unsigned int>(value);
    }
    return output.str();
}

bool verifyEd25519(std::span<const std::uint8_t> publicKey,
                   std::span<const std::uint8_t> message,
                   std::span<const std::uint8_t> signature) {
    if (publicKey.size() != 32 || signature.size() != 64) {
        return false;
    }

    const std::unique_ptr<EVP_PKEY, KeyDeleter> key(EVP_PKEY_new_raw_public_key(
        EVP_PKEY_ED25519, nullptr, publicKey.data(), publicKey.size()));
    if (!key) {
        return false;
    }

    const std::unique_ptr<EVP_MD_CTX, ContextDeleter> context(EVP_MD_CTX_new());
    if (!context || EVP_DigestVerifyInit(context.get(), nullptr, nullptr, nullptr, key.get()) != 1) {
        return false;
    }

    return EVP_DigestVerify(context.get(), signature.data(), signature.size(), message.data(),
                            message.size()) == 1;
}

}  // namespace easydesk::security
