#include "video/render/d3d11_video_renderer.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <iterator>
#include <utility>

#include <d3d11.h>
#include <d3dcompiler.h>
#include <dxgi.h>
#include <wrl/client.h>

#include "third_party/libyuv/include/libyuv/convert_argb.h"

namespace easydesk::media {
namespace {

using Microsoft::WRL::ComPtr;

constexpr auto kStartupTimeout = std::chrono::seconds(5);

constexpr char kVertexShader[] = R"(
struct VertexOutput {
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
};

VertexOutput main(uint vertexId : SV_VertexID) {
    float2 positions[3] = {
        float2(-1.0, -1.0),
        float2(-1.0,  3.0),
        float2( 3.0, -1.0)
    };
    float2 coordinates[3] = {
        float2(0.0,  1.0),
        float2(0.0, -1.0),
        float2(2.0,  1.0)
    };
    VertexOutput output;
    output.position = float4(positions[vertexId], 0.0, 1.0);
    output.uv = coordinates[vertexId];
    return output;
}
)";

constexpr char kPixelShader[] = R"(
Texture2D desktopTexture : register(t0);
SamplerState desktopSampler : register(s0);

float4 main(float4 position : SV_POSITION, float2 uv : TEXCOORD0) : SV_TARGET {
    return desktopTexture.Sample(desktopSampler, uv);
}
)";

bool compileShader(const char *source,
                   const char *profile,
                   ComPtr<ID3DBlob> &bytecode,
                   std::string &error) {
    ComPtr<ID3DBlob> compilerError;
    const HRESULT result = D3DCompile(
        source, std::strlen(source), nullptr, nullptr, nullptr, "main", profile,
        D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &bytecode, &compilerError);
    if (SUCCEEDED(result))
        return true;
    if (compilerError && compilerError->GetBufferPointer()) {
        error.assign(static_cast<const char *>(compilerError->GetBufferPointer()),
                     compilerError->GetBufferSize());
    } else {
        error = "D3D shader compilation failed.";
    }
    return false;
}

}  // namespace

struct D3d11VideoRenderer::RenderResources {
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    ComPtr<IDXGISwapChain> swapChain;
    ComPtr<ID3D11RenderTargetView> renderTarget;
    ComPtr<ID3D11VertexShader> vertexShader;
    ComPtr<ID3D11PixelShader> pixelShader;
    ComPtr<ID3D11SamplerState> sampler;
    ComPtr<ID3D11Texture2D> sourceTexture;
    ComPtr<ID3D11ShaderResourceView> sourceView;
    int sourceWidth = 0;
    int sourceHeight = 0;
    int targetWidth = 0;
    int targetHeight = 0;
};

D3d11VideoRenderer::D3d11VideoRenderer(HWND window) : m_window(window) {}

D3d11VideoRenderer::~D3d11VideoRenderer() {
    stop();
}

bool D3d11VideoRenderer::start(std::string *error) {
    std::unique_lock<std::mutex> lock(m_mutex);
    if (m_started || m_renderThread.joinable()) {
        if (error != nullptr)
            *error = "Video renderer is already running.";
        return false;
    }
    if (m_window == nullptr || !IsWindow(m_window)) {
        if (error != nullptr)
            *error = "Video renderer requires a valid window.";
        return false;
    }

    m_statistics = {};
    m_error.clear();
    m_ready = false;
    m_failed = false;
    m_stopRequested = false;
    m_started = true;
    m_renderThread = std::thread(&D3d11VideoRenderer::renderLoop, this);
    const bool initialized = m_changed.wait_for(lock, kStartupTimeout, [this] {
        return m_ready || m_failed;
    });
    if (initialized && m_ready)
        return true;
    if (!initialized)
        m_error = "Video renderer initialization timed out.";
    if (error != nullptr)
        *error = m_error.empty() ? "Video renderer initialization failed." : m_error;
    lock.unlock();
    stop();
    return false;
}

void D3d11VideoRenderer::stop() {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_stopRequested = true;
        m_pendingFrame.reset();
    }
    m_changed.notify_all();
    if (m_renderThread.joinable())
        m_renderThread.join();

    std::lock_guard<std::mutex> lock(m_mutex);
    m_resources.reset();
    m_started = false;
    m_ready = false;
}

bool D3d11VideoRenderer::isReady() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_ready && !m_failed;
}

std::string D3d11VideoRenderer::errorString() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_error;
}

D3d11VideoRenderer::Statistics D3d11VideoRenderer::statistics() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_statistics;
}

void D3d11VideoRenderer::OnFrame(const webrtc::VideoFrame &frame) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_started || m_stopRequested || m_failed)
        return;
    ++m_statistics.receivedFrames;
    if (m_pendingFrame.has_value())
        ++m_statistics.droppedFrames;
    m_pendingFrame = frame;
    m_changed.notify_all();
}

