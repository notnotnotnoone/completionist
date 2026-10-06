#define NOMINMAX
#include "fixture.h"
#include "material.h"
#include "shaders.h"
#include "text.h"
#include "wic_layout.h"
#include "../src/render_protocol.h"

#include <windows.h>
#include <d2d1_1.h>
#include <dwrite.h>
#include <wincodec.h>
#include <wrl/client.h>
#include <cstdint>
#include <filesystem>
#include <vector>

namespace renderer {
namespace {
using Microsoft::WRL::ComPtr;
constexpr UINT kBaseWidth = 640;
constexpr UINT kBaseHeight = 360;

struct TextureMapGuard final {
    ID3D11DeviceContext* context;
    ID3D11Resource* resource;
    bool active = true;
    ~TextureMapGuard() { unmap(); }
    void unmap() {
        if (active) {
            context->Unmap(resource, 0);
            active = false;
        }
    }
};

class ComApartment final {
public:
    ComApartment() : result_(CoInitializeEx(nullptr, COINIT_MULTITHREADED)) {}
    ~ComApartment() { if (SUCCEEDED(result_)) CoUninitialize(); }
    bool initialized() const { return SUCCEEDED(result_); }
private:
    HRESULT result_;
};

bool savePng(const std::wstring& path, ID3D11DeviceContext* context, ID3D11Texture2D* texture) {
    D3D11_TEXTURE2D_DESC desc{};
    texture->GetDesc(&desc);
    if (desc.Format != DXGI_FORMAT_B8G8R8A8_UNORM) return false;
    desc.Usage = D3D11_USAGE_STAGING;
    desc.BindFlags = 0;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    desc.MiscFlags = 0;
    ComPtr<ID3D11Device> device;
    context->GetDevice(&device);
    ComPtr<ID3D11Texture2D> staging;
    if (FAILED(device->CreateTexture2D(&desc, nullptr, &staging))) return false;
    context->CopyResource(staging.Get(), texture);
    context->Flush();
    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (FAILED(context->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped))) return false;
    TextureMapGuard mapGuard{context, staging.Get()};
    const std::size_t rowPitch = mapped.RowPitch;
    constexpr std::size_t maxSize = std::numeric_limits<std::size_t>::max();
    if (rowPitch == 0 || static_cast<std::size_t>(desc.Height) > maxSize / rowPitch) return false;
    const std::size_t availableBytes = rowPitch * static_cast<std::size_t>(desc.Height);
    WicMemoryLayout layout{};
    if (!makeWicMemoryLayout(desc.Width, desc.Height, mapped.RowPitch, availableBytes, layout)) return false;

    bool ok = false;
    {
        ComPtr<IWICImagingFactory> factory;
        ComPtr<IWICStream> stream;
        ComPtr<IWICBitmapEncoder> encoder;
        ComPtr<IWICBitmapFrameEncode> frame;
        ComPtr<IPropertyBag2> properties;
        ComPtr<IWICBitmap> bitmap;
        WICPixelFormatGUID pixelFormat = GUID_WICPixelFormat32bppBGRA;
        const bool created = SUCCEEDED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                             IID_PPV_ARGS(&factory))) &&
            SUCCEEDED(factory->CreateBitmapFromMemory(desc.Width, desc.Height, GUID_WICPixelFormat32bppBGRA,
                 layout.strideBytes, layout.bufferSizeBytes, static_cast<BYTE*>(mapped.pData), &bitmap)) &&
            SUCCEEDED(factory->CreateStream(&stream)) && SUCCEEDED(stream->InitializeFromFilename(path.c_str(), GENERIC_WRITE)) &&
            SUCCEEDED(factory->CreateEncoder(GUID_ContainerFormatPng, nullptr, &encoder)) &&
            SUCCEEDED(encoder->Initialize(stream.Get(), WICBitmapEncoderNoCache)) &&
            SUCCEEDED(encoder->CreateNewFrame(&frame, &properties));
        ok = created && SUCCEEDED(frame->Initialize(properties.Get())) &&
            SUCCEEDED(frame->SetSize(desc.Width, desc.Height)) && SUCCEEDED(frame->SetPixelFormat(&pixelFormat)) &&
            SUCCEEDED(frame->WriteSource(bitmap.Get(), nullptr)) && SUCCEEDED(frame->Commit()) &&
            SUCCEEDED(encoder->Commit());
    }
    mapGuard.unmap();
    return ok;
}
}

bool saveTexturePng(const std::wstring& path, ID3D11DeviceContext* context, ID3D11Texture2D* texture) {
    return context && texture && !path.empty() && savePng(path, context, texture);
}

