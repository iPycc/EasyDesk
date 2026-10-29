#include "media/capture/dxgi_desktop_capture.h"

#include <QVarLengthArray>

#include <iterator>
#include <limits>
#include <utility>

using Microsoft::WRL::ComPtr;

namespace {
class AcquiredFrame final {
  public:
    explicit AcquiredFrame(IDXGIOutputDuplication *duplication) : m_duplication(duplication) {}
    ~AcquiredFrame() {
        if (m_acquired)
            m_duplication->ReleaseFrame();
    }

    void markAcquired() {
        m_acquired = true;
    }

    HRESULT release() {
        if (!m_acquired)
            return S_OK;
        m_acquired = false;
        return m_duplication->ReleaseFrame();
    }

  private:
    IDXGIOutputDuplication *m_duplication = nullptr;
    bool m_acquired = false;
};

DesktopRotation mapRotation(DXGI_MODE_ROTATION rotation) {
    switch (rotation) {
    case DXGI_MODE_ROTATION_ROTATE90:
        return DesktopRotation::Rotate90;
    case DXGI_MODE_ROTATION_ROTATE180:
        return DesktopRotation::Rotate180;
    case DXGI_MODE_ROTATION_ROTATE270:
        return DesktopRotation::Rotate270;
    default:
        return DesktopRotation::Identity;
    }
}

DesktopPointerShapeType mapPointerShape(UINT type) {
    switch (type) {
    case DXGI_OUTDUPL_POINTER_SHAPE_TYPE_MONOCHROME:
        return DesktopPointerShapeType::Monochrome;
    case DXGI_OUTDUPL_POINTER_SHAPE_TYPE_COLOR:
        return DesktopPointerShapeType::Color;
    case DXGI_OUTDUPL_POINTER_SHAPE_TYPE_MASKED_COLOR:
        return DesktopPointerShapeType::MaskedColor;
    default:
        return DesktopPointerShapeType::Unknown;
    }
}

QRect toRect(const RECT &rect) {
    return QRect(rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top);
}
} // namespace

DxgiDesktopCapture::~DxgiDesktopCapture() {
    stop();
}

bool DxgiDesktopCapture::start(int displayIndex) {
    stop();
    m_ownerThread = std::this_thread::get_id();
    if (displayIndex < 0) {
        m_lastError = E_INVALIDARG;
        return false;
    }
    return openDisplay(displayIndex);
}

void DxgiDesktopCapture::stop() {
    m_duplication.Reset();
    m_context.Reset();
    m_device.Reset();
    m_pointerShape = {};
    m_display = {};
    m_ownerThread = {};
}

bool DxgiDesktopCapture::openDisplay(int displayIndex) {
    ComPtr<IDXGIFactory1> factory;
    m_lastError = CreateDXGIFactory1(IID_PPV_ARGS(&factory));
    if (FAILED(m_lastError))
        return false;

    int currentDisplay = 0;
    for (UINT adapterIndex = 0;; ++adapterIndex) {
        ComPtr<IDXGIAdapter1> adapter;
        const HRESULT adapterResult = factory->EnumAdapters1(adapterIndex, &adapter);
        if (adapterResult == DXGI_ERROR_NOT_FOUND)
            break;
        if (FAILED(adapterResult)) {
            m_lastError = adapterResult;
            return false;
        }

        for (UINT outputIndex = 0;; ++outputIndex) {
            ComPtr<IDXGIOutput> output;
            const HRESULT outputResult = adapter->EnumOutputs(outputIndex, &output);
            if (outputResult == DXGI_ERROR_NOT_FOUND)
                break;
            if (FAILED(outputResult)) {
                m_lastError = outputResult;
                return false;
            }

            DXGI_OUTPUT_DESC outputDescription{};
            m_lastError = output->GetDesc(&outputDescription);
            if (FAILED(m_lastError))
                return false;
            if (!outputDescription.AttachedToDesktop)
                continue;
            if (currentDisplay++ != displayIndex)
                continue;

            constexpr D3D_FEATURE_LEVEL requestedLevels[] = {
                D3D_FEATURE_LEVEL_11_1,
                D3D_FEATURE_LEVEL_11_0,
            };
            D3D_FEATURE_LEVEL selectedLevel{};
            m_lastError = D3D11CreateDevice(
                adapter.Get(), D3D_DRIVER_TYPE_UNKNOWN, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT,
                requestedLevels, static_cast<UINT>(std::size(requestedLevels)), D3D11_SDK_VERSION,
                &m_device, &selectedLevel, &m_context);
            if (FAILED(m_lastError))
                return false;

            ComPtr<IDXGIOutput1> output1;
            m_lastError = output.As(&output1);
            if (FAILED(m_lastError))
                return false;
            m_lastError = output1->DuplicateOutput(m_device.Get(), &m_duplication);
            if (FAILED(m_lastError))
                return false;

            DXGI_OUTDUPL_DESC duplicationDescription{};
            m_duplication->GetDesc(&duplicationDescription);
            m_display.index = displayIndex;
            m_display.deviceName = QString::fromWCharArray(outputDescription.DeviceName);
            m_display.desktopRect = toRect(outputDescription.DesktopCoordinates);
            m_display.textureSize = QSize(static_cast<int>(duplicationDescription.ModeDesc.Width),
                                          static_cast<int>(duplicationDescription.ModeDesc.Height));
            m_display.rotation = mapRotation(duplicationDescription.Rotation);
            m_display.generation = ++m_generation;
            m_lastError = S_OK;
            return true;
        }
    }

    m_lastError = DXGI_ERROR_NOT_FOUND;
    return false;
}

