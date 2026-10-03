#include "popup.h"

#include <algorithm>
#include <cmath>
#include <cwctype>
#include <string>
#include <winreg.h>

#include "log.h"
#include "popup_layout.h"
#include "popup_palette.h"

namespace completionist {

namespace {

constexpr wchar_t kClassName[] = L"CompletionistPopup";

const std::vector<int> kNoMarks;  // empty default when a reply carries no marks

int Scale(int value, UINT dpi) { return MulDiv(value, static_cast<int>(dpi), 96); }

UINT DpiOf(HWND hwnd) {
    UINT dpi = GetDpiForWindow(hwnd);
    return dpi ? dpi : 96;
}

std::wstring OneLine(const std::wstring& text) {
    std::wstring out = text;
    for (wchar_t& c : out)
        if (c == L'\r' || c == L'\n' || c == L'\t') c = L' ';
    return out;
}

COLORREF Color(renderer::palette::Color c) { return RGB(c.r, c.g, c.b); }

bool HasStatus(render::AiState state) {
    return state == render::AiState::Manual || state == render::AiState::Scheduled ||
           state == render::AiState::Working || state == render::AiState::Streaming ||
           state == render::AiState::Unavailable;
}

int SelectedCorrectionIndex(const PopupContent& content, int selection) {
    return selection >= 0 && selection < static_cast<int>(content.words.size()) &&
                   selection < static_cast<int>(content.marks.size()) && !content.marks[selection].empty()
               ? selection : -1;
}

std::wstring AiStatus(const PopupContent& content) {
    const wchar_t* reason = content.triggerReason == "idle" ? L" · Idle pause" :
                            content.triggerReason == "manual" ? L" · Manual shortcut" :
                            content.triggerReason == "paused" ? L" · Paused" :
                            content.triggerReason == "unavailable" ? L" · Unavailable" : L"";
    switch (content.ai) {
        case render::AiState::Manual: return L"Ctrl+Space to request a phrase" + std::wstring(reason);
        case render::AiState::Scheduled:
            return std::to_wstring(static_cast<double>(content.phraseWaitMs) / 1000.0).substr(0, 3) + L"s until AI" + reason;
        case render::AiState::Working:
            return L"Working · " + std::to_wstring(static_cast<double>(content.phraseElapsedMs) / 1000.0).substr(0, 3) + L"s" + reason;
        case render::AiState::Streaming: return L"Streaming phrase" + std::wstring(reason);
        case render::AiState::Unavailable: return L"AI unavailable";
        default: return {};
    }
}

int Px(float dip, UINT dpi) { return static_cast<int>(std::lround(dip * (dpi ? dpi : 96) / 96.0)); }

RECT RectPx(const layout::DipRect& rect, UINT dpi) {
    return {Px(rect.left, dpi), Px(rect.top, dpi), Px(rect.right, dpi), Px(rect.bottom, dpi)};
}

}  // namespace

bool Popup::Create(HINSTANCE module, MessageHook hook, void* context) {
    module_ = module;
    hook_ = hook;
    context_ = context;

    WNDCLASSEXW wc = {sizeof(wc)};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = module;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = kClassName;
    RegisterClassExW(&wc);  // fails harmlessly if another thread of this process already registered it

    hwnd_ = CreateWindowExW(WS_EX_TOPMOST | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW, kClassName, L"", WS_POPUP, 0, 0, 10, 10,
                            nullptr, nullptr, module, nullptr);
    if (!hwnd_) {
        LogError(L"could not create the popup window, error %lu", GetLastError());
        return false;
    }
    SetWindowLongPtrW(hwnd_, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
    return true;
}

void Popup::Destroy() {
    if (hwnd_) {
        SetWindowLongPtrW(hwnd_, GWLP_USERDATA, 0);
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
        UnregisterClassW(kClassName, module_);  // fails harmlessly while other threads still use the class
    }
    if (font_) {
        DeleteObject(font_);
        font_ = nullptr;
    }
    shown_ = false;
}

void Popup::EnsureFont(UINT dpi) {
    if (font_ && fontDpi_ == dpi) return;
    if (font_) DeleteObject(font_);
    font_ = CreateFontW(-MulDiv(settings_.font_size, static_cast<int>(dpi), 72), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
                        L"Segoe UI");
    fontDpi_ = dpi;
}

void Popup::SetSettings(const PopupSettings& settings) {
    if (settings_.font_size != settings.font_size) fontDpi_ = 0;  // rebuild the font on the next Show
    settings_ = settings;
}

void Popup::Show(const PopupContent& content, int selection, const RECT& caret) {
    if (!hwnd_ || (content.words.empty() && content.phrase.empty() && !HasStatus(content.ai))) {
        Hide();
        return;
    }
    content_ = content;
    content_.phrase = OneLine(content_.phrase);
    selection_ = selection;

    // Move first (hidden) so the DPI is that of the monitor the popup will appear on.
    SetWindowPos(hwnd_, HWND_TOPMOST, caret.left, caret.bottom, 0, 0, SWP_NOSIZE | SWP_NOACTIVATE);
    dpi_ = DpiOf(hwnd_);
    EnsureFont(dpi_);

    HDC dc = GetDC(hwnd_);
    HGDIOBJ old = SelectObject(dc, font_);
    TEXTMETRICW metrics = {};
    GetTextMetricsW(dc, &metrics);
    const float scale = static_cast<float>(dpi_) / 96.0f;
    layout::ContentMetrics measured{};
    measured.fontSizeDip = static_cast<float>(settings_.font_size) * 96.0f / 72.0f;
    measured.rowGapDip = 2.0f;
    const int rowHeight = metrics.tmHeight + Scale(12, dpi_);
    measured.phraseHeightDip = static_cast<float>(rowHeight) / scale;
    const bool hasAiStatus = HasStatus(content_.ai);
    const bool hasCorrection = SelectedCorrectionIndex(content_, selection_) >= 0;
    measured.hasAuxiliaryShelf = hasCorrection;
    const int statusLines = (hasAiStatus ? 1 : 0) + (hasCorrection ? 1 : 0);
    measured.statusHeightDip = statusLines ? static_cast<float>((metrics.tmHeight + Scale(4, dpi_)) * statusLines) / scale : 0.0f;
    render::Snapshot snapshot{};
    snapshot.settings = settings_;
    snapshot.ai = content_.ai;
    snapshot.phrase = content_.phrase;
    snapshot.phraseLead = content_.phraseLead;
    snapshot.typedFragment = content_.phraseLead;
    snapshot.selection = selection;
    for (std::size_t i = 0; i < content_.words.size(); ++i) {
        render::Candidate candidate{};
        candidate.text = content_.words[i];
        if (i < content_.origins.size()) candidate.origin = content_.origins[i];
        snapshot.words.push_back(std::move(candidate));
        std::wstring label = content_.words[i];
        if (i < content_.origins.size()) {
            if (content_.origins[i] == "local") label += L"  Local";
            else if (content_.origins[i] == "learned") label += L"  Learned";
        }
        SIZE size{};
        GetTextExtentPoint32W(dc, label.c_str(), static_cast<int>(label.size()), &size);
        measured.rows.push_back({static_cast<float>(size.cx) / scale, static_cast<float>(rowHeight) / scale});
        measured.measuredContentWidthDip = std::max(measured.measuredContentWidthDip, static_cast<float>(size.cx) / scale);
    }
    std::wstring phraseText = content_.phraseLead + content_.phrase;
    SIZE phraseSize{};
    if (!phraseText.empty()) GetTextExtentPoint32W(dc, phraseText.c_str(), static_cast<int>(phraseText.size()), &phraseSize);
    measured.measuredContentWidthDip = std::max(measured.measuredContentWidthDip, static_cast<float>(phraseSize.cx) / scale);
    SelectObject(dc, old);
    ReleaseDC(hwnd_, dc);

    RECT work = {GetSystemMetrics(SM_XVIRTUALSCREEN), GetSystemMetrics(SM_YVIRTUALSCREEN),
                 GetSystemMetrics(SM_XVIRTUALSCREEN) + GetSystemMetrics(SM_CXVIRTUALSCREEN),
                 GetSystemMetrics(SM_YVIRTUALSCREEN) + GetSystemMetrics(SM_CYVIRTUALSCREEN)};
    MONITORINFO monitor = {sizeof(monitor)};
    if (GetMonitorInfoW(MonitorFromRect(&caret, MONITOR_DEFAULTTONEAREST), &monitor)) work = monitor.rcWork;
    render::Rect workBounds{work.left, work.top, work.right, work.bottom};
    snapshot.caret = {caret.left, caret.top, caret.right, caret.bottom};
    layout::WorkArea workArea{workBounds, dpi_};
    layout::Layout placed = layout::Place(snapshot, workArea, measured);
    const int x = placed.menuBounds.left;
    const int y = placed.menuBounds.top;
    const int width = placed.menuBounds.right - x;
    const int height = placed.menuBounds.bottom - y;

    LogDebug(L"popup at %d,%d size %dx%d (caret %ld,%ld-%ld,%ld, dpi %u)", x, y, width, height, caret.left, caret.top, caret.right, caret.bottom, dpi_);
    SetWindowPos(hwnd_, HWND_TOPMOST, x, y, width, height, SWP_NOACTIVATE | SWP_SHOWWINDOW);
    InvalidateRect(hwnd_, nullptr, FALSE);
    shown_ = true;
}

void Popup::SetSelection(int selection) {
    if (!shown_) return;
    if (selection == selection_) return;
    selection_ = selection;
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void Popup::Hide() {
    if (hwnd_ && shown_) ShowWindow(hwnd_, SW_HIDE);
    shown_ = false;
}

void Popup::Paint() {
    PAINTSTRUCT paint;
    HDC screen = BeginPaint(hwnd_, &paint);
    RECT client;
    GetClientRect(hwnd_, &client);
    int width = client.right - client.left;
    int height = client.bottom - client.top;

    HDC dc = CreateCompatibleDC(screen);
    HBITMAP bitmap = CreateCompatibleBitmap(screen, width, height);
    HGDIOBJ oldBitmap = SelectObject(dc, bitmap);
    HGDIOBJ oldFont = SelectObject(dc, font_ ? font_ : GetStockObject(DEFAULT_GUI_FONT));
    SetBkMode(dc, TRANSPARENT);

    HIGHCONTRASTW contrast{sizeof(contrast)};
    const bool highContrast = SystemParametersInfoW(SPI_GETHIGHCONTRAST, sizeof(contrast), &contrast, 0) &&
                              (contrast.dwFlags & HCF_HIGHCONTRASTON) != 0;
    DWORD useLightTheme = 1;
    DWORD valueBytes = sizeof(useLightTheme);
    RegGetValueW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                 L"AppsUseLightTheme", RRF_RT_REG_DWORD, nullptr, &useLightTheme, &valueBytes);
    const bool dark = useLightTheme == 0;
    const auto& theme = dark ? renderer::palette::kDark : renderer::palette::kLight;
    const COLORREF backgroundColor = highContrast ? GetSysColor(COLOR_WINDOW) : Color(theme.surface);
    const COLORREF borderColor = highContrast ? GetSysColor(COLOR_WINDOWTEXT) : Color(theme.line);
    const COLORREF textColor = highContrast ? GetSysColor(COLOR_WINDOWTEXT) : Color(theme.ink);
    const COLORREF mutedColor = highContrast ? GetSysColor(COLOR_GRAYTEXT) : Color(theme.muted);
    const COLORREF typedColor = highContrast ? textColor : Color(theme.sign);
    const COLORREF ghostColor = highContrast ? mutedColor : Color(theme.ghost);
    const COLORREF selectedColor = highContrast ? GetSysColor(COLOR_HIGHLIGHT) : Color(theme.accent);
    const COLORREF selectedText = highContrast ? GetSysColor(COLOR_HIGHLIGHTTEXT) : Color(theme.onAccent);
    const COLORREF guessedColor = highContrast ? textColor : Color(theme.warn);

    HBRUSH border = CreateSolidBrush(borderColor);
    FillRect(dc, &client, border);
    DeleteObject(border);
    RECT inner = {1, 1, width - 1, height - 1};
    HBRUSH background = CreateSolidBrush(backgroundColor);
    FillRect(dc, &inner, background);
    DeleteObject(background);

    TEXTMETRICW metrics = {};
    GetTextMetricsW(dc, &metrics);
    const int pad = Scale(15, dpi_);
    const bool hasStatus = HasStatus(content_.ai);
    const int selectedCorrection = SelectedCorrectionIndex(content_, selection_);
    const bool hasCorrection = selectedCorrection >= 0;
    const float scale = static_cast<float>(dpi_) / 96.0f;
    layout::ContentMetrics measured{};
    measured.fontSizeDip = static_cast<float>(settings_.font_size) * 96.0f / 72.0f;
    measured.phraseHeightDip = static_cast<float>(metrics.tmHeight + Scale(12, dpi_)) / scale;
    measured.hasAuxiliaryShelf = hasCorrection;
    const int statusLines = (hasStatus ? 1 : 0) + (hasCorrection ? 1 : 0);
    measured.statusHeightDip = statusLines ? static_cast<float>((metrics.tmHeight + Scale(4, dpi_)) * statusLines) / scale : 0.0f;
    render::Snapshot snapshot{};
    snapshot.settings = settings_;
    snapshot.ai = content_.ai;
    snapshot.phrase = content_.phrase;
    snapshot.caret = {};
    snapshot.selection = selection_;
    for (std::size_t i = 0; i < content_.words.size(); ++i) {
        snapshot.words.push_back({content_.words[i], i < content_.origins.size() ? content_.origins[i] : "", {}});
        measured.rows.push_back({0, measured.phraseHeightDip});
    }
    layout::WorkArea work{{0, 0, width, height}, dpi_};
    auto placed = layout::Place(snapshot, work, measured);

    if (hasStatus || hasCorrection) {
        const RECT statusBounds = RectPx(placed.statusClip, dpi_);
        RECT status = statusBounds;
        const std::wstring statusText = AiStatus(content_);
        SetTextColor(dc, mutedColor);
        const int line = metrics.tmHeight + Scale(4, dpi_);
        if (hasStatus) {
            status.bottom = std::min(status.bottom, status.top + line);
            DrawTextW(dc, statusText.c_str(), static_cast<int>(statusText.size()), &status,
                      DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
        }
        if (hasCorrection) {
            std::wstring correction = content_.phraseLead + L" → " + content_.words[selectedCorrection];
            RECT compare = statusBounds;
            compare.top = statusBounds.top + (hasStatus ? line : 0);
            compare.bottom = std::min(statusBounds.bottom, compare.top + line);
            SetTextColor(dc, typedColor);
            DrawTextW(dc, correction.c_str(), static_cast<int>(correction.size()), &compare,
                      DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
        }
    }

    for (const auto& rowLayout : placed.rowOrder) {
        RECT row = RectPx(rowLayout.bounds, dpi_);
        row.left += 1; row.right += 1; row.top += 1; row.bottom += 1;
        const bool isPhrase = rowLayout.kind == layout::RowKind::Phrase;
        const int wordIndex = static_cast<int>(rowLayout.wordIndex);
        const bool selected = isPhrase ? selection_ == -1 : selection_ == wordIndex;
        if (selected) {
            HBRUSH fill = CreateSolidBrush(selectedColor);
            FillRect(dc, &row, fill);
            DeleteObject(fill);
        }
        int y = row.top + (row.bottom - row.top - metrics.tmHeight) / 2;
        int x = row.left + pad;
        const int textState = SaveDC(dc);
        IntersectClipRect(dc, x, row.top, row.right - pad, row.bottom);

        if (isPhrase) {
            // The typed part of the word, then the phrase as ghost text.
            SIZE size = {};
            const std::wstring& lead = content_.phraseLead;
            SetTextColor(dc, selected ? selectedText : typedColor);
            TextOutW(dc, x, y, lead.c_str(), static_cast<int>(lead.size()));
            GetTextExtentPoint32W(dc, lead.c_str(), static_cast<int>(lead.size()), &size);
            RECT text = {x + size.cx, y, row.right - pad, y + metrics.tmHeight};
            SetTextColor(dc, selected ? selectedText : ghostColor);
            DrawTextW(dc, content_.phrase.c_str(), -1, &text, DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
            std::size_t underlineEnd = 0;
            while (underlineEnd < content_.phrase.size() && iswspace(content_.phrase[underlineEnd])) ++underlineEnd;
            while (underlineEnd < content_.phrase.size() && !iswspace(content_.phrase[underlineEnd])) ++underlineEnd;
            if (underlineEnd > 0) {
                SIZE advance{};
                GetTextExtentPoint32W(dc, content_.phrase.c_str(), static_cast<int>(underlineEnd), &advance);
                int leadWidth = 0;
                GetTextExtentPoint32W(dc, lead.c_str(), static_cast<int>(lead.size()), &size);
                leadWidth = static_cast<int>(size.cx);
                const int underlineLeft = x + leadWidth;
                const int underlineRight = std::min(row.right - pad, underlineLeft + advance.cx);
                if (underlineRight > underlineLeft) {
                    HPEN pen = CreatePen(PS_SOLID, std::max(1, Scale(1, dpi_)), selected ? selectedText : typedColor);
                    HGDIOBJ oldPen = SelectObject(dc, pen);
                    MoveToEx(dc, underlineLeft, y + metrics.tmHeight - 1, nullptr);
                    LineTo(dc, underlineRight, y + metrics.tmHeight - 1);
                    SelectObject(dc, oldPen);
                    DeleteObject(pen);
                }
            }
            RestoreDC(dc, textState);
            continue;
        }

        const std::wstring& word = content_.words[wordIndex];
        int typed = std::min(static_cast<int>(word.size()), content_.typedChars);
        const std::vector<int>& marked =
            wordIndex < static_cast<int>(content_.marks.size()) ? content_.marks[wordIndex] : kNoMarks;
        // Typed letters in blue, guessed letters in amber, the rest in plain text. Each run is
        // drawn separately so the colours meet exactly at the letter boundaries.
        int cursor = 0;
        auto flush = [&](int end, COLORREF normal, COLORREF selectedColour) {
            if (end <= cursor) return;
            SetTextColor(dc, selected ? selectedColour : normal);
            TextOutW(dc, x, y, word.c_str() + cursor, end - cursor);
            SIZE advance = {};
            GetTextExtentPoint32W(dc, word.c_str() + cursor, end - cursor, &advance);
            x += advance.cx;
            cursor = end;
        };
        std::size_t m = 0;
        while (m < marked.size() && marked[m] < typed) {
            int pos = marked[m];
            if (pos < cursor) {
                ++m;
                continue;
            }
            flush(pos, typedColor, selectedText);
            flush(pos + 1, guessedColor, selectedText);
            ++m;
        }
        flush(typed, typedColor, selectedText);
        while (m < marked.size()) {
            int pos = marked[m];
            if (pos < cursor || pos >= static_cast<int>(word.size())) {
                ++m;
                continue;
            }
            flush(pos, textColor, selectedText);
            flush(pos + 1, guessedColor, selectedText);
            ++m;
        }
        flush(static_cast<int>(word.size()), textColor, selectedText);
        if (wordIndex < static_cast<int>(content_.origins.size())) {
            const std::string& origin = content_.origins[wordIndex];
            const wchar_t* label = origin == "local" ? L"  Local" : origin == "learned" ? L"  Learned" : nullptr;
            if (label) {
                SetTextColor(dc, selected ? selectedText : mutedColor);
                TextOutW(dc, x, y, label, static_cast<int>(wcslen(label)));
            }
        }
        RestoreDC(dc, textState);
    }

    // Local pipe health is independent of AI lifecycle and must remain visible for words-only menus.
    const int indicator = std::max(1, Scale(6, dpi_));
    const int indicatorRight = width - Scale(4, dpi_);
    const int indicatorLeft = indicatorRight - indicator;
    const int indicatorTop = std::max(1, (Scale(15, dpi_) - indicator) / 2);
    const COLORREF connectionColor = highContrast ? GetSysColor(COLOR_WINDOWTEXT) : Color(theme.engine);
    HBRUSH indicatorBrush = CreateSolidBrush(content_.engineConnected ? connectionColor : backgroundColor);
    HPEN indicatorPen = CreatePen(PS_SOLID, std::max(1, Scale(1, dpi_)), connectionColor);
    HGDIOBJ oldIndicatorBrush = SelectObject(dc, indicatorBrush);
    HGDIOBJ oldIndicatorPen = SelectObject(dc, indicatorPen);
    Ellipse(dc, indicatorLeft, indicatorTop, indicatorRight, indicatorTop + indicator);
    SelectObject(dc, oldIndicatorBrush);
    SelectObject(dc, oldIndicatorPen);
    DeleteObject(indicatorBrush);
    DeleteObject(indicatorPen);

    BitBlt(screen, 0, 0, width, height, dc, 0, 0, SRCCOPY);
    SelectObject(dc, oldFont);
    SelectObject(dc, oldBitmap);
    DeleteObject(bitmap);
    DeleteDC(dc);
    EndPaint(hwnd_, &paint);
}

LRESULT CALLBACK Popup::WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    auto* popup = reinterpret_cast<Popup*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    switch (message) {
        case WM_MOUSEACTIVATE:
            return MA_NOACTIVATE;
        case WM_NCHITTEST:
            return HTTRANSPARENT;  // clicks fall through to whatever is underneath
        case WM_ERASEBKGND:
            return 1;
        case WM_SETTINGCHANGE:
        case WM_THEMECHANGED:
            if (popup) InvalidateRect(hwnd, nullptr, FALSE);
            break;
        case WM_PAINT:
            if (popup) popup->Paint();
            else ValidateRect(hwnd, nullptr);
            return 0;
        default:
            if (popup && popup->hook_ && popup->hook_(popup->context_, message, wParam, lParam)) return 0;
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

}  // namespace completionist