bool saveTextureCropPng(const std::wstring& path,ID3D11DeviceContext* context,ID3D11Texture2D* texture,const RECT& crop) {
    if(!context || !texture || crop.left<0 || crop.top<0 || crop.right<=crop.left || crop.bottom<=crop.top) return false;
    D3D11_TEXTURE2D_DESC desc{}; texture->GetDesc(&desc);
    if(static_cast<UINT>(crop.right)>desc.Width || static_cast<UINT>(crop.bottom)>desc.Height) return false;
    desc.Width=static_cast<UINT>(crop.right-crop.left); desc.Height=static_cast<UINT>(crop.bottom-crop.top);
    desc.Usage=D3D11_USAGE_DEFAULT; desc.BindFlags=0; desc.CPUAccessFlags=0; desc.MiscFlags=0;
    ComPtr<ID3D11Device> device; context->GetDevice(&device);
    ComPtr<ID3D11Texture2D> cropped;
    if(FAILED(device->CreateTexture2D(&desc,nullptr,&cropped))) return false;
    const D3D11_BOX box{static_cast<UINT>(crop.left),static_cast<UINT>(crop.top),0,
        static_cast<UINT>(crop.right),static_cast<UINT>(crop.bottom),1};
    context->CopySubresourceRegion(cropped.Get(),0,0,0,0,texture,0,&box);
    return saveTexturePng(path,context,cropped.Get());
}

