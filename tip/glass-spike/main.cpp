// THROWAWAY: standalone native material probe; no TSF, engine, or desktop capture.
#define NOMINMAX
#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <vector>
#include <string>
#include <fstream>
#include <algorithm>
#include <cstdio>
#include <cmath>
using Microsoft::WRL::ComPtr;
constexpr int W=1040,H=720;
HWND windowHandle;
ComPtr<ID3D11Device> device;
ComPtr<ID3D11DeviceContext> context;
ComPtr<IDXGISwapChain> swap;
ComPtr<ID3D11RenderTargetView> target;
ComPtr<ID3D11VertexShader> vs;
ComPtr<ID3D11PixelShader> ps;
ComPtr<ID3D11Buffer> constants;
ComPtr<ID3D11SamplerState> sampler;
ComPtr<ID3D11ShaderResourceView> scene,labels,blurredScene;
struct Params { float width=W,height=H,x=560,y=240,strength=18,time=0,dark=0,plain=0,collapsed=0,px=650,py=240,pad=0; } params;
bool dragging=false,animate=true;
POINT grab{};
void check(HRESULT hr,const char* what) {
    if(FAILED(hr)) { char message[240]; sprintf_s(message,"%s failed: 0x%08X",what,static_cast<unsigned>(hr)); throw std::runtime_error(message); }
}
struct Canvas {
    HDC dc=CreateCompatibleDC(nullptr); HBITMAP bmp=nullptr; HGDIOBJ old=nullptr; unsigned* bits=nullptr; int width,height;
    Canvas(int w,int h):width(w),height(h) {
        BITMAPINFO bi{}; bi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER); bi.bmiHeader.biWidth=w; bi.bmiHeader.biHeight=-h;
        bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;bi.bmiHeader.biCompression=BI_RGB;
        bmp=CreateDIBSection(dc,&bi,DIB_RGB_COLORS,reinterpret_cast<void**>(&bits),nullptr,0);
        if(!bmp) throw std::runtime_error("DIB creation failed");
        old=SelectObject(dc,bmp);SetBkMode(dc,TRANSPARENT);
    }
    ~Canvas(){SelectObject(dc,old);DeleteObject(bmp);DeleteDC(dc);}
    void rectangle(int x,int y,int w,int h,COLORREF color){ RECT r{x,y,x+w,y+h};HBRUSH b=CreateSolidBrush(color);FillRect(dc,&r,b);DeleteObject(b); }
    void text(int x,int y,const wchar_t* value,int fontHeight,COLORREF color,int weight=FW_NORMAL) {
        HFONT f=CreateFontW(-fontHeight,0,0,0,weight,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,DEFAULT_PITCH,L"Segoe UI");
        HGDIOBJ previous=SelectObject(dc,f);SetTextColor(dc,color);TextOutW(dc,x,y,value,static_cast<int>(wcslen(value)));SelectObject(dc,previous);DeleteObject(f);
    }
    ComPtr<ID3D11ShaderResourceView> upload() {
        GdiFlush();D3D11_TEXTURE2D_DESC desc{};desc.Width=width;desc.Height=height;desc.MipLevels=1;desc.ArraySize=1;
        desc.Format=DXGI_FORMAT_B8G8R8A8_UNORM;desc.SampleDesc.Count=1;desc.Usage=D3D11_USAGE_IMMUTABLE;desc.BindFlags=D3D11_BIND_SHADER_RESOURCE;
        D3D11_SUBRESOURCE_DATA data{bits,static_cast<UINT>(width*4),0};ComPtr<ID3D11Texture2D> tex;ComPtr<ID3D11ShaderResourceView> view;
        check(device->CreateTexture2D(&desc,&data,&tex),"Texture");check(device->CreateShaderResourceView(tex.Get(),nullptr,&view),"Texture view");return view;
    }
};
void makeScene() {
    Canvas c(W,H);bool dark=params.dark>.5;
    COLORREF paper=dark?RGB(14,21,18):RGB(238,242,236),ink=dark?RGB(228,235,230):RGB(22,34,28);
    COLORREF muted=dark?RGB(151,166,157):RGB(85,99,91),pine=dark?RGB(15,107,72):RGB(10,90,61);
    c.rectangle(0,0,W,H,paper);
    c.text(44,30,L"Completionist / native material study",19,ink,FW_SEMIBOLD);
    c.text(44,62,L"Drag the lens across text. Wheel: refraction.  A: frost comparison.  D: theme.  M: motion.  Esc: close.",13,muted);
    c.rectangle(44,115,952,1,dark?RGB(37,48,42):RGB(213,221,214));
    c.text(65,150,L"A clearer thought.",48,ink,FW_SEMIBOLD);
    c.text(65,222,L"I want to separate the idea from the noise.",23,ink);
    const wchar_t* lines[]={L"The surface should feel like a lens: clear through its center,",L"curved at the edge, with light resting along the rim.",L"Words stay sharp. The world underneath bends.",L"A little refraction reveals the material. Too much hides the work."};
    for(int i=0;i<4;i++) c.text(65,282+i*43,lines[i],18,muted);
    for(int y=314;y<490;y+=43) c.rectangle(60,y,920,1,dark?RGB(37,48,42):RGB(213,221,214));
    c.rectangle(68,493,250,30,pine);c.text(83,498,L"EVERGREEN  /  material probe",13,RGB(255,255,255),FW_SEMIBOLD);
    c.rectangle(450,493,150,30,dark?RGB(192,24,141):RGB(171,63,132));
    c.rectangle(700,493,275,30,dark?RGB(52,66,58):RGB(183,195,186));
    c.text(270,635,L"C++  /  D3D11 + HLSL    |    simulated backdrop    |    no desktop capture",13,muted);
    scene=c.upload();
    // True separable Gaussian convolution, once per static fixture/theme.
    constexpr int radius=18;
    constexpr float sigma=6.f;
    float weights[radius*2+1];float total=0;
    for(int k=-radius;k<=radius;k++){weights[k+radius]=std::exp(-float(k*k)/(2*sigma*sigma));total+=weights[k+radius];}
    for(float& weight:weights)weight/=total;
    std::vector<float> horizontal(W*H*3);
    for(int y=0;y<H;y++)for(int x=0;x<W;x++)for(int channel=0;channel<3;channel++) {
        float sum=0;
        for(int k=-radius;k<=radius;k++) {
            unsigned value=c.bits[y*W+std::clamp(x+k,0,W-1)];
            sum+=float((value>>(channel*8))&255)*weights[k+radius];
        }
        horizontal[(y*W+x)*3+channel]=sum;
    }
    for(int y=0;y<H;y++)for(int x=0;x<W;x++) {
        unsigned value=0xff000000;
        for(int channel=0;channel<3;channel++) {
            float sum=0;
            for(int k=-radius;k<=radius;k++)sum+=horizontal[(std::clamp(y+k,0,H-1)*W+x)*3+channel]*weights[k+radius];
            value|=static_cast<unsigned>(std::clamp(sum+.5f,0.f,255.f))<<(channel*8);
        }
        c.bits[y*W+x]=value;
    }
    blurredScene=c.upload();
}
void makeLabels() {
    Canvas c(350,286);c.rectangle(0,0,350,286,RGB(0,0,0));COLORREF white=RGB(255,255,255);
    c.text(18,16,L"Completionist",11,white,FW_SEMIBOLD);
    c.text(20,44,L"separate",21,white,FW_SEMIBOLD);c.text(280,50,L"Local",11,white);
    c.text(20,86,L"separation",19,white);c.text(280,91,L"Local",11,white);
    c.text(20,126,L"separately",19,white);c.text(264,131,L"Learned",11,white);
    c.text(20,155,L"seper  \x2192  separate",11,white);
    c.text(35,189,L"AI",11,white,FW_SEMIBOLD);c.text(179,189,L"0.4s until AI",11,white);
    c.text(20,215,L"the idea from the noise",17,white);c.text(20,240,L"Tense   Present  (simulated)",11,white);
    labels=c.upload();
}
void initialize(HWND hwnd,const std::wstring& directory) {
    DXGI_SWAP_CHAIN_DESC desc{};desc.BufferDesc.Width=W;desc.BufferDesc.Height=H;desc.BufferDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count=1;desc.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;desc.BufferCount=2;desc.OutputWindow=hwnd;desc.Windowed=TRUE;desc.SwapEffect=DXGI_SWAP_EFFECT_DISCARD;
    D3D_FEATURE_LEVEL level;
    HRESULT hr=D3D11CreateDeviceAndSwapChain(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&desc,&swap,&device,&level,&context);
    if(FAILED(hr)) hr=D3D11CreateDeviceAndSwapChain(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&desc,&swap,&device,&level,&context);
    check(hr,"D3D11 initialization");ComPtr<ID3D11Texture2D> back;check(swap->GetBuffer(0,IID_PPV_ARGS(&back)),"Backbuffer");
    check(device->CreateRenderTargetView(back.Get(),nullptr,&target),"Render target");
    ComPtr<ID3DBlob> vertex,pixel,errors;
    auto shader=directory+L"\\glass.hlsl";
    check(D3DCompileFromFile(shader.c_str(),nullptr,nullptr,"VS","vs_4_0",D3DCOMPILE_ENABLE_STRICTNESS,0,&vertex,&errors),"Vertex shader compile");
    hr=D3DCompileFromFile(shader.c_str(),nullptr,nullptr,"PS","ps_4_0",D3DCOMPILE_ENABLE_STRICTNESS,0,&pixel,&errors);
    if(FAILED(hr) && errors) OutputDebugStringA(static_cast<char*>(errors->GetBufferPointer()));
    check(hr,"Pixel shader compile");
    check(device->CreateVertexShader(vertex->GetBufferPointer(),vertex->GetBufferSize(),nullptr,&vs),"Vertex shader");
    check(device->CreatePixelShader(pixel->GetBufferPointer(),pixel->GetBufferSize(),nullptr,&ps),"Pixel shader");
    D3D11_BUFFER_DESC bd{};bd.ByteWidth=sizeof(Params);bd.Usage=D3D11_USAGE_DEFAULT;bd.BindFlags=D3D11_BIND_CONSTANT_BUFFER;
    check(device->CreateBuffer(&bd,nullptr,&constants),"Constants");
    D3D11_SAMPLER_DESC sd{};sd.Filter=D3D11_FILTER_MIN_MAG_MIP_LINEAR;sd.AddressU=sd.AddressV=sd.AddressW=D3D11_TEXTURE_ADDRESS_CLAMP;sd.MaxLOD=D3D11_FLOAT32_MAX;
    check(device->CreateSamplerState(&sd,&sampler),"Sampler");makeScene();makeLabels();
}
void render(bool present=true) {
    context->UpdateSubresource(constants.Get(),0,nullptr,&params,0,0);
    ID3D11RenderTargetView* rt=target.Get();context->OMSetRenderTargets(1,&rt,nullptr);
    D3D11_VIEWPORT vp{0,0,static_cast<float>(W),static_cast<float>(H),0,1};context->RSSetViewports(1,&vp);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context->VSSetShader(vs.Get(),nullptr,0);context->PSSetShader(ps.Get(),nullptr,0);
    ID3D11Buffer* cb=constants.Get();context->PSSetConstantBuffers(0,1,&cb);
    ID3D11ShaderResourceView* textures[]={scene.Get(),labels.Get(),blurredScene.Get()};context->PSSetShaderResources(0,3,textures);
    ID3D11SamplerState* s=sampler.Get();context->PSSetSamplers(0,1,&s);context->Draw(3,0);
    if(present) check(swap->Present(1,0),"Present");
}
void exportImage(const std::wstring& path) {
    render(false);ComPtr<ID3D11Texture2D> back,staging;check(swap->GetBuffer(0,IID_PPV_ARGS(&back)),"Export buffer");
    D3D11_TEXTURE2D_DESC td;back->GetDesc(&td);td.Usage=D3D11_USAGE_STAGING;td.BindFlags=0;td.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
    check(device->CreateTexture2D(&td,nullptr,&staging),"Export texture");context->CopyResource(staging.Get(),back.Get());
    D3D11_MAPPED_SUBRESOURCE map;check(context->Map(staging.Get(),0,D3D11_MAP_READ,0,&map),"Export map");
    std::vector<unsigned char> bytes(W*H*4);
    for(int y=0;y<H;y++) for(int x=0;x<W;x++) {
        const auto* src=static_cast<unsigned char*>(map.pData)+y*map.RowPitch+x*4;auto* dst=bytes.data()+(y*W+x)*4;
        dst[0]=src[2];dst[1]=src[1];dst[2]=src[0];dst[3]=255;
    }
    context->Unmap(staging.Get(),0);BITMAPFILEHEADER fh{};fh.bfType=0x4d42;fh.bfOffBits=sizeof(fh)+sizeof(BITMAPINFOHEADER);fh.bfSize=fh.bfOffBits+static_cast<DWORD>(bytes.size());
    BITMAPINFOHEADER ih{};ih.biSize=sizeof(ih);ih.biWidth=W;ih.biHeight=-H;ih.biPlanes=1;ih.biBitCount=32;ih.biSizeImage=static_cast<DWORD>(bytes.size());
    std::ofstream file(path,std::ios::binary);file.write(reinterpret_cast<char*>(&fh),sizeof(fh));file.write(reinterpret_cast<char*>(&ih),sizeof(ih));file.write(reinterpret_cast<char*>(bytes.data()),bytes.size());
    if(!file) throw std::runtime_error("Export file write failed");
}
LRESULT CALLBACK procedure(HWND hwnd,UINT msg,WPARAM w,LPARAM l) {
    int x=static_cast<short>(LOWORD(l)),y=static_cast<short>(HIWORD(l));
    switch(msg) {
    case WM_DESTROY:PostQuitMessage(0);return 0;
    case WM_LBUTTONDOWN:
        if(x>params.x && x<params.x+350 && y>params.y && y<params.y+286) {dragging=true;grab={x-static_cast<LONG>(params.x),y-static_cast<LONG>(params.y)};SetCapture(hwnd);}
        else if(x>=28 && x<=218 && y>=H-108 && y<=H-30) params.collapsed=1-params.collapsed;
        return 0;
    case WM_LBUTTONUP:dragging=false;ReleaseCapture();return 0;
    case WM_MOUSEMOVE:params.px=static_cast<float>(x);params.py=static_cast<float>(y);
        if(dragging){params.x=static_cast<float>(std::clamp(x-grab.x,0L,static_cast<LONG>(W-350)));params.y=static_cast<float>(std::clamp(y-grab.y,90L,static_cast<LONG>(H-286)));}return 0;
    case WM_MOUSEWHEEL:params.strength=std::clamp(params.strength+static_cast<short>(HIWORD(w))/120.f*3,0.f,60.f);return 0;
    case WM_KEYDOWN:
        if(w==VK_ESCAPE) DestroyWindow(hwnd);
        if(w=='D'){params.dark=1-params.dark;makeScene();}
        if(w=='A')params.plain=1-params.plain;
        if(w=='M')animate=!animate;
        return 0;
    }
    return DefWindowProcW(hwnd,msg,w,l);
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,PWSTR args,int) {
    try {
        SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
        bool exporting=wcsstr(args,L"--export")!=nullptr;
        if(wcsstr(args,L"--dark")) params.dark=1;
        if(wcsstr(args,L"--frost")) params.plain=1;
        wchar_t exe[MAX_PATH];GetModuleFileNameW(nullptr,exe,MAX_PATH);std::wstring dir=exe;dir=dir.substr(0,dir.find_last_of(L"\\"));
        WNDCLASSW wc{};wc.hInstance=instance;wc.lpfnWndProc=procedure;wc.lpszClassName=L"CompletionistGlassSpike";wc.hCursor=LoadCursor(nullptr,IDC_ARROW);RegisterClassW(&wc);
        RECT r{0,0,W,H};DWORD style=WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX;AdjustWindowRect(&r,style,FALSE);
        windowHandle=CreateWindowW(wc.lpszClassName,L"Completionist — liquid glass spike (throwaway)",style,CW_USEDEFAULT,CW_USEDEFAULT,r.right-r.left,r.bottom-r.top,nullptr,nullptr,instance,nullptr);
        if(!windowHandle) throw std::runtime_error("Window creation failed");initialize(windowHandle,dir);params.time=1.6f;
        if(exporting){exportImage(dir+(params.plain>.5?L"\\frost.bmp":params.dark>.5?L"\\glass-dark.bmp":L"\\glass-light.bmp"));DestroyWindow(windowHandle);return 0;}
        BOOL motion=TRUE;SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION,0,&motion,0);animate=motion!=FALSE;
        ShowWindow(windowHandle,SW_SHOWNORMAL);MSG msg{};ULONGLONG start=GetTickCount64();
        while(msg.message!=WM_QUIT) {
            while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessageW(&msg);}
            if(msg.message==WM_QUIT)break;
            if(animate)params.time=static_cast<float>(GetTickCount64()-start)/1000;
            if(!IsIconic(windowHandle))render();else Sleep(60);
        }
        return 0;
    } catch(const std::exception& e) { if(wcsstr(args,L"--export")) {OutputDebugStringA(e.what());return 1;}MessageBoxA(nullptr,e.what(),"Glass spike failed",MB_ICONERROR);return 1; }
}
