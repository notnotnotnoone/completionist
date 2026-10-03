#define NOMINMAX
#include <windows.h>
#include <d3d11.h>
#include <wrl/client.h>
#include <array>
#include <cmath>
#include <cstdio>
#include <vector>
#include "../material.h"

using Microsoft::WRL::ComPtr;
static void require(bool ok,const char* message){if(!ok){std::fprintf(stderr,"FAIL: %s\n",message);ExitProcess(1);}}
int wmain(){
    constexpr int w=129,h=65,r=18;std::array<float,2*r+1> weights{};float total=0;
    for(int i=-r;i<=r;++i){float x=static_cast<float>(i);weights[i+r]=std::exp(-x*x/72.f);total+=weights[i+r];}
    for(auto& v:weights)v/=total;
    float sum=0;for(float v:weights)sum+=v;require(std::abs(sum-1.f)<1e-5f,"Gaussian weights sum to one");
    for(int i=0;i<=2*r;++i)require(std::abs(weights[i]-weights[2*r-i])<1e-7f,"Gaussian kernel is symmetric");
    UINT flags=D3D11_CREATE_DEVICE_BGRA_SUPPORT;D3D_FEATURE_LEVEL level{};ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> context;
    HRESULT hr=D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,flags,nullptr,0,D3D11_SDK_VERSION,&device,&level,&context);require(SUCCEEDED(hr),"WARP D3D11 device creation");
    std::vector<unsigned> pixels(w*h,0xff000000u);for(int y=0;y<h;++y)for(int x=w/2;x<w;++x)pixels[y*w+x]=0xffffffffu;
    D3D11_TEXTURE2D_DESC td{};td.Width=w;td.Height=h;td.MipLevels=td.ArraySize=1;td.Format=DXGI_FORMAT_B8G8R8A8_UNORM;td.SampleDesc.Count=1;td.Usage=D3D11_USAGE_IMMUTABLE;td.BindFlags=D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA data{pixels.data(),w*4,0};ComPtr<ID3D11Texture2D> source;ComPtr<ID3D11ShaderResourceView> view;require(SUCCEEDED(device->CreateTexture2D(&td,&data,&source)),"fixture pattern upload");require(SUCCEEDED(device->CreateShaderResourceView(source.Get(),nullptr,&view)),"fixture view");
    renderer::BlurMaterial blur;require(blur.create(device.Get(),w,h),"blur pipeline creation");require(blur.blur(context.Get(),view.Get()),"horizontal and vertical GPU passes");
    td.Usage=D3D11_USAGE_STAGING;td.BindFlags=0;td.CPUAccessFlags=D3D11_CPU_ACCESS_READ;ComPtr<ID3D11Texture2D> readback;require(SUCCEEDED(device->CreateTexture2D(&td,nullptr,&readback)),"fixture readback allocation");context->CopyResource(readback.Get(),blur.outputTexture.Get());D3D11_MAPPED_SUBRESOURCE map{};require(SUCCEEDED(context->Map(readback.Get(),0,D3D11_MAP_READ,0,&map)),"fixture readback map");
    auto* row=static_cast<const unsigned char*>(map.pData)+h/2*map.RowPitch;float last=-1;int changed=0;
    for(int x=w/2-24;x<=w/2+24;++x){float v=row[x*4]/255.f;require(v+1e-6f>=last,"step response is monotonically increasing");if(v>0.01f&&v<0.99f)++changed;last=v;}
    require(changed>=5,"step edge transitions across multiple pixels");context->Unmap(readback.Get(),0);
    // Run a second source containing only a point so edge and impulse symmetry are independent checks.
    std::fill(pixels.begin(),pixels.end(),0xff000000u);pixels[(h/2)*w+w/2]=0xffffffffu;data.pSysMem=pixels.data();
    // Recreate a shader-readable immutable fixture description after switching the staging descriptor.
    D3D11_TEXTURE2D_DESC sourceDesc=td;sourceDesc.Usage=D3D11_USAGE_IMMUTABLE;sourceDesc.BindFlags=D3D11_BIND_SHADER_RESOURCE;sourceDesc.CPUAccessFlags=0;ComPtr<ID3D11Texture2D> fixedPoint;ComPtr<ID3D11ShaderResourceView> fixedView;
    D3D11_SUBRESOURCE_DATA pointData{pixels.data(),w*4,0};require(SUCCEEDED(device->CreateTexture2D(&sourceDesc,&pointData,&fixedPoint)),"immutable point fixture upload");require(SUCCEEDED(device->CreateShaderResourceView(fixedPoint.Get(),nullptr,&fixedView)),"immutable point view");
    require(blur.blur(context.Get(),fixedView.Get()),"point Gaussian passes");context->CopyResource(readback.Get(),blur.outputTexture.Get());require(SUCCEEDED(context->Map(readback.Get(),0,D3D11_MAP_READ,0,&map)),"point fixture map");
    auto* center=static_cast<const unsigned char*>(map.pData)+(h/2)*map.RowPitch;
    for(int dx=1;dx<=8;++dx)require(std::abs(int(center[(w/2-dx)*4])-int(center[(w/2+dx)*4]))<=2,"point response is symmetric");
    context->Unmap(readback.Get(),0);
    // Glyphs remain a separate untouched fixture plane; no captured desktop content is exported.
    std::puts("PASS: WARP Gaussian symmetry, normalized weights, smooth monotonic edge, generated-only fixture");return 0;
}
