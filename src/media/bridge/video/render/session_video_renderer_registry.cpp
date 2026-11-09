#include "video/render/session_video_renderer_registry.h"

#include "video/render/d3d11_video_renderer.h"

#include <utility>

#include "api/video/video_source_interface.h"

namespace easydesk::media {

struct SessionVideoRendererRegistry::Entry {
    webrtc::scoped_refptr<webrtc::VideoTrackInterface> track;
    std::unique_ptr<D3d11VideoRenderer> renderer;
};

SessionVideoRendererRegistry &SessionVideoRendererRegistry::instance() {
    static SessionVideoRendererRegistry registry;
    return registry;
}

SessionVideoRendererRegistry::~SessionVideoRendererRegistry() {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto &[session, entry] : m_entries) {
        if (entry->track && entry->renderer)
            entry->track->RemoveSink(entry->renderer.get());
        if (entry->renderer)
            entry->renderer->stop();
    }
    m_entries.clear();
}

EasyDeskMediaStatus SessionVideoRendererRegistry::attach(PeerConnectionSession *session,
                                                         HWND window,
                                                         std::string *error) {
    if (error != nullptr)
        error->clear();
    if (session == nullptr || window == nullptr || !IsWindow(window)) {
        if (error != nullptr)
            *error = "Video rendering requires a valid native window.";
        return EASYDESK_MEDIA_STATUS_INVALID_ARGUMENT;
    }

    auto renderer = std::make_unique<D3d11VideoRenderer>(window);
    std::string startError;
    if (!renderer->start(&startError)) {
        if (error != nullptr) {
            *error = startError.empty() ? "D3D11 video renderer initialization failed."
                                       : std::move(startError);
        }
        return EASYDESK_MEDIA_STATUS_INTERNAL_ERROR;
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    auto &entry = m_entries[session];
    if (!entry)
        entry = std::make_unique<Entry>();
    if (entry->track && entry->renderer)
        entry->track->RemoveSink(entry->renderer.get());
    if (entry->renderer)
        entry->renderer->stop();
    entry->renderer = std::move(renderer);
    if (entry->track)
        entry->track->AddOrUpdateSink(entry->renderer.get(), webrtc::VideoSinkWants());
    return EASYDESK_MEDIA_STATUS_OK;
}

void SessionVideoRendererRegistry::bindRemoteTrack(
    PeerConnectionSession *session,
    webrtc::scoped_refptr<webrtc::VideoTrackInterface> track) {
    if (session == nullptr || !track)
        return;

    std::lock_guard<std::mutex> lock(m_mutex);
    auto &entry = m_entries[session];
    if (!entry)
        entry = std::make_unique<Entry>();
    if (entry->track.get() == track.get())
        return;
    if (entry->track && entry->renderer)
        entry->track->RemoveSink(entry->renderer.get());
    entry->track = std::move(track);
    if (entry->renderer)
        entry->track->AddOrUpdateSink(entry->renderer.get(), webrtc::VideoSinkWants());
}

void SessionVideoRendererRegistry::detach(PeerConnectionSession *session) {
    if (session == nullptr)
        return;

    std::lock_guard<std::mutex> lock(m_mutex);
    const auto found = m_entries.find(session);
    if (found == m_entries.end())
        return;
    Entry &entry = *found->second;
    if (entry.track && entry.renderer)
        entry.track->RemoveSink(entry.renderer.get());
    if (entry.renderer)
        entry.renderer->stop();
    m_entries.erase(found);
}

EasyDeskMediaStatus SessionVideoRendererRegistry::statistics(
    PeerConnectionSession *session, EasyDeskMediaVideoRendererStats *stats,
    std::string *error) {
    if (error)
        error->clear();
    if (!session || !stats || stats->size < sizeof(EasyDeskMediaVideoRendererStats)) {
        if (error)
            *error = "Invalid video renderer statistics arguments.";
        return EASYDESK_MEDIA_STATUS_INVALID_ARGUMENT;
    }
    std::lock_guard<std::mutex> lock(m_mutex);
    const auto found = m_entries.find(session);
    if (found == m_entries.end() || !found->second->renderer) {
        if (error)
            *error = "Video renderer is not attached.";
        return EASYDESK_MEDIA_STATUS_INVALID_STATE;
    }
    const auto &renderer = found->second->renderer;
    if (!renderer->isReady()) {
        if (error) {
            *error = renderer->errorString();
            if (error->empty())
                *error = "Video renderer is not ready.";
        }
        return EASYDESK_MEDIA_STATUS_INTERNAL_ERROR;
    }
    const auto snapshot = renderer->statistics();
    stats->received_frames = snapshot.receivedFrames;
    stats->rendered_frames = snapshot.renderedFrames;
    stats->dropped_frames = snapshot.droppedFrames;
    stats->source_width = static_cast<uint32_t>(snapshot.sourceWidth);
    stats->source_height = static_cast<uint32_t>(snapshot.sourceHeight);
    for (auto &value : stats->reserved)
        value = 0;
    return EASYDESK_MEDIA_STATUS_OK;
}

}  // namespace easydesk::media