void D3d11VideoRenderer::renderLoop() {
    std::string error;
    if (!initialize(error)) {
        fail(std::move(error));
        return;
    }
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_ready = true;
    }
    m_changed.notify_all();

    while (true) {
        std::optional<webrtc::VideoFrame> frame;
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_changed.wait(lock, [this] {
                return m_stopRequested || m_pendingFrame.has_value();
            });
            if (m_stopRequested)
                break;
            frame = std::move(m_pendingFrame);
            m_pendingFrame.reset();
        }

        if (!renderFrame(*frame, error)) {
            fail(std::move(error));
            break;
        }
        std::lock_guard<std::mutex> lock(m_mutex);
        ++m_statistics.renderedFrames;
        m_statistics.sourceWidth = frame->width();
        m_statistics.sourceHeight = frame->height();
    }
}

bool D3d11VideoRenderer::initialize(std::string &error) {
    auto resources = std::make_unique<RenderResources>();
    RECT client{};
    if (!GetClientRect(m_window, &client)) {
        error = "Cannot read the video window dimensions.";
        return false;
    }

    DXGI_SWAP_CHAIN_DESC swap{};
    swap.BufferDesc.Width = std::max<LONG>(1, client.right - client.left);
    swap.BufferDesc.Height = std::max<LONG>(1, client.bottom - client.top);
    swap.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    swap.SampleDesc.Count = 1;
    swap.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swap.BufferCount = 2;
    swap.OutputWindow = m_window;
    swap.Windowed = TRUE;
    swap.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    constexpr D3D_FEATURE_LEVEL featureLevels[] = {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
    };
    D3D_FEATURE_LEVEL selected{};
    HRESULT result = D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
        D3D11_CREATE_DEVICE_BGRA_SUPPORT, featureLevels,
        static_cast<UINT>(std::size(featureLevels)), D3D11_SDK_VERSION, &swap,
        &resources->swapChain, &resources->device, &selected,
        &resources->context);
    if (FAILED(result)) {
        result = D3D11CreateDeviceAndSwapChain(
            nullptr, D3D_DRIVER_TYPE_WARP, nullptr,
            D3D11_CREATE_DEVICE_BGRA_SUPPORT, featureLevels,
            static_cast<UINT>(std::size(featureLevels)), D3D11_SDK_VERSION,
            &swap, &resources->swapChain, &resources->device, &selected,
            &resources->context);
    }
    if (FAILED(result)) {
        error = "D3D11 video device initialization failed.";
        return false;
    }

    ComPtr<ID3DBlob> vertexBytecode;
    ComPtr<ID3DBlob> pixelBytecode;
    if (!compileShader(kVertexShader, "vs_5_0", vertexBytecode, error) ||
        !compileShader(kPixelShader, "ps_5_0", pixelBytecode, error)) {
        return false;
    }
    result = resources->device->CreateVertexShader(
        vertexBytecode->GetBufferPointer(), vertexBytecode->GetBufferSize(),
        nullptr, &resources->vertexShader);
    if (FAILED(result)) {
        error = "D3D11 vertex shader creation failed.";
        return false;
    }
    result = resources->device->CreatePixelShader(
        pixelBytecode->GetBufferPointer(), pixelBytecode->GetBufferSize(),
        nullptr, &resources->pixelShader);
    if (FAILED(result)) {
        error = "D3D11 pixel shader creation failed.";
        return false;
    }

    D3D11_SAMPLER_DESC sampler{};
    sampler.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sampler.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampler.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampler.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampler.MaxLOD = D3D11_FLOAT32_MAX;
    result = resources->device->CreateSamplerState(&sampler, &resources->sampler);
    if (FAILED(result)) {
        error = "D3D11 sampler creation failed.";
        return false;
    }

    m_resources = std::move(resources);
    return true;
}

