// Completionist's TSF text service: an English "keyboard" that never transforms keys itself. It watches the
// text around the caret, asks the engine for word completions and a phrase continuation, draws them in
// a popup at the caret and lets Tab/Up/Down/Esc/Ctrl+Right/Ctrl+Space drive it. Everything else about
// typing is left to the app.
//
// Threading: TSF calls arrive on the app's UI thread. The engine round trip runs on the engine
// client's worker thread; replies come back as WM_COMPLETIONIST_REPLY messages on the UI thread.
// No exception may cross a COM boundary, so every entry point is wrapped in COMPLETIONIST_GUARD.

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <msctf.h>
#include <olectl.h>
#include <strsafe.h>
#include <initguid.h>
#include <inputscope.h>

#include <cwctype>
#include <functional>
#include <memory>
#include <new>
#include <string>
#include <vector>

#include "engine_client.h"
#include "log.h"
#include "popup.h"
#include "popup_model.h"
#include "protocol.h"
#include "resource.h"

namespace {

using completionist::EngineClient;
using completionist::LogDebug;
using completionist::LogError;

// {71B17AFC-9D1A-4E42-A7C3-2F4151AC6ABF}
constexpr CLSID CLSID_CompletionistService = {0x71b17afc, 0x9d1a, 0x4e42, {0xa7, 0xc3, 0x2f, 0x41, 0x51, 0xac, 0x6a, 0xbf}};
// {61BEEED0-FFE4-4E6C-AEC1-9333F155674B}
constexpr GUID GUID_CompletionistProfile = {0x61beeed0, 0xffe4, 0x4e6c, {0xae, 0xc1, 0x93, 0x33, 0xf1, 0x55, 0x67, 0x4b}};
constexpr wchar_t kClsidKey[] = L"CLSID\\{71B17AFC-9D1A-4E42-A7C3-2F4151AC6ABF}";
constexpr wchar_t kDescription[] = L"Completionist";
// Registered under each English variant, so it shows up whichever one the user has installed.
constexpr LANGID kLangIds[] = {
    MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US),
    MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_CAN),
    MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_UK),
};
const GUID kCategories[] = {
    GUID_TFCAT_TIP_KEYBOARD,
    GUID_TFCAT_TIPCAP_COMLESS,
    GUID_TFCAT_TIPCAP_SYSTRAYSUPPORT,
    GUID_TFCAT_TIPCAP_IMMERSIVESUPPORT,  // without it, Settings won't list the keyboard
};

constexpr LONG kBeforeChars = 8000;  // how much text before the caret goes to the engine
constexpr LONG kAfterChars = 2000;
constexpr UINT_PTR kRetryTimer = 1;
constexpr UINT_PTR kArmTimer = 2;  // fires when the phrase row becomes the highlighted one
constexpr UINT kRetryDelayMs = 40;
constexpr int kMaxRetries = 3;

HINSTANCE g_module = nullptr;
LONG g_objects = 0;  // live COM objects plus server locks

#define COMPLETIONIST_GUARD_BEGIN try {
#define COMPLETIONIST_GUARD_END(fallback)                       \
    }                                                   \
    catch (...) {                                       \
        LogError(L"exception caught in %S", __func__);  \
        return fallback;                                \
    }

// ---------------------------------------------------------------------------------------------
// Text helpers

bool IsWordChar(wchar_t c) { return iswalpha(c) || c == L'\''; }

std::wstring TrailingWord(const std::wstring& before) {
    size_t start = before.size();
    while (start > 0 && IsWordChar(before[start - 1])) --start;
    while (start < before.size() && before[start] == L'\'') ++start;  // a word starts with a letter
    return before.substr(start);
}

bool StartsWithLetter(const std::wstring& after) { return !after.empty() && iswalpha(after[0]); }

// Whether everything in `text` from `from` on is a letter, digit or apostrophe (no word boundary crossed).
bool OnlyWordChars(const std::wstring& text, size_t from) {
    for (size_t i = from; i < text.size(); ++i)
        if (!(iswalnum(text[i]) || text[i] == L'\'')) return false;
    return true;
}

bool EqualsIgnoreCase(const std::wstring& a, const std::wstring& b) {
    return a.size() == b.size() && CompareStringOrdinal(a.c_str(), static_cast<int>(a.size()), b.c_str(),
                                                        static_cast<int>(b.size()), TRUE) == CSTR_EQUAL;
}

// The next word of a phrase with the whitespace in front of it: what Ctrl+Right inserts.
std::wstring NextPhraseWord(const std::wstring& phrase) {
    size_t i = 0;
    while (i < phrase.size() && iswspace(phrase[i])) ++i;
    size_t j = i;
    while (j < phrase.size() && !iswspace(phrase[j])) ++j;
    return j > i ? phrase.substr(0, j) : phrase;
}

