// Typer Milestone 0 spike: a minimal English TSF text service.
//
// It answers three questions in every app it's used in:
//   1. Where is the caret? (ITfContextView::GetTextExt) -> a small popup is drawn there.
//   2. How much text around the caret does the app expose? (ITfRange::ShiftStart/ShiftEnd)
//   3. Can a text service swallow a key and insert text? Tab uppercases the current word.
// Everything is logged to %LOCALAPPDATA%\Typer\spike.log. Throwaway code: not the real DLL.

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <msctf.h>
#include <olectl.h>
#include <strsafe.h>
#include <initguid.h>
#include <inputscope.h>

#include <cwctype>
#include <new>
#include <string>

namespace {

// {FA6A3DA5-2BEA-4B4B-83F7-977C5C594472}
constexpr CLSID CLSID_TyperSpike = {0xfa6a3da5, 0x2bea, 0x4b4b, {0x83, 0xf7, 0x97, 0x7c, 0x5c, 0x59, 0x44, 0x72}};
// {11A9FA0E-A1A6-4373-8C5F-28A8D14D4700}
constexpr GUID GUID_TyperSpikeProfile = {0x11a9fa0e, 0xa1a6, 0x4373, {0x8c, 0x5f, 0x28, 0xa8, 0xd1, 0x4d, 0x47, 0x00}};
constexpr wchar_t kClsidKey[] = L"CLSID\\{FA6A3DA5-2BEA-4B4B-83F7-977C5C594472}";
constexpr LANGID kLangId = MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US);
constexpr wchar_t kDescription[] = L"Typer Spike";
constexpr wchar_t kPopupClass[] = L"TyperSpikePopup";
constexpr LONG kContextChars = 10000;
constexpr UINT_PTR kRetryTimer = 1;

const GUID kCategories[] = {
    GUID_TFCAT_TIP_KEYBOARD,
    GUID_TFCAT_TIPCAP_COMLESS,
    GUID_TFCAT_TIPCAP_SYSTRAYSUPPORT,
};

HINSTANCE g_module = nullptr;
LONG g_objects = 0;  // live COM objects plus server locks

// ---------------------------------------------------------------------------------------------
// Logging

