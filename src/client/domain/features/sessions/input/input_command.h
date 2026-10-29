#pragma once

#include <QByteArray>
#include <QString>

#include <variant>

enum class InputStream : quint8 {
    Reliable,
    Pointer,
};

enum class RemoteMouseButton : quint8 {
    Left,
    Right,
    Middle,
    X1,
    X2,
};

struct PointerMoveInput {
    quint32 normalizedX = 0;
    quint32 normalizedY = 0;
};

struct PointerButtonInput {
    RemoteMouseButton button = RemoteMouseButton::Left;
    bool pressed = false;
    quint32 normalizedX = 0;
    quint32 normalizedY = 0;
};

struct WheelInput {
    qint32 horizontalDelta = 0;
    qint32 verticalDelta = 0;
    quint32 normalizedX = 0;
    quint32 normalizedY = 0;
};

struct KeyInput {
    quint16 scanCode = 0;
    bool extended = false;
    bool pressed = false;
    bool repeat = false;
};

struct TextCommitInput {
    QString text;
};

struct ReleaseAllInput {};

using InputEvent = std::variant<PointerMoveInput, PointerButtonInput, WheelInput, KeyInput,
                                TextCommitInput, ReleaseAllInput>;

struct InputCommandContext {
    QString sessionId;
    quint64 sessionEpoch = 0;
    quint64 permissionEpoch = 0;
    quint64 sequence = 0;
    quint64 displayGeneration = 0;
    QByteArray leaseId;
    InputStream stream = InputStream::Reliable;
};

struct InputCommand {
    InputCommandContext context;
    InputEvent event;
};
