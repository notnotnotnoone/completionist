#pragma once
#define NOMINMAX
#include <windows.h>
#include <d3d11.h>
#include <dxgi1_6.h>
#include <vector>
#include <wrl/client.h>

namespace renderer {
using Microsoft::WRL::ComPtr;
enum class CaptureFailure { none, unsupported_hdr, access_lost, device_removed, unavailable };
struct Capture {
    ComPtr<IDXGIOutputDuplication> duplication;
    ComPtr<ID3D11Texture2D> frame;
    ComPtr<ID3D11Texture2D> moveScratch;
    RECT desktop{};
    DXGI_MODE_ROTATION rotation=DXGI_MODE_ROTATION_UNSPECIFIED;
    UINT outputWidth=0, outputHeight=0;
    DXGI_FORMAT format=DXGI_FORMAT_UNKNOWN;
    bool hasFrame=false;
    bool frameUpdated=false;
    std::vector<RECT> changedRects;
    std::vector<unsigned char> metadataBuffer;
    std::vector<DXGI_OUTDUPL_MOVE_RECT> moveRects;
    std::vector<RECT> dirtyRects;
    UINT outstanding=0;
    bool deviceRecreationUsed=false;
    CaptureFailure failure=CaptureFailure::none;
    bool initialize(ID3D11Device* device, IDXGIAdapter1* adapter, UINT outputIndex=0);
    bool acquire(ID3D11DeviceContext* context, UINT timeoutMs);
    void invalidate(CaptureFailure reason);
    bool permitDeviceRecreation();
    void recordMoveUpdate(const DXGI_OUTDUPL_MOVE_RECT& move);
    void recordDirtyUpdate(const RECT& dirty);
    bool panelCrop(const RECT& desktopPanel, UINT paddingPixels, RECT* outputCrop) const;
    bool updatesPanel(const RECT& desktopPanel, UINT paddingPixels) const;
    static bool intersectingUpdate(const RECT& left, const RECT& right);
    void release();
    void shutdown();
};
}
