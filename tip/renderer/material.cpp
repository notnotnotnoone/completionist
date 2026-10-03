#include "material.h"
#include "shaders.h"
#include "resource_lifetime.h"
#include <algorithm>
#include <cmath>

namespace renderer {
struct alignas(16) Constants { float direction[2]; float size[2]; float radius; float sigma; float reserved[2]; float weights[80]; };
struct alignas(16) LensConstants { float outputSize[2]; float panelSize[2]; float panelOrigin[2]; float cornerRadius; float displacement; float padding[2]; float reserved[2]; float tint[4]; };
void BlurMaterial::reset() {
    ResetResources(vertex,horizontal,vertical,lensShader,sampler,glassBlend,scissorRasterizer,weights,lensConstants,
        scratchTexture,scratchTarget,scratchView,outputTexture,outputTarget,outputView,glassTexture,glassTarget,glassView);
    horizontalRegions.clear(); verticalRegions.clear(); width=height=0;
}
bool BlurMaterial::create(ID3D11Device* d, UINT w, UINT h) {
    if(!d || !w || !h) return false;
    width=w; height=h;
    if(FAILED(d->CreateVertexShader(shaders_fullscreenVs,shaders::fullscreenVsSize,nullptr,&vertex)) ||
       FAILED(d->CreatePixelShader(shaders_blurPs,shaders::blurPsSize,nullptr,&horizontal)) ||
       FAILED(d->CreatePixelShader(shaders_lensPs,shaders::lensPsSize,nullptr,&lensShader))) return false;
    vertical=horizontal;
    D3D11_SAMPLER_DESC sd{}; sd.Filter=D3D11_FILTER_MIN_MAG_MIP_LINEAR; sd.AddressU=sd.AddressV=sd.AddressW=D3D11_TEXTURE_ADDRESS_CLAMP; sd.MaxLOD=D3D11_FLOAT32_MAX;
    if(FAILED(d->CreateSamplerState(&sd,&sampler))) return false;
    D3D11_BLEND_DESC blend{};
    blend.RenderTarget[0].BlendEnable=TRUE;
    blend.RenderTarget[0].SrcBlend=D3D11_BLEND_ONE;
    blend.RenderTarget[0].DestBlend=D3D11_BLEND_INV_SRC_ALPHA;
    blend.RenderTarget[0].BlendOp=D3D11_BLEND_OP_ADD;
    blend.RenderTarget[0].SrcBlendAlpha=D3D11_BLEND_ONE;
    blend.RenderTarget[0].DestBlendAlpha=D3D11_BLEND_INV_SRC_ALPHA;
    blend.RenderTarget[0].BlendOpAlpha=D3D11_BLEND_OP_ADD;
    blend.RenderTarget[0].RenderTargetWriteMask=D3D11_COLOR_WRITE_ENABLE_ALL;
    if(FAILED(d->CreateBlendState(&blend,&glassBlend))) return false;
    D3D11_RASTERIZER_DESC raster{}; raster.FillMode=D3D11_FILL_SOLID; raster.CullMode=D3D11_CULL_NONE;
    raster.DepthClipEnable=TRUE; raster.ScissorEnable=TRUE;
    if(FAILED(d->CreateRasterizerState(&raster,&scissorRasterizer))) return false;
    D3D11_BUFFER_DESC bd{}; bd.ByteWidth=sizeof(Constants); bd.Usage=D3D11_USAGE_DYNAMIC; bd.BindFlags=D3D11_BIND_CONSTANT_BUFFER; bd.CPUAccessFlags=D3D11_CPU_ACCESS_WRITE;
    if(FAILED(d->CreateBuffer(&bd,nullptr,&this->weights))) return false;
    bd.ByteWidth=sizeof(LensConstants);
    if(FAILED(d->CreateBuffer(&bd,nullptr,&lensConstants))) return false;
    auto make=[&](ComPtr<ID3D11Texture2D>& texture,ComPtr<ID3D11RenderTargetView>& target,ComPtr<ID3D11ShaderResourceView>& view){
        D3D11_TEXTURE2D_DESC td{};td.Width=w;td.Height=h;td.MipLevels=1;td.ArraySize=1;td.Format=DXGI_FORMAT_B8G8R8A8_UNORM;td.SampleDesc.Count=1;td.Usage=D3D11_USAGE_DEFAULT;td.BindFlags=D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE;
        return SUCCEEDED(d->CreateTexture2D(&td,nullptr,&texture))&&SUCCEEDED(d->CreateRenderTargetView(texture.Get(),nullptr,&target))&&SUCCEEDED(d->CreateShaderResourceView(texture.Get(),nullptr,&view));};
    return make(scratchTexture,scratchTarget,scratchView)&&make(outputTexture,outputTarget,outputView)&&
        make(glassTexture,glassTarget,glassView);
}
bool BlurMaterial::blur(ID3D11DeviceContext* c, ID3D11ShaderResourceView* input, float dpi) {
    return blurRegions(c,input,dpi,nullptr,0);
}

bool BlurMaterial::blurRegions(ID3D11DeviceContext* c,ID3D11ShaderResourceView* input,
                               float dpi,const RECT* panels,size_t panelCount) {
    if(!c||!input||!width||!height||dpi<48.0f||dpi>192.0f) return false;
    if(panelCount>0 && !panels) return false;
    const float scale=dpi/96.0f;
    const int radius=static_cast<int>(std::lround(18.0f*scale));
    const int displacement=static_cast<int>(std::ceil(18.0f*scale));
    const float sigma=6.0f*scale;
    float kernel[73]{};
    float sum=0;
    for(int i=-36;i<=36;++i){float x=static_cast<float>(i);float value=std::exp(-(x*x)/(2.0f*sigma*sigma));kernel[i+36]=value;sum+=value;}
    for(float& value:kernel)value/=sum;
    D3D11_MAPPED_SUBRESOURCE map{}; if(FAILED(c->Map(weights.Get(),0,D3D11_MAP_WRITE_DISCARD,0,&map))) return false;
    auto* constants=static_cast<Constants*>(map.pData); *constants={}; constants->direction[0]=1.f;constants->direction[1]=0.f;constants->size[0]=static_cast<float>(width);constants->size[1]=static_cast<float>(height);constants->radius=static_cast<float>(radius);constants->sigma=sigma;
    for(int i=-36;i<=36;++i)constants->weights[i+36]=kernel[i+36];
    c->Unmap(weights.Get(),0);
    D3D11_VIEWPORT viewport{0,0,static_cast<float>(width),static_cast<float>(height),0,1};
    c->IASetInputLayout(nullptr); c->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    c->VSSetShader(vertex.Get(),nullptr,0); c->PSSetShader(horizontal.Get(),nullptr,0);
    c->PSSetSamplers(0,1,sampler.GetAddressOf()); c->PSSetConstantBuffers(0,1,weights.GetAddressOf());
    c->RSSetViewports(1,&viewport); c->RSSetState(scissorRasterizer.Get());
    ID3D11RenderTargetView* target=scratchTarget.Get(); c->OMSetRenderTargets(1,&target,nullptr);
    c->PSSetShaderResources(0,1,&input);
    horizontalRegions.clear(); verticalRegions.clear();
    if(panelCount==0) horizontalRegions.push_back({0,0,static_cast<LONG>(width),static_cast<LONG>(height)});
    for(size_t index=0;index<panelCount;++index) {
        const auto& panel=panels[index];
        auto expand=[&](int pad) { return RECT{(std::max)(0L,panel.left-pad),(std::max)(0L,panel.top-pad),
            (std::min)(static_cast<LONG>(width),panel.right+pad),(std::min)(static_cast<LONG>(height),panel.bottom+pad)}; };
        horizontalRegions.push_back(expand(radius+displacement));
        verticalRegions.push_back(expand(displacement));
    }
    auto drawRegions=[&](const std::vector<RECT>& regions) {
        for(const RECT& region:regions) {
            if(region.right<=region.left || region.bottom<=region.top) continue;
            D3D11_RECT scissor{region.left,region.top,region.right,region.bottom};
            c->RSSetScissorRects(1,&scissor); c->Draw(3,0);
        }
    };
    drawRegions(horizontalRegions);
    ID3D11ShaderResourceView* nullView=nullptr; c->PSSetShaderResources(0,1,&nullView);
    if(FAILED(c->Map(weights.Get(),0,D3D11_MAP_WRITE_DISCARD,0,&map))) { c->OMSetRenderTargets(0,nullptr,nullptr); c->RSSetState(nullptr); return false; }
    constants=static_cast<Constants*>(map.pData); *constants={}; constants->direction[1]=1; constants->size[0]=static_cast<float>(width); constants->size[1]=static_cast<float>(height);
    constants->radius=static_cast<float>(radius); constants->sigma=sigma;
    for(int i=-36;i<=36;++i) constants->weights[i+36]=kernel[i+36];
    c->Unmap(weights.Get(),0); c->PSSetShader(vertical.Get(),nullptr,0);
    target=outputTarget.Get(); c->OMSetRenderTargets(1,&target,nullptr);
    ID3D11ShaderResourceView* source=scratchView.Get(); c->PSSetShaderResources(0,1,&source);
    drawRegions(panelCount==0?horizontalRegions:verticalRegions);
    c->PSSetShaderResources(0,1,&nullView); c->OMSetRenderTargets(0,nullptr,nullptr); c->RSSetState(nullptr);
    return true;
}

bool BlurMaterial::renderLens(ID3D11DeviceContext* c, const D3D11_VIEWPORT& vp,
                              float radiusDip, float strength, float dpi, const float tint[4], bool clearTarget) {
    if (!c || !outputView || !glassTarget || !lensShader || !lensConstants || !tint ||
        vp.Width <= 0 || vp.Height <= 0 || dpi < 48 || dpi > 768) return false;
    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (FAILED(c->Map(lensConstants.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) return false;
    auto* values = static_cast<LensConstants*>(mapped.pData);
    *values = {};
    values->outputSize[0] = static_cast<float>(width);
    values->outputSize[1] = static_cast<float>(height);
    values->panelSize[0] = vp.Width;
    values->panelSize[1] = vp.Height;
    values->panelOrigin[0] = vp.TopLeftX;
    values->panelOrigin[1] = vp.TopLeftY;
    const float scale = dpi / 96.0f;
    values->cornerRadius = (std::min)(radiusDip * scale, (std::min)(vp.Width, vp.Height) * 0.5f);
    values->displacement = std::clamp(strength * scale, 0.0f, 48.0f * scale);
    for (unsigned i = 0; i < 4; ++i) values->tint[i] = std::clamp(tint[i], 0.0f, 1.0f);
    c->Unmap(lensConstants.Get(), 0);

    D3D11_VIEWPORT full{0, 0, static_cast<float>(width), static_cast<float>(height), 0, 1};
    c->RSSetViewports(1, &full);
    c->RSSetState(scissorRasterizer.Get());
    D3D11_RECT scissor{static_cast<LONG>(vp.TopLeftX),static_cast<LONG>(vp.TopLeftY),
        static_cast<LONG>(vp.TopLeftX+vp.Width),static_cast<LONG>(vp.TopLeftY+vp.Height)};
    c->RSSetScissorRects(1,&scissor);
    c->IASetInputLayout(nullptr);
    c->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    c->VSSetShader(vertex.Get(), nullptr, 0);
    c->PSSetShader(lensShader.Get(), nullptr, 0);
    c->PSSetSamplers(0, 1, sampler.GetAddressOf());
    c->PSSetConstantBuffers(1, 1, lensConstants.GetAddressOf());
    ID3D11RenderTargetView* target = glassTarget.Get();
    c->OMSetRenderTargets(1, &target, nullptr);
    const float blendFactor[4]{};
    c->OMSetBlendState(glassBlend.Get(), blendFactor, 0xffffffffU);
    if (clearTarget) { const float clear[4]{}; c->ClearRenderTargetView(target, clear); }
    ID3D11ShaderResourceView* source = outputView.Get();
    c->PSSetShaderResources(0, 1, &source);
    c->Draw(3, 0);
    ID3D11ShaderResourceView* nullView = nullptr;
    c->PSSetShaderResources(0, 1, &nullView);
    c->OMSetBlendState(nullptr, nullptr, 0xffffffffU);
    c->OMSetRenderTargets(0, nullptr, nullptr);
    c->RSSetState(nullptr);
    return true;
}
}