DesktopCaptureResult DxgiDesktopCapture::capture(unsigned int timeoutMilliseconds) {
    if (!m_duplication)
        return errorResult(E_HANDLE);
    if (m_ownerThread != std::this_thread::get_id()) {
        DesktopCaptureResult result;
        result.status = DesktopCaptureStatus::WrongThread;
        result.error = E_UNEXPECTED;
        m_lastError = result.error;
        return result;
    }

    DXGI_OUTDUPL_FRAME_INFO frameInfo{};
    ComPtr<IDXGIResource> desktopResource;
    const HRESULT acquireResult =
        m_duplication->AcquireNextFrame(timeoutMilliseconds, &frameInfo, &desktopResource);
    if (FAILED(acquireResult))
        return errorResult(acquireResult);

    AcquiredFrame acquired(m_duplication.Get());
    acquired.markAcquired();

    ComPtr<ID3D11Texture2D> sourceTexture;
    HRESULT result = desktopResource.As(&sourceTexture);
    if (FAILED(result))
        return errorResult(result);

    D3D11_TEXTURE2D_DESC textureDescription{};
    sourceTexture->GetDesc(&textureDescription);
    textureDescription.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    textureDescription.CPUAccessFlags = 0;
    textureDescription.MiscFlags = 0;
    textureDescription.Usage = D3D11_USAGE_DEFAULT;

    DesktopCaptureFrame frame;
    result = m_device->CreateTexture2D(&textureDescription, nullptr, &frame.texture);
    if (FAILED(result))
        return errorResult(result);
    m_context->CopyResource(frame.texture.Get(), sourceTexture.Get());

    result = readMoveRegions(frameInfo.TotalMetadataBufferSize, frame.moveRegions);
    if (FAILED(result))
        return errorResult(result);
    result = readDirtyRegions(frameInfo.TotalMetadataBufferSize, frame.dirtyRegions);
    if (FAILED(result))
        return errorResult(result);
    result = updatePointerShape(frameInfo);
    if (FAILED(result))
        return errorResult(result);

    frame.size = QSize(static_cast<int>(textureDescription.Width),
                       static_cast<int>(textureDescription.Height));
    frame.pointer.visible = frameInfo.PointerPosition.Visible != FALSE;
    frame.pointer.position = QPoint(frameInfo.PointerPosition.Position.x,
                                    frameInfo.PointerPosition.Position.y);
    frame.pointer.shape = m_pointerShape;
    frame.displayGeneration = m_display.generation;
    frame.accumulatedFrames = frameInfo.AccumulatedFrames;
    frame.presentationTimeQpc = frameInfo.LastPresentTime.QuadPart;

    const HRESULT releaseResult = acquired.release();
    if (FAILED(releaseResult))
        return errorResult(releaseResult);

    m_lastError = S_OK;
    DesktopCaptureResult captureResult;
    captureResult.status = DesktopCaptureStatus::Frame;
    captureResult.error = S_OK;
    captureResult.frame = std::move(frame);
    return captureResult;
}

