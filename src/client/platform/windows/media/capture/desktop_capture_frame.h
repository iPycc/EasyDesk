#pragma once

#include <QByteArray>
#include <QPoint>
#include <QRect>
#include <QSize>
#include <QVector>

#include <d3d11.h>
#include <wrl/client.h>

enum class DesktopRotation {
    Identity,
    Rotate90,
    Rotate180,
    Rotate270,
};

enum class DesktopPointerShapeType {
    Unknown,
    Monochrome,
    Color,
    MaskedColor,
};

struct DesktopPointerShape {
    DesktopPointerShapeType type = DesktopPointerShapeType::Unknown;
    QSize size;
    int pitch = 0;
    QPoint hotspot;
    QByteArray pixels;
};

struct DesktopPointerFrame {
    bool visible = false;
    QPoint position;
    DesktopPointerShape shape;
};

struct DesktopMoveRegion {
    QPoint source;
    QRect destination;
};

struct DesktopCaptureFrame {
    Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
    QSize size;
    QVector<QRect> dirtyRegions;
    QVector<DesktopMoveRegion> moveRegions;
    DesktopPointerFrame pointer;
    quint64 displayGeneration = 0;
    quint32 accumulatedFrames = 0;
    qint64 presentationTimeQpc = 0;
};
