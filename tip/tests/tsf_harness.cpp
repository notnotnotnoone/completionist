// A stand-in for an app's text field, hosting the real TyperTip.dll through real TSF (no registration,
// no admin, no settings changes). It implements ITextStoreACP, "types" characters into it the way an
// app would, sends keys through ITfKeystrokeMgr like an app's key handler, and checks what the text
// service did: popup shown, keys consumed, words inserted. Needs the engine running on the default pipe.
//
//   tsf_harness.exe <path-to-TyperTip.dll> <screenshot-folder> [unaware]
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <msctf.h>
#include <textstor.h>

#include <cstdio>
#include <deque>
#include <functional>
#include <string>

// {71B17AFC-9D1A-4E42-A7C3-2F4151AC6ABF}
constexpr CLSID CLSID_TyperService = {0x71b17afc, 0x9d1a, 0x4e42, {0xa7, 0xc3, 0x2f, 0x41, 0x51, 0xac, 0x6a, 0xbf}};

static int g_failures = 0;
static void Check(bool ok, const char* what) {
    std::printf("  %s %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok) ++g_failures;
}

static HWND g_window = nullptr;
static std::function<bool()> g_refocus;  // re-takes the foreground and TSF focus; set once the harness window exists

// ---------------------------------------------------------------------------------------------
class TextStore final : public ITextStoreACP {
public:
    std::wstring text;
    LONG selStart = 0, selEnd = 0;

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
        if (riid == IID_IUnknown || riid == IID_ITextStoreACP) {
            *ppv = static_cast<ITextStoreACP*>(this);
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    STDMETHODIMP_(ULONG) AddRef() override { return 2; }
    STDMETHODIMP_(ULONG) Release() override { return 1; }

    STDMETHODIMP AdviseSink(REFIID riid, IUnknown* unk, DWORD) override {
        if (riid != IID_ITextStoreACPSink) return E_INVALIDARG;
        if (sink_) return E_FAIL;
        return unk->QueryInterface(IID_ITextStoreACPSink, reinterpret_cast<void**>(&sink_));
    }
    STDMETHODIMP UnadviseSink(IUnknown*) override {
        if (sink_) sink_->Release();
        sink_ = nullptr;
        return S_OK;
    }
    STDMETHODIMP RequestLock(DWORD flags, HRESULT* session) override {
        if (!sink_) return E_UNEXPECTED;
        if (lock_) {
            if (flags & TS_LF_SYNC) {
                *session = TS_E_SYNCHRONOUS;
                return S_OK;
            }
            pending_.push_back(flags);
            *session = TS_S_ASYNC;
            return S_OK;
        }
        lock_ = flags & ~TS_LF_SYNC;
        *session = sink_->OnLockGranted(flags);
        while (!pending_.empty()) {
            lock_ = pending_.front();
            pending_.pop_front();
            sink_->OnLockGranted(lock_);
        }
        lock_ = 0;
        return S_OK;
    }
    STDMETHODIMP GetStatus(TS_STATUS* status) override {
        status->dwDynamicFlags = 0;
        status->dwStaticFlags = TS_SS_NOHIDDENTEXT;
        return S_OK;
    }
    STDMETHODIMP QueryInsert(LONG start, LONG end, ULONG cch, LONG* rs, LONG* re) override {
        *rs = start;
        *re = end;
        (void)cch;
        return S_OK;
    }
    STDMETHODIMP GetSelection(ULONG index, ULONG count, TS_SELECTION_ACP* sel, ULONG* fetched) override {
        if (!(lock_ & TS_LF_READ)) return TS_E_NOLOCK;
        if (index != 0 && index != TS_DEFAULT_SELECTION) return TS_E_NOSELECTION;
        if (count < 1) return E_INVALIDARG;
        sel[0].acpStart = selStart;
        sel[0].acpEnd = selEnd;
        sel[0].style.ase = TS_AE_END;
        sel[0].style.fInterimChar = FALSE;
        *fetched = 1;
        return S_OK;
    }
    STDMETHODIMP SetSelection(ULONG count, const TS_SELECTION_ACP* sel) override {
        if ((lock_ & TS_LF_READWRITE) != TS_LF_READWRITE) return TS_E_NOLOCK;
        if (count >= 1) {
            selStart = sel[0].acpStart;
            selEnd = sel[0].acpEnd;
        }
        return S_OK;
    }
    STDMETHODIMP GetText(LONG start, LONG end, WCHAR* plain, ULONG plainReq, ULONG* plainRet, TS_RUNINFO* runs,
                         ULONG runsReq, ULONG* runsRet, LONG* next) override {
        if (!(lock_ & TS_LF_READ)) return TS_E_NOLOCK;
        LONG last = static_cast<LONG>(text.size());
        if (end == -1) end = last;
        if (start < 0 || start > last || end > last || end < start) return TS_E_INVALIDPOS;
        ULONG n = std::min<ULONG>(plainReq, static_cast<ULONG>(end - start));
        if (plain) std::copy(text.begin() + start, text.begin() + start + n, plain);
        *plainRet = n;
        if (runs && runsReq > 0) {
            runs[0].uCount = n;
            runs[0].type = TS_RT_PLAIN;
            *runsRet = 1;
        } else if (runsRet) {
            *runsRet = 0;
        }
        *next = start + static_cast<LONG>(n);
        return S_OK;
    }
    STDMETHODIMP SetText(DWORD, LONG start, LONG end, const WCHAR* chars, ULONG cch, TS_TEXTCHANGE* change) override {
        if ((lock_ & TS_LF_READWRITE) != TS_LF_READWRITE) return TS_E_NOLOCK;
        LONG last = static_cast<LONG>(text.size());
        if (start < 0 || end > last || end < start) return TS_E_INVALIDPOS;
        text.replace(start, end - start, chars, cch);
        change->acpStart = start;
        change->acpOldEnd = end;
        change->acpNewEnd = start + static_cast<LONG>(cch);
        selStart = selEnd = change->acpNewEnd;
        return S_OK;
    }
    STDMETHODIMP GetFormattedText(LONG, LONG, IDataObject**) override { return E_NOTIMPL; }
    STDMETHODIMP GetEmbedded(LONG, REFGUID, REFIID, IUnknown**) override { return E_NOTIMPL; }
    STDMETHODIMP QueryInsertEmbedded(const GUID*, const FORMATETC*, BOOL* ok) override {
        *ok = FALSE;
        return S_OK;
    }
    STDMETHODIMP InsertEmbedded(DWORD, LONG, LONG, IDataObject*, TS_TEXTCHANGE*) override { return E_NOTIMPL; }
    STDMETHODIMP InsertTextAtSelection(DWORD flags, const WCHAR* chars, ULONG cch, LONG* start, LONG* end,
                                       TS_TEXTCHANGE* change) override {
        if ((lock_ & TS_LF_READWRITE) != TS_LF_READWRITE) return TS_E_NOLOCK;
        LONG s = selStart, e = selEnd;
        if (flags & TS_IAS_QUERYONLY) {
            if (start) *start = s;
            if (end) *end = s + static_cast<LONG>(cch);
            return S_OK;
        }
        text.replace(s, e - s, chars, cch);
        selStart = selEnd = s + static_cast<LONG>(cch);
        if (start) *start = s;
        if (end) *end = selEnd;
        if (change) {
            change->acpStart = s;
            change->acpOldEnd = e;
            change->acpNewEnd = selEnd;
        }
        return S_OK;
    }
    STDMETHODIMP InsertEmbeddedAtSelection(DWORD, IDataObject*, LONG*, LONG*, TS_TEXTCHANGE*) override { return E_NOTIMPL; }
    STDMETHODIMP RequestSupportedAttrs(DWORD, ULONG, const TS_ATTRID*) override { return E_NOTIMPL; }
    STDMETHODIMP RequestAttrsAtPosition(LONG, ULONG, const TS_ATTRID*, DWORD) override { return E_NOTIMPL; }
    STDMETHODIMP RequestAttrsTransitioningAtPosition(LONG, ULONG, const TS_ATTRID*, DWORD) override { return E_NOTIMPL; }
    STDMETHODIMP FindNextAttrTransition(LONG, LONG, ULONG, const TS_ATTRID*, DWORD, LONG*, BOOL*, LONG*) override { return E_NOTIMPL; }
    STDMETHODIMP RetrieveRequestedAttrs(ULONG, TS_ATTRVAL*, ULONG* fetched) override {
        *fetched = 0;
        return S_OK;
    }
    STDMETHODIMP GetEndACP(LONG* acp) override {
        if (!(lock_ & TS_LF_READ)) return TS_E_NOLOCK;
        *acp = static_cast<LONG>(text.size());
        return S_OK;
    }
    STDMETHODIMP GetActiveView(TsViewCookie* view) override {
        *view = 1;
        return S_OK;
    }
    STDMETHODIMP GetACPFromPoint(TsViewCookie, const POINT*, DWORD, LONG*) override { return E_NOTIMPL; }
    STDMETHODIMP GetTextExt(TsViewCookie, LONG start, LONG end, RECT* rc, BOOL* clipped) override {
        if (!(lock_ & TS_LF_READ)) return TS_E_NOLOCK;
        POINT origin = {24, 40};  // client coordinates of the text's top-left
        ClientToScreen(g_window, &origin);
        rc->left = origin.x + start * 8;
        rc->right = origin.x + end * 8;
        rc->top = origin.y;
        rc->bottom = origin.y + 22;
        *clipped = FALSE;
        return S_OK;
    }
    STDMETHODIMP GetScreenExt(TsViewCookie, RECT* rc) override { return GetWindowRect(g_window, rc) ? S_OK : E_FAIL; }
    STDMETHODIMP GetWnd(TsViewCookie, HWND* hwnd) override {
        *hwnd = g_window;
        return S_OK;
    }

    // What an app does when the user types: change the text, then tell TSF.
    void TypeChar(wchar_t c) {
        LONG start = selStart;
        text.replace(selStart, selEnd - selStart, 1, c);
        selStart = selEnd = start + 1;
        TS_TEXTCHANGE change = {start, start, start + 1};
        sink_->OnTextChange(0, &change);
        sink_->OnSelectionChange();
    }
    void Type(const std::wstring& s) {
        for (wchar_t c : s) TypeChar(c);
    }
    void SetTextDirectly(const std::wstring& s, LONG caretAt) {  // e.g. the app loads a document
        text = s;
        selStart = selEnd = caretAt;
        TS_TEXTCHANGE change = {0, 0, static_cast<LONG>(s.size())};
        sink_->OnTextChange(0, &change);
        sink_->OnSelectionChange();
    }
    void MoveCaret(LONG from, LONG to) {
        selStart = from;
        selEnd = to;
        sink_->OnSelectionChange();
    }

private:
    ITextStoreACPSink* sink_ = nullptr;
    DWORD lock_ = 0;
    std::deque<DWORD> pending_;
};

// ---------------------------------------------------------------------------------------------

static void Pump(DWORD ms) {
    ULONGLONG end = GetTickCount64() + ms;
    while (GetTickCount64() < end) {
        MSG msg;
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        MsgWaitForMultipleObjects(0, nullptr, FALSE, 5, QS_ALLINPUT);
    }
}

static HWND PopupWindow() { return FindWindowW(L"TyperPopup", nullptr); }
static bool PopupVisible() {
    HWND popup = PopupWindow();
    return popup && IsWindowVisible(popup);
}
static bool WaitForPopup(bool visible, DWORD timeoutMs = 2500) {
    ULONGLONG end = GetTickCount64() + timeoutMs;
    while (GetTickCount64() < end) {
        Pump(20);
        if (PopupVisible() == visible) return true;
    }
    return PopupVisible() == visible;
}

static void SaveScreenshot(const std::wstring& folder, const wchar_t* name) {
    Pump(250);  // let the popup finish painting
    RECT rc = {};
    HWND popup = PopupWindow();
    if (popup && IsWindowVisible(popup)) GetWindowRect(popup, &rc);
    else return;
    RECT area = {rc.left - 60, rc.top - 70, rc.right + 220, rc.bottom + 30};  // popup plus the "text field" above it
    int w = area.right - area.left, h = area.bottom - area.top;
    HDC screen = GetDC(nullptr);
    HDC dc = CreateCompatibleDC(screen);
    BITMAPINFO bi = {};
    bi.bmiHeader = {sizeof(BITMAPINFOHEADER), w, -h, 1, 32, BI_RGB};
    void* bits = nullptr;
    HBITMAP bitmap = CreateDIBSection(screen, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    HGDIOBJ old = SelectObject(dc, bitmap);
    BitBlt(dc, 0, 0, w, h, screen, area.left, area.top, SRCCOPY | CAPTUREBLT);
    std::wstring path = folder + L"\\" + name + L".raw";
    HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file != INVALID_HANDLE_VALUE) {
        DWORD written;
        int dims[2] = {w, h};
        WriteFile(file, dims, sizeof(dims), &written, nullptr);
        WriteFile(file, bits, static_cast<DWORD>(w) * h * 4, &written, nullptr);
        CloseHandle(file);
    }
    SelectObject(dc, old);
    DeleteObject(bitmap);
    DeleteDC(dc);
    ReleaseDC(nullptr, screen);
}

// Real TSF only routes keys to a *registered* text service, which needs admin. So the harness calls the
// DLL's key-event interface directly, in the order TSF would: test, key down, then the matching key up.
struct Keys {
    ITfKeyEventSink* sink;
    ITfContext* context;
    // Returns whether the text service consumed the key. With `ctrl`, Ctrl is really held (via SendInput)
    // while the key is offered, since the text service reads the keyboard state.
    bool Press(WPARAM vk, bool* testEaten = nullptr, bool ctrl = false) {
        if (ctrl) {
            SendCtrl(true);
            Sleep(40);
        }
        BOOL test = FALSE, eaten = FALSE;
        sink->OnTestKeyDown(context, vk, 0, &test);
        if (testEaten) *testEaten = test;
        if (test) {
            sink->OnKeyDown(context, vk, 0, &eaten);
            BOOL upEaten = FALSE;
            sink->OnTestKeyUp(context, vk, 0, &upEaten);
            sink->OnKeyUp(context, vk, 0, &upEaten);
        }
        if (ctrl) SendCtrl(false);
        return eaten != FALSE;
    }

    static void SendCtrl(bool down) {
        INPUT input = {};
        input.type = INPUT_KEYBOARD;
        input.ki.wVk = VK_CONTROL;
        input.ki.dwFlags = down ? 0 : KEYEVENTF_KEYUP;
        SendInput(1, &input, sizeof(input));
    }
};

// Makes `window` the foreground window even when another app has been in use (by attaching to its input queue).
static void BringToFront(HWND window) {
    HWND front = GetForegroundWindow();
    DWORD frontThread = front ? GetWindowThreadProcessId(front, nullptr) : 0;
    DWORD self = GetCurrentThreadId();
    bool attached = frontThread && frontThread != self && AttachThreadInput(self, frontThread, TRUE);
    SetWindowPos(window, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
    SetForegroundWindow(window);
    SetActiveWindow(window);
    SetFocus(window);
    if (attached) AttachThreadInput(self, frontThread, FALSE);
}

static LRESULT CALLBACK HostProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

#include "phrase_scenarios.h"
#include "resilience_scenarios.h"

int wmain(int argc, wchar_t** argv) {
    if (argc < 3) {
        std::printf("usage: tsf_harness <TyperTip.dll> <screenshot-folder> [unaware]\n");
        return 2;
    }
    std::wstring shots = argv[2];
    // Optional words after the folder: "unaware" (pretend to be an old DPI-unaware app) and "auto"
    // (run only the automatic-phrase scenarios; the engine must list this app as allow-listed).
    bool unaware = false, autoOnly = false;
    std::wstring resilienceFlags;  // "resilience <folder>": run only the engine-dies-and-returns scenarios
    for (int i = 3; i < argc; ++i) {
        unaware = unaware || std::wstring(argv[i]) == L"unaware";
        autoOnly = autoOnly || std::wstring(argv[i]) == L"auto";
        if (std::wstring(argv[i]) == L"resilience" && i + 1 < argc) resilienceFlags = argv[i + 1];
    }
    if (!unaware) SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    std::printf("spike at start: %s\n", GetModuleHandleW(L"TyperSpike.dll") ? "YES" : "no");
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    std::printf("spike after CoInitialize: %s\n", GetModuleHandleW(L"TyperSpike.dll") ? "YES" : "no");

    // Activate our thread manager first, before any window exists: creating a window makes Windows
    // activate the machine's selected keyboard (e.g. the old spike) on this thread.
    ITfThreadMgr* threadMgr = nullptr;
    CoCreateInstance(CLSID_TF_ThreadMgr, nullptr, CLSCTX_INPROC_SERVER, IID_ITfThreadMgr, reinterpret_cast<void**>(&threadMgr));
    TfClientId appClient = 0;
    // Don't let TSF activate whatever keyboard the machine has selected (e.g. the old spike): only ours runs.
    ITfThreadMgrEx* threadMgrEx = nullptr;
    if (SUCCEEDED(threadMgr->QueryInterface(IID_ITfThreadMgrEx, reinterpret_cast<void**>(&threadMgrEx)))) {
        HRESULT exHr = threadMgrEx->ActivateEx(&appClient, TF_TMAE_NOACTIVATETIP);
        std::printf("ActivateEx(NOACTIVATETIP) hr=0x%08lx client=%lu\n", exHr, appClient);
        threadMgrEx->Release();
    } else {
        std::printf("no ITfThreadMgrEx; using Activate\n");
        threadMgr->Activate(&appClient);
    }

    WNDCLASSW wc = {};
    wc.lpfnWndProc = HostProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"TyperHarnessHost";
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    RegisterClassW(&wc);
    g_window = CreateWindowExW(0, L"TyperHarnessHost", L"Typer harness", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 200, 200, 640, 260,
                               nullptr, nullptr, wc.hInstance, nullptr);
    ShowWindow(g_window, SW_SHOWNORMAL);
    BringToFront(g_window);
    Pump(200);

    HMODULE dll = LoadLibraryW(argv[1]);
    if (!dll) return std::printf("cannot load the DLL\n"), 1;
    auto getClass = reinterpret_cast<HRESULT(STDAPICALLTYPE*)(REFCLSID, REFIID, void**)>(GetProcAddress(dll, "DllGetClassObject"));
    IClassFactory* factory = nullptr;
    getClass(CLSID_TyperService, IID_IClassFactory, reinterpret_cast<void**>(&factory));

    std::printf("spike loaded after thread manager activation: %s\n", GetModuleHandleW(L"TyperSpike.dll") ? "YES" : "no");
    ITfTextInputProcessorEx* tip = nullptr;
    factory->CreateInstance(nullptr, IID_ITfTextInputProcessorEx, reinterpret_cast<void**>(&tip));
    // Each text service has its own client id, which TSF hands out per CLSID (as it does when it activates a TIP).
    TfClientId tipClient = 0;
    ITfClientId* clientIds = nullptr;
    if (SUCCEEDED(threadMgr->QueryInterface(IID_ITfClientId, reinterpret_cast<void**>(&clientIds)))) {
        clientIds->GetClientId(CLSID_TyperService, &tipClient);
        clientIds->Release();
    }
    HRESULT activateHr = tip->ActivateEx(threadMgr, tipClient, 0);
    std::printf("TIP activated hr=0x%08lx client=%lu\n", activateHr, tipClient);

    ITfDocumentMgr* docMgr = nullptr;
    threadMgr->CreateDocumentMgr(&docMgr);
    TextStore store;
    ITfContext* context = nullptr;
    TfEditCookie cookie = 0;
    HRESULT hr = docMgr->CreateContext(appClient, 0, static_cast<ITextStoreACP*>(&store), &context, &cookie);
    if (FAILED(hr)) return std::printf("CreateContext failed 0x%08lx\n", hr), 1;
    docMgr->Push(context);
    ITfDocumentMgr* prev = nullptr;
    // TSF only gives our document focus while our window is the foreground one, and a desktop in use can
    // take that away at any time. So take it back, and say so if that keeps failing.
    g_refocus = [threadMgr, docMgr]() -> bool {
        for (int attempt = 0; attempt < 10; ++attempt) {
            BringToFront(g_window);
            threadMgr->SetFocus(docMgr);
            Pump(150);
            BOOL threadFocus = FALSE;
            ITfDocumentMgr* focused = nullptr;
            threadMgr->IsThreadFocus(&threadFocus);
            threadMgr->GetFocus(&focused);
            bool ours = threadFocus && focused == docMgr;
            if (focused) focused->Release();
            if (ours) return true;
        }
        return false;
    };
    bool focusOk = g_refocus();
    Pump(300);
    std::printf("harness has TSF focus: %s\n", focusOk ? "yes" : "NO (something else keeps the foreground; results will be unreliable, so stop using the PC while this runs)");
    std::printf("spike loaded after focus: %s\n", GetModuleHandleW(L"TyperSpike.dll") ? "YES" : "no");

    ITfKeyEventSink* keySink = nullptr;
    tip->QueryInterface(IID_ITfKeyEventSink, reinterpret_cast<void**>(&keySink));
    Keys keys{keySink, context};
    (void)prev;

    // Wait for the engine connection.
    Pump(600);

    if (!resilienceFlags.empty()) {
        ResilienceScenarios(store, keys, resilienceFlags);
    } else if (autoOnly) {
        AutoPhraseScenarios(store, keys, shots);
    } else {
    std::printf("scenario 1: typing a word shows the popup; Down moves, Tab accepts\n");
    store.Type(L"I would like to recomm");
    Check(WaitForPopup(true), "popup appears at the caret");
    if (PopupVisible()) {
        RECT rc;
        GetWindowRect(PopupWindow(), &rc);
        POINT origin = {24, 40};
        ClientToScreen(g_window, &origin);
        int caretX = origin.x + static_cast<int>(store.text.size()) * 8;
        Check(rc.left <= caretX && caretX - rc.left < 300 && rc.top >= origin.y + 15 && rc.top < origin.y + 60, "popup sits just under the caret");
        Check(rc.bottom - rc.top > 40, "popup shows several rows");
    }
    SaveScreenshot(shots, L"1-popup");
    bool testEaten = false;
    Check(keys.Press(VK_DOWN, &testEaten) && testEaten, "Down is consumed while the popup is open");
    Pump(50);
    SaveScreenshot(shots, L"2-down");
    Check(!keys.Press(VK_RETURN), "Enter is NOT consumed");
    Check(PopupVisible(), "popup still open after Enter test (Enter passes through)");
    Check(keys.Press(VK_UP), "Up is consumed");
    Check(keys.Press(VK_TAB), "Tab is consumed");
    Pump(200);
    Check(store.text == L"I would like to recommend", "Tab replaced the typed part with the top word");
    Check(WaitForPopup(false, 800), "popup closes after accepting");
    Pump(400);
    Check(!PopupVisible(), "popup does not pop straight back up for the accepted word");

    std::printf("scenario 2: Down then Tab takes the second word\n");
    store.SetTextDirectly(L"", 0);
    Pump(100);
    store.Type(L"the recomm");
    Check(WaitForPopup(true), "popup appears");
    keys.Press(VK_DOWN);
    keys.Press(VK_TAB);
    Pump(200);
    Check(store.text.size() > 4 && store.text != L"the recommend" && store.text.rfind(L"the recomm", 0) == 0, "second word inserted");
    std::printf("    text is now: %ls\n", store.text.c_str());

    std::printf("scenario 3: Esc dismisses for the word, comes back for the next word\n");
    store.SetTextDirectly(L"", 0);
    Pump(100);
    store.Type(L"recomm");
    Check(WaitForPopup(true), "popup appears");
    Check(keys.Press(VK_ESCAPE), "Esc is consumed");
    Check(!PopupVisible(), "popup hidden after Esc");
    store.Type(L"e");
    Pump(400);
    Check(!PopupVisible(), "stays hidden while typing the same word");
    store.Type(L"nd ");
    Pump(200);
    store.Type(L"wor");
    Check(WaitForPopup(true), "popup returns for the next word");
    bool tabWhenClosedEaten = true;
    store.Type(L" ");
    WaitForPopup(false, 1500);
    keys.Press(VK_TAB, &tabWhenClosedEaten);
    Check(!tabWhenClosedEaten, "Tab is NOT consumed when the popup is closed");

    std::printf("scenario 4: keys typed before fresh words arrive are not stolen\n");
    store.SetTextDirectly(L"", 0);
    Pump(100);
    store.Type(L"recomm");
    WaitForPopup(true);
    store.TypeChar(L'e');  // the words on screen are now for old text, and the reply hasn't come yet
    bool eatenDuringStale = true;
    keys.Press(VK_TAB, &eatenDuringStale);
    Check(!eatenDuringStale, "Tab passes through while the popup is stale");
    Pump(1500);
    Check(store.text == L"recomme", "text untouched");

    std::printf("scenario 5: no popup mid-word, with a selection, or where keyboards are disabled\n");
    store.SetTextDirectly(L"recommend", 5);  // caret inside the word
    Pump(500);
    Check(!PopupVisible(), "no popup with the caret in the middle of a word");
    store.SetTextDirectly(L"recomm", 6);
    store.MoveCaret(0, 6);  // select everything
    Pump(500);
    Check(!PopupVisible(), "no popup while text is selected");

    ITfCompartmentMgr* compartments = nullptr;
    context->QueryInterface(IID_ITfCompartmentMgr, reinterpret_cast<void**>(&compartments));
    ITfCompartment* disabled = nullptr;
    compartments->GetCompartment(GUID_COMPARTMENT_KEYBOARD_DISABLED, &disabled);
    VARIANT on;
    VariantInit(&on);
    on.vt = VT_I4;
    on.lVal = 1;
    disabled->SetValue(appClient, &on);
    store.SetTextDirectly(L"", 0);
    Pump(100);
    store.Type(L"recomm");
    Pump(800);
    Check(!PopupVisible(), "no popup where the app has disabled keyboards (password fields)");
    on.lVal = 0;
    disabled->SetValue(appClient, &on);
    store.Type(L"e");
    Check(WaitForPopup(true), "popup returns once keyboards are enabled again");
    disabled->Release();
    compartments->Release();

    std::printf("scenario 6: learning a new word from typing\n");
    store.SetTextDirectly(L"", 0);
    Pump(100);
    for (int i = 0; i < 3; ++i) {
        store.Type(L"zorblax ");
        Pump(100);
    }
    store.Type(L"zor");
    Check(WaitForPopup(true), "popup appears for a word learned from typing");
    SaveScreenshot(shots, L"3-learned");
    keys.Press(VK_TAB);
    Pump(200);
    Check(store.text.size() >= 6 && store.text.substr(store.text.size() - 7) == L"zorblax", "the learned word is offered and accepted");

    std::printf("scenario 7: rapid typing keeps up\n");
    store.SetTextDirectly(L"", 0);
    Pump(100);
    ULONGLONG t0 = GetTickCount64();
    store.Type(L"the quick brown fox jumps over the lazy do");
    bool shown = WaitForPopup(true);
    ULONGLONG shownAfter = GetTickCount64() - t0;
    Check(shown, "popup follows fast typing");
    std::printf("    popup up %llu ms after the last character\n", shownAfter);

    HotkeyPhraseScenarios(store, keys, shots);
    }

    // Tear down.
    tip->Deactivate();
    tip->Release();
    context->Release();
    docMgr->Pop(TF_POPF_ALL);
    docMgr->Release();
    keySink->Release();
    threadMgr->Deactivate();
    threadMgr->Release();
    factory->Release();
    Pump(50);
    std::printf("\n%s (%d failed)\n", g_failures ? "FAILED" : "all passed", g_failures);
    return g_failures ? 1 : 0;
}
