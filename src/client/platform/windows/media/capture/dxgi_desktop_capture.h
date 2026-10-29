#pragma once

#include "media/capture/desktop_capture_frame.h"

#include <QString>

#include <dxgi1_2.h>

#include <thread>

struct DesktopDisplayDescriptor {
    int index = -1;
    QString deviceName;
    QRect desktopRect;
    QSize textureSize;
    DesktopRotation rotation = DesktopRotation::Identity;
    quint64 generation = 0;
};

enum class DesktopCaptureStatus {
    Frame,
    Timeout,
    AccessLost,
    DeviceLost,
    WrongThread,
    Failed,
};

struct DesktopCaptureResult {
    DesktopCaptureStatus status = DesktopCaptureStatus::Failed;
    HRESULT error = E_FAIL;
    DesktopCaptureFrame frame;
};

class DxgiDesktopCapture final {
  public:
    DxgiDesktopCapture() = default;
    ~DxgiDesktopCapture();

    DxgiDesktopCapture(const DxgiDesktopCapture &) = delete;
    DxgiDesktopCapture &operator=(const DxgiDesktopCapture &) = delete;

    bool start(int displayIndex);
    void stop();
    DesktopCaptureResult capture(unsigned int timeoutMilliseconds);

    bool isStarted() const;
    HRESULT lastError() const;
    DesktopDisplayDescriptor display() const;
    ID3D11Device *device() const;
    ID3D11DeviceContext *context() const;

  private:
    Microsoft::WRL::ComPtr<ID3D11Device> m_device;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> m_context;
    Microsoft::WRL::ComPtr<IDXGIOutputDuplication> m_duplication;
    DesktopDisplayDescriptor m_display;
    DesktopPointerShape m_pointerShape;
    std::thread::id m_ownerThread;
    quint64 m_generation = 0;
    HRESULT m_lastError = S_OK;

    bool openDisplay(int displayIndex);
    HRESULT readMoveRegions(UINT metadataCapacity, QVector<DesktopMoveRegion> &regions) const;
    HRESULT readDirtyRegions(UINT metadataCapacity, QVector<QRect> &regions) const;
    HRESULT updatePointerShape(const DXGI_OUTDUPL_FRAME_INFO &frameInfo);
    DesktopCaptureResult errorResult(HRESULT error);
};
