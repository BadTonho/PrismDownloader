#ifndef ED25519VERIFIER_H
#define ED25519VERIFIER_H

#include <QByteArray>

// Verifies a detached RFC 8032 Ed25519 signature. The private signing key is
// never part of the application or its source tree.
class Ed25519Verifier {
public:
    static bool verify(const QByteArray &message, const QByteArray &signature,
                       const QByteArray &publicKey);
};

#endif // ED25519VERIFIER_H
