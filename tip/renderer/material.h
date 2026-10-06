#pragma once
#define NOMINMAX
#include <windows.h>
#include <d3d11.h>
#include <cstddef>
#include <wrl/client.h>
#include <vector>

namespace renderer {
using Microsoft::WRL::ComPtr;
struct BlurMaterial {
    ComPtr<ID3D11VertexShader> vertex;
    ComPtr<ID3D11PixelShader> horizontal;
    ComPtr<ID3D11PixelShader> vertical;
    ComPtr<ID3D11PixelShader> lensShader;
    ComPtr<ID3D11SamplerState> sampler;
    ComPtr<ID3D11BlendState> glassBlend;
    ComPtr<ID3D11RasterizerState> scissorRasterizer;
    ComPtr<ID3D11Buffer> weights;
    ComPtr<ID3D11Buffer> lensConstants;
    ComPtr<ID3D11Texture2D> scratchTexture;
    ComPtr<ID3D11RenderTargetView> scratchTarget;
    ComPtr<ID3D11ShaderResourceView> scratchView;
    ComPtr<ID3D11Texture2D> outputTexture;
    ComPtr<ID3D11RenderTargetView> outputTarget;
    ComPtr<ID3D11ShaderResourceView> outputView;
    ComPtr<ID3D11Texture2D> glassTexture;
    ComPtr<ID3D11RenderTargetView> glassTarget;
    ComPtr<ID3D11ShaderResourceView> glassView;
    ComPtr<ID3D11Texture2D> luminanceSamples;
    std::vector<RECT> horizontalRegions, verticalRegions;
    UINT width=0, height=0;
    float pointer[2]{-1,-1};
    bool create(ID3D11Device* device, UINT w, UINT h);
    void reset();
    bool blur(ID3D11DeviceContext* context, ID3D11ShaderResourceView* input, float dpi = 96.0f);
    bool blurRegions(ID3D11DeviceContext* context, ID3D11ShaderResourceView* input,
                     float dpi, const RECT* panelRegions, size_t panelCount);
    bool renderLens(ID3D11DeviceContext* context, const D3D11_VIEWPORT& viewport,
                    float radiusDip, float strength, float dpi, const float tint[4], bool clearTarget = false);
    bool backdropDark(ID3D11DeviceContext* context, const D3D11_VIEWPORT& viewport, bool previous);
};
}
