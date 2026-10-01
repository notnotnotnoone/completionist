#include "popup.h"

#include <algorithm>

#include "log.h"

namespace completionist {

namespace {

constexpr wchar_t kClassName[] = L"CompletionistPopup";

constexpr COLORREF kBackground = RGB(31, 34, 42);
constexpr COLORREF kBorder = RGB(66, 71, 84);
constexpr COLORREF kText = RGB(226, 229, 236);
constexpr COLORREF kTyped = RGB(122, 162, 255);
constexpr COLORREF kGhost = RGB(146, 154, 172);
constexpr COLORREF kHighlight = RGB(38, 79, 176);
constexpr COLORREF kHighlightText = RGB(255, 255, 255);
constexpr COLORREF kHighlightTyped = RGB(190, 214, 255);
constexpr COLORREF kHighlightGhost = RGB(214, 224, 244);
constexpr COLORREF kGuessed = RGB(255, 203, 107);  // amber: guessed letters of a typo correction
constexpr COLORREF kHighlightGuessed = RGB(255, 225, 160);  // amber on the highlighted row
constexpr int kMaxPhraseWidth = 520;  // at 96 DPI

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
    font_ = CreateFontW(-MulDiv(9, static_cast<int>(dpi), 72), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
                        L"Segoe UI");
    fontDpi_ = dpi;
}

int Popup::RowHeight(HDC dc) const {
    TEXTMETRICW metrics = {};
    GetTextMetricsW(dc, &metrics);
    return metrics.tmHeight + Scale(8, dpi_);
}

void Popup::Show(const PopupContent& content, int selection, const RECT& caret) {
    if (!hwnd_ || (content.words.empty() && content.phrase.empty())) {
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
    int rowHeight = RowHeight(dc);
    int widest = 0;
    for (const std::wstring& word : content_.words) {
        SIZE size = {};
        GetTextExtentPoint32W(dc, word.c_str(), static_cast<int>(word.size()), &size);
        widest = std::max(widest, static_cast<int>(size.cx));
    }
    if (!content_.phrase.empty()) {
        std::wstring row = content_.phraseLead + content_.phrase;
        SIZE size = {};
        GetTextExtentPoint32W(dc, row.c_str(), static_cast<int>(row.size()), &size);
        widest = std::max(widest, std::min(static_cast<int>(size.cx), Scale(kMaxPhraseWidth, dpi_)));
    }
    SelectObject(dc, old);
    ReleaseDC(hwnd_, dc);

    int rows = static_cast<int>(content_.words.size()) + (content_.phrase.empty() ? 0 : 1);
    int width = std::max(widest + Scale(24, dpi_), Scale(120, dpi_));
    int height = rowHeight * rows + 2;  // +2 for the border

    RECT work = {0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN)};
    MONITORINFO monitor = {sizeof(monitor)};
    if (GetMonitorInfoW(MonitorFromRect(&caret, MONITOR_DEFAULTTONEAREST), &monitor)) work = monitor.rcWork;

    int x = caret.left;
    int y = caret.bottom + Scale(2, dpi_);
    if (y + height > work.bottom) y = caret.top - height - Scale(2, dpi_);  // no room below: open above
    x = std::min<int>(x, work.right - width);
    x = std::max<int>(x, work.left);
    y = std::max<int>(y, work.top);

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

    HBRUSH border = CreateSolidBrush(kBorder);
    FillRect(dc, &client, border);
    DeleteObject(border);
    RECT inner = {1, 1, width - 1, height - 1};
    HBRUSH background = CreateSolidBrush(kBackground);
    FillRect(dc, &inner, background);
    DeleteObject(background);

    TEXTMETRICW metrics = {};
    GetTextMetricsW(dc, &metrics);
    int rowHeight = metrics.tmHeight + Scale(8, dpi_);
    int pad = Scale(10, dpi_);
    bool hasPhrase = !content_.phrase.empty();
    int rows = static_cast<int>(content_.words.size()) + (hasPhrase ? 1 : 0);

    for (int r = 0; r < rows; ++r) {
        RECT row = {1, 1 + r * rowHeight, width - 1, 1 + (r + 1) * rowHeight};
        bool isPhrase = hasPhrase && r == 0;
        int wordIndex = r - (hasPhrase ? 1 : 0);
        bool selected = isPhrase ? selection_ == -1 : selection_ == wordIndex;
        if (selected) {
            HBRUSH fill = CreateSolidBrush(kHighlight);
            FillRect(dc, &row, fill);
            DeleteObject(fill);
        }
        int y = row.top + (rowHeight - metrics.tmHeight) / 2;
        int x = row.left + pad;

        if (isPhrase) {
            // The typed part of the word, then the phrase as ghost text.
            SIZE size = {};
            const std::wstring& lead = content_.phraseLead;
            SetTextColor(dc, selected ? kHighlightTyped : kTyped);
            TextOutW(dc, x, y, lead.c_str(), static_cast<int>(lead.size()));
            GetTextExtentPoint32W(dc, lead.c_str(), static_cast<int>(lead.size()), &size);
            RECT text = {x + size.cx, y, row.right - pad, y + metrics.tmHeight};
            SetTextColor(dc, selected ? kHighlightGhost : kGhost);
            DrawTextW(dc, content_.phrase.c_str(), -1, &text, DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
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
            flush(pos, kTyped, kHighlightTyped);
            flush(pos + 1, kGuessed, kHighlightGuessed);
            ++m;
        }
        flush(typed, kTyped, kHighlightTyped);
        while (m < marked.size()) {
            int pos = marked[m];
            if (pos < cursor || pos >= static_cast<int>(word.size())) {
                ++m;
                continue;
            }
            flush(pos, kText, kHighlightText);
            flush(pos + 1, kGuessed, kHighlightGuessed);
            ++m;
        }
        flush(static_cast<int>(word.size()), kText, kHighlightText);
    }

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