std::wstring ExeName() {
    wchar_t path[MAX_PATH] = {};
    DWORD len = GetModuleFileNameW(nullptr, path, MAX_PATH);
    if (len == 0 || len >= MAX_PATH) return L"";
    const wchar_t* name = wcsrchr(path, L'\\');
    std::wstring exe = name ? name + 1 : path;
    CharLowerBuffW(exe.data(), static_cast<DWORD>(exe.size()));
    return exe;
}

completionist::Key ToKey(WPARAM vk) {
    switch (vk) {
        case VK_TAB: return completionist::Key::Tab;
        case VK_UP: return completionist::Key::Up;
        case VK_DOWN: return completionist::Key::Down;
        case VK_LEFT: return completionist::Key::Left;
        case VK_RIGHT: return completionist::Key::Right;
        case VK_SPACE: return completionist::Key::Space;
        case VK_ESCAPE: return completionist::Key::Escape;
        case VK_RETURN: return completionist::Key::Enter;
        default: return completionist::Key::Other;
    }
}

// Held right now, by the thread's view of the keyboard or the system's (they agree in real use; the
// system one also works when keys are injected for tests while another window has the focus).
bool KeyHeld(int vk) { return ((GetKeyState(vk) | GetAsyncKeyState(vk)) & 0x8000) != 0; }

completionist::Modifiers CurrentModifiers() {
    completionist::Modifiers m;
    m.ctrl = KeyHeld(VK_CONTROL);
    m.alt = KeyHeld(VK_MENU);
    m.shift = KeyHeld(VK_SHIFT);
    return m;
}

std::uint64_t NowMs() { return GetTickCount64(); }

// ---------------------------------------------------------------------------------------------
// TSF helpers (all require a valid edit cookie)

// Text next to `anchor`, up to `limit` characters.
std::wstring ReadBeside(ITfRange* anchor, TfEditCookie ec, bool before, LONG limit) {
    ITfRange* range = nullptr;
    if (FAILED(anchor->Clone(&range))) return {};
    range->Collapse(ec, before ? TF_ANCHOR_START : TF_ANCHOR_END);
    LONG moved = 0;
    HRESULT hr = before ? range->ShiftStart(ec, -limit, &moved, nullptr) : range->ShiftEnd(ec, limit, &moved, nullptr);
    std::wstring text;
    if (SUCCEEDED(hr) && moved != 0) {
        text.resize(static_cast<size_t>(limit));
        ULONG got = 0;
        if (FAILED(range->GetText(ec, 0, text.data(), static_cast<ULONG>(limit), &got))) got = 0;
        text.resize(got);
    }
    range->Release();
    return text;
}

const char* ScopeName(InputScope scope) {
    switch (scope) {
        case IS_DEFAULT: return "IS_DEFAULT";
        case IS_URL: return "IS_URL";
        case IS_EMAIL_USERNAME: return "IS_EMAIL_USERNAME";
        case IS_EMAIL_SMTPEMAILADDRESS: return "IS_EMAIL_SMTPEMAILADDRESS";
        case IS_NUMBER_FULLWIDTH: return "IS_NUMBER_FULLWIDTH";
        case IS_NUMBER: return "IS_NUMBER";
        case IS_DIGITS: return "IS_DIGITS";
        case IS_TELEPHONE_FULLTELEPHONENUMBER: return "IS_TELEPHONE_FULLTELEPHONENUMBER";
        case IS_TELEPHONE_LOCALNUMBER: return "IS_TELEPHONE_LOCALNUMBER";
        case IS_PASSWORD: return "IS_PASSWORD";
        case IS_SEARCH: return "IS_SEARCH";
        case IS_TEXT: return "IS_TEXT";
        case IS_CHAT: return "IS_CHAT";
        case IS_EMAILNAME_OR_ADDRESS: return "IS_EMAILNAME_OR_ADDRESS";
        default: return nullptr;
    }
}

// Input scope is an *app* property: the application supplies it, so it's read via GetAppProperty.
std::vector<std::string> ReadInputScopes(ITfContext* context, TfEditCookie ec, ITfRange* range) {
    std::vector<std::string> names;
    ITfReadOnlyProperty* property = nullptr;
    if (FAILED(context->GetAppProperty(GUID_PROP_INPUTSCOPE, &property)) || !property) return names;
    VARIANT value;
    VariantInit(&value);
    if (SUCCEEDED(property->GetValue(ec, range, &value)) && value.vt == VT_UNKNOWN && value.punkVal) {
        ITfInputScope* inputScope = nullptr;
        if (SUCCEEDED(value.punkVal->QueryInterface(IID_ITfInputScope, reinterpret_cast<void**>(&inputScope)))) {
            InputScope* scopes = nullptr;
            UINT count = 0;
            if (SUCCEEDED(inputScope->GetInputScopes(&scopes, &count)) && scopes) {
                for (UINT i = 0; i < count; ++i) {
                    const char* name = ScopeName(scopes[i]);
                    names.push_back(name ? name : "IS_" + std::to_string(static_cast<int>(scopes[i])));
                }
                CoTaskMemFree(scopes);
            }
            inputScope->Release();
        }
    }
    VariantClear(&value);
    property->Release();
    return names;
}

