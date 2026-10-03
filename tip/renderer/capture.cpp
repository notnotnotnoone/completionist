#include "capture.h"
#include <utility>

namespace renderer {
struct FrameGuard { IDXGIOutputDuplication* duplication; UINT* count; bool active=true; ~FrameGuard(){if(active){duplication->ReleaseFrame();--*count;}} void release(){if(active){duplication->ReleaseFrame();--*count;active=false;}} };
bool Capture::initialize(ID3D11Device* device,IDXGIAdapter1* adapter,UINT index){
    shutdown();if(!device||!adapter)return false;ComPtr<IDXGIOutput> output;
    if(FAILED(adapter->EnumOutputs(index,&output))){failure=CaptureFailure::unavailable;return false;}
    DXGI_OUTPUT_DESC desc{};output->GetDesc(&desc);ComPtr<IDXGIOutput6> output6;
    if(SUCCEEDED(output.As(&output6))){DXGI_OUTPUT_DESC1 d{};if(SUCCEEDED(output6->GetDesc1(&d))&&d.BitsPerColor>8){failure=CaptureFailure::unsupported_hdr;return false;}}
    ComPtr<IDXGIOutput1> output1;if(FAILED(output.As(&output1))||FAILED(output1->DuplicateOutput(device,&duplication))){failure=CaptureFailure::unavailable;return false;}
    failure=CaptureFailure::none;return true;
}
bool Capture::acquire(ID3D11DeviceContext* context,UINT timeout){
    if(!duplication||!context)return false;release();DXGI_OUTDUPL_FRAME_INFO info{};ComPtr<IDXGIResource> resource;
    HRESULT hr=duplication->AcquireNextFrame(timeout,&info,&resource);
    if(hr==DXGI_ERROR_WAIT_TIMEOUT)return false;
    if(hr==DXGI_ERROR_ACCESS_LOST){failure=CaptureFailure::access_lost;return false;}
    if(FAILED(hr)){failure=CaptureFailure::device_removed;return false;}
    ++outstanding;FrameGuard guard{duplication.Get(),&outstanding};
    ComPtr<ID3D11Texture2D> acquired;if(FAILED(resource.As(&acquired))){failure=CaptureFailure::unavailable;return false;}
    D3D11_TEXTURE2D_DESC desc{};acquired->GetDesc(&desc);if(desc.Format!=DXGI_FORMAT_B8G8R8A8_UNORM&&desc.Format!=DXGI_FORMAT_R8G8B8A8_UNORM){failure=CaptureFailure::unsupported_hdr;return false;}
    // Only retain the GPU texture. The duplication frame is released before the caller can render or resize.
    D3D11_TEXTURE2D_DESC copy=desc;copy.Usage=D3D11_USAGE_DEFAULT;copy.BindFlags=D3D11_BIND_SHADER_RESOURCE;copy.CPUAccessFlags=0;copy.MiscFlags=0;
    ComPtr<ID3D11Texture2D> retained;
    // The source texture itself remains valid after ReleaseFrame only until the next acquire, so copy is required.
    ComPtr<ID3D11Device> owner;context->GetDevice(&owner);
    if(FAILED(owner->CreateTexture2D(&copy,nullptr,&retained))){failure=CaptureFailure::unavailable;return false;}
    context->CopyResource(retained.Get(),acquired.Get());guard.release();frame=std::move(retained);failure=CaptureFailure::none;return true;
}
void Capture::release(){frame.Reset();}
void Capture::shutdown(){release();if(duplication){duplication.Reset();}outstanding=0;}
}
