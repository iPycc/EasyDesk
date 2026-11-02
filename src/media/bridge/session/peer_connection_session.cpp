#include "session/peer_connection_session.h"

#include "common/media_error.h"
#include "runtime/media_runtime_internal.h"
#include "session/media_event_queue.h"
#include "session/session_description_observers.h"
#include "video/capture/desktop_video_source.h"
#include "video/render/session_video_renderer_registry.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <memory>
#include <string>
#include <utility>

#include "api/jsep.h"
#include "api/media_types.h"
#include "api/rtc_error.h"
#include "api/rtp_transceiver_interface.h"

namespace easydesk::media {
namespace {

constexpr uint64_t kKnownCapabilities = EASYDESK_MEDIA_CAPABILITY_VIEW_SCREEN |
                                        EASYDESK_MEDIA_CAPABILITY_SEND_INPUT |
                                        EASYDESK_MEDIA_CAPABILITY_TRANSFER_FILES;
constexpr uint32_t kMaximumIceServerCount = 8;
constexpr uint32_t kMaximumIceUrlSize = 2048;
constexpr uint32_t kMaximumIceCredentialSize = 512;
constexpr uint32_t kMaximumSdpSize = 64u * 1024u;
constexpr uint32_t kMaximumSdpMidSize = 256;
constexpr uint32_t kMaximumCandidateSize = 16u * 1024u;

bool hasNonzeroSessionId(const uint8_t *sessionId) {
    return std::any_of(sessionId, sessionId + EASYDESK_MEDIA_SESSION_ID_SIZE,
                       [](uint8_t value) { return value != 0; });
}

bool validCapabilities(uint64_t capabilities) {
    return (capabilities & ~kKnownCapabilities) == 0 &&
           ((capabilities & EASYDESK_MEDIA_CAPABILITY_SEND_INPUT) == 0 ||
            (capabilities & EASYDESK_MEDIA_CAPABILITY_VIEW_SCREEN) != 0);
}

bool validInitialCapabilities(uint64_t capabilities) {
    return validCapabilities(capabilities) &&
           (capabilities & EASYDESK_MEDIA_CAPABILITY_VIEW_SCREEN) != 0;
}

bool readBoundedString(const char *data,
                       uint32_t size,
                       uint32_t maximumSize,
                       std::string *result) {
    if (result == nullptr || size > maximumSize || (size > 0 && data == nullptr) ||
        (size > 0 && std::memchr(data, '\0', size) != nullptr))
        return false;
    result->assign(data == nullptr ? "" : data, size);
    return true;
}

bool supportedIceUrl(const std::string &url) {
    return url.rfind("stun:", 0) == 0 || url.rfind("stuns:", 0) == 0 ||
           url.rfind("turn:", 0) == 0 || url.rfind("turns:", 0) == 0;
}

EasyDeskMediaConnectionState connectionState(
    webrtc::PeerConnectionInterface::PeerConnectionState state) {
    using State = webrtc::PeerConnectionInterface::PeerConnectionState;
    switch (state) {
    case State::kNew:
        return EASYDESK_MEDIA_CONNECTION_NEW;
    case State::kConnecting:
        return EASYDESK_MEDIA_CONNECTION_CONNECTING;
    case State::kConnected:
        return EASYDESK_MEDIA_CONNECTION_CONNECTED;
    case State::kDisconnected:
        return EASYDESK_MEDIA_CONNECTION_DISCONNECTED;
    case State::kFailed:
        return EASYDESK_MEDIA_CONNECTION_FAILED;
    case State::kClosed:
        return EASYDESK_MEDIA_CONNECTION_CLOSED;
    }
    return EASYDESK_MEDIA_CONNECTION_FAILED;
}

EasyDeskMediaIceConnectionState iceConnectionState(
    webrtc::PeerConnectionInterface::IceConnectionState state) {
    using State = webrtc::PeerConnectionInterface::IceConnectionState;
    switch (state) {
    case State::kIceConnectionNew:
        return EASYDESK_MEDIA_ICE_NEW;
    case State::kIceConnectionChecking:
        return EASYDESK_MEDIA_ICE_CHECKING;
    case State::kIceConnectionConnected:
        return EASYDESK_MEDIA_ICE_CONNECTED;
    case State::kIceConnectionCompleted:
        return EASYDESK_MEDIA_ICE_COMPLETED;
    case State::kIceConnectionFailed:
        return EASYDESK_MEDIA_ICE_FAILED;
    case State::kIceConnectionDisconnected:
        return EASYDESK_MEDIA_ICE_DISCONNECTED;
    case State::kIceConnectionClosed:
        return EASYDESK_MEDIA_ICE_CLOSED;
    case State::kIceConnectionMax:
        break;
    }
    return EASYDESK_MEDIA_ICE_FAILED;
}

}  // namespace

PeerConnectionSession::PeerConnectionSession(std::shared_ptr<MediaRuntime> runtime,
                                             uint32_t eventQueueCapacityBytes)
    : m_runtime(std::move(runtime)),
      m_events(std::make_shared<MediaEventQueue>(eventQueueCapacityBytes)) {}

PeerConnectionSession::~PeerConnectionSession() {
    close();
}

EasyDeskMediaStatus PeerConnectionSession::start(const EasyDeskMediaSessionConfig &config,
                                                 EasyDeskMediaError *error) {
    if (!m_runtime || !m_runtime->ready() || config.abi_version != EASYDESK_MEDIA_ABI_VERSION ||
        config.session_epoch == 0 || config.permission_epoch == 0 ||
        config.ice_generation == 0 || !hasNonzeroSessionId(config.session_id) ||
        !validInitialCapabilities(config.capabilities) ||
        (config.role != EASYDESK_MEDIA_SESSION_ROLE_CONTROLLER &&
         config.role != EASYDESK_MEDIA_SESSION_ROLE_CONTROLLED) ||
        config.ice_server_count > kMaximumIceServerCount ||
        (config.ice_server_count > 0 && config.ice_servers == nullptr)) {
        setMediaError(error, EASYDESK_MEDIA_STATUS_INVALID_ARGUMENT,
                      "Invalid media session configuration.");
        return EASYDESK_MEDIA_STATUS_INVALID_ARGUMENT;
    }

    webrtc::PeerConnectionInterface::RTCConfiguration rtcConfig;
    rtcConfig.sdp_semantics = webrtc::SdpSemantics::kUnifiedPlan;
    for (uint32_t index = 0; index < config.ice_server_count; ++index) {
        const EasyDeskMediaIceServerConfig &source = config.ice_servers[index];
        if (source.size < sizeof(EasyDeskMediaIceServerConfig)) {
            setMediaError(error, EASYDESK_MEDIA_STATUS_INVALID_ARGUMENT,
                          "Invalid ICE server configuration size.");
            return EASYDESK_MEDIA_STATUS_INVALID_ARGUMENT;
        }
        std::string url;
        std::string username;
        std::string credential;
        if (!readBoundedString(source.url_utf8, source.url_size, kMaximumIceUrlSize, &url) ||
            !readBoundedString(source.username_utf8, source.username_size,
                               kMaximumIceCredentialSize, &username) ||
            !readBoundedString(source.credential_utf8, source.credential_size,
                               kMaximumIceCredentialSize, &credential) ||
            url.empty() || !supportedIceUrl(url)) {
            setMediaError(error, EASYDESK_MEDIA_STATUS_INVALID_ARGUMENT,
                          "Invalid ICE server URL or credential size.");
            return EASYDESK_MEDIA_STATUS_INVALID_ARGUMENT;
        }
        if ((url.rfind("turn:", 0) == 0 || url.rfind("turns:", 0) == 0) &&
            (username.empty() || credential.empty())) {
            setMediaError(error, EASYDESK_MEDIA_STATUS_INVALID_ARGUMENT,
                          "TURN servers require temporary username and credential values.");
            return EASYDESK_MEDIA_STATUS_INVALID_ARGUMENT;
        }
        webrtc::PeerConnectionInterface::IceServer server;
        server.urls.push_back(std::move(url));
        server.username = std::move(username);
        server.password = std::move(credential);
        rtcConfig.servers.push_back(std::move(server));
    }

    m_role = config.role;
    m_iceGeneration = config.ice_generation;
    m_permissionEpoch.store(config.permission_epoch);
    m_capabilities.store(config.capabilities);

    std::string failure;
    m_runtime->signalingThread()->BlockingCall([&] {
        webrtc::PeerConnectionDependencies dependencies(this);
        auto created = m_runtime->factory()->CreatePeerConnectionOrError(
            rtcConfig, std::move(dependencies));
        if (!created.ok()) {
            failure = created.error().message();
            return;
        }
        m_peer = created.MoveValue();

        if (m_role == EASYDESK_MEDIA_SESSION_ROLE_CONTROLLER) {
            webrtc::RtpTransceiverInit video;
            video.direction = webrtc::RtpTransceiverDirection::kRecvOnly;
            auto added = m_peer->AddTransceiver(webrtc::MediaType::VIDEO, video);
            if (!added.ok()) {
                failure = added.error().message();
                m_peer->Close();
                m_peer = nullptr;
                return;
            }
        }

        if (m_role == EASYDESK_MEDIA_SESSION_ROLE_CONTROLLER) {
            const auto createChannel = [this, &failure](const char *label,
                                                        const webrtc::DataChannelInit &init) {
                auto channel = m_peer->CreateDataChannelOrError(label, &init);
                if (!channel.ok()) {
                    failure = channel.error().message();
                    return false;
                }
                m_dataChannels.push_back(channel.MoveValue());
                return true;
            };

            webrtc::DataChannelInit reliable;
            reliable.ordered = true;
            if (!createChannel("control.v1", reliable))
                return;
            if ((config.capabilities & EASYDESK_MEDIA_CAPABILITY_SEND_INPUT) != 0) {
                if (!createChannel("input.v1", reliable))
                    return;
                webrtc::DataChannelInit pointer;
                pointer.ordered = false;
                pointer.maxRetransmits = 0;
                if (!createChannel("pointer.v1", pointer))
                    return;
            }
        }
    });

    if (!failure.empty() || !m_peer) {
        setMediaError(error, EASYDESK_MEDIA_STATUS_INTERNAL_ERROR,
                      failure.empty() ? "Peer connection initialization failed." : failure);
        close();
        return EASYDESK_MEDIA_STATUS_INTERNAL_ERROR;
    }

    setMediaError(error, EASYDESK_MEDIA_STATUS_OK, "");
    return EASYDESK_MEDIA_STATUS_OK;
}

EasyDeskMediaStatus PeerConnectionSession::createOffer(EasyDeskMediaError *error) {
    if (m_closed.load() || !m_peer || m_role != EASYDESK_MEDIA_SESSION_ROLE_CONTROLLER) {
        setMediaError(error, EASYDESK_MEDIA_STATUS_INVALID_STATE,
                      "Only an active controller session can create an offer.");
        return EASYDESK_MEDIA_STATUS_INVALID_STATE;
    }
    m_runtime->signalingThread()->BlockingCall([this] {
        auto observer = createDescriptionObserver(m_peer, m_events, m_iceGeneration);
        m_peer->CreateOffer(observer.get(),
                            webrtc::PeerConnectionInterface::RTCOfferAnswerOptions());
    });
    setMediaError(error, EASYDESK_MEDIA_STATUS_OK, "");
    return EASYDESK_MEDIA_STATUS_OK;
}

EasyDeskMediaStatus PeerConnectionSession::createAnswer(EasyDeskMediaError *error) {
    if (m_closed.load() || !m_peer || m_role != EASYDESK_MEDIA_SESSION_ROLE_CONTROLLED) {
        setMediaError(error, EASYDESK_MEDIA_STATUS_INVALID_STATE,
                      "Only an active controlled session can create an answer.");
        return EASYDESK_MEDIA_STATUS_INVALID_STATE;
    }
    m_runtime->signalingThread()->BlockingCall([this] {
        auto observer = createDescriptionObserver(m_peer, m_events, m_iceGeneration);
        m_peer->CreateAnswer(observer.get(),
                             webrtc::PeerConnectionInterface::RTCOfferAnswerOptions());
    });
    setMediaError(error, EASYDESK_MEDIA_STATUS_OK, "");
    return EASYDESK_MEDIA_STATUS_OK;
}

EasyDeskMediaStatus PeerConnectionSession::setRemoteDescription(
    uint32_t iceGeneration,
    EasyDeskMediaSdpType type,
    const uint8_t *sdp,
    uint32_t size,
    EasyDeskMediaError *error) {
    if (m_closed.load() || !m_peer || sdp == nullptr || size == 0 ||
        size > kMaximumSdpSize || std::memchr(sdp, '\0', size) != nullptr ||
        iceGeneration != m_iceGeneration ||
        (type != EASYDESK_MEDIA_SDP_OFFER && type != EASYDESK_MEDIA_SDP_ANSWER) ||
        (m_role == EASYDESK_MEDIA_SESSION_ROLE_CONTROLLER &&
         type != EASYDESK_MEDIA_SDP_ANSWER) ||
        (m_role == EASYDESK_MEDIA_SESSION_ROLE_CONTROLLED &&
         type != EASYDESK_MEDIA_SDP_OFFER)) {
        setMediaError(error, EASYDESK_MEDIA_STATUS_INVALID_ARGUMENT,
                      "Invalid or stale remote session description.");
        return EASYDESK_MEDIA_STATUS_INVALID_ARGUMENT;
    }

    const std::string text(reinterpret_cast<const char *>(sdp), size);
    if (text.find("a=fingerprint:sha-256 ") == std::string::npos) {
        setMediaError(error, EASYDESK_MEDIA_STATUS_SDP_ERROR,
                      "Remote SDP is missing a SHA-256 DTLS fingerprint.");
        return EASYDESK_MEDIA_STATUS_SDP_ERROR;
    }

    EasyDeskMediaStatus status = EASYDESK_MEDIA_STATUS_OK;
    std::string failure;
    m_runtime->signalingThread()->BlockingCall([&] {
        webrtc::SdpParseError parseError;
        auto description = webrtc::CreateSessionDescription(
            type == EASYDESK_MEDIA_SDP_OFFER ? webrtc::SdpType::kOffer
                                             : webrtc::SdpType::kAnswer,
            text, &parseError);
        if (!description) {
            status = EASYDESK_MEDIA_STATUS_SDP_ERROR;
            failure = parseError.description;
            return;
        }
        m_peer->SetRemoteDescription(
            std::move(description),
            createRemoteDescriptionObserver(m_peer, m_events, type, m_iceGeneration));
    });
    setMediaError(error, status, failure);
    return status;
}

EasyDeskMediaStatus PeerConnectionSession::addIceCandidate(
    uint32_t iceGeneration,
    const uint8_t *sdpMid,
    uint32_t sdpMidSize,
    int32_t sdpMlineIndex,
    const uint8_t *candidate,
    uint32_t candidateSize,
    EasyDeskMediaError *error) {
    if (m_closed.load() || !m_peer || sdpMid == nullptr || sdpMidSize == 0 ||
        sdpMidSize > kMaximumSdpMidSize || candidate == nullptr || candidateSize == 0 ||
        candidateSize > kMaximumCandidateSize || sdpMlineIndex < 0 ||
        iceGeneration != m_iceGeneration || std::memchr(sdpMid, '\0', sdpMidSize) != nullptr ||
        std::memchr(candidate, '\0', candidateSize) != nullptr) {
        setMediaError(error, EASYDESK_MEDIA_STATUS_INVALID_ARGUMENT,
                      "Invalid or stale ICE candidate.");
        return EASYDESK_MEDIA_STATUS_INVALID_ARGUMENT;
    }

    const std::string mid(reinterpret_cast<const char *>(sdpMid), sdpMidSize);
    const std::string text(reinterpret_cast<const char *>(candidate), candidateSize);
    EasyDeskMediaStatus status = EASYDESK_MEDIA_STATUS_OK;
    std::string failure;
    m_runtime->signalingThread()->BlockingCall([&] {
        webrtc::SdpParseError parseError;
        std::unique_ptr<webrtc::IceCandidate> parsed(
            webrtc::CreateIceCandidate(mid, sdpMlineIndex, text, &parseError));
        if (!parsed) {
            status = EASYDESK_MEDIA_STATUS_ICE_ERROR;
            failure = parseError.description;
            return;
        }
        if (!m_peer->AddIceCandidate(parsed.get())) {
            status = EASYDESK_MEDIA_STATUS_ICE_ERROR;
            failure = "WebRTC rejected the ICE candidate.";
        }
    });
    setMediaError(error, status, failure);
    return status;
}

EasyDeskMediaStatus PeerConnectionSession::setPermissions(uint64_t permissionEpoch,
                                                          uint64_t capabilities,
                                                          EasyDeskMediaError *error) {
    const uint64_t currentCapabilities = m_capabilities.load();
    if (m_closed.load() || permissionEpoch <= m_permissionEpoch.load() ||
        !validCapabilities(capabilities) ||
        (capabilities & ~currentCapabilities) != 0) {
        setMediaError(error, EASYDESK_MEDIA_STATUS_INVALID_ARGUMENT,
                      "Permissions must use a newer epoch and may only revoke capabilities.");
        return EASYDESK_MEDIA_STATUS_INVALID_ARGUMENT;
    }

    m_capabilities.store(capabilities);
    m_permissionEpoch.store(permissionEpoch);
    MediaEventRecord event;
    event.type = EASYDESK_MEDIA_EVENT_PERMISSION_CHANGED;
    event.payload.resize(sizeof(capabilities));
    std::memcpy(event.payload.data(), &capabilities, sizeof(capabilities));
    if (!emitEvent(std::move(event))) {
        setMediaError(error, EASYDESK_MEDIA_STATUS_QUEUE_OVERFLOW,
                      "Media event queue overflowed and the session was closed.");
        return EASYDESK_MEDIA_STATUS_QUEUE_OVERFLOW;
    }
    if ((capabilities & EASYDESK_MEDIA_CAPABILITY_VIEW_SCREEN) == 0)
        close();
    setMediaError(error, EASYDESK_MEDIA_STATUS_OK, "");
    return EASYDESK_MEDIA_STATUS_OK;
}

EasyDeskMediaStatus PeerConnectionSession::pollEvent(EasyDeskMediaEvent *event,
                                                     uint8_t *payload,
                                                     uint32_t capacity,
                                                     uint32_t *needed,
                                                     uint32_t timeoutMs) {
    return m_events->poll(event, payload, capacity, needed, timeoutMs);
}

EasyDeskMediaStatus PeerConnectionSession::close() {
    m_closed.store(true);
    if (m_cleanupComplete.exchange(true))
        return EASYDESK_MEDIA_STATUS_OK;

    SessionVideoRendererRegistry::instance().detach(this);

    if (m_runtime && m_runtime->signalingThread()) {
        auto cleanup = [this] {
            if (m_peer)
                m_peer->Close();
            m_dataChannels.clear();
            m_peer = nullptr;
        };
        if (m_runtime->signalingThread()->IsCurrent())
            cleanup();
        else
            m_runtime->signalingThread()->BlockingCall(cleanup);
    }
    m_events->close();
    return EASYDESK_MEDIA_STATUS_OK;
}

void PeerConnectionSession::OnSignalingChange(
    webrtc::PeerConnectionInterface::SignalingState state) {
    if (state == webrtc::PeerConnectionInterface::kStable &&
        m_role == EASYDESK_MEDIA_SESSION_ROLE_CONTROLLER && m_peer && !m_closed.load()) {
        for (const auto &receiver : m_peer->GetReceivers()) {
            if (!receiver || receiver->media_type() != webrtc::MediaType::VIDEO)
                continue;
            auto track = receiver->track();
            if (!track || track->kind() != webrtc::MediaStreamTrackInterface::kVideoKind)
                continue;
            SessionVideoRendererRegistry::instance().bindRemoteTrack(
                this, webrtc::scoped_refptr<webrtc::VideoTrackInterface>(
                          static_cast<webrtc::VideoTrackInterface *>(track.get())));
            break;
        }
        return;
    }

    if (state != webrtc::PeerConnectionInterface::kHaveRemoteOffer ||
        m_role != EASYDESK_MEDIA_SESSION_ROLE_CONTROLLED || !m_peer ||
        m_closed.load()) {
        return;
    }

    std::string failure;
    for (const auto &transceiver : m_peer->GetTransceivers()) {
        if (!transceiver || transceiver->media_type() != webrtc::MediaType::VIDEO)
            continue;
        if (transceiver->sender()->track())
            return;

        auto source = DesktopVideoSource::create(0, 30);
        if (!source || !source->start(&failure)) {
            if (failure.empty())
                failure = "Desktop capture initialization failed.";
            break;
        }
        auto track = m_runtime->factory()->CreateVideoTrack(source, "desktop.video.v1");
        if (!track) {
            source->stop();
            failure = "WebRTC could not create the desktop video track.";
            break;
        }
        track->set_content_hint(webrtc::VideoTrackInterface::ContentHint::kText);
        const webrtc::RTCError direction = transceiver->SetDirectionWithError(
            webrtc::RtpTransceiverDirection::kSendOnly);
        if (!direction.ok()) {
            failure = direction.message();
            break;
        }
        if (!transceiver->sender()->SetTrack(track.get())) {
            failure = "WebRTC rejected the desktop video track.";
            break;
        }
        return;
    }

    if (failure.empty())
        failure = "The remote offer has no video transceiver.";
    MediaEventRecord event;
    event.type = EASYDESK_MEDIA_EVENT_ERROR;
    event.code = EASYDESK_MEDIA_STATUS_INTERNAL_ERROR;
    event.iceGeneration = m_iceGeneration;
    event.payload.assign(failure.begin(), failure.end());
    emitEvent(std::move(event));
    if (m_peer)
        m_peer->Close();
    m_closed.store(true);
}

void PeerConnectionSession::OnDataChannel(
    webrtc::scoped_refptr<webrtc::DataChannelInterface> channel) {
    if (!channel)
        return;
    const std::string label = channel->label();
    const uint64_t capabilities = m_capabilities.load();
    const bool allowed = label == "control.v1" ||
                         ((label == "input.v1" || label == "pointer.v1") &&
                          (capabilities & EASYDESK_MEDIA_CAPABILITY_SEND_INPUT) != 0) ||
                         ((label == "files.meta.v1" || label == "files.data.v1") &&
                          (capabilities & EASYDESK_MEDIA_CAPABILITY_TRANSFER_FILES) != 0);
    if (!allowed) {
        channel->Close();
        return;
    }
    m_dataChannels.push_back(channel);
    MediaEventRecord event;
    event.type = EASYDESK_MEDIA_EVENT_DATA_CHANNEL;
    event.payload.assign(label.begin(), label.end());
    emitEvent(std::move(event));
}

void PeerConnectionSession::OnIceGatheringChange(
    webrtc::PeerConnectionInterface::IceGatheringState state) {
    if (state != webrtc::PeerConnectionInterface::kIceGatheringComplete)
        return;
    MediaEventRecord event;
    event.type = EASYDESK_MEDIA_EVENT_ICE_GATHERING_COMPLETE;
    event.iceGeneration = m_iceGeneration;
    emitEvent(std::move(event));
}

void PeerConnectionSession::OnIceCandidate(const webrtc::IceCandidate *candidate) {
    if (candidate == nullptr)
        return;
    const std::string mid = candidate->sdp_mid();
    const std::string text = candidate->ToString();
    MediaEventRecord event;
    event.type = EASYDESK_MEDIA_EVENT_LOCAL_CANDIDATE;
    event.iceGeneration = m_iceGeneration;
    event.sdpMlineIndex = candidate->sdp_mline_index();
    event.labelSize = static_cast<uint32_t>(mid.size());
    event.payload.reserve(mid.size() + text.size());
    event.payload.insert(event.payload.end(), mid.begin(), mid.end());
    event.payload.insert(event.payload.end(), text.begin(), text.end());
    emitEvent(std::move(event));
}

void PeerConnectionSession::OnIceConnectionChange(
    webrtc::PeerConnectionInterface::IceConnectionState state) {
    MediaEventRecord event;
    event.type = EASYDESK_MEDIA_EVENT_ICE_CONNECTION_STATE;
    event.code = static_cast<int32_t>(iceConnectionState(state));
    event.iceGeneration = m_iceGeneration;
    emitEvent(std::move(event));
}

void PeerConnectionSession::OnConnectionChange(
    webrtc::PeerConnectionInterface::PeerConnectionState state) {
    MediaEventRecord event;
    event.type = EASYDESK_MEDIA_EVENT_CONNECTION_STATE;
    event.code = static_cast<int32_t>(connectionState(state));
    event.iceGeneration = m_iceGeneration;
    emitEvent(std::move(event));
}

bool PeerConnectionSession::emitEvent(MediaEventRecord event) {
    if (m_events->push(std::move(event)))
        return true;
    if (m_peer) {
        if (m_runtime->signalingThread()->IsCurrent())
            m_peer->Close();
        else
            m_runtime->signalingThread()->BlockingCall([this] {
                if (m_peer)
                    m_peer->Close();
            });
    }
    m_closed.store(true);
    return false;
}

}  // namespace easydesk::media