// Whether the app has switched keyboard input methods off for this context (e.g. a password field).
bool KeyboardDisabled(ITfContext* context) {
    bool disabled = false;
    ITfCompartmentMgr* compartments = nullptr;
    if (SUCCEEDED(context->QueryInterface(IID_ITfCompartmentMgr, reinterpret_cast<void**>(&compartments)))) {
        ITfCompartment* compartment = nullptr;
        if (SUCCEEDED(compartments->GetCompartment(GUID_COMPARTMENT_KEYBOARD_DISABLED, &compartment))) {
            VARIANT value;
            VariantInit(&value);
            if (SUCCEEDED(compartment->GetValue(&value))) disabled = value.vt == VT_I4 && value.lVal != 0;
            VariantClear(&value);
            compartment->Release();
        }
        compartments->Release();
    }
    return disabled;
}

// ---------------------------------------------------------------------------------------------
// Edit sessions

class EditSession final : public ITfEditSession {
public:
    explicit EditSession(std::function<void(TfEditCookie)> body) : body_(std::move(body)) { InterlockedIncrement(&g_objects); }
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
        COMPLETIONIST_GUARD_BEGIN
        body_(ec);
        return S_OK;
        COMPLETIONIST_GUARD_END(E_FAIL)
    }

private:
    ~EditSession() { InterlockedDecrement(&g_objects); }
    LONG refs_ = 1;
    std::function<void(TfEditCookie)> body_;
};

// ---------------------------------------------------------------------------------------------
// The text service

