#include "material.h"
#include "shaders.h"
#include <array>
#include <cmath>

namespace renderer {
struct alignas(16) Constants { float direction[2]; float size[2]; float weights[40]; };
bool BlurMaterial::create(ID3D11Device* d, UINT w, UINT h) {
    if(!d || !w || !h) return false;
    width=w; height=h;
    if(FAILED(d->CreateVertexShader(shaders_fullscreenVs,shaders::fullscreenVsSize,nullptr,&vertex)) ||
       FAILED(d->CreatePixelShader(shaders_blurPs,shaders::blurPsSize,nullptr,&horizontal))) return false;
    vertical=horizontal;
    D3D11_SAMPLER_DESC sd{}; sd.Filter=D3D11_FILTER_MIN_MAG_MIP_LINEAR; sd.AddressU=sd.AddressV=sd.AddressW=D3D11_TEXTURE_ADDRESS_CLAMP; sd.MaxLOD=D3D11_FLOAT32_MAX;
    if(FAILED(d->CreateSamplerState(&sd,&sampler))) return false;
    std::array<float,37> gaussianWeights{}; float total=0;
    for(int i=-18;i<=18;++i){ auto x=static_cast<float>(i); gaussianWeights[static_cast<size_t>(i+18)]=std::exp(-(x*x)/(2.f*36.f)); total+=gaussianWeights[static_cast<size_t>(i+18)]; }
    for(auto& x:gaussianWeights) x/=total;
    D3D11_BUFFER_DESC bd{}; bd.ByteWidth=sizeof(Constants); bd.Usage=D3D11_USAGE_DYNAMIC; bd.BindFlags=D3D11_BIND_CONSTANT_BUFFER; bd.CPUAccessFlags=D3D11_CPU_ACCESS_WRITE;
    if(FAILED(d->CreateBuffer(&bd,nullptr,&this->weights))) return false;
    auto make=[&](ComPtr<ID3D11Texture2D>& texture,ComPtr<ID3D11RenderTargetView>& target,ComPtr<ID3D11ShaderResourceView>& view){
        D3D11_TEXTURE2D_DESC td{};td.Width=w;td.Height=h;td.MipLevels=1;td.ArraySize=1;td.Format=DXGI_FORMAT_B8G8R8A8_UNORM;td.SampleDesc.Count=1;td.Usage=D3D11_USAGE_DEFAULT;td.BindFlags=D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE;
        return SUCCEEDED(d->CreateTexture2D(&td,nullptr,&texture))&&SUCCEEDED(d->CreateRenderTargetView(texture.Get(),nullptr,&target))&&SUCCEEDED(d->CreateShaderResourceView(texture.Get(),nullptr,&view));};
    return make(scratchTexture,scratchTarget,scratchView)&&make(outputTexture,outputTarget,outputView);
}
bool BlurMaterial::blur(ID3D11DeviceContext* c, ID3D11ShaderResourceView* input) {
    if(!c||!input||!width||!height) return false;
    D3D11_MAPPED_SUBRESOURCE map{}; if(FAILED(c->Map(weights.Get(),0,D3D11_MAP_WRITE_DISCARD,0,&map))) return false;
    auto* constants=static_cast<Constants*>(map.pData); *constants={}; constants->direction[0]=1.f;constants->direction[1]=0.f;constants->size[0]=static_cast<float>(width);constants->size[1]=static_cast<float>(height);
    float sum=0; for(int i=-18;i<=18;++i){float x=static_cast<float>(i);float v=std::exp(-(x*x)/72.f);constants->weights[i+18]=v;sum+=v;}for(auto& x:constants->weights)x/=sum;
    c->Unmap(weights.Get(),0); D3D11_VIEWPORT vp{0,0,static_cast<float>(width),static_cast<float>(height),0,1};
    c->IASetInputLayout(nullptr);c->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);c->VSSetShader(vertex.Get(),nullptr,0);c->PSSetShader(horizontal.Get(),nullptr,0);c->PSSetSamplers(0,1,sampler.GetAddressOf());c->PSSetConstantBuffers(0,1,weights.GetAddressOf());c->RSSetViewports(1,&vp);
    ID3D11RenderTargetView* target=scratchTarget.Get();c->OMSetRenderTargets(1,&target,nullptr);c->PSSetShaderResources(0,1,&input);c->Draw(3,0);ID3D11ShaderResourceView* nullView=nullptr;c->PSSetShaderResources(0,1,&nullView);
    if(FAILED(c->Map(weights.Get(),0,D3D11_MAP_WRITE_DISCARD,0,&map)))return false;constants=static_cast<Constants*>(map.pData); *constants={};constants->direction[0]=0;constants->direction[1]=1;constants->size[0]=static_cast<float>(width);constants->size[1]=static_cast<float>(height);for(int i=-18;i<=18;++i){float x=static_cast<float>(i);constants->weights[i+18]=std::exp(-(x*x)/72.f)/sum;}c->Unmap(weights.Get(),0);
    target=outputTarget.Get();c->OMSetRenderTargets(1,&target,nullptr);auto* source=scratchView.Get();c->PSSetShaderResources(0,1,&source);c->Draw(3,0);c->PSSetShaderResources(0,1,&nullView);c->OMSetRenderTargets(0,nullptr,nullptr);return true;
}
}
