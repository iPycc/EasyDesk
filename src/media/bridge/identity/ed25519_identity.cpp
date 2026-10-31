#include "easydesk_media_abi.h"

#include "common/media_error.h"

#include <openssl/curve25519.h>

EasyDeskMediaStatus easydesk_identity_ed25519_keypair_from_seed(
    const uint8_t seed[EASYDESK_IDENTITY_ED25519_SEED_SIZE],
    uint8_t public_key[EASYDESK_IDENTITY_ED25519_PUBLIC_KEY_SIZE],
    uint8_t private_key[EASYDESK_IDENTITY_ED25519_PRIVATE_KEY_SIZE],
    EasyDeskMediaError *error) {
    easydesk::media::setMediaError(error, EASYDESK_MEDIA_STATUS_OK, "");
    if (!seed || !public_key || !private_key) {
        easydesk::media::setMediaError(error, EASYDESK_MEDIA_STATUS_INVALID_ARGUMENT,
                                       "Ed25519 key buffers are required");
        return EASYDESK_MEDIA_STATUS_INVALID_ARGUMENT;
    }
    ED25519_keypair_from_seed(public_key, private_key, seed);
    return EASYDESK_MEDIA_STATUS_OK;
}

EasyDeskMediaStatus easydesk_identity_ed25519_sign(
    const uint8_t private_key[EASYDESK_IDENTITY_ED25519_PRIVATE_KEY_SIZE],
    const uint8_t *message,
    uint32_t message_size,
    uint8_t signature[EASYDESK_IDENTITY_ED25519_SIGNATURE_SIZE],
    EasyDeskMediaError *error) {
    easydesk::media::setMediaError(error, EASYDESK_MEDIA_STATUS_OK, "");
    if (!private_key || (!message && message_size != 0) || !signature) {
        easydesk::media::setMediaError(error, EASYDESK_MEDIA_STATUS_INVALID_ARGUMENT,
                                       "Ed25519 signing arguments are invalid");
        return EASYDESK_MEDIA_STATUS_INVALID_ARGUMENT;
    }
    if (ED25519_sign(signature, message, message_size, private_key) != 1) {
        easydesk::media::setMediaError(error, EASYDESK_MEDIA_STATUS_INTERNAL_ERROR,
                                       "Ed25519 signing failed");
        return EASYDESK_MEDIA_STATUS_INTERNAL_ERROR;
    }
    return EASYDESK_MEDIA_STATUS_OK;
}

EasyDeskMediaStatus easydesk_identity_ed25519_verify(
    const uint8_t public_key[EASYDESK_IDENTITY_ED25519_PUBLIC_KEY_SIZE],
    const uint8_t *message,
    uint32_t message_size,
    const uint8_t signature[EASYDESK_IDENTITY_ED25519_SIGNATURE_SIZE],
    EasyDeskMediaError *error) {
    easydesk::media::setMediaError(error, EASYDESK_MEDIA_STATUS_OK, "");
    if (!public_key || (!message && message_size != 0) || !signature) {
        easydesk::media::setMediaError(error, EASYDESK_MEDIA_STATUS_INVALID_ARGUMENT,
                                       "Ed25519 verification arguments are invalid");
        return EASYDESK_MEDIA_STATUS_INVALID_ARGUMENT;
    }
    if (ED25519_verify(message, message_size, signature, public_key) != 1) {
        easydesk::media::setMediaError(error, EASYDESK_MEDIA_STATUS_INVALID_ARGUMENT,
                                       "Ed25519 signature is invalid");
        return EASYDESK_MEDIA_STATUS_INVALID_ARGUMENT;
    }
    return EASYDESK_MEDIA_STATUS_OK;
}