bool renderFixture(const std::wstring& outputPath, UINT dpi, bool dark, int fontSizePoints) {
    if (outputPath.empty() || dpi < 96 || fontSizePoints < 7 || fontSizePoints > 24) return false;
    const UINT width = kBaseWidth * dpi / 96;
    const UINT height = kBaseHeight * dpi / 96;
    ComApartment apartment;
    if (!apartment.initialized()) return false;
    UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#if defined(_DEBUG)
    flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    D3D_FEATURE_LEVEL level{};
    if (FAILED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, flags, nullptr, 0,
                                 D3D11_SDK_VERSION, &device, &level, &context))) return false;
    (void)level;

    std::vector<std::uint32_t> pixels(static_cast<size_t>(width) * height);
    for (UINT y = 0; y < height; ++y) {
        for (UINT x = 0; x < width; ++x) {
            const bool stripe = ((x / 24U) + (y / 24U)) % 2U == 0;
            const std::uint8_t r = static_cast<std::uint8_t>(stripe ? 24U + x % 80U : 180U - y % 80U);
            const std::uint8_t g = static_cast<std::uint8_t>(stripe ? 74U + y % 100U : 36U + x % 100U);
            const std::uint8_t b = static_cast<std::uint8_t>(stripe ? 50U : 110U + (x + y) % 100U);
            pixels[static_cast<size_t>(y) * width + x] = 0xff000000U | (static_cast<std::uint32_t>(r) << 16U) |
                (static_cast<std::uint32_t>(g) << 8U) | b;
        }
    }
    D3D11_TEXTURE2D_DESC inputDesc{};
    inputDesc.Width = width; inputDesc.Height = height; inputDesc.MipLevels = 1; inputDesc.ArraySize = 1;
    inputDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM; inputDesc.SampleDesc.Count = 1;
    inputDesc.Usage = D3D11_USAGE_DEFAULT; inputDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA inputData{pixels.data(), width * sizeof(std::uint32_t), 0};
    ComPtr<ID3D11Texture2D> input;
    ComPtr<ID3D11ShaderResourceView> inputView;
    if (FAILED(device->CreateTexture2D(&inputDesc, &inputData, &input)) ||
        FAILED(device->CreateShaderResourceView(input.Get(), nullptr, &inputView))) return false;

    BlurMaterial blur;
    if (!blur.create(device.Get(), width, height) || !blur.blur(context.Get(), inputView.Get(), static_cast<float>(dpi))) return false;

    ComPtr<IDXGIDevice> dxgiDevice;
    ComPtr<ID2D1Factory1> d2dFactory;
    ComPtr<ID2D1Device> d2dDevice;
    ComPtr<ID2D1DeviceContext> drawing;
    ComPtr<IDWriteFactory> writeFactory;
    ComPtr<IDXGISurface> surface;
    ComPtr<ID2D1Bitmap1> target;
    if (FAILED(device->QueryInterface(IID_PPV_ARGS(&dxgiDevice))) ||
        FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, D2D1_FACTORY_OPTIONS{}, d2dFactory.GetAddressOf())) ||
        FAILED(d2dFactory->CreateDevice(dxgiDevice.Get(), &d2dDevice)) ||
        FAILED(d2dDevice->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &drawing)) ||
        FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
                                   reinterpret_cast<IUnknown**>(writeFactory.GetAddressOf()))) ||
        FAILED(blur.outputTexture.As(&surface))) return false;
    auto properties = D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
        static_cast<float>(dpi), static_cast<float>(dpi));
    if (FAILED(drawing->CreateBitmapFromDxgiSurface(surface.Get(), &properties, &target))) return false;
    drawing->SetTarget(target.Get());
    drawing->SetDpi(static_cast<float>(dpi), static_cast<float>(dpi));
    drawing->BeginDraw();
    const auto& colors = dark ? palette::kDark : palette::kLight;

    completionist::render::Snapshot snapshot{};
    snapshot.caret = {static_cast<LONG>(36 * dpi / 96), static_cast<LONG>(98 * dpi / 96),
                      static_cast<LONG>(38 * dpi / 96), static_cast<LONG>(118 * dpi / 96)};
    snapshot.words = {{L"receive", "local", {3}}, {L"review", "learned", {}}};
    snapshot.selection = 0;
    snapshot.typedFragment = L"recieve";
    snapshot.phraseLead = L" the";
    snapshot.phrase = L" outline remains readable at every display scale";
    snapshot.partialBegin = 0;
    snapshot.partialLength = 9;
    snapshot.ai = completionist::render::AiState::Streaming;
    snapshot.elapsedMs = 800;
    snapshot.engineConnected = true;
    snapshot.settings.font_size = fontSizePoints;
    completionist::layout::WorkArea work{{0, 0, static_cast<LONG>(width), static_cast<LONG>(height)}, dpi};
    renderer::text::TextRenderer textRenderer(writeFactory.Get());
    renderer::text::PreparedText prepared;
    completionist::layout::Layout layout;
    const bool laidOut = textRenderer.Prepare(snapshot, 330.0f, &prepared);
    if (laidOut) layout = completionist::layout::Place(snapshot, work, prepared.metrics);
    const float materialTint[4]{
        static_cast<float>(colors.surface.r) / 255.0f,
        static_cast<float>(colors.surface.g) / 255.0f,
        static_cast<float>(colors.surface.b) / 255.0f,
        dark ? .35f : .24f};
    auto panelViewport = [](const completionist::render::Rect& bounds) {
        return D3D11_VIEWPORT{static_cast<float>(bounds.left), static_cast<float>(bounds.top),
            static_cast<float>(bounds.right - bounds.left), static_cast<float>(bounds.bottom - bounds.top), 0, 1};
    };
    bool materialReady = false;
    if (laidOut) {
        const auto menuViewport = panelViewport(layout.menuBounds);
        const auto dockViewport = panelViewport(layout.dockBounds);
        materialReady = blur.renderLens(context.Get(), menuViewport, 26.0f, 18.0f,
                                        static_cast<float>(dpi), materialTint, true) &&
            blur.renderLens(context.Get(), dockViewport, 26.0f, 18.0f,
                            static_cast<float>(dpi), materialTint, false);
    }
    ComPtr<IDXGISurface> glassSurface;
    ComPtr<ID2D1Bitmap1> glassLayer;
    const auto glassProperties = D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_NONE,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
        static_cast<float>(dpi), static_cast<float>(dpi));
    materialReady = materialReady && SUCCEEDED(blur.glassTexture.As(&glassSurface)) &&
        SUCCEEDED(drawing->CreateBitmapFromDxgiSurface(glassSurface.Get(), &glassProperties, &glassLayer));
    bool sharpTextDrawn = false;
    if (laidOut && materialReady) {
        drawing->DrawBitmap(glassLayer.Get(), D2D1::RectF(0, 0, static_cast<float>(width) / (static_cast<float>(dpi) / 96.0f),
            static_cast<float>(height) / (static_cast<float>(dpi) / 96.0f)), 1.0f,
            D2D1_INTERPOLATION_MODE_NEAREST_NEIGHBOR);
        const float scale = static_cast<float>(dpi) / 96.0f;
        drawing->SetTransform(D2D1::Matrix3x2F::Translation(
            static_cast<float>(layout.menuBounds.left) / scale,
            static_cast<float>(layout.menuBounds.top) / scale));
        const bool menuDrawn = textRenderer.Draw(drawing.Get(), snapshot, layout, prepared, colors,
                                                  renderer::text::Surface::Menu,1.0f,true);
        drawing->SetTransform(D2D1::Matrix3x2F::Translation(
            static_cast<float>(layout.dockBounds.left) / scale,
            static_cast<float>(layout.dockBounds.top) / scale));
        const bool dockDrawn = textRenderer.Draw(drawing.Get(), snapshot, layout, prepared, colors,
                                                  renderer::text::Surface::Dock,1.0f,true);
        sharpTextDrawn = menuDrawn && dockDrawn;
        drawing->SetTransform(D2D1::Matrix3x2F::Identity());
        prepared.Reset();
    }
    const HRESULT drawResult = drawing->EndDraw();
    drawing->SetTarget(nullptr);
    if (!laidOut || !materialReady || !sharpTextDrawn || FAILED(drawResult)) return false;
    return savePng(outputPath, context.Get(), blur.outputTexture.Get());
}

bool renderFixtureMatrix(const std::wstring& outputDirectory) {
    if (outputDirectory.empty()) return false;
    const std::filesystem::path directory(outputDirectory);
    std::error_code error;
    std::filesystem::create_directories(directory, error);
    if (error) return false;
    for (UINT dpi : {96U, 144U, 192U}) {
        for (bool dark : {false, true}) {
            const auto name = std::wstring(L"completionist-") + std::to_wstring(dpi) +
                (dark ? L"-dark.png" : L"-light.png");
            if (!renderFixture((directory / name).wstring(), dpi, dark, 12)) return false;
            const auto largestFontName = std::wstring(L"completionist-") + std::to_wstring(dpi) +
                (dark ? L"-dark-24pt.png" : L"-light-24pt.png");
            if (!renderFixture((directory / largestFontName).wstring(), dpi, dark, 24)) return false;
        }
    }
    return true;
}
}