class CompletionistService final : public ITfTextInputProcessorEx,
                           public ITfThreadMgrEventSink,
                           public ITfTextEditSink,
                           public ITfKeyEventSink,
                           public ITfCompositionSink {
public:
    CompletionistService() { InterlockedIncrement(&g_objects); }

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
    STDMETHODIMP Activate(ITfThreadMgr* threadMgr, TfClientId clientId) override { return ActivateEx(threadMgr, clientId, 0); }

    STDMETHODIMP ActivateEx(ITfThreadMgr* threadMgr, TfClientId clientId, DWORD flags) override {
        COMPLETIONIST_GUARD_BEGIN
        completionist::RefreshLogLevel();
        threadMgr_ = threadMgr;
        threadMgr_->AddRef();
        clientId_ = clientId;
        app_ = ExeName();

        ITfSource* source = nullptr;
        if (SUCCEEDED(threadMgr_->QueryInterface(IID_ITfSource, reinterpret_cast<void**>(&source)))) {
            source->AdviseSink(IID_ITfThreadMgrEventSink, static_cast<ITfThreadMgrEventSink*>(this), &threadMgrCookie_);
            source->Release();
        }
        HRESULT keyHr = E_FAIL;
        ITfKeystrokeMgr* keystrokes = nullptr;
        if (SUCCEEDED(threadMgr_->QueryInterface(IID_ITfKeystrokeMgr, reinterpret_cast<void**>(&keystrokes)))) {
            keyHr = keystrokes->AdviseKeyEventSink(clientId_, static_cast<ITfKeyEventSink*>(this), TRUE);
            keystrokes->Release();
        }
        if (!popup_.Create(g_module, &CompletionistService::PopupHook, this)) LogError(L"could not create the popup window");
        EngineClient::Instance().Acquire();
        acquired_ = true;
        LogDebug(L"activate flags=0x%lx keysink=0x%08lx app=%s", flags, keyHr, app_.c_str());

        ITfDocumentMgr* focus = nullptr;
        if (SUCCEEDED(threadMgr_->GetFocus(&focus)) && focus) {
            OnSetFocus(focus, nullptr);
            focus->Release();
        }
        return S_OK;
        COMPLETIONIST_GUARD_END(E_FAIL)
    }

    STDMETHODIMP Deactivate() override {
        COMPLETIONIST_GUARD_BEGIN
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
        HideAll();
        popup_.Destroy();
        if (acquired_) {
            EngineClient::Instance().Release();
            acquired_ = false;
        }
        clientId_ = TF_CLIENTID_NULL;
        LogDebug(L"deactivate");
        return S_OK;
        COMPLETIONIST_GUARD_END(S_OK)
    }

    // ITfThreadMgrEventSink
    STDMETHODIMP OnInitDocumentMgr(ITfDocumentMgr*) override { return S_OK; }
    STDMETHODIMP OnUninitDocumentMgr(ITfDocumentMgr*) override { return S_OK; }
    STDMETHODIMP OnPushContext(ITfContext*) override { return S_OK; }
    STDMETHODIMP OnPopContext(ITfContext*) override { return S_OK; }
    STDMETHODIMP OnSetFocus(ITfDocumentMgr* focus, ITfDocumentMgr*) override {
        COMPLETIONIST_GUARD_BEGIN
        ITfContext* context = nullptr;
        if (focus) focus->GetTop(&context);
        WatchContext(context);
        HideAll();
        if (context) {
            QueueInspect(context);
            context->Release();
        }
        return S_OK;
        COMPLETIONIST_GUARD_END(S_OK)
    }

    // ITfTextEditSink
    STDMETHODIMP OnEndEdit(ITfContext* context, TfEditCookie, ITfEditRecord*) override {
        COMPLETIONIST_GUARD_BEGIN
        model_.MarkStale();  // the words on screen belong to text that just changed
        retries_ = 0;
        QueueInspect(context);
        return S_OK;
        COMPLETIONIST_GUARD_END(S_OK)
    }

    // ITfKeyEventSink
    STDMETHODIMP OnSetFocus(BOOL foreground) override {
        COMPLETIONIST_GUARD_BEGIN
        if (!foreground) HideAll();
        return S_OK;
        COMPLETIONIST_GUARD_END(S_OK)
    }

    STDMETHODIMP OnTestKeyDown(ITfContext*, WPARAM key, LPARAM, BOOL* eaten) override {
        COMPLETIONIST_GUARD_BEGIN
        *eaten = model_.Peek(ToKey(key), CurrentModifiers(), NowMs()).consume;
        return S_OK;
        COMPLETIONIST_GUARD_END(S_OK)
    }

    STDMETHODIMP OnKeyDown(ITfContext* context, WPARAM key, LPARAM, BOOL* eaten) override {
        COMPLETIONIST_GUARD_BEGIN
        completionist::KeyDecision decision = model_.OnKey(ToKey(key), CurrentModifiers(), NowMs());
        *eaten = decision.consume;
        if (!decision.consume) return S_OK;
        eatenKey_ = key;
        switch (decision.action) {
            case completionist::Action::Accept: Accept(context, decision.index); break;
            case completionist::Action::AcceptPhrase:
                popup_.Hide();
                InsertPhrase(context, phrase_, "phrase");
                break;
            case completionist::Action::AcceptPhraseWord: InsertPhrase(context, NextPhraseWord(phrase_), "phrase_word"); break;
            case completionist::Action::RequestPhrase:
                dismissed_ = false;  // asking outweighs an earlier Esc
                dismissedBefore_.clear();
                hotkeyPending_ = true;
                QueueInspect(context);  // reads the text now, then sends the request
                break;
            case completionist::Action::Dismiss: {
                dismissed_ = true;
                dismissedBefore_ = promptBefore_;
                phrase_.clear();
                popup_.Hide();
                completionist::protocol::Request dismiss;
                dismiss.id = latestId_;
                dismiss.event = "dismiss";
                dismiss.app = app_;
                EngineClient::Instance().Send(std::move(dismiss), nullptr);
                break;
            }
            case completionist::Action::MoveHighlight: Render(); break;
            case completionist::Action::None: break;
        }
        return S_OK;
        COMPLETIONIST_GUARD_END(S_OK)
    }

    // The key-up of a key we consumed is consumed too, so the app never sees half a keystroke.
    STDMETHODIMP OnTestKeyUp(ITfContext*, WPARAM key, LPARAM, BOOL* eaten) override {
        *eaten = eatenKey_ != 0 && key == eatenKey_;
        return S_OK;
    }
    STDMETHODIMP OnKeyUp(ITfContext*, WPARAM key, LPARAM, BOOL* eaten) override {
        *eaten = eatenKey_ != 0 && key == eatenKey_;
        if (*eaten) eatenKey_ = 0;
        return S_OK;
    }
    STDMETHODIMP OnPreservedKey(ITfContext*, REFGUID, BOOL* eaten) override {
        *eaten = FALSE;
        return S_OK;
    }

    // ITfCompositionSink
    STDMETHODIMP OnCompositionTerminated(TfEditCookie, ITfComposition*) override { return S_OK; }

private:
    ~CompletionistService() { InterlockedDecrement(&g_objects); }

    // Hide the popup and forget any words or phrase still being computed.
    void HideAll() {
        popup_.Hide();
        model_.Close();
        phrase_.clear();
        latestId_ = 0;
        hotkeyPending_ = false;
        if (popup_.hwnd()) KillTimer(popup_.hwnd(), kArmTimer);
    }

    // Draws the popup for the current model state, and arranges a redraw when the phrase row becomes
    // the highlighted one.
    void Render() {
        if (!model_.visible()) {
            popup_.Hide();
            if (popup_.hwnd()) KillTimer(popup_.hwnd(), kArmTimer);
            return;
        }
        std::uint64_t now = NowMs();
        completionist::PopupContent content;
        content.words = words_;
        content.typedChars = static_cast<int>(promptWord_.size());
        content.phrase = phrase_;
        content.phraseLead = promptWord_;
        popup_.Show(content, model_.selection(now), caret_);
        if (popup_.hwnd()) {
            std::uint64_t armedAt = model_.armed_at(now);
            if (armedAt) SetTimer(popup_.hwnd(), kArmTimer, static_cast<UINT>(armedAt - now + 5), nullptr);
            else KillTimer(popup_.hwnd(), kArmTimer);
        }
    }

    // Reads the context in an async read session; coalesces bursts of edits into one read.
    void QueueInspect(ITfContext* context) {
        if (inspectQueued_ || !context) return;
        inspectQueued_ = true;
        auto* session = new (std::nothrow) EditSession([this, context = ComPtrHold(context)](TfEditCookie ec) mutable {
            inspectQueued_ = false;
            Inspect(context.get(), ec);
        });
        if (!session) {
            inspectQueued_ = false;
            return;
        }
        HRESULT sessionHr = S_OK;
        HRESULT hr = context->RequestEditSession(clientId_, session, TF_ES_ASYNCDONTCARE | TF_ES_READ, &sessionHr);
        session->Release();
        if (FAILED(hr)) {
            // Chromium apps fail this transiently around focus changes; try again shortly.
            inspectQueued_ = false;
            LogDebug(L"RequestEditSession failed hr=0x%08lx", hr);
            ScheduleRetry();
        }
    }

    void ScheduleRetry() {
        if (retries_ < kMaxRetries && popup_.hwnd() && watched_) {
            ++retries_;
            SetTimer(popup_.hwnd(), kRetryTimer, kRetryDelayMs, nullptr);
        }
    }

    // Holds a reference to an ITfContext for as long as an edit session needs it.
    struct ComPtrHold {
        explicit ComPtrHold(ITfContext* p) : ptr_(p) { ptr_->AddRef(); }
        ComPtrHold(const ComPtrHold& o) : ptr_(o.ptr_) { ptr_->AddRef(); }
        ~ComPtrHold() { ptr_->Release(); }
        ITfContext* get() const { return ptr_; }
        ITfContext* ptr_;
    };

    void Inspect(ITfContext* context, TfEditCookie ec) {
        bool hotkey = hotkeyPending_;
        hotkeyPending_ = false;

        TF_SELECTION selection = {};
        ULONG fetched = 0;
        if (FAILED(context->GetSelection(ec, TF_DEFAULT_SELECTION, 1, &selection, &fetched)) || fetched == 0) {
            HideAll();
            return;
        }
        BOOL selectionEmpty = TRUE;
        selection.range->IsEmpty(ec, &selectionEmpty);
        if (!selectionEmpty) {  // text is selected: nothing to complete
            selection.range->Release();
            HideAll();
            return;
        }

        std::vector<std::string> scopes = ReadInputScopes(context, ec, selection.range);
        bool secret = KeyboardDisabled(context);
        for (const std::string& scope : scopes) secret = secret || scope == "IS_PASSWORD";
        if (secret) {  // never send anything typed into a password field, not even to the local engine
            selection.range->Release();
            HideAll();
            return;
        }

        std::wstring before = ReadBeside(selection.range, ec, true, kBeforeChars);
        std::wstring after = ReadBeside(selection.range, ec, false, kAfterChars);
        std::wstring word = TrailingWord(before);

        RECT caret = {};
        BOOL clipped = FALSE;
        HRESULT extentHr = E_FAIL;
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
                        if (SUCCEEDED(view->GetTextExt(ec, point, &previous, &clipped)))
                            caret = {previous.right, previous.top, previous.right, previous.bottom};
                    }
                }
                point->Release();
            }
            HWND hwnd = nullptr;
            if (SUCCEEDED(view->GetWnd(&hwnd)) && hwnd) GetWindowTextW(GetAncestor(hwnd, GA_ROOT), title, 256);
            view->Release();
        }
        selection.range->Release();

        if (extentHr == TF_E_NOLAYOUT) {  // the app hasn't laid the text out yet
            HideAll();
            hotkeyPending_ = hotkey;
            ScheduleRetry();
            return;
        }

        // After Esc, stay quiet while the writer is still in the same word (or the same gap).
        bool suppressed = false;
        if (dismissed_) {
            suppressed = before.size() >= dismissedBefore_.size() && before.compare(0, dismissedBefore_.size(), dismissedBefore_) == 0 &&
                         OnlyWordChars(before, dismissedBefore_.size());
            if (!suppressed) {
                dismissed_ = false;
                dismissedBefore_.clear();
            }
        }
        // After Tab on a word, don't pop straight back up for the word just inserted.
        if (!acceptedWord_.empty() && word != acceptedWord_) acceptedWord_.clear();

        // What may be shown for this text. The request is sent regardless, so the engine can learn from
        // what is typed even where suggestions are held back.
        bool caretOk = SUCCEEDED(extentHr) && !StartsWithLetter(after);
        wordsAllowed_ = caretOk && !word.empty() && !suppressed && acceptedWord_.empty();
        phraseAllowed_ = caretOk && !suppressed;
        if (!wordsAllowed_ && !phraseAllowed_) {
            popup_.Hide();
            model_.Close();
            phrase_.clear();
        }

        completionist::protocol::Request request;
        request.id = EngineClient::Instance().NextId();
        request.event = "keystroke";
        request.app = app_;
        request.title = title;
        request.input_scope = scopes;
        request.before = before;
        request.after = after;
        request.quiet = !phraseAllowed_;  // no point paying for a phrase nobody will see

        latestId_ = request.id;
        promptWord_ = word;
        promptBefore_ = before;
        caret_ = caret;
        uint32_t id = request.id;
        EngineClient::Instance().Send(std::move(request), popup_.hwnd());
        if (hotkey && caretOk) {
            completionist::protocol::Request ask;  // same id, so the streamed phrase comes back to this text
            ask.id = id;
            ask.event = "hotkey";
            ask.app = app_;
            ask.title = title;
            ask.input_scope = std::move(scopes);
            ask.before = before;
            ask.after = after;
            EngineClient::Instance().Send(std::move(ask), nullptr);
        }
        LogDebug(L"inspect word=\"%s\" words=%d phrase=%d hotkey=%d", word.c_str(), wordsAllowed_, phraseAllowed_, hotkey);
    }

    void OnReply(const completionist::protocol::WordReply& reply) {
        if (reply.id != latestId_) return;  // for text that has since changed
        if (reply.kind == completionist::protocol::ReplyKind::Phrase) {
            phrase_ = phraseAllowed_ ? reply.phrase : std::wstring();
            model_.SetPhrase(!phrase_.empty(), NowMs());
            Render();
            return;
        }
        model_.SetPhraseAvailable(reply.phrase_mode != "off");
        bool useWords = wordsAllowed_ && !reply.words.empty() && reply.replace == static_cast<int>(promptWord_.size());
        words_ = useWords ? reply.words : std::vector<std::wstring>();
        phrase_ = phraseAllowed_ ? reply.phrase : std::wstring();
        model_.Open(words_.size());
        model_.SetPhrase(!phrase_.empty(), NowMs());
        Render();
    }

    // Replaces the typed part of the current word with words_[index].
    void Accept(ITfContext* context, std::size_t index) {
        popup_.Hide();
        if (index >= words_.size()) return;
        std::wstring chosen = words_[index];
        std::wstring typed = promptWord_;
        if (typed.empty()) return;

        auto body = [this, context = ComPtrHold(context), chosen, typed](TfEditCookie ec) mutable {
            ReplaceWord(context.get(), ec, typed, chosen);
        };
        RunWriteSession(context, body);
    }

    // Inserts phrase text at the caret (the whole phrase, or its next word).
    void InsertPhrase(ITfContext* context, const std::wstring& text, const char* kind) {
        if (text.empty()) return;
        std::wstring expectedBefore = promptBefore_;
        auto body = [this, context = ComPtrHold(context), text, expectedBefore, kind](TfEditCookie ec) mutable {
            InsertAtCaret(context.get(), ec, text, expectedBefore, kind);
        };
        RunWriteSession(context, body);
    }

    void RunWriteSession(ITfContext* context, const std::function<void(TfEditCookie)>& body) {
        // Synchronous first, so a key typed straight after Tab can't slip in before the text changes.
        auto* session = new (std::nothrow) EditSession(body);
        if (!session) return;
        HRESULT sessionHr = S_OK;
        HRESULT hr = context->RequestEditSession(clientId_, session, TF_ES_SYNC | TF_ES_READWRITE, &sessionHr);
        session->Release();
        if (FAILED(hr)) {
            LogDebug(L"sync edit refused hr=0x%08lx; retrying async", hr);
            session = new (std::nothrow) EditSession(body);
            if (!session) return;
            hr = context->RequestEditSession(clientId_, session, TF_ES_ASYNCDONTCARE | TF_ES_READWRITE, &sessionHr);
            session->Release();
            if (FAILED(hr)) LogError(L"could not request an edit session hr=0x%08lx", hr);
        }
    }

    void ReplaceWord(ITfContext* context, TfEditCookie ec, const std::wstring& typed, const std::wstring& chosen) {
        TF_SELECTION selection = {};
        ULONG fetched = 0;
        if (FAILED(context->GetSelection(ec, TF_DEFAULT_SELECTION, 1, &selection, &fetched)) || fetched == 0) return;
        // The words on screen were for `typed`; make sure that's still what precedes the caret.
        std::wstring current = TrailingWord(ReadBeside(selection.range, ec, true, 200));
        if (!EqualsIgnoreCase(current, typed)) {
            LogDebug(L"accept skipped: the text changed from \"%s\" to \"%s\"", typed.c_str(), current.c_str());
            selection.range->Release();
            return;
        }
        ITfRange* range = nullptr;
        if (FAILED(selection.range->Clone(&range))) {
            selection.range->Release();
            return;
        }
        selection.range->Release();

        range->Collapse(ec, TF_ANCHOR_START);
        LONG moved = 0;
        range->ShiftStart(ec, -static_cast<LONG>(typed.size()), &moved, nullptr);
        HRESULT setHr = range->SetText(ec, 0, chosen.c_str(), static_cast<LONG>(chosen.size()));
        if (SUCCEEDED(setHr)) {
            MoveCaretToEnd(context, ec, range);
            acceptedWord_ = chosen;  // don't pop straight back up for the word just inserted
            completionist::protocol::Request accept;
            accept.id = EngineClient::Instance().NextId();
            accept.event = "accept";
            accept.app = app_;
            accept.before = promptBefore_;
            accept.accepted = chosen;
            EngineClient::Instance().Send(std::move(accept), nullptr);
        } else {
            LogError(L"SetText failed hr=0x%08lx", setHr);
        }
        range->Release();
    }

    void InsertAtCaret(ITfContext* context, TfEditCookie ec, const std::wstring& text, const std::wstring& expectedBefore, const char* kind) {
        TF_SELECTION selection = {};
        ULONG fetched = 0;
        if (FAILED(context->GetSelection(ec, TF_DEFAULT_SELECTION, 1, &selection, &fetched)) || fetched == 0) return;
        BOOL empty = TRUE;
        selection.range->IsEmpty(ec, &empty);
        // The phrase was written for the text before the caret at the time; check it's still there.
        std::wstring tail = ReadBeside(selection.range, ec, true, 200);
        size_t check = std::min<size_t>(40, expectedBefore.size());
        bool same = check == 0 || (tail.size() >= check && tail.compare(tail.size() - check, check, expectedBefore, expectedBefore.size() - check, check) == 0);
        if (!empty || !same) {
            LogDebug(L"phrase insert skipped: the text changed");
            selection.range->Release();
            return;
        }
        ITfRange* range = nullptr;
        if (FAILED(selection.range->Clone(&range))) {
            selection.range->Release();
            return;
        }
        selection.range->Release();
        range->Collapse(ec, TF_ANCHOR_END);
        HRESULT setHr = range->SetText(ec, 0, text.c_str(), static_cast<LONG>(text.size()));
        if (SUCCEEDED(setHr)) {
            MoveCaretToEnd(context, ec, range);
            phrase_.erase(0, std::min(text.size(), phrase_.size()));  // what's left, until the engine confirms
            completionist::protocol::Request accept;
            accept.id = EngineClient::Instance().NextId();
            accept.event = "accept";
            accept.kind = kind;
            accept.app = app_;
            accept.before = expectedBefore;
            accept.accepted = text;
            EngineClient::Instance().Send(std::move(accept), nullptr);
        } else {
            LogError(L"phrase SetText failed hr=0x%08lx", setHr);
        }
        range->Release();
    }

    static void MoveCaretToEnd(ITfContext* context, TfEditCookie ec, ITfRange* range) {
        range->Collapse(ec, TF_ANCHOR_END);
        TF_SELECTION caret = {};
        caret.range = range;
        caret.style.ase = TF_AE_NONE;
        caret.style.fInterimChar = FALSE;
        context->SetSelection(ec, 1, &caret);
    }

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
        inspectQueued_ = false;
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

    static bool PopupHook(void* self, UINT message, WPARAM wParam, LPARAM lParam) {
        auto* service = static_cast<CompletionistService*>(self);
        try {
            if (message == completionist::WM_COMPLETIONIST_REPLY) {
                std::unique_ptr<completionist::protocol::WordReply> reply(reinterpret_cast<completionist::protocol::WordReply*>(lParam));
                if (reply) service->OnReply(*reply);
                return true;
            }
            if (message == WM_TIMER && wParam == kRetryTimer) {
                KillTimer(service->popup_.hwnd(), kRetryTimer);
                if (service->watched_) service->QueueInspect(service->watched_);
                return true;
            }
            if (message == WM_TIMER && wParam == kArmTimer) {
                KillTimer(service->popup_.hwnd(), kArmTimer);
                service->Render();  // the phrase row is now the highlighted one
                return true;
            }
        } catch (...) {
            LogError(L"exception caught in PopupHook");
            return true;
        }
        return false;
    }

    LONG refs_ = 1;
    ITfThreadMgr* threadMgr_ = nullptr;
    TfClientId clientId_ = TF_CLIENTID_NULL;
    DWORD threadMgrCookie_ = TF_INVALID_COOKIE;
    ITfContext* watched_ = nullptr;
    DWORD watchCookie_ = TF_INVALID_COOKIE;
    bool acquired_ = false;
    std::wstring app_;

    completionist::Popup popup_;
    completionist::PopupModel model_;
    std::vector<std::wstring> words_;
    std::wstring phrase_;  // the phrase continuation on screen (what's left of it)
    WPARAM eatenKey_ = 0;

    bool inspectQueued_ = false;
    int retries_ = 0;

    // State of the request the popup is waiting on or showing.
    std::uint32_t latestId_ = 0;
    std::wstring promptWord_;    // the typed part of the word the words complete
    std::wstring promptBefore_;  // text before the caret when it was asked
    RECT caret_ = {};
    bool wordsAllowed_ = false;   // this text may show word completions
    bool phraseAllowed_ = false;  // ...and/or a phrase
    bool hotkeyPending_ = false;  // Ctrl+Space was pressed: ask for a phrase once the text has been read
    bool dismissed_ = false;      // after Esc: quiet while the writer stays in the same word (or gap)
    std::wstring dismissedBefore_;
    std::wstring acceptedWord_;  // after Tab: stay quiet while the text is exactly this word
};

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
        COMPLETIONIST_GUARD_BEGIN
        if (!ppv) return E_INVALIDARG;
        *ppv = nullptr;
        if (outer) return CLASS_E_NOAGGREGATION;
        auto* service = new (std::nothrow) CompletionistService();
        if (!service) return E_OUTOFMEMORY;
        HRESULT hr = service->QueryInterface(riid, ppv);
        service->Release();
        return hr;
        COMPLETIONIST_GUARD_END(E_FAIL)
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
    if (clsid != CLSID_CompletionistService) return CLASS_E_CLASSNOTAVAILABLE;
    return g_factory.QueryInterface(riid, ppv);
}

