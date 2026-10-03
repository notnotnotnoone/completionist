#define NOMINMAX
#include <windows.h>
#include <d3d11.h>
#include <wrl/client.h>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <utility>
#include <vector>
#include "../material.h"

using Microsoft::WRL::ComPtr;
static void require(bool ok,const char* message){if(!ok){std::fprintf(stderr,"FAIL: %s\n",message);ExitProcess(1);}}
static std::vector<unsigned char> renderFixture(ID3D11Device* device,ID3D11DeviceContext* context,
        renderer::BlurMaterial& blur,ID3D11Texture2D* readback,const std::vector<unsigned>& pixels,int w,int h){
    D3D11_TEXTURE2D_DESC desc{};desc.Width=w;desc.Height=h;desc.MipLevels=desc.ArraySize=1;desc.Format=DXGI_FORMAT_B8G8R8A8_UNORM;desc.SampleDesc.Count=1;desc.Usage=D3D11_USAGE_IMMUTABLE;desc.BindFlags=D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA data{pixels.data(),static_cast<UINT>(w*4),0};ComPtr<ID3D11Texture2D> texture;ComPtr<ID3D11ShaderResourceView> view;
    require(SUCCEEDED(device->CreateTexture2D(&desc,&data,&texture)),"generated fixture upload");require(SUCCEEDED(device->CreateShaderResourceView(texture.Get(),nullptr,&view)),"generated fixture view");
    require(blur.blur(context,view.Get()),"production horizontal/vertical GPU blur");context->CopyResource(readback,blur.outputTexture.Get());D3D11_MAPPED_SUBRESOURCE map{};
    require(SUCCEEDED(context->Map(readback,0,D3D11_MAP_READ,0,&map)),"generated fixture readback map");
    std::vector<unsigned char> result(static_cast<size_t>(w*h*4));for(int y=0;y<h;++y)std::memcpy(result.data()+static_cast<size_t>(y*w*4),static_cast<const unsigned char*>(map.pData)+static_cast<size_t>(y)*map.RowPitch,static_cast<size_t>(w*4));context->Unmap(readback,0);return result;
}
static unsigned char at(const std::vector<unsigned char>& image,int w,int x,int y){return image[(static_cast<size_t>(y*w+x))*4];}
int wmain(){
    constexpr int w=129,h=65,r=18,centerX=w/2,centerY=h/2;constexpr float sigma=6.f;
    std::array<float,2*r+1> expected{};float normalization=0;
    for(int i=-r;i<=r;++i){float x=static_cast<float>(i);expected[i+r]=std::exp(-(x*x)/(2*sigma*sigma));normalization+=expected[i+r];}
    for(auto& value:expected)value/=normalization;
    float sum=0;for(float value:expected)sum+=value;require(std::abs(sum-1.f)<1e-5f,"specification reference kernel normalization");
    UINT flags=D3D11_CREATE_DEVICE_BGRA_SUPPORT;D3D_FEATURE_LEVEL level{};ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> context;
    require(SUCCEEDED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,flags,nullptr,0,D3D11_SDK_VERSION,&device,&level,&context)),"WARP D3D11 device creation");
    renderer::BlurMaterial blur;require(blur.create(device.Get(),w,h),"production blur pipeline creation");
    D3D11_TEXTURE2D_DESC rd{};rd.Width=w;rd.Height=h;rd.MipLevels=rd.ArraySize=1;rd.Format=DXGI_FORMAT_B8G8R8A8_UNORM;rd.SampleDesc.Count=1;rd.Usage=D3D11_USAGE_STAGING;rd.CPUAccessFlags=D3D11_CPU_ACCESS_READ;ComPtr<ID3D11Texture2D> readback;
    require(SUCCEEDED(device->CreateTexture2D(&rd,nullptr,&readback)),"fixture readback allocation");

    // A constant generated image detects DC loss from unnormalized shader constants in either pass.
    std::vector<unsigned> pixels(w*h,0xff494949u);auto image=renderFixture(device.Get(),context.Get(),blur,readback.Get(),pixels,w,h);
    for(auto [x,y]:std::array<std::pair<int,int>,5>{{{0,0},{centerX,0},{0,centerY},{centerX,centerY},{w-1,h-1}}})
        require(std::abs(int(at(image,w,x,y))-73)<=1,"two-pass Gaussian preserves a nonzero constant image");

    // A one-pixel-wide vertical line is a 1D impulse for the horizontal production shader.
    std::fill(pixels.begin(),pixels.end(),0xff000000u);for(int y=0;y<h;++y)pixels[static_cast<size_t>(y*w+centerX)]=0xffb4b4b4u;
    image=renderFixture(device.Get(),context.Get(),blur,readback.Get(),pixels,w,h);
    for(int distance:std::array<int,9>{0,1,2,4,6,9,12,15,18}){
        int actual=at(image,w,centerX+distance,centerY);int predicted=static_cast<int>(std::lround(180.f*expected[distance+r]));
        require(std::abs(actual-predicted)<=1,"GPU impulse profile matches normalized sigma-6 radius-18 Gaussian (UNORM tolerance)");
        if(distance)require(std::abs(int(at(image,w,centerX-distance,centerY))-predicted)<=1,"GPU impulse profile symmetry");
    }

    // A two-level step checks the profile through the convolution and gradual transitions.
    std::fill(pixels.begin(),pixels.end(),0xff333333u);for(int y=0;y<h;++y)for(int x=centerX;x<w;++x)pixels[static_cast<size_t>(y*w+x)]=0xffccccccu;
    image=renderFixture(device.Get(),context.Get(),blur,readback.Get(),pixels,w,h);int previous=-1;int intermediate=0;
    for(int x=centerX-24;x<=centerX+24;++x){int actual=at(image,w,x,centerY);require(actual>=previous,"step edge transitions monotonically");if(actual>51&&actual<204)++intermediate;previous=actual;}
    require(intermediate>=5,"step edge transitions across multiple pixels");
    for(int offset:std::array<int,9>{-18,-12,-8,-4,0,4,8,12,18}){
        float cdf=0;for(int i=-r;i<=offset;++i)cdf+=expected[i+r];int predicted=static_cast<int>(std::lround(51.f+(204.f-51.f)*cdf));
        require(std::abs(int(at(image,w,centerX+offset,centerY))-predicted)<=2,"GPU step profile matches Gaussian cumulative response (UNORM tolerance)");
    }
    std::puts("PASS: production WARP shader matches normalized sigma-6/radius-18 impulse and step profiles; DC preserved");return 0;
}