void Log(const wchar_t* format, ...) {
    wchar_t message[3000];
    va_list args;
    va_start(args, format);
    StringCchVPrintfW(message, ARRAYSIZE(message), format, args);
    va_end(args);

    wchar_t exe[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, exe, MAX_PATH);
    const wchar_t* exeName = wcsrchr(exe, L'\\');
    exeName = exeName ? exeName + 1 : exe;

    SYSTEMTIME t;
    GetLocalTime(&t);
    wchar_t line[3300];
    StringCchPrintfW(line, ARRAYSIZE(line), L"%02d:%02d:%02d.%03d %s[%lu] %s\r\n", t.wHour, t.wMinute, t.wSecond,
                     t.wMilliseconds, exeName, GetCurrentProcessId(), message);

    char utf8[10000];
    int bytes = WideCharToMultiByte(CP_UTF8, 0, line, -1, utf8, sizeof(utf8), nullptr, nullptr);
    if (bytes <= 1) return;

    wchar_t path[MAX_PATH];
    DWORD len = GetEnvironmentVariableW(L"LOCALAPPDATA", path, MAX_PATH);
    if (len == 0 || len >= MAX_PATH) return;
    StringCchCatW(path, MAX_PATH, L"\\Typer");
    CreateDirectoryW(path, nullptr);
    StringCchCatW(path, MAX_PATH, L"\\spike.log");
    HANDLE file = CreateFileW(path, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                              OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return;
    DWORD written;
    WriteFile(file, utf8, static_cast<DWORD>(bytes - 1), &written, nullptr);
    CloseHandle(file);
}

// Last/first `count` characters of `text`, with line breaks made visible.
std::wstring Snippet(const std::wstring& text, size_t count, bool fromEnd) {
    std::wstring part = text.size() <= count ? text : fromEnd ? text.substr(text.size() - count) : text.substr(0, count);
    std::wstring out;
    for (wchar_t c : part) {
        if (c == L'\r') continue;
        if (c == L'\n') out += L"\\n";
        else if (c == L'"') out += L"\\\"";
        else out += c;
    }
    return out;
}

std::wstring TrailingWord(const std::wstring& before) {
    size_t start = before.size();
    while (start > 0 && (iswalpha(before[start - 1]) || before[start - 1] == L'\'')) --start;
    while (start < before.size() && before[start] == L'\'') ++start;  // a word starts with a letter
    return before.substr(start);
}

const wchar_t* ScopeName(InputScope scope) {
    switch (scope) {
        case IS_DEFAULT: return L"IS_DEFAULT";
        case IS_URL: return L"IS_URL";
        case IS_EMAIL_USERNAME: return L"IS_EMAIL_USERNAME";
        case IS_EMAIL_SMTPEMAILADDRESS: return L"IS_EMAIL_SMTPEMAILADDRESS";
        case IS_NUMBER: return L"IS_NUMBER";
        case IS_PASSWORD: return L"IS_PASSWORD";
        case IS_SEARCH: return L"IS_SEARCH";
        case IS_TEXT: return L"IS_TEXT";
        case IS_CHAT: return L"IS_CHAT";
        case IS_EMAILNAME_OR_ADDRESS: return L"IS_EMAILNAME_OR_ADDRESS";
        default: return nullptr;
    }
}

// ---------------------------------------------------------------------------------------------
// TSF helpers (all require a valid edit cookie)

// Text next to `anchor`, up to kContextChars. `available` gets how far the range could move.
std::wstring ReadBeside(ITfRange* anchor, TfEditCookie ec, bool before, LONG* available) {
    *available = 0;
    ITfRange* range = nullptr;
    if (FAILED(anchor->Clone(&range))) return {};
    range->Collapse(ec, before ? TF_ANCHOR_START : TF_ANCHOR_END);
    LONG moved = 0;
    HRESULT hr = before ? range->ShiftStart(ec, -kContextChars, &moved, nullptr)
                        : range->ShiftEnd(ec, kContextChars, &moved, nullptr);
    std::wstring text;
    if (SUCCEEDED(hr)) {
        *available = moved < 0 ? -moved : moved;
        text.resize(kContextChars);
        ULONG got = 0;
        if (FAILED(range->GetText(ec, 0, text.data(), kContextChars, &got))) got = 0;
        text.resize(got);
    }
    range->Release();
    return text;
}

std::wstring ReadInputScopes(ITfContext* context, TfEditCookie ec, ITfRange* range) {
    std::wstring names;
    ITfProperty* property = nullptr;
    if (FAILED(context->GetProperty(GUID_PROP_INPUTSCOPE, &property))) return L"(no property)";
    VARIANT value;
    VariantInit(&value);
    if (SUCCEEDED(property->GetValue(ec, range, &value)) && value.vt == VT_UNKNOWN && value.punkVal) {
        ITfInputScope* inputScope = nullptr;
        if (SUCCEEDED(value.punkVal->QueryInterface(IID_ITfInputScope, reinterpret_cast<void**>(&inputScope)))) {
            InputScope* scopes = nullptr;
            UINT count = 0;
            if (SUCCEEDED(inputScope->GetInputScopes(&scopes, &count)) && scopes) {
                for (UINT i = 0; i < count; ++i) {
                    if (!names.empty()) names += L",";
                    const wchar_t* name = ScopeName(scopes[i]);
                    names += name ? name : L"IS_" + std::to_wstring(static_cast<int>(scopes[i]));
                }
                CoTaskMemFree(scopes);
            }
            inputScope->Release();
        }
    }
    VariantClear(&value);
    property->Release();
    return names.empty() ? L"(none)" : names;
}

class TyperSpike;
using EditFn = void (*)(TyperSpike*, ITfContext*, TfEditCookie);

// ---------------------------------------------------------------------------------------------
// The text service

class TyperSpike final : public ITfTextInputProcessorEx,
                         public ITfThreadMgrEventSink,
                         public ITfTextEditSink,
                         public ITfKeyEventSink,
                         public ITfCompositionSink {
public:
    TyperSpike() { InterlockedIncrement(&g_objects); }

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
        if (!ppv) return E_INVALIDARG;
        *ppv = nullptr;
        if (riid == IID_IUnknown || riid == IID_ITfTextInputProcessor || riid == IID_ITfTextInputProcessorEx)
            *ppv = static_cast<ITfTextInputProcessorEx*>(this);
        else if (riid == IID_ITfThreadMgrEventSink)
            *ppv = static_cast<ITfThreadMgrEventSink*>(this);
        else if (riid == IID_ITfTextEditSink)
            *ppv = static_cast<ITfTextEditSink*>(this);
        else if (riid == IID_ITfKeyEventSink)
            *ppv = static_cast<ITfKeyEventSink*>(this);
        else if (riid == IID_ITfCompositionSink)
            *ppv = static_cast<ITfCompositionSink*>(this);
        else
            return E_NOINTERFACE;
        AddRef();
        return S_OK;
    }
    STDMETHODIMP_(ULONG) AddRef() override { return InterlockedIncrement(&refs_); }
    STDMETHODIMP_(ULONG) Release() override {
        LONG refs = InterlockedDecrement(&refs_);
        if (refs == 0) delete this;
        return refs;
    }

    // ITfTextInputProcessor(Ex)
    STDMETHODIMP Activate(ITfThreadMgr* threadMgr, TfClientId clientId) override {
        return ActivateEx(threadMgr, clientId, 0);
    }

    STDMETHODIMP ActivateEx(ITfThreadMgr* threadMgr, TfClientId clientId, DWORD flags) override {
        threadMgr_ = threadMgr;
        threadMgr_->AddRef();
        clientId_ = clientId;

        ITfSource* source = nullptr;
        if (SUCCEEDED(threadMgr_->QueryInterface(IID_ITfSource, reinterpret_cast<void**>(&source)))) {
            source->AdviseSink(IID_ITfThreadMgrEventSink, static_cast<ITfThreadMgrEventSink*>(this),
                               &threadMgrCookie_);
            source->Release();
        }
        HRESULT keyHr = E_FAIL;
        ITfKeystrokeMgr* keystrokes = nullptr;
        if (SUCCEEDED(threadMgr_->QueryInterface(IID_ITfKeystrokeMgr, reinterpret_cast<void**>(&keystrokes)))) {
            keyHr = keystrokes->AdviseKeyEventSink(clientId_, static_cast<ITfKeyEventSink*>(this), TRUE);
            keystrokes->Release();
        }
        CreatePopup();
        Log(L"activate flags=0x%lx keysink=0x%08lx", flags, keyHr);

        ITfDocumentMgr* focus = nullptr;
        if (SUCCEEDED(threadMgr_->GetFocus(&focus)) && focus) {
            OnSetFocus(focus, nullptr);
            focus->Release();
        }
        return S_OK;
    }

    STDMETHODIMP Deactivate() override {
        WatchContext(nullptr);
        if (threadMgr_) {
            ITfKeystrokeMgr* keystrokes = nullptr;
            if (SUCCEEDED(threadMgr_->QueryInterface(IID_ITfKeystrokeMgr, reinterpret_cast<void**>(&keystrokes)))) {
                keystrokes->UnadviseKeyEventSink(clientId_);
                keystrokes->Release();
            }
            ITfSource* source = nullptr;
            if (threadMgrCookie_ != TF_INVALID_COOKIE &&
                SUCCEEDED(threadMgr_->QueryInterface(IID_ITfSource, reinterpret_cast<void**>(&source)))) {
                source->UnadviseSink(threadMgrCookie_);
                source->Release();
            }
            threadMgrCookie_ = TF_INVALID_COOKIE;
            threadMgr_->Release();
            threadMgr_ = nullptr;
        }
        DestroyPopup();
        clientId_ = TF_CLIENTID_NULL;
        Log(L"deactivate");
        return S_OK;
    }

    // ITfThreadMgrEventSink
    STDMETHODIMP OnInitDocumentMgr(ITfDocumentMgr*) override { return S_OK; }
    STDMETHODIMP OnUninitDocumentMgr(ITfDocumentMgr*) override { return S_OK; }
    STDMETHODIMP OnPushContext(ITfContext*) override { return S_OK; }
    STDMETHODIMP OnPopContext(ITfContext*) override { return S_OK; }
    STDMETHODIMP OnSetFocus(ITfDocumentMgr* focus, ITfDocumentMgr*) override {
        ITfContext* context = nullptr;
        if (focus) focus->GetTop(&context);
        WatchContext(context);
        if (context) {
            Log(L"focus -> new context");
            RequestEdit(context, TF_ES_READ, [](TyperSpike* tip, ITfContext* c, TfEditCookie ec) { tip->Inspect(c, ec); });
            context->Release();
        } else {
            HidePopup();
        }
        return S_OK;
    }

    // ITfTextEditSink
    STDMETHODIMP OnEndEdit(ITfContext* context, TfEditCookie, ITfEditRecord*) override {
        retries_ = 0;
        RequestEdit(context, TF_ES_READ, [](TyperSpike* tip, ITfContext* c, TfEditCookie ec) { tip->Inspect(c, ec); });
        return S_OK;
    }

    // ITfKeyEventSink
    STDMETHODIMP OnSetFocus(BOOL) override { return S_OK; }
    STDMETHODIMP OnTestKeyDown(ITfContext*, WPARAM key, LPARAM, BOOL* eaten) override {
        *eaten = WantsKey(key);
        return S_OK;
    }
    STDMETHODIMP OnKeyDown(ITfContext* context, WPARAM key, LPARAM, BOOL* eaten) override {
        *eaten = WantsKey(key);
        if (*eaten) {
            Log(L"tab eaten, replacing \"%s\"", currentWord_.c_str());
            RequestEdit(context, TF_ES_READWRITE,
                        [](TyperSpike* tip, ITfContext* c, TfEditCookie ec) { tip->UppercaseCurrentWord(c, ec); });
        }
        return S_OK;
    }
    STDMETHODIMP OnTestKeyUp(ITfContext*, WPARAM, LPARAM, BOOL* eaten) override {
        *eaten = FALSE;
        return S_OK;
    }
    STDMETHODIMP OnKeyUp(ITfContext*, WPARAM, LPARAM, BOOL* eaten) override {
        *eaten = FALSE;
        return S_OK;
    }
    STDMETHODIMP OnPreservedKey(ITfContext*, REFGUID, BOOL* eaten) override {
        *eaten = FALSE;
        return S_OK;
    }

    // ITfCompositionSink
    STDMETHODIMP OnCompositionTerminated(TfEditCookie, ITfComposition*) override { return S_OK; }

    // Edit-session bodies -----------------------------------------------------------------

    void Inspect(ITfContext* context, TfEditCookie ec) {
        TF_SELECTION selection = {};
        ULONG fetched = 0;
        if (FAILED(context->GetSelection(ec, TF_DEFAULT_SELECTION, 1, &selection, &fetched)) || fetched == 0) {
            Log(L"inspect: no selection");
            HidePopup();
            return;
        }
        BOOL selectionEmpty = TRUE;
        selection.range->IsEmpty(ec, &selectionEmpty);

        LONG availableBefore = 0, availableAfter = 0;
        std::wstring before = ReadBeside(selection.range, ec, true, &availableBefore);
        std::wstring after = ReadBeside(selection.range, ec, false, &availableAfter);
        std::wstring scopes = ReadInputScopes(context, ec, selection.range);
        currentWord_ = TrailingWord(before);

        RECT caret = {};
        BOOL clipped = FALSE;
        HRESULT extentHr = E_FAIL;
        bool usedPreviousChar = false;
        wchar_t title[256] = L"";
        ITfContextView* view = nullptr;
        if (SUCCEEDED(context->GetActiveView(&view))) {
            ITfRange* point = nullptr;
            if (SUCCEEDED(selection.range->Clone(&point))) {
                point->Collapse(ec, TF_ANCHOR_END);
                extentHr = view->GetTextExt(ec, point, &caret, &clipped);
                if (SUCCEEDED(extentHr) && caret.bottom == caret.top) {
                    // Some apps return an empty rect for an empty range; measure the previous character.
                    LONG moved = 0;
                    if (SUCCEEDED(point->ShiftStart(ec, -1, &moved, nullptr)) && moved != 0) {
                        RECT previous = {};
                        if (SUCCEEDED(view->GetTextExt(ec, point, &previous, &clipped))) {
                            caret = {previous.right, previous.top, previous.right, previous.bottom};
                            usedPreviousChar = true;
                        }
                    }
                }
                point->Release();
            }
            HWND hwnd = nullptr;
            if (SUCCEEDED(view->GetWnd(&hwnd)) && hwnd) GetWindowTextW(GetAncestor(hwnd, GA_ROOT), title, 256);
            view->Release();
        }
        selection.range->Release();

        Log(L"inspect sel_empty=%d before=%ld after=%ld word=\"%s\" caret=(%ld,%ld)-(%ld,%ld) extent_hr=0x%08lx "
            L"prev_char=%d clipped=%d scope=%s title=\"%s\" tail=\"%s\" head=\"%s\"",
            selectionEmpty, availableBefore, availableAfter, currentWord_.c_str(), caret.left, caret.top, caret.right,
            caret.bottom, extentHr, usedPreviousChar, clipped, scopes.c_str(), Snippet(title, 60, false).c_str(),
            Snippet(before, 40, true).c_str(), Snippet(after, 20, false).c_str());

        if (extentHr == TF_E_NOLAYOUT && retries_ < 3 && popup_) {
            ++retries_;
            SetTimer(popup_, kRetryTimer, 50, nullptr);  // the app hasn't laid out the text yet
            return;
        }
        if (FAILED(extentHr) || currentWord_.empty()) {
            HidePopup();
            return;
        }
        wchar_t text[400];
        StringCchPrintfW(text, ARRAYSIZE(text), L" \x25B8 %s   \x2502 ctx %ld / %ld   \x2502 %s   \x2502 Tab = UPPERCASE ",
                         currentWord_.c_str(), availableBefore, availableAfter, scopes.c_str());
        ShowPopup(caret, text);
    }

    void UppercaseCurrentWord(ITfContext* context, TfEditCookie ec) {
        TF_SELECTION selection = {};
        ULONG fetched = 0;
        if (FAILED(context->GetSelection(ec, TF_DEFAULT_SELECTION, 1, &selection, &fetched)) || fetched == 0) return;
        LONG available = 0;
        std::wstring word = TrailingWord(ReadBeside(selection.range, ec, true, &available));
        ITfRange* range = nullptr;
        if (word.empty() || FAILED(selection.range->Clone(&range))) {
            selection.range->Release();
            return;
        }
        selection.range->Release();

        range->Collapse(ec, TF_ANCHOR_START);
        LONG moved = 0;
        range->ShiftStart(ec, -static_cast<LONG>(word.size()), &moved, nullptr);
        std::wstring upper = word;
        CharUpperBuffW(upper.data(), static_cast<DWORD>(upper.size()));

        HRESULT setHr = range->SetText(ec, 0, upper.c_str(), static_cast<LONG>(upper.size()));
        HRESULT compositionHr = S_FALSE;
        if (FAILED(setHr)) {
            // Fall back to the IME way: wrap the word in a composition, replace it, end the composition.
            ITfContextComposition* compositions = nullptr;
            compositionHr = context->QueryInterface(IID_ITfContextComposition, reinterpret_cast<void**>(&compositions));
            if (SUCCEEDED(compositionHr)) {
                ITfComposition* composition = nullptr;
                compositionHr = compositions->StartComposition(ec, range, this, &composition);
                if (SUCCEEDED(compositionHr) && composition) {
                    ITfRange* compositionRange = nullptr;
                    if (SUCCEEDED(composition->GetRange(&compositionRange))) {
                        compositionHr = compositionRange->SetText(ec, 0, upper.c_str(), static_cast<LONG>(upper.size()));
                        compositionRange->Release();
                    }
                    composition->EndComposition(ec);
                    composition->Release();
                }
                compositions->Release();
            }
        }
        range->Collapse(ec, TF_ANCHOR_END);
        TF_SELECTION caret = {};
        caret.range = range;
        caret.style.ase = TF_AE_NONE;
        caret.style.fInterimChar = FALSE;
        HRESULT selectHr = context->SetSelection(ec, 1, &caret);
        range->Release();
        Log(L"uppercase \"%s\": settext_hr=0x%08lx composition_hr=0x%08lx select_hr=0x%08lx", word.c_str(), setHr,
            compositionHr, selectHr);
    }

    void OnRetryTimer() {
        if (popup_) KillTimer(popup_, kRetryTimer);
        if (watched_)
            RequestEdit(watched_, TF_ES_READ, [](TyperSpike* tip, ITfContext* c, TfEditCookie ec) { tip->Inspect(c, ec); });
    }

private:
    ~TyperSpike() { InterlockedDecrement(&g_objects); }

    bool WantsKey(WPARAM key) const {
        bool modifier = (GetKeyState(VK_CONTROL) | GetKeyState(VK_MENU) | GetKeyState(VK_SHIFT)) & 0x8000;
        return key == VK_TAB && !modifier && popupVisible_ && !currentWord_.empty();
    }

    void RequestEdit(ITfContext* context, DWORD access, EditFn fn);

    // Watch the focused context for edits (and stop watching the previous one).
    void WatchContext(ITfContext* context) {
        if (watched_) {
            ITfSource* source = nullptr;
            if (watchCookie_ != TF_INVALID_COOKIE &&
                SUCCEEDED(watched_->QueryInterface(IID_ITfSource, reinterpret_cast<void**>(&source)))) {
                source->UnadviseSink(watchCookie_);
                source->Release();
            }
            watchCookie_ = TF_INVALID_COOKIE;
            watched_->Release();
            watched_ = nullptr;
        }
        if (!context) return;
        ITfSource* source = nullptr;
        if (SUCCEEDED(context->QueryInterface(IID_ITfSource, reinterpret_cast<void**>(&source)))) {
            if (SUCCEEDED(source->AdviseSink(IID_ITfTextEditSink, static_cast<ITfTextEditSink*>(this), &watchCookie_))) {
                watched_ = context;
                watched_->AddRef();
            }
            source->Release();
        }
    }

    // Popup ------------------------------------------------------------------------------

    static LRESULT CALLBACK PopupProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
        auto* tip = reinterpret_cast<TyperSpike*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        switch (message) {
            case WM_MOUSEACTIVATE:
                return MA_NOACTIVATE;
            case WM_TIMER:
                if (tip && wParam == kRetryTimer) tip->OnRetryTimer();
                return 0;
            case WM_PAINT: {
                PAINTSTRUCT paint;
                HDC dc = BeginPaint(hwnd, &paint);
                RECT client;
                GetClientRect(hwnd, &client);
                HBRUSH background = CreateSolidBrush(RGB(30, 30, 30));
                FillRect(dc, &client, background);
                DeleteObject(background);
                if (tip) {
                    SetBkMode(dc, TRANSPARENT);
                    SetTextColor(dc, RGB(240, 240, 240));
                    HGDIOBJ old = SelectObject(dc, GetStockObject(DEFAULT_GUI_FONT));
                    DrawTextW(dc, tip->popupText_.c_str(), -1, &client, DT_SINGLELINE | DT_VCENTER | DT_LEFT | DT_NOPREFIX);
                    SelectObject(dc, old);
                }
                EndPaint(hwnd, &paint);
                return 0;
            }
        }
        return DefWindowProcW(hwnd, message, wParam, lParam);
    }

    void CreatePopup() {
        WNDCLASSEXW wc = {sizeof(wc)};
        wc.lpfnWndProc = PopupProc;
        wc.hInstance = g_module;
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.lpszClassName = kPopupClass;
        RegisterClassExW(&wc);  // fails harmlessly if another thread already registered it
        popup_ = CreateWindowExW(WS_EX_TOPMOST | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW, kPopupClass, L"", WS_POPUP | WS_BORDER,
                                 0, 0, 10, 10, nullptr, nullptr, g_module, nullptr);
        if (popup_) SetWindowLongPtrW(popup_, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
    }

    void DestroyPopup() {
        if (popup_) {
            KillTimer(popup_, kRetryTimer);
            SetWindowLongPtrW(popup_, GWLP_USERDATA, 0);
            DestroyWindow(popup_);
            popup_ = nullptr;
        }
        popupVisible_ = false;
        UnregisterClassW(kPopupClass, g_module);  // fails harmlessly while other threads still use it
    }

    void ShowPopup(const RECT& caret, const wchar_t* text) {
        if (!popup_) return;
        popupText_ = text;
        HDC dc = GetDC(popup_);
        HGDIOBJ old = SelectObject(dc, GetStockObject(DEFAULT_GUI_FONT));
        SIZE size = {};
        GetTextExtentPoint32W(dc, popupText_.c_str(), static_cast<int>(popupText_.size()), &size);
        SelectObject(dc, old);
        ReleaseDC(popup_, dc);
        SetWindowPos(popup_, HWND_TOPMOST, caret.left, caret.bottom + 2, size.cx + 12, size.cy + 8,
                     SWP_NOACTIVATE | SWP_SHOWWINDOW);
        InvalidateRect(popup_, nullptr, TRUE);
        popupVisible_ = true;
    }

    void HidePopup() {
        if (popup_) ShowWindow(popup_, SW_HIDE);
        popupVisible_ = false;
    }

    LONG refs_ = 1;
    ITfThreadMgr* threadMgr_ = nullptr;
    TfClientId clientId_ = TF_CLIENTID_NULL;
    DWORD threadMgrCookie_ = TF_INVALID_COOKIE;
    ITfContext* watched_ = nullptr;
    DWORD watchCookie_ = TF_INVALID_COOKIE;
    HWND popup_ = nullptr;
    std::wstring popupText_;
    std::wstring currentWord_;
    bool popupVisible_ = false;
    int retries_ = 0;
};

