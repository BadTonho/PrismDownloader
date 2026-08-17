#include "Ed25519Verifier.h"

#include <openssl/evp.h>

bool Ed25519Verifier::verify(const QByteArray &message, const QByteArray &signature,
                             const QByteArray &publicKey)
{
    if (signature.size() != 64 || publicKey.size() != 32) {
        return false;
    }

    EVP_PKEY *key = EVP_PKEY_new_raw_public_key(
        EVP_PKEY_ED25519, nullptr,
        reinterpret_cast<const unsigned char *>(publicKey.constData()),
        static_cast<size_t>(publicKey.size()));
    if (!key) {
        return false;
    }

    EVP_MD_CTX *context = EVP_MD_CTX_new();
    if (!context) {
        EVP_PKEY_free(key);
        return false;
    }

    const int initialized = EVP_DigestVerifyInit(context, nullptr, nullptr, nullptr, key);
    const int verified = initialized == 1
        ? EVP_DigestVerify(context,
                           reinterpret_cast<const unsigned char *>(signature.constData()),
                           static_cast<size_t>(signature.size()),
                           reinterpret_cast<const unsigned char *>(message.constData()),
                           static_cast<size_t>(message.size()))
        : 0;
    EVP_MD_CTX_free(context);
    EVP_PKEY_free(key);
    return verified == 1;
}
