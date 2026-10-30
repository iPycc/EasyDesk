#include "protocol/session/session_candidate.hpp"

#include "protocol/cbor/deterministic_cbor.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <limits>
#include <string_view>
#include <utility>

namespace easydesk::protocol {
namespace {

constexpr std::string_view kCandidatesType = "session.candidates";
constexpr std::string_view kGatheringCompleteType = "session.gathering_complete";
constexpr std::size_t kMaximumMidBytes = 64;
constexpr std::size_t kMaximumUfragBytes = 256;
constexpr std::size_t kMaximumCandidateBytes = 2048;

template <std::size_t Size>
std::optional<std::array<std::uint8_t, Size>> readFixedBytes(cbor::Reader& reader) {
    auto bytes = reader.readBytes(Size);
    if (!bytes || bytes->size() != Size)
        return std::nullopt;
    std::array<std::uint8_t, Size> result{};
    std::copy(bytes->begin(), bytes->end(), result.begin());
    return result;
}

bool isSingleLine(std::string_view value, std::size_t maximumBytes) {
    return !value.empty() && value.size() <= maximumBytes && cbor::isValidUtf8(value) &&
           value.find('\0') == std::string_view::npos &&
           value.find('\r') == std::string_view::npos &&
           value.find('\n') == std::string_view::npos;
}

bool isUfrag(std::string_view value) {
    return isSingleLine(value, kMaximumUfragBytes) &&
           std::all_of(value.begin(), value.end(), [](unsigned char character) {
               return std::isalnum(character) != 0 || character == '+' || character == '/';
           });
}

bool validCandidate(const SessionCandidate& candidate) {
    return isSingleLine(candidate.sdpMid, kMaximumMidBytes) &&
           candidate.sdpMLineIndex <= 255 && isUfrag(candidate.usernameFragment) &&
           isSingleLine(candidate.candidate, kMaximumCandidateBytes) &&
           candidate.candidate.starts_with("candidate:");
}

bool validCandidates(const std::vector<SessionCandidate>& candidates) {
    if (candidates.empty() || candidates.size() > kMaximumCandidateBatchSize)
        return false;
    for (std::size_t index = 0; index < candidates.size(); ++index) {
        if (!validCandidate(candidates[index]) ||
            std::find(candidates.begin(), candidates.begin() + static_cast<std::ptrdiff_t>(index),
                      candidates[index]) !=
                candidates.begin() + static_cast<std::ptrdiff_t>(index)) {
            return false;
        }
    }
    return true;
}

}  // namespace

std::optional<std::vector<std::uint8_t>> encodeSessionCandidatesPayload(
    const SessionCandidatesPayload& payload) {
    if (payload.sequence == 0 || payload.expiresAt <= 0 || !validCandidates(payload.candidates))
        return std::nullopt;

    cbor::Writer writer;
    writer.beginArray(7);
    writer.writeText(kSessionSigningDomain);
    writer.writeText(kCandidatesType);
    writer.writeBytes(payload.descriptionDigest);
    writer.writeUnsigned(payload.iceGeneration);
    writer.beginArray(payload.candidates.size());
    for (const SessionCandidate& candidate : payload.candidates) {
        writer.beginArray(4);
        writer.writeText(candidate.sdpMid);
        writer.writeUnsigned(candidate.sdpMLineIndex);
        writer.writeText(candidate.usernameFragment);
        writer.writeText(candidate.candidate);
    }
    writer.writeUnsigned(payload.sequence);
    writer.writeUnsigned(static_cast<std::uint64_t>(payload.expiresAt));
    return writer.takeBytes();
}

std::optional<SessionCandidatesPayload> parseSessionCandidatesPayload(
    std::span<const std::uint8_t> signedPayload) {
    cbor::Reader reader(signedPayload);
    const auto arraySize = reader.readArraySize();
    const auto domain = reader.readText(kSessionSigningDomain.size());
    const auto type = reader.readText(kCandidatesType.size());
    const auto descriptionDigest = readFixedBytes<kSessionDigestSize>(reader);
    const auto iceGeneration = reader.readUnsigned();
    const auto candidateCount = reader.readArraySize();
    if (!arraySize || *arraySize != 7 || !domain || *domain != kSessionSigningDomain ||
        !type || *type != kCandidatesType || !descriptionDigest || !iceGeneration ||
        *iceGeneration > std::numeric_limits<std::uint32_t>::max() || !candidateCount ||
        *candidateCount == 0 || *candidateCount > kMaximumCandidateBatchSize) {
        return std::nullopt;
    }

    std::vector<SessionCandidate> candidates;
    candidates.reserve(*candidateCount);
    for (std::size_t index = 0; index < *candidateCount; ++index) {
        const auto entrySize = reader.readArraySize();
        auto mid = reader.readText(kMaximumMidBytes);
        const auto mline = reader.readUnsigned();
        auto ufrag = reader.readText(kMaximumUfragBytes);
        auto candidate = reader.readText(kMaximumCandidateBytes);
        if (!entrySize || *entrySize != 4 || !mid || !mline ||
            *mline > std::numeric_limits<std::uint32_t>::max() || !ufrag || !candidate) {
            return std::nullopt;
        }
        candidates.push_back({
            .sdpMid = std::move(*mid),
            .sdpMLineIndex = static_cast<std::uint32_t>(*mline),
            .usernameFragment = std::move(*ufrag),
            .candidate = std::move(*candidate),
        });
    }
    const auto sequence = reader.readUnsigned();
    const auto expiresAt = reader.readUnsigned();
    if (!sequence || *sequence == 0 || !expiresAt || *expiresAt == 0 ||
        *expiresAt > std::numeric_limits<std::int64_t>::max() || !reader.finished() ||
        !validCandidates(candidates)) {
        return std::nullopt;
    }
    return SessionCandidatesPayload{
        .descriptionDigest = *descriptionDigest,
        .iceGeneration = static_cast<std::uint32_t>(*iceGeneration),
        .candidates = std::move(candidates),
        .sequence = *sequence,
        .expiresAt = static_cast<std::int64_t>(*expiresAt),
    };
}

std::optional<std::vector<std::uint8_t>> encodeSessionGatheringCompletePayload(
    const SessionGatheringCompletePayload& payload) {
    if (payload.sequence == 0 || payload.expiresAt <= 0)
        return std::nullopt;
    cbor::Writer writer;
    writer.beginArray(6);
    writer.writeText(kSessionSigningDomain);
    writer.writeText(kGatheringCompleteType);
    writer.writeBytes(payload.descriptionDigest);
    writer.writeUnsigned(payload.iceGeneration);
    writer.writeUnsigned(payload.sequence);
    writer.writeUnsigned(static_cast<std::uint64_t>(payload.expiresAt));
    return writer.takeBytes();
}

std::optional<SessionGatheringCompletePayload> parseSessionGatheringCompletePayload(
    std::span<const std::uint8_t> signedPayload) {
    cbor::Reader reader(signedPayload);
    const auto arraySize = reader.readArraySize();
    const auto domain = reader.readText(kSessionSigningDomain.size());
    const auto type = reader.readText(kGatheringCompleteType.size());
    const auto descriptionDigest = readFixedBytes<kSessionDigestSize>(reader);
    const auto iceGeneration = reader.readUnsigned();
    const auto sequence = reader.readUnsigned();
    const auto expiresAt = reader.readUnsigned();
    if (!arraySize || *arraySize != 6 || !domain || *domain != kSessionSigningDomain ||
        !type || *type != kGatheringCompleteType || !descriptionDigest || !iceGeneration ||
        *iceGeneration > std::numeric_limits<std::uint32_t>::max() || !sequence ||
        *sequence == 0 || !expiresAt || *expiresAt == 0 ||
        *expiresAt > std::numeric_limits<std::int64_t>::max() || !reader.finished()) {
        return std::nullopt;
    }
    return SessionGatheringCompletePayload{
        .descriptionDigest = *descriptionDigest,
        .iceGeneration = static_cast<std::uint32_t>(*iceGeneration),
        .sequence = *sequence,
        .expiresAt = static_cast<std::int64_t>(*expiresAt),
    };
}

}  // namespace easydesk::protocol
