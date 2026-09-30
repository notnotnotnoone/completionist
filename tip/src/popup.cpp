#include "popup.h"

#include <algorithm>

namespace typer {

namespace {

constexpr wchar_t kClassName[] = L"TyperPopup";

constexpr COLORREF kBackground = RGB(31, 34, 42);
constexpr COLORREF kBorder = RGB(66, 71, 84);
constexpr COLORREF kText = RGB(226, 229, 236);
constexpr COLORREF kTyped = RGB(122, 162, 255);
constexpr COLORREF kHighlight = RGB(38, 79, 176);
constexpr COLORREF kHighlightText = RGB(255, 255, 255);
constexpr COLORREF kHighlightTyped = RGB(190, 214, 255);

int Scale(int value, UINT dpi) { return MulDiv(value, static_cast<int>(dpi), 96); }

UINT DpiOf(HWND hwnd) {
    UINT dpi = GetDpiForWindow(hwnd);
    return dpi ? dpi : 96;
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
    if (!hwnd_) return false;
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

void Popup::Show(const std::vector<std::wstring>& words, std::size_t highlight, int typedChars, const RECT& caret) {
    if (!hwnd_ || words.empty()) {
        Hide();
        return;
    }
    words_ = words;
    highlight_ = std::min(highlight, words_.size() - 1);
    typedChars_ = typedChars;

    // Move first (hidden) so the DPI is that of the monitor the popup will appear on.
    SetWindowPos(hwnd_, HWND_TOPMOST, caret.left, caret.bottom, 0, 0, SWP_NOSIZE | SWP_NOACTIVATE);
    dpi_ = DpiOf(hwnd_);
    EnsureFont(dpi_);

    HDC dc = GetDC(hwnd_);
    HGDIOBJ old = SelectObject(dc, font_);
    TEXTMETRICW metrics = {};
    GetTextMetricsW(dc, &metrics);
    int widest = 0;
    for (const std::wstring& word : words_) {
        SIZE size = {};
        GetTextExtentPoint32W(dc, word.c_str(), static_cast<int>(word.size()), &size);
        widest = std::max(widest, static_cast<int>(size.cx));
    }
    SelectObject(dc, old);
    ReleaseDC(hwnd_, dc);

    int rowHeight = metrics.tmHeight + Scale(8, dpi_);
    int width = std::max(widest + Scale(24, dpi_), Scale(120, dpi_));
    int height = rowHeight * static_cast<int>(words_.size()) + 2;  // +2 for the border

    RECT work = {0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN)};
    MONITORINFO monitor = {sizeof(monitor)};
    if (GetMonitorInfoW(MonitorFromRect(&caret, MONITOR_DEFAULTTONEAREST), &monitor)) work = monitor.rcWork;

    int x = caret.left;
    int y = caret.bottom + Scale(2, dpi_);
    if (y + height > work.bottom) y = caret.top - height - Scale(2, dpi_);  // no room below: open above
    x = std::min<int>(x, work.right - width);
    x = std::max<int>(x, work.left);
    y = std::max<int>(y, work.top);

    SetWindowPos(hwnd_, HWND_TOPMOST, x, y, width, height, SWP_NOACTIVATE | SWP_SHOWWINDOW);
    InvalidateRect(hwnd_, nullptr, FALSE);
    shown_ = true;
}

void Popup::SetHighlight(std::size_t highlight) {
    if (!shown_ || words_.empty()) return;
    highlight_ = std::min(highlight, words_.size() - 1);
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
    for (std::size_t i = 0; i < words_.size(); ++i) {
        RECT row = {1, 1 + static_cast<int>(i) * rowHeight, width - 1, 1 + static_cast<int>(i + 1) * rowHeight};
        bool selected = i == highlight_;
        if (selected) {
            HBRUSH fill = CreateSolidBrush(kHighlight);
            FillRect(dc, &row, fill);
            DeleteObject(fill);
        }
        const std::wstring& word = words_[i];
        int typed = std::min(static_cast<int>(word.size()), typedChars_);
        int y = row.top + (rowHeight - metrics.tmHeight) / 2;
        int x = row.left + pad;

        SIZE size = {};
        SetTextColor(dc, selected ? kHighlightTyped : kTyped);
        TextOutW(dc, x, y, word.c_str(), typed);
        GetTextExtentPoint32W(dc, word.c_str(), typed, &size);
        SetTextColor(dc, selected ? kHighlightText : kText);
        TextOutW(dc, x + size.cx, y, word.c_str() + typed, static_cast<int>(word.size()) - typed);
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

}  // namespace typer
