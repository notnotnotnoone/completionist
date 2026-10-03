#pragma once
#include <d3d11.h>
#include <wrl/client.h>
#include <vector>

namespace renderer {
using Microsoft::WRL::ComPtr;
struct BlurMaterial {
    ComPtr<ID3D11VertexShader> vertex;
    ComPtr<ID3D11PixelShader> horizontal;
    ComPtr<ID3D11PixelShader> vertical;
    ComPtr<ID3D11SamplerState> sampler;
    ComPtr<ID3D11Buffer> weights;
    ComPtr<ID3D11Texture2D> scratchTexture;
    ComPtr<ID3D11RenderTargetView> scratchTarget;
    ComPtr<ID3D11ShaderResourceView> scratchView;
    ComPtr<ID3D11Texture2D> outputTexture;
    ComPtr<ID3D11RenderTargetView> outputTarget;
    ComPtr<ID3D11ShaderResourceView> outputView;
    UINT width=0, height=0;
    bool create(ID3D11Device* device, UINT w, UINT h);
    bool blur(ID3D11DeviceContext* context, ID3D11ShaderResourceView* input);
};
}