class EditSession final : public ITfEditSession {
public:
    EditSession(TyperSpike* tip, ITfContext* context, EditFn fn) : tip_(tip), context_(context), fn_(fn) {
        tip_->AddRef();
        context_->AddRef();
        InterlockedIncrement(&g_objects);
    }
    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
        if (!ppv) return E_INVALIDARG;
        *ppv = nullptr;
        if (riid != IID_IUnknown && riid != IID_ITfEditSession) return E_NOINTERFACE;
        *ppv = static_cast<ITfEditSession*>(this);
        AddRef();
        return S_OK;
    }
    STDMETHODIMP_(ULONG) AddRef() override { return InterlockedIncrement(&refs_); }
    STDMETHODIMP_(ULONG) Release() override {
        LONG refs = InterlockedDecrement(&refs_);
        if (refs == 0) delete this;
        return refs;
    }
    STDMETHODIMP DoEditSession(TfEditCookie ec) override {
        fn_(tip_, context_, ec);
        return S_OK;
    }

private:
    ~EditSession() {
        tip_->Release();
        context_->Release();
        InterlockedDecrement(&g_objects);
    }
    LONG refs_ = 1;
    TyperSpike* tip_;
    ITfContext* context_;
    EditFn fn_;
};

void TyperSpike::RequestEdit(ITfContext* context, DWORD access, EditFn fn) {
    auto* session = new (std::nothrow) EditSession(this, context, fn);
    if (!session) return;
    HRESULT sessionHr = S_OK;
    HRESULT hr = context->RequestEditSession(clientId_, session, TF_ES_ASYNCDONTCARE | access, &sessionHr);
    if (FAILED(hr)) Log(L"RequestEditSession failed hr=0x%08lx", hr);
    session->Release();
}

