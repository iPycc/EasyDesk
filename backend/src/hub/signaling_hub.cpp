#include "hub/signaling_hub.hpp"

#include "hub/signaling_peer.hpp"

#include <boost/json/object.hpp>

#include <utility>

namespace easydesk::hub {

bool SignalingHub::connect(const std::string& deviceId,
                           const std::shared_ptr<SignalingPeer>& peer) {
    std::vector<std::shared_ptr<SignalingPeer>> watchers;
    {
        const std::scoped_lock lock(mutex_);
        const auto existing = connections_.find(deviceId);
        if (existing != connections_.end()) {
            if (!existing->second.peer.expired()) {
                return false;
            }
            connections_.erase(existing);
        }

        connections_.emplace(deviceId, Connection{.peer = peer, .subscriptions = {}});
        watchers = watchersForLocked(deviceId, peer.get());
    }
    announce(watchers, deviceId, true);
    return true;
}

void SignalingHub::disconnect(const std::string& deviceId, const SignalingPeer* peer) {
    std::vector<std::shared_ptr<SignalingPeer>> watchers;
    {
        const std::scoped_lock lock(mutex_);
        const auto existing = connections_.find(deviceId);
        if (existing == connections_.end()) {
            return;
        }
        const auto active = existing->second.peer.lock();
        if (active && active.get() != peer) {
            return;
        }
        connections_.erase(existing);
        watchers = watchersForLocked(deviceId, peer);
    }
    announce(watchers, deviceId, false);
}

std::optional<std::vector<PresenceState>> SignalingHub::subscribe(
    const std::string& subscriberId,
    const SignalingPeer* subscriber,
    const std::vector<std::string>& deviceIds) {
    const std::scoped_lock lock(mutex_);
    const auto connection = connections_.find(subscriberId);
    if (connection == connections_.end()) {
        return std::nullopt;
    }
    const auto active = connection->second.peer.lock();
    if (!active || active.get() != subscriber) {
        return std::nullopt;
    }

    connection->second.subscriptions = {deviceIds.begin(), deviceIds.end()};
    std::vector<PresenceState> states;
    states.reserve(deviceIds.size());
    for (const auto& deviceId : deviceIds) {
        const auto target = connections_.find(deviceId);
        states.push_back({
            .deviceId = deviceId,
            .online = target != connections_.end() && !target->second.peer.expired(),
        });
    }
    return states;
}

RouteResult SignalingHub::route(const std::string& senderId,
                                const SignalingPeer* sender,
                                const std::string& targetId,
                                boost::json::object message) {
    std::shared_ptr<SignalingPeer> target;
    {
        const std::scoped_lock lock(mutex_);
        const auto source = connections_.find(senderId);
        if (source == connections_.end()) {
            return RouteResult::senderMissing;
        }
        const auto activeSender = source->second.peer.lock();
        if (!activeSender || activeSender.get() != sender) {
            return RouteResult::senderMissing;
        }

        const auto destination = connections_.find(targetId);
        if (destination == connections_.end() || !(target = destination->second.peer.lock())) {
            return RouteResult::targetOffline;
        }
    }

    target->deliver(std::move(message));
    return RouteResult::delivered;
}

std::vector<std::shared_ptr<SignalingPeer>> SignalingHub::watchersForLocked(
    const std::string& deviceId,
    const SignalingPeer* excluded) {
    std::vector<std::shared_ptr<SignalingPeer>> watchers;
    for (auto& [id, connection] : connections_) {
        (void)id;
        if (!connection.subscriptions.contains(deviceId)) {
            continue;
        }
        if (auto peer = connection.peer.lock(); peer && peer.get() != excluded) {
            watchers.push_back(std::move(peer));
        }
    }
    return watchers;
}

void SignalingHub::announce(const std::vector<std::shared_ptr<SignalingPeer>>& watchers,
                            const std::string& deviceId,
                            bool online) {
    for (const auto& watcher : watchers) {
        boost::json::object message;
        message["type"] = "presence.changed";
        message["device_id"] = deviceId;
        message["online"] = online;
        watcher->deliver(std::move(message));
    }
}

}  // namespace easydesk::hub
