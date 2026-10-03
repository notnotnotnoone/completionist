#define NOMINMAX
#include "fixture.h"
#include "material.h"
#include "shaders.h"
#include "wic_layout.h"

#include <windows.h>
#include <d2d1_1.h>
#include <dwrite.h>
#include <wincodec.h>
#include <wrl/client.h>
#include <cstdint>
#include <vector>

namespace renderer {
namespace {
using Microsoft::WRL::ComPtr;
constexpr UINT kWidth = 640;
constexpr UINT kHeight = 360;

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

bool renderFixture(const std::wstring& outputPath) {
    if (outputPath.empty()) return false;
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

    std::vector<std::uint32_t> pixels(static_cast<size_t>(kWidth) * kHeight);
    for (UINT y = 0; y < kHeight; ++y) {
        for (UINT x = 0; x < kWidth; ++x) {
            const bool stripe = ((x / 24U) + (y / 24U)) % 2U == 0;
            const std::uint8_t r = static_cast<std::uint8_t>(stripe ? 24U + x % 80U : 180U - y % 80U);
            const std::uint8_t g = static_cast<std::uint8_t>(stripe ? 74U + y % 100U : 36U + x % 100U);
            const std::uint8_t b = static_cast<std::uint8_t>(stripe ? 50U : 110U + (x + y) % 100U);
            pixels[static_cast<size_t>(y) * kWidth + x] = 0xff000000U | (static_cast<std::uint32_t>(r) << 16U) |
                (static_cast<std::uint32_t>(g) << 8U) | b;
        }
    }
    D3D11_TEXTURE2D_DESC inputDesc{};
    inputDesc.Width = kWidth; inputDesc.Height = kHeight; inputDesc.MipLevels = 1; inputDesc.ArraySize = 1;
    inputDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM; inputDesc.SampleDesc.Count = 1;
    inputDesc.Usage = D3D11_USAGE_DEFAULT; inputDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA inputData{pixels.data(), kWidth * sizeof(std::uint32_t), 0};
    ComPtr<ID3D11Texture2D> input;
    ComPtr<ID3D11ShaderResourceView> inputView;
    if (FAILED(device->CreateTexture2D(&inputDesc, &inputData, &input)) ||
        FAILED(device->CreateShaderResourceView(input.Get(), nullptr, &inputView))) return false;

    BlurMaterial blur;
    if (!blur.create(device.Get(), kWidth, kHeight) || !blur.blur(context.Get(), inputView.Get())) return false;

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
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));
    if (FAILED(drawing->CreateBitmapFromDxgiSurface(surface.Get(), &properties, &target))) return false;
    drawing->SetTarget(target.Get());
    drawing->BeginDraw();
    ComPtr<ID2D1SolidColorBrush> shadow;
    ComPtr<ID2D1SolidColorBrush> glass;
    ComPtr<ID2D1SolidColorBrush> ink;
    ComPtr<IDWriteTextFormat> format;
    const bool resourcesReady = SUCCEEDED(drawing->CreateSolidColorBrush(D2D1::ColorF(0, 0.06f), &shadow)) &&
        SUCCEEDED(drawing->CreateSolidColorBrush(D2D1::ColorF(10.f / 255.f, 90.f / 255.f, 61.f / 255.f, 0.16f), &glass)) &&
        SUCCEEDED(drawing->CreateSolidColorBrush(D2D1::ColorF(1, 1, 1, 0.98f), &ink)) &&
        SUCCEEDED(writeFactory->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_NORMAL,
            DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 21.0f, L"en-us", &format));
    if (resourcesReady) {
        const auto panel = D2D1::RoundedRect(D2D1::RectF(111, 83, 529, 279), 26, 26);
        drawing->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(111, 88, 529, 284), 26, 26), shadow.Get());
        drawing->FillRoundedRectangle(panel, glass.Get());
        drawing->DrawTextW(L"FIXTURE  Sharp foreground", 25, format.Get(), D2D1::RectF(137, 112, 504, 151), ink.Get());
        drawing->DrawTextW(L"Local     Evergreen tint", 22, format.Get(), D2D1::RectF(137, 162, 504, 202), ink.Get());
        drawing->DrawTextW(L"Generated pattern only", 22, format.Get(), D2D1::RectF(137, 212, 504, 252), ink.Get());
    }
    const HRESULT drawResult = drawing->EndDraw();
    drawing->SetTarget(nullptr);
    if (!resourcesReady || FAILED(drawResult)) return false;
    return savePng(outputPath, context.Get(), blur.outputTexture.Get());
}
}