// ---------------------------------------------------------------------------------------------
// COM plumbing

class ClassFactory final : public IClassFactory {
public:
    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
        if (!ppv) return E_INVALIDARG;
        *ppv = nullptr;
        if (riid != IID_IUnknown && riid != IID_IClassFactory) return E_NOINTERFACE;
        *ppv = static_cast<IClassFactory*>(this);
        return S_OK;
    }
    STDMETHODIMP_(ULONG) AddRef() override { return 2; }  // static object
    STDMETHODIMP_(ULONG) Release() override { return 1; }
    STDMETHODIMP CreateInstance(IUnknown* outer, REFIID riid, void** ppv) override {
        if (!ppv) return E_INVALIDARG;
        *ppv = nullptr;
        if (outer) return CLASS_E_NOAGGREGATION;
        auto* tip = new (std::nothrow) TyperSpike();
        if (!tip) return E_OUTOFMEMORY;
        HRESULT hr = tip->QueryInterface(riid, ppv);
        tip->Release();
        return hr;
    }
    STDMETHODIMP LockServer(BOOL lock) override {
        if (lock) InterlockedIncrement(&g_objects);
        else InterlockedDecrement(&g_objects);
        return S_OK;
    }
};

ClassFactory g_factory;

}  // namespace

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        g_module = instance;
        DisableThreadLibraryCalls(instance);
    }
    return TRUE;
}

