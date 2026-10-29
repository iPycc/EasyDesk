#pragma once

#include <memory>
#include <boost/json/object.hpp>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace easydesk::hub {

class SignalingPeer;

struct PresenceState {
    std::string deviceId;
    bool online;
};

enum class RouteResult {
    delivered,
    targetOffline,
    senderMissing,
};

class SignalingHub final {
public:
    bool connect(const std::string& deviceId, const std::shared_ptr<SignalingPeer>& peer);
    void disconnect(const std::string& deviceId, const SignalingPeer* peer);

    std::optional<std::vector<PresenceState>> subscribe(
        const std::string& subscriberId,
        const SignalingPeer* subscriber,
        const std::vector<std::string>& deviceIds);
    RouteResult route(const std::string& senderId,
                      const SignalingPeer* sender,
                      const std::string& targetId,
                      boost::json::object message);

private:
    struct Connection {
        std::weak_ptr<SignalingPeer> peer;
        std::unordered_set<std::string> subscriptions;
    };

    std::vector<std::shared_ptr<SignalingPeer>> watchersForLocked(
        const std::string& deviceId,
        const SignalingPeer* excluded = nullptr);
    static void announce(const std::vector<std::shared_ptr<SignalingPeer>>& watchers,
                         const std::string& deviceId,
                         bool online);

    std::mutex mutex_;
    std::unordered_map<std::string, Connection> connections_;
};

}  // namespace easydesk::hub
