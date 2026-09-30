// Loads out\TyperTip.dll without registering it, activates the text service on a real TSF thread
// manager and tears it down, many times. Catches crashes, hangs and leaks in the lifecycle (popup
// window, engine client thread, sinks) that would otherwise take down every app the user types in.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <msctf.h>

#include <cstdio>

// {71B17AFC-9D1A-4E42-A7C3-2F4151AC6ABF}
constexpr CLSID CLSID_TyperService = {0x71b17afc, 0x9d1a, 0x4e42, {0xa7, 0xc3, 0x2f, 0x41, 0x51, 0xac, 0x6a, 0xbf}};

static void Pump(DWORD ms) {
    ULONGLONG end = GetTickCount64() + ms;
    while (GetTickCount64() < end) {
        MSG msg;
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) DispatchMessageW(&msg);
        MsgWaitForMultipleObjects(0, nullptr, FALSE, 5, QS_ALLINPUT);
    }
}

#define REQUIRE(expr)                                                    \
    do {                                                                 \
        if (!(expr)) {                                                   \
            std::printf("FAIL line %d: %s\n", __LINE__, #expr);          \
            return 1;                                                    \
        }                                                                \
    } while (0)

int wmain(int argc, wchar_t** argv) {
    if (argc < 2) {
        std::printf("usage: load_test <path-to-TyperTip.dll> [cycles]\n");
        return 2;
    }
    int cycles = argc > 2 ? _wtoi(argv[2]) : 30;
    REQUIRE(SUCCEEDED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED)));

    HMODULE dll = LoadLibraryW(argv[1]);
    REQUIRE(dll != nullptr);
    auto getClassObject = reinterpret_cast<HRESULT(STDAPICALLTYPE*)(REFCLSID, REFIID, void**)>(GetProcAddress(dll, "DllGetClassObject"));
    auto canUnload = reinterpret_cast<HRESULT(STDAPICALLTYPE*)()>(GetProcAddress(dll, "DllCanUnloadNow"));
    REQUIRE(getClassObject && canUnload);

    IClassFactory* factory = nullptr;
    REQUIRE(SUCCEEDED(getClassObject(CLSID_TyperService, IID_IClassFactory, reinterpret_cast<void**>(&factory))));

    ITfThreadMgr* threadMgr = nullptr;
    REQUIRE(SUCCEEDED(CoCreateInstance(CLSID_TF_ThreadMgr, nullptr, CLSCTX_INPROC_SERVER, IID_ITfThreadMgr, reinterpret_cast<void**>(&threadMgr))));
    TfClientId clientId = 0;
    REQUIRE(SUCCEEDED(threadMgr->Activate(&clientId)));

    ULONGLONG start = GetTickCount64();
    for (int i = 0; i < cycles; ++i) {
        ITfTextInputProcessorEx* tip = nullptr;
        REQUIRE(SUCCEEDED(factory->CreateInstance(nullptr, IID_ITfTextInputProcessorEx, reinterpret_cast<void**>(&tip))));
        REQUIRE(SUCCEEDED(tip->ActivateEx(threadMgr, clientId, 0)));
        Pump(i % 3 == 0 ? 60 : 5);  // sometimes long enough for the client thread to try connecting
        REQUIRE(SUCCEEDED(tip->Deactivate()));
        tip->Release();
    }
    ULONGLONG elapsed = GetTickCount64() - start;

    // Two instances at once (two threads' worth of documents in one process), torn down in reverse.
    ITfTextInputProcessorEx *a = nullptr, *b = nullptr;
    REQUIRE(SUCCEEDED(factory->CreateInstance(nullptr, IID_ITfTextInputProcessorEx, reinterpret_cast<void**>(&a))));
    REQUIRE(SUCCEEDED(factory->CreateInstance(nullptr, IID_ITfTextInputProcessorEx, reinterpret_cast<void**>(&b))));
    REQUIRE(SUCCEEDED(a->ActivateEx(threadMgr, clientId, 0)));
    REQUIRE(SUCCEEDED(b->ActivateEx(threadMgr, clientId, 0)));
    Pump(50);
    REQUIRE(SUCCEEDED(b->Deactivate()));
    REQUIRE(SUCCEEDED(a->Deactivate()));
    a->Release();
    b->Release();

    threadMgr->Deactivate();
    threadMgr->Release();
    factory->Release();
    Pump(20);
    REQUIRE(canUnload() == S_OK);  // nothing leaked: the DLL would be allowed to unload
    std::printf("load_test ok: %d activate/deactivate cycles in %llu ms, DLL unloadable\n", cycles, elapsed);
    CoUninitialize();
    return 0;
}
