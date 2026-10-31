#include "network/signaling/protocol/session/session_capability.h"

#include <algorithm>

namespace easydesk::client::signaling {
namespace {

constexpr int kSessionPublicKeySize = 32;
constexpr int kMaximumCapabilityGrants = 4;

bool isKnownCapability(SessionCapability capability) {
    return capability == SessionCapability::ViewScreen ||
           capability == SessionCapability::SendInput ||
           capability == SessionCapability::FileReceive;
}

bool byteArrayLess(const QByteArray &left, const QByteArray &right) {
    return std::lexicographical_compare(
        left.cbegin(), left.cend(), right.cbegin(), right.cend(),
        [](char leftByte, char rightByte) {
            return static_cast<unsigned char>(leftByte) <
                   static_cast<unsigned char>(rightByte);
        });
}

bool grantLess(const SessionCapabilityGrant &left,
               const SessionCapabilityGrant &right) {
    if (left.subjectPublicKey != right.subjectPublicKey)
        return byteArrayLess(left.subjectPublicKey, right.subjectPublicKey);
    return static_cast<quint8>(left.capability) <
           static_cast<quint8>(right.capability);
}

bool grantsEqual(const SessionCapabilityGrant &left,
                 const SessionCapabilityGrant &right) {
    return left.subjectPublicKey == right.subjectPublicKey &&
           left.capability == right.capability;
}

}  // namespace

bool canonicalizeSessionCapabilityGrants(
    QVector<SessionCapabilityGrant> &grants,
    const QByteArray &controllerPublicKey,
    const QByteArray &controlledPublicKey,
    QString &error) {
    if (controllerPublicKey.size() != kSessionPublicKeySize ||
        controlledPublicKey.size() != kSessionPublicKeySize ||
        controllerPublicKey == controlledPublicKey || grants.isEmpty() ||
        grants.size() > kMaximumCapabilityGrants) {
        error = QStringLiteral("Session capability participants or count are invalid.");
        return false;
    }

    std::sort(grants.begin(), grants.end(), grantLess);
    bool controllerMayView = false;
    bool controllerMaySendInput = false;
    for (int index = 0; index < grants.size(); ++index) {
        const SessionCapabilityGrant &grant = grants.at(index);
        const bool participant = grant.subjectPublicKey == controllerPublicKey ||
                                 grant.subjectPublicKey == controlledPublicKey;
        const bool controllerOnly = grant.capability == SessionCapability::ViewScreen ||
                                    grant.capability == SessionCapability::SendInput;
        if (!isKnownCapability(grant.capability) || !participant ||
            (controllerOnly && grant.subjectPublicKey != controllerPublicKey) ||
            (index > 0 && grantsEqual(grants.at(index - 1), grant))) {
            error = QStringLiteral("Session capability grants are invalid.");
            return false;
        }
        controllerMayView = controllerMayView ||
                            grant.capability == SessionCapability::ViewScreen;
        controllerMaySendInput = controllerMaySendInput ||
                                 grant.capability == SessionCapability::SendInput;
    }
    if (controllerMaySendInput && !controllerMayView) {
        error = QStringLiteral("Input control requires screen viewing permission.");
        return false;
    }
    error.clear();
    return true;
}

bool sessionCapabilityGrantsAreSubset(
    const QVector<SessionCapabilityGrant> &granted,
    const QVector<SessionCapabilityGrant> &requested) {
    return std::includes(requested.cbegin(), requested.cend(),
                         granted.cbegin(), granted.cend(), grantLess);
}

}  // namespace easydesk::client::signaling