STDAPI DllCanUnloadNow() { return g_objects == 0 ? S_OK : S_FALSE; }

STDAPI DllUnregisterServer() {
    ITfInputProcessorProfileMgr* profiles = nullptr;
    if (SUCCEEDED(CoCreateInstance(CLSID_TF_InputProcessorProfiles, nullptr, CLSCTX_INPROC_SERVER,
                                   IID_ITfInputProcessorProfileMgr, reinterpret_cast<void**>(&profiles)))) {
        for (LANGID langId : kLangIds) profiles->UnregisterProfile(CLSID_CompletionistService, langId, GUID_CompletionistProfile, 0);
        profiles->Release();
    }
    ITfCategoryMgr* categories = nullptr;
    if (SUCCEEDED(CoCreateInstance(CLSID_TF_CategoryMgr, nullptr, CLSCTX_INPROC_SERVER, IID_ITfCategoryMgr,
                                   reinterpret_cast<void**>(&categories)))) {
        for (const GUID& category : kCategories) categories->UnregisterCategory(CLSID_CompletionistService, category, CLSID_CompletionistService);
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
        for (LANGID langId : kLangIds) {
            // A negative icon index means "the icon resource with this id" (as the Windows IME samples do).
            hr = profiles->RegisterProfile(CLSID_CompletionistService, langId, GUID_CompletionistProfile, kDescription,
                                           static_cast<ULONG>(wcslen(kDescription)), path, length,
                                           static_cast<ULONG>(-IDI_COMPLETIONIST), nullptr, 0, TRUE, 0);
            if (FAILED(hr)) break;
        }
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
            hr = categories->RegisterCategory(CLSID_CompletionistService, category, CLSID_CompletionistService);
            if (FAILED(hr)) break;
        }
        categories->Release();
    }
    if (FAILED(hr)) DllUnregisterServer();
    return hr;
}
