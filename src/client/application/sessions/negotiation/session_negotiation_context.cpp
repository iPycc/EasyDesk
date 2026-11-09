#include "sessions/negotiation/session_negotiation_context.h"

namespace {

bool containsDigits(const QString &value, int length) {
    if (value.size() != length)
        return false;
    for (const QChar character : value) {
        if (character < QLatin1Char('0') || character > QLatin1Char('9'))
            return false;
    }
    return true;
}

}  // namespace

bool SessionNegotiationContext::isValid() const {
    return requestId.size() == 16 && sessionId.size() == 16 &&
           !signedRequestPayload.isEmpty() && !signedAcceptPayload.isEmpty() &&
           acceptanceNonce.size() == 32 && peerPublicKey.size() == 32 &&
           containsDigits(localDeviceId, 9) && containsDigits(peerDeviceId, 9) &&
           localDeviceId != peerDeviceId && sessionEpoch > 0 &&
           permissionEpoch > 0 && expiresAt > 0 &&
           capabilities.testFlag(SessionCapability::ViewDesktop) &&
           capabilities.testFlag(SessionCapability::TransferFiles) ==
               (localMayReceiveFiles || peerMayReceiveFiles);
}