STDAPI DllGetClassObject(REFCLSID clsid, REFIID riid, void** ppv) {
    if (!ppv) return E_INVALIDARG;
    *ppv = nullptr;
    if (clsid != CLSID_TyperSpike) return CLASS_E_CLASSNOTAVAILABLE;
    return g_factory.QueryInterface(riid, ppv);
}

STDAPI DllCanUnloadNow() { return g_objects == 0 ? S_OK : S_FALSE; }

STDAPI DllUnregisterServer() {
    ITfInputProcessorProfileMgr* profiles = nullptr;
    if (SUCCEEDED(CoCreateInstance(CLSID_TF_InputProcessorProfiles, nullptr, CLSCTX_INPROC_SERVER,
                                   IID_ITfInputProcessorProfileMgr, reinterpret_cast<void**>(&profiles)))) {
        profiles->UnregisterProfile(CLSID_TyperSpike, kLangId, GUID_TyperSpikeProfile, 0);
        profiles->Release();
    }
    ITfCategoryMgr* categories = nullptr;
    if (SUCCEEDED(CoCreateInstance(CLSID_TF_CategoryMgr, nullptr, CLSCTX_INPROC_SERVER, IID_ITfCategoryMgr,
                                   reinterpret_cast<void**>(&categories)))) {
        for (const GUID& category : kCategories) categories->UnregisterCategory(CLSID_TyperSpike, category, CLSID_TyperSpike);
        categories->Release();
    }
    RegDeleteTreeW(HKEY_CLASSES_ROOT, kClsidKey);
    return S_OK;
}

