#pragma once

#include <boost/json/object.hpp>

namespace easydesk::hub {

class SignalingPeer {
public:
    virtual ~SignalingPeer() = default;
    virtual void deliver(boost::json::object message) = 0;
};

}  // namespace easydesk::hub
