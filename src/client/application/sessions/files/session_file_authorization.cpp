#include "sessions/files/session_file_authorization.h"

bool applyFileAuthorization(
    SessionNegotiationContext &context,
    const QVector<easydesk::client::signaling::SessionCapabilityGrant> &grants,
    const QByteArray &localPublicKey, const QByteArray &peerPublicKey) {
    if (localPublicKey.size() != 32 || peerPublicKey.size() != 32 || localPublicKey == peerPublicKey)
        return false;
    bool localReceive = false;
    bool peerReceive = false;
    for (const auto &grant : grants) {
        if (grant.capability != easydesk::client::signaling::SessionCapability::FileReceive)
            continue;
        if (grant.subjectPublicKey == localPublicKey && !localReceive)
            localReceive = true;
        else if (grant.subjectPublicKey == peerPublicKey && !peerReceive)
            peerReceive = true;
        else
            return false;
    }
    context.localMayReceiveFiles = localReceive;
    context.peerMayReceiveFiles = peerReceive;
    if (localReceive || peerReceive)
        context.capabilities |= SessionCapability::TransferFiles;
    else
        context.capabilities &= ~SessionCapabilities(SessionCapability::TransferFiles);
    return true;
}