STDAPI DllRegisterServer() {
    wchar_t path[MAX_PATH];
    DWORD length = GetModuleFileNameW(g_module, path, MAX_PATH);
    if (length == 0 || length >= MAX_PATH) return E_FAIL;

    std::wstring server = std::wstring(kClsidKey) + L"\\InprocServer32";
    const wchar_t apartment[] = L"Apartment";
    if (RegSetKeyValueW(HKEY_CLASSES_ROOT, kClsidKey, nullptr, REG_SZ, kDescription, sizeof(kDescription)) != ERROR_SUCCESS ||
        RegSetKeyValueW(HKEY_CLASSES_ROOT, server.c_str(), nullptr, REG_SZ, path, (length + 1) * sizeof(wchar_t)) != ERROR_SUCCESS ||
        RegSetKeyValueW(HKEY_CLASSES_ROOT, server.c_str(), L"ThreadingModel", REG_SZ, apartment, sizeof(apartment)) != ERROR_SUCCESS) {
        return SELFREG_E_CLASS;
    }

    ITfInputProcessorProfileMgr* profiles = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_TF_InputProcessorProfiles, nullptr, CLSCTX_INPROC_SERVER,
                                  IID_ITfInputProcessorProfileMgr, reinterpret_cast<void**>(&profiles));
    if (SUCCEEDED(hr)) {
        hr = profiles->RegisterProfile(CLSID_TyperSpike, kLangId, GUID_TyperSpikeProfile, kDescription,
                                       static_cast<ULONG>(wcslen(kDescription)), path, length, 0, nullptr, 0, TRUE, 0);
        profiles->Release();
    }
    if (FAILED(hr)) {
        DllUnregisterServer();
        return hr;
    }

    ITfCategoryMgr* categories = nullptr;
    hr = CoCreateInstance(CLSID_TF_CategoryMgr, nullptr, CLSCTX_INPROC_SERVER, IID_ITfCategoryMgr,
                          reinterpret_cast<void**>(&categories));
    if (SUCCEEDED(hr)) {
        for (const GUID& category : kCategories) {
            hr = categories->RegisterCategory(CLSID_TyperSpike, category, CLSID_TyperSpike);
            if (FAILED(hr)) break;
        }
        categories->Release();
    }
    if (FAILED(hr)) DllUnregisterServer();
    return hr;
}