HRESULT DxgiDesktopCapture::readMoveRegions(UINT metadataCapacity,
                                            QVector<DesktopMoveRegion> &regions) const {
    if (metadataCapacity == 0)
        return S_OK;
    if (metadataCapacity > static_cast<UINT>((std::numeric_limits<int>::max)()))
        return E_OUTOFMEMORY;

    const UINT capacityCount =
        (metadataCapacity + sizeof(DXGI_OUTDUPL_MOVE_RECT) - 1) /
        sizeof(DXGI_OUTDUPL_MOVE_RECT);
    QVarLengthArray<DXGI_OUTDUPL_MOVE_RECT, 32> nativeRegions(
        static_cast<int>(capacityCount));
    UINT requiredBytes = 0;
    const HRESULT result = m_duplication->GetFrameMoveRects(
        capacityCount * sizeof(DXGI_OUTDUPL_MOVE_RECT), nativeRegions.data(), &requiredBytes);
    if (FAILED(result))
        return result;

    const UINT count = requiredBytes / sizeof(DXGI_OUTDUPL_MOVE_RECT);
    regions.reserve(static_cast<int>(count));
    for (UINT index = 0; index < count; ++index) {
        const auto &region = nativeRegions[static_cast<int>(index)];
        regions.push_back({QPoint(region.SourcePoint.x, region.SourcePoint.y),
                           toRect(region.DestinationRect)});
    }
    return S_OK;
}

HRESULT DxgiDesktopCapture::readDirtyRegions(UINT metadataCapacity,
                                             QVector<QRect> &regions) const {
    if (metadataCapacity == 0)
        return S_OK;
    if (metadataCapacity > static_cast<UINT>((std::numeric_limits<int>::max)()))
        return E_OUTOFMEMORY;

    const UINT capacityCount = (metadataCapacity + sizeof(RECT) - 1) / sizeof(RECT);
    QVarLengthArray<RECT, 64> nativeRegions(static_cast<int>(capacityCount));
    UINT requiredBytes = 0;
    const HRESULT result = m_duplication->GetFrameDirtyRects(
        capacityCount * sizeof(RECT), nativeRegions.data(), &requiredBytes);
    if (FAILED(result))
        return result;

    const UINT count = requiredBytes / sizeof(RECT);
    regions.reserve(static_cast<int>(count));
    for (UINT index = 0; index < count; ++index)
        regions.push_back(toRect(nativeRegions[static_cast<int>(index)]));
    return S_OK;
}

HRESULT DxgiDesktopCapture::updatePointerShape(const DXGI_OUTDUPL_FRAME_INFO &frameInfo) {
    if (frameInfo.PointerShapeBufferSize == 0)
        return S_OK;
    if (frameInfo.PointerShapeBufferSize >
        static_cast<UINT>((std::numeric_limits<int>::max)()))
        return E_OUTOFMEMORY;

    QByteArray bytes(static_cast<int>(frameInfo.PointerShapeBufferSize), Qt::Uninitialized);
    UINT requiredBytes = 0;
    DXGI_OUTDUPL_POINTER_SHAPE_INFO shapeInfo{};
    const HRESULT result = m_duplication->GetFramePointerShape(
        frameInfo.PointerShapeBufferSize, bytes.data(), &requiredBytes, &shapeInfo);
    if (FAILED(result))
        return result;
    bytes.resize(static_cast<int>(requiredBytes));

    m_pointerShape.type = mapPointerShape(shapeInfo.Type);
    m_pointerShape.size =
        QSize(static_cast<int>(shapeInfo.Width), static_cast<int>(shapeInfo.Height));
    m_pointerShape.pitch = static_cast<int>(shapeInfo.Pitch);
    m_pointerShape.hotspot =
        QPoint(static_cast<int>(shapeInfo.HotSpot.x), static_cast<int>(shapeInfo.HotSpot.y));
    m_pointerShape.pixels = std::move(bytes);
    return S_OK;
}

DesktopCaptureResult DxgiDesktopCapture::errorResult(HRESULT error) {
    m_lastError = error;
    DesktopCaptureResult result;
    result.error = error;
    if (error == DXGI_ERROR_WAIT_TIMEOUT)
        result.status = DesktopCaptureStatus::Timeout;
    else if (error == DXGI_ERROR_ACCESS_LOST)
        result.status = DesktopCaptureStatus::AccessLost;
    else if (error == DXGI_ERROR_DEVICE_REMOVED || error == DXGI_ERROR_DEVICE_RESET)
        result.status = DesktopCaptureStatus::DeviceLost;
    else
        result.status = DesktopCaptureStatus::Failed;
    return result;
}

bool DxgiDesktopCapture::isStarted() const {
    return m_duplication != nullptr;
}

HRESULT DxgiDesktopCapture::lastError() const {
    return m_lastError;
}

DesktopDisplayDescriptor DxgiDesktopCapture::display() const {
    return m_display;
}

ID3D11Device *DxgiDesktopCapture::device() const {
    return m_device.Get();
}

ID3D11DeviceContext *DxgiDesktopCapture::context() const {
    return m_context.Get();
}
