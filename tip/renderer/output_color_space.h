#pragma once

#define NOMINMAX
#include <dxgi1_6.h>
#include <wrl/client.h>

#include "live_policy.h"

namespace renderer {

constexpr bool IsSupportedSdr709ColorSpace(DXGI_COLOR_SPACE_TYPE colorSpace) {
    return colorSpace == DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709 ||
           colorSpace == DXGI_COLOR_SPACE_RGB_STUDIO_G22_NONE_P709;
}

inline OutputColorSpace QueryOutputColorSpace(IDXGIAdapter* adapter, UINT outputIndex) {
    if (!adapter) return OutputColorSpace::Unknown;
    Microsoft::WRL::ComPtr<IDXGIOutput> output;
    if (FAILED(adapter->EnumOutputs(outputIndex, &output))) return OutputColorSpace::Unknown;
    Microsoft::WRL::ComPtr<IDXGIOutput6> output6;
    if (FAILED(output.As(&output6))) return OutputColorSpace::Unknown;
    DXGI_OUTPUT_DESC1 description{};
    if (FAILED(output6->GetDesc1(&description))) return OutputColorSpace::Unknown;
    const bool supportedSdr709 = IsSupportedSdr709ColorSpace(description.ColorSpace);
    return ClassifyOutputColorSpace(true, supportedSdr709);
}

}  // namespace renderer
