#include "capture.h"
#include "resource_lifetime.h"
#include <algorithm>
#include <cstring>
#include <utility>

namespace renderer {
namespace {
struct FrameGuard final {
    IDXGIOutputDuplication* duplication;
    UINT* count;
    bool active=true;
    ~FrameGuard() { release(); }
    void release() { if (active) { duplication->ReleaseFrame(); --*count; active=false; } }
};

RECT ClipRect(const RECT& value, UINT width, UINT height) {
    RECT result{(std::max)(0L,value.left),(std::max)(0L,value.top),
        (std::min)(static_cast<LONG>(width),value.right),(std::min)(static_cast<LONG>(height),value.bottom)};
    if (result.right < result.left) result.right=result.left;
    if (result.bottom < result.top) result.bottom=result.top;
    return result;
}

bool CreateLike(ID3D11Device* device, const D3D11_TEXTURE2D_DESC& source, ComPtr<ID3D11Texture2D>* target) {
    D3D11_TEXTURE2D_DESC desc=source;
    desc.Usage=D3D11_USAGE_DEFAULT; desc.BindFlags=D3D11_BIND_SHADER_RESOURCE;
    desc.CPUAccessFlags=0; desc.MiscFlags=0;
    return SUCCEEDED(device->CreateTexture2D(&desc,nullptr,target->ReleaseAndGetAddressOf()));
}
}

bool Capture::initialize(ID3D11Device* device,IDXGIAdapter1* adapter,UINT index) {
    shutdown();
    if (!device || !adapter) return false;
    ComPtr<IDXGIOutput> output;
    if (FAILED(adapter->EnumOutputs(index,&output))) { invalidate(CaptureFailure::unavailable); return false; }
    DXGI_OUTPUT_DESC desc{};
    if (FAILED(output->GetDesc(&desc))) { invalidate(CaptureFailure::unavailable); return false; }
    desktop=desc.DesktopCoordinates;
    rotation=desc.Rotation;
    ComPtr<IDXGIOutput6> output6;
    if (SUCCEEDED(output.As(&output6))) {
        DXGI_OUTPUT_DESC1 color{};
        if (SUCCEEDED(output6->GetDesc1(&color)) && (color.BitsPerColor>8 || color.ColorSpace!=DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709)) {
            invalidate(CaptureFailure::unsupported_hdr); return false;
        }
    }
    ComPtr<IDXGIOutput1> output1;
    if (FAILED(output.As(&output1)) || FAILED(output1->DuplicateOutput(device,&duplication))) {
        invalidate(CaptureFailure::unavailable); return false;
    }
    failure=CaptureFailure::none;
    return true;
}

bool Capture::acquire(ID3D11DeviceContext* context,UINT timeout) {
    if (!duplication || !context) { invalidate(CaptureFailure::unavailable); return false; }
    frameUpdated=false;
    changedRects.clear();
    DXGI_OUTDUPL_FRAME_INFO info{};
    ComPtr<IDXGIResource> resource;
    const HRESULT hr=duplication->AcquireNextFrame(timeout,&info,&resource);
    if (hr==DXGI_ERROR_WAIT_TIMEOUT) return false;
    if (hr==DXGI_ERROR_ACCESS_LOST) { invalidate(CaptureFailure::access_lost); return false; }
    if (FAILED(hr)) {
        invalidate(hr==DXGI_ERROR_DEVICE_REMOVED || hr==DXGI_ERROR_DEVICE_RESET
            ? CaptureFailure::device_removed : CaptureFailure::unavailable);
        return false;
    }
    ++outstanding;
    FrameGuard guard{duplication.Get(),&outstanding};
    ComPtr<ID3D11Texture2D> acquired;
    if (FAILED(resource.As(&acquired))) { invalidate(CaptureFailure::unavailable); return false; }
    D3D11_TEXTURE2D_DESC desc{};
    acquired->GetDesc(&desc);
    if ((desc.Format!=DXGI_FORMAT_B8G8R8A8_UNORM && desc.Format!=DXGI_FORMAT_R8G8B8A8_UNORM) ||
        desc.Width==0 || desc.Height==0) { invalidate(CaptureFailure::unsupported_hdr); return false; }

    ComPtr<ID3D11Device> device;
    context->GetDevice(&device);
    const bool recreate=!frame || outputWidth!=desc.Width || outputHeight!=desc.Height || format!=desc.Format;
    if (recreate) {
        if (!CreateLike(device.Get(),desc,&frame) || !CreateLike(device.Get(),desc,&moveScratch)) {
            invalidate(CaptureFailure::unavailable); return false;
        }
        outputWidth=desc.Width; outputHeight=desc.Height; format=desc.Format; hasFrame=false;
    }

    moveRects.clear();
    dirtyRects.clear();
    if (hasFrame && info.TotalMetadataBufferSize>0) {
        metadataBuffer.resize(info.TotalMetadataBufferSize);
        UINT supplied=info.TotalMetadataBufferSize;
        if (FAILED(duplication->GetFrameMoveRects(supplied,
                reinterpret_cast<DXGI_OUTDUPL_MOVE_RECT*>(metadataBuffer.data()),&supplied))) {
            invalidate(CaptureFailure::unavailable); return false;
        }
        moveRects.resize(supplied/sizeof(DXGI_OUTDUPL_MOVE_RECT));
        if(supplied) std::memcpy(moveRects.data(),metadataBuffer.data(),supplied);
        supplied=info.TotalMetadataBufferSize;
        if (FAILED(duplication->GetFrameDirtyRects(supplied,
                reinterpret_cast<RECT*>(metadataBuffer.data()),&supplied))) {
            invalidate(CaptureFailure::unavailable); return false;
        }
        dirtyRects.resize(supplied/sizeof(RECT));
        if(supplied) std::memcpy(dirtyRects.data(),metadataBuffer.data(),supplied);
    }
    if (hasFrame) changedRects.clear();
    if (!hasFrame) {
        context->CopyResource(frame.Get(),acquired.Get());
        changedRects={{0,0,static_cast<LONG>(desc.Width),static_cast<LONG>(desc.Height)}};
    } else {
        for (const auto& move:moveRects) {
            const RECT originalDestination=move.DestinationRect;
            RECT source{move.SourcePoint.x,move.SourcePoint.y,
                move.SourcePoint.x+(move.DestinationRect.right-move.DestinationRect.left),
                move.SourcePoint.y+(move.DestinationRect.bottom-move.DestinationRect.top)};
            RECT destination=ClipRect(originalDestination,desc.Width,desc.Height);
            source.left+=destination.left-originalDestination.left;
            source.top+=destination.top-originalDestination.top;
            source.right=source.left+(destination.right-destination.left);
            source.bottom=source.top+(destination.bottom-destination.top);
            source=ClipRect(source,desc.Width,desc.Height);
            const LONG copyWidth=(std::min)(source.right-source.left,destination.right-destination.left);
            const LONG copyHeight=(std::min)(source.bottom-source.top,destination.bottom-destination.top);
            if (copyWidth<=0 || copyHeight<=0) continue;
            recordMoveUpdate(move);
            D3D11_BOX box{static_cast<UINT>(source.left),static_cast<UINT>(source.top),0,
                static_cast<UINT>(source.left+copyWidth),static_cast<UINT>(source.top+copyHeight),1};
            context->CopySubresourceRegion(moveScratch.Get(),0,destination.left,destination.top,0,frame.Get(),0,&box);
        }
        for (const auto& move:moveRects) {
            const RECT destination=ClipRect(move.DestinationRect,desc.Width,desc.Height);
            if (destination.right<=destination.left || destination.bottom<=destination.top) continue;
            D3D11_BOX box{static_cast<UINT>(destination.left),static_cast<UINT>(destination.top),0,
                static_cast<UINT>(destination.right),static_cast<UINT>(destination.bottom),1};
            context->CopySubresourceRegion(frame.Get(),0,destination.left,destination.top,0,moveScratch.Get(),0,&box);
        }
        for (const auto& rect:dirtyRects) {
            const RECT clipped=ClipRect(rect,desc.Width,desc.Height);
            if (clipped.right<=clipped.left || clipped.bottom<=clipped.top) continue;
            recordDirtyUpdate(rect);
            D3D11_BOX box{static_cast<UINT>(clipped.left),static_cast<UINT>(clipped.top),0,
                static_cast<UINT>(clipped.right),static_cast<UINT>(clipped.bottom),1};
            context->CopySubresourceRegion(frame.Get(),0,clipped.left,clipped.top,0,acquired.Get(),0,&box);
        }
    }
    guard.release();
    hasFrame=true;
    frameUpdated=!changedRects.empty();
    failure=CaptureFailure::none;
    return true;
}

bool Capture::panelCrop(const RECT& panel,UINT padding,RECT* crop) const {
    if (!crop || desktop.right<=desktop.left || desktop.bottom<=desktop.top) return false;
    RECT relative{(std::max)(0L,panel.left-desktop.left),(std::max)(0L,panel.top-desktop.top),
        (std::min)(desktop.right-desktop.left,panel.right-desktop.left),
        (std::min)(desktop.bottom-desktop.top,panel.bottom-desktop.top)};
    if(relative.right<=relative.left || relative.bottom<=relative.top) return false;
    relative.left-=(std::min)(static_cast<LONG>(padding),relative.left);
    relative.top-=(std::min)(static_cast<LONG>(padding),relative.top);
    relative.right+=(std::min)(static_cast<LONG>(padding),desktop.right-desktop.left-relative.right);
    relative.bottom+=(std::min)(static_cast<LONG>(padding),desktop.bottom-desktop.top-relative.bottom);
    if (rotation==DXGI_MODE_ROTATION_ROTATE90 || rotation==DXGI_MODE_ROTATION_ROTATE270) {
        if(rotation==DXGI_MODE_ROTATION_ROTATE90)
            *crop={relative.top,static_cast<LONG>(outputHeight)-relative.right,
                relative.bottom,static_cast<LONG>(outputHeight)-relative.left};
        else
            *crop={static_cast<LONG>(outputWidth)-relative.bottom,relative.left,
                static_cast<LONG>(outputWidth)-relative.top,relative.right};
    } else if (rotation==DXGI_MODE_ROTATION_ROTATE180) {
        *crop={static_cast<LONG>(outputWidth)-relative.right,static_cast<LONG>(outputHeight)-relative.bottom,
            static_cast<LONG>(outputWidth)-relative.left,static_cast<LONG>(outputHeight)-relative.top};
    } else *crop=relative;
    return crop->right>crop->left && crop->bottom>crop->top;
}

bool Capture::intersectingUpdate(const RECT& left,const RECT& right) {
    return left.left<right.right && left.right>right.left && left.top<right.bottom && left.bottom>right.top;
}

void Capture::invalidate(CaptureFailure reason) { release(); failure=reason; }

void Capture::recordMoveUpdate(const DXGI_OUTDUPL_MOVE_RECT& move) {
    RECT source{move.SourcePoint.x,move.SourcePoint.y,
        move.SourcePoint.x+(move.DestinationRect.right-move.DestinationRect.left),
        move.SourcePoint.y+(move.DestinationRect.bottom-move.DestinationRect.top)};
    source=ClipRect(source,outputWidth,outputHeight);
    const RECT destination=ClipRect(move.DestinationRect,outputWidth,outputHeight);
    if(source.right>source.left && source.bottom>source.top) changedRects.push_back(source);
    if(destination.right>destination.left && destination.bottom>destination.top) changedRects.push_back(destination);
}

void Capture::recordDirtyUpdate(const RECT& dirty) {
    const RECT clipped=ClipRect(dirty,outputWidth,outputHeight);
    if(clipped.right>clipped.left && clipped.bottom>clipped.top) changedRects.push_back(clipped);
}

bool Capture::permitDeviceRecreation() {
    if (failure!=CaptureFailure::device_removed || deviceRecreationUsed) return false;
    deviceRecreationUsed=true;
    return true;
}

bool Capture::updatesPanel(const RECT& panel,UINT padding) const {
    if (!frameUpdated) return false;
    RECT crop{};
    if (!panelCrop(panel,padding,&crop)) return false;
    return std::any_of(changedRects.begin(),changedRects.end(),[&](const RECT& changed) {
        return intersectingUpdate(crop,changed);
    });
}

void Capture::release() { ResetResources(frame,moveScratch); hasFrame=false; frameUpdated=false; changedRects.clear(); metadataBuffer.clear(); moveRects.clear(); dirtyRects.clear(); outputWidth=outputHeight=0; format=DXGI_FORMAT_UNKNOWN; }
void Capture::shutdown() { release(); if (duplication) duplication.Reset(); outstanding=0; desktop={}; rotation=DXGI_MODE_ROTATION_UNSPECIFIED; }
}
