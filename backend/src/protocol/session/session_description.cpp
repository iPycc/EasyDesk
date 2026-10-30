#include "protocol/session/session_description.hpp"

#include "protocol/cbor/deterministic_cbor.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <limits>
#include <string_view>
#include <utility>

namespace easydesk::protocol {
namespace {

constexpr std::string_view kOfferType = "session.offer";
constexpr std::string_view kAnswerType = "session.answer";
constexpr std::string_view kFingerprintPrefix = "a=fingerprint:";
constexpr std::string_view kSha256Prefix = "a=fingerprint:sha-256 ";

template <std::size_t Size>
std::optional<std::array<std::uint8_t, Size>> readFixedBytes(cbor::Reader& reader) {
    auto bytes = reader.readBytes(Size);
    if (!bytes || bytes->size() != Size)
        return std::nullopt;
    std::array<std::uint8_t, Size> result{};
    std::copy(bytes->begin(), bytes->end(), result.begin());
    return result;
}

std::optional<std::uint8_t> hexByte(char high, char low) {
    const auto nibble = [](char value) -> std::optional<std::uint8_t> {
        if (value >= '0' && value <= '9')
            return static_cast<std::uint8_t>(value - '0');
        const char lowered = static_cast<char>(std::tolower(static_cast<unsigned char>(value)));
        if (lowered >= 'a' && lowered <= 'f')
            return static_cast<std::uint8_t>(lowered - 'a' + 10);
        return std::nullopt;
    };
    const auto highNibble = nibble(high);
    const auto lowNibble = nibble(low);
    if (!highNibble || !lowNibble)
        return std::nullopt;
    return static_cast<std::uint8_t>((*highNibble << 4U) | *lowNibble);
}

std::optional<SessionDigest> parseFingerprint(std::string_view text) {
    if (text.size() != 95)
        return std::nullopt;
    SessionDigest fingerprint{};
    for (std::size_t index = 0; index < fingerprint.size(); ++index) {
        const std::size_t offset = index * 3;
        const auto value = hexByte(text[offset], text[offset + 1]);
        if (!value || (index + 1 < fingerprint.size() && text[offset + 2] != ':'))
            return std::nullopt;
        fingerprint[index] = *value;
    }
    return fingerprint;
}

bool sdpBindsFingerprint(std::string_view sdp, const SessionDigest& expected) {
    if (sdp.empty() || sdp.size() > kMaximumSessionDescriptionBytes ||
        sdp.find('\0') != std::string_view::npos)
        return false;

    bool found = false;
    std::size_t offset = 0;
    while (offset <= sdp.size()) {
        const std::size_t end = sdp.find('\n', offset);
        std::string_view line = sdp.substr(
            offset, end == std::string_view::npos ? sdp.size() - offset : end - offset);
        if (!line.empty() && line.back() == '\r')
            line.remove_suffix(1);
        if (line.starts_with(kFingerprintPrefix)) {
            if (!line.starts_with(kSha256Prefix))
                return false;
            const auto fingerprint = parseFingerprint(line.substr(kSha256Prefix.size()));
            if (!fingerprint || *fingerprint != expected)
                return false;
            found = true;
        }
        if (end == std::string_view::npos)
            break;
        offset = end + 1;
    }
    return found;
}

SessionDigest descriptionDigest(std::string_view sdp) {
    return sessionPayloadDigest({reinterpret_cast<const std::uint8_t*>(sdp.data()), sdp.size()});
}

}  // namespace

std::optional<std::vector<std::uint8_t>> encodeSessionOfferPayload(
    const SessionOfferPayload& payload) {
    if (payload.sequence == 0 || !cbor::isValidUtf8(payload.sdp) ||
        !sdpBindsFingerprint(payload.sdp, payload.dtlsFingerprint))
        return std::nullopt;

    cbor::Writer writer;
    writer.beginArray(9);
    writer.writeText(kSessionSigningDomain);
    writer.writeText(kOfferType);
    writer.writeBytes(payload.acceptDigest);
    writer.writeUnsigned(payload.iceGeneration);
    writer.writeBytes(payload.nonce);
    writer.writeText(payload.sdp);
    writer.writeBytes(descriptionDigest(payload.sdp));
    writer.writeBytes(payload.dtlsFingerprint);
    writer.writeUnsigned(payload.sequence);
    return writer.takeBytes();
}

std::optional<SessionOfferPayload> parseSessionOfferPayload(
    std::span<const std::uint8_t> signedPayload) {
    cbor::Reader reader(signedPayload);
    const auto arraySize = reader.readArraySize();
    const auto domain = reader.readText(kSessionSigningDomain.size());
    const auto type = reader.readText(kOfferType.size());
    const auto acceptDigest = readFixedBytes<kSessionDigestSize>(reader);
    const auto iceGeneration = reader.readUnsigned();
    const auto nonce = readFixedBytes<kSessionNonceSize>(reader);
    const auto sdp = reader.readText(kMaximumSessionDescriptionBytes);
    const auto sdpDigest = readFixedBytes<kSessionDigestSize>(reader);
    const auto fingerprint = readFixedBytes<kSessionDigestSize>(reader);
    const auto sequence = reader.readUnsigned();
    if (!arraySize || *arraySize != 9 || !domain || *domain != kSessionSigningDomain ||
        !type || *type != kOfferType || !acceptDigest || !iceGeneration ||
        *iceGeneration > std::numeric_limits<std::uint32_t>::max() || !nonce || !sdp ||
        !sdpDigest || *sdpDigest != descriptionDigest(*sdp) || !fingerprint || !sequence ||
        *sequence == 0 || !reader.finished() || !sdpBindsFingerprint(*sdp, *fingerprint)) {
        return std::nullopt;
    }
    return SessionOfferPayload{
        .acceptDigest = *acceptDigest,
        .iceGeneration = static_cast<std::uint32_t>(*iceGeneration),
        .nonce = *nonce,
        .sdp = *sdp,
        .dtlsFingerprint = *fingerprint,
        .sequence = *sequence,
    };
}

std::optional<std::vector<std::uint8_t>> encodeSessionAnswerPayload(
    const SessionAnswerPayload& payload) {
    if (payload.sequence == 0 || !cbor::isValidUtf8(payload.sdp) ||
        !sdpBindsFingerprint(payload.sdp, payload.dtlsFingerprint))
        return std::nullopt;

    cbor::Writer writer;
    writer.beginArray(8);
    writer.writeText(kSessionSigningDomain);
    writer.writeText(kAnswerType);
    writer.writeBytes(payload.offerDigest);
    writer.writeUnsigned(payload.iceGeneration);
    writer.writeText(payload.sdp);
    writer.writeBytes(descriptionDigest(payload.sdp));
    writer.writeBytes(payload.dtlsFingerprint);
    writer.writeUnsigned(payload.sequence);
    return writer.takeBytes();
}

std::optional<SessionAnswerPayload> parseSessionAnswerPayload(
    std::span<const std::uint8_t> signedPayload) {
    cbor::Reader reader(signedPayload);
    const auto arraySize = reader.readArraySize();
    const auto domain = reader.readText(kSessionSigningDomain.size());
    const auto type = reader.readText(kAnswerType.size());
    const auto offerDigest = readFixedBytes<kSessionDigestSize>(reader);
    const auto iceGeneration = reader.readUnsigned();
    const auto sdp = reader.readText(kMaximumSessionDescriptionBytes);
    const auto sdpDigest = readFixedBytes<kSessionDigestSize>(reader);
    const auto fingerprint = readFixedBytes<kSessionDigestSize>(reader);
    const auto sequence = reader.readUnsigned();
    if (!arraySize || *arraySize != 8 || !domain || *domain != kSessionSigningDomain ||
        !type || *type != kAnswerType || !offerDigest || !iceGeneration ||
        *iceGeneration > std::numeric_limits<std::uint32_t>::max() || !sdp || !sdpDigest ||
        *sdpDigest != descriptionDigest(*sdp) || !fingerprint || !sequence || *sequence == 0 ||
        !reader.finished() || !sdpBindsFingerprint(*sdp, *fingerprint)) {
        return std::nullopt;
    }
    return SessionAnswerPayload{
        .offerDigest = *offerDigest,
        .iceGeneration = static_cast<std::uint32_t>(*iceGeneration),
        .sdp = *sdp,
        .dtlsFingerprint = *fingerprint,
        .sequence = *sequence,
    };
}

}  // namespace easydesk::protocol
