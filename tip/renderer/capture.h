#pragma once
#define NOMINMAX
#include <windows.h>
#include <d3d11.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

namespace renderer {
using Microsoft::WRL::ComPtr;
enum class CaptureFailure { none, unsupported_hdr, access_lost, device_removed, unavailable };
struct Capture {
    ComPtr<IDXGIOutputDuplication> duplication;
    ComPtr<ID3D11Texture2D> frame;
    UINT outstanding=0;
    CaptureFailure failure=CaptureFailure::none;
    bool initialize(ID3D11Device* device, IDXGIAdapter1* adapter, UINT outputIndex=0);
    bool acquire(ID3D11DeviceContext* context, UINT timeoutMs);
    void release();
    void shutdown();
};
}