bool D3d11VideoRenderer::renderFrame(const webrtc::VideoFrame &frame,
                                     std::string &error) {
    if (!m_resources || !IsWindow(m_window)) {
        error = "The video rendering window is no longer available.";
        return false;
    }
    const auto i420 = frame.video_frame_buffer()->ToI420();
    if (!i420) {
        error = "The decoded video frame cannot be converted to I420.";
        return false;
    }

    RenderResources &resources = *m_resources;
    const int width = i420->width();
    const int height = i420->height();
    if (width <= 0 || height <= 0) {
        error = "The decoded video frame has invalid dimensions.";
        return false;
    }

    if (!resources.sourceTexture || resources.sourceWidth != width ||
        resources.sourceHeight != height) {
        resources.sourceView.Reset();
        resources.sourceTexture.Reset();
        D3D11_TEXTURE2D_DESC texture{};
        texture.Width = static_cast<UINT>(width);
        texture.Height = static_cast<UINT>(height);
        texture.MipLevels = 1;
        texture.ArraySize = 1;
        texture.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        texture.SampleDesc.Count = 1;
        texture.Usage = D3D11_USAGE_DYNAMIC;
        texture.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        texture.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        HRESULT result = resources.device->CreateTexture2D(
            &texture, nullptr, &resources.sourceTexture);
        if (FAILED(result)) {
            error = "D3D11 video texture creation failed.";
            return false;
        }
        result = resources.device->CreateShaderResourceView(
            resources.sourceTexture.Get(), nullptr, &resources.sourceView);
        if (FAILED(result)) {
            error = "D3D11 video texture view creation failed.";
            return false;
        }
        resources.sourceWidth = width;
        resources.sourceHeight = height;
    }

    D3D11_MAPPED_SUBRESOURCE mapped{};
    HRESULT result = resources.context->Map(resources.sourceTexture.Get(), 0,
                                            D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    if (FAILED(result)) {
        error = "D3D11 video texture mapping failed.";
        return false;
    }
    const int conversion = libyuv::I420ToARGB(
        i420->DataY(), i420->StrideY(), i420->DataU(), i420->StrideU(),
        i420->DataV(), i420->StrideV(), static_cast<uint8_t *>(mapped.pData),
        static_cast<int>(mapped.RowPitch), width, height);
    resources.context->Unmap(resources.sourceTexture.Get(), 0);
    if (conversion != 0) {
        error = "I420 video frame conversion failed.";
        return false;
    }

    RECT client{};
    if (!GetClientRect(m_window, &client)) {
        error = "Cannot read the video window dimensions.";
        return false;
    }
    const int targetWidth = client.right - client.left;
    const int targetHeight = client.bottom - client.top;
    if (targetWidth <= 0 || targetHeight <= 0)
        return true;

    if (!resources.renderTarget || resources.targetWidth != targetWidth ||
        resources.targetHeight != targetHeight) {
        resources.context->OMSetRenderTargets(0, nullptr, nullptr);
        resources.renderTarget.Reset();
        result = resources.swapChain->ResizeBuffers(
            0, static_cast<UINT>(targetWidth), static_cast<UINT>(targetHeight),
            DXGI_FORMAT_UNKNOWN, 0);
        if (FAILED(result)) {
            error = "D3D11 video swap chain resize failed.";
            return false;
        }
        ComPtr<ID3D11Texture2D> backBuffer;
        result = resources.swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer));
        if (FAILED(result) || !backBuffer) {
            error = "D3D11 video back buffer is unavailable.";
            return false;
        }
        result = resources.device->CreateRenderTargetView(
            backBuffer.Get(), nullptr, &resources.renderTarget);
        if (FAILED(result)) {
            error = "D3D11 video render target creation failed.";
            return false;
        }
        resources.targetWidth = targetWidth;
        resources.targetHeight = targetHeight;
    }

    const float sourceAspect = static_cast<float>(width) / height;
    const float targetAspect = static_cast<float>(targetWidth) / targetHeight;
    D3D11_VIEWPORT viewport{};
    if (sourceAspect > targetAspect) {
        viewport.Width = static_cast<float>(targetWidth);
        viewport.Height = viewport.Width / sourceAspect;
        viewport.TopLeftY = (targetHeight - viewport.Height) * 0.5f;
    } else {
        viewport.Height = static_cast<float>(targetHeight);
        viewport.Width = viewport.Height * sourceAspect;
        viewport.TopLeftX = (targetWidth - viewport.Width) * 0.5f;
    }
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;

    constexpr float clearColor[] = {0.0f, 0.0f, 0.0f, 1.0f};
    ID3D11RenderTargetView *target = resources.renderTarget.Get();
    ID3D11ShaderResourceView *sourceView = resources.sourceView.Get();
    ID3D11SamplerState *sampler = resources.sampler.Get();
    resources.context->OMSetRenderTargets(1, &target, nullptr);
    resources.context->ClearRenderTargetView(target, clearColor);
    resources.context->RSSetViewports(1, &viewport);
    resources.context->IASetInputLayout(nullptr);
    resources.context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    resources.context->VSSetShader(resources.vertexShader.Get(), nullptr, 0);
    resources.context->PSSetShader(resources.pixelShader.Get(), nullptr, 0);
    resources.context->PSSetShaderResources(0, 1, &sourceView);
    resources.context->PSSetSamplers(0, 1, &sampler);
    resources.context->Draw(3, 0);
    sourceView = nullptr;
    resources.context->PSSetShaderResources(0, 1, &sourceView);

    result = resources.swapChain->Present(0, 0);
    if (result == DXGI_STATUS_OCCLUDED)
        return true;
    if (FAILED(result)) {
        error = "D3D11 video presentation failed.";
        return false;
    }
    return true;
}

void D3d11VideoRenderer::fail(std::string error) {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_error = std::move(error);
        m_failed = true;
        m_ready = false;
        m_stopRequested = true;
        m_pendingFrame.reset();
    }
    m_changed.notify_all();
}

}  // namespace easydesk::media
