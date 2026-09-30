// The suggestion list drawn next to the caret: an optional phrase row on top, then word rows. It's a
// topmost, click-through window that never takes focus, drawn in the host app's process (so it inherits
// that app's DPI awareness and stacks correctly).
#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <string>
#include <vector>

namespace completionist {

struct PopupContent {
    std::vector<std::wstring> words;
    int typedChars = 0;        // the first typedChars characters of each word are drawn as already typed
    std::wstring phrase;       // continuation shown as the top row (empty: no phrase row)
    std::wstring phraseLead;   // what's already typed of the current word, drawn before the phrase
};

class Popup {
public:
    // Called for messages the popup doesn't handle itself (used to receive engine replies and timers).
    using MessageHook = bool (*)(void* context, UINT message, WPARAM wParam, LPARAM lParam);

    ~Popup() { Destroy(); }

    bool Create(HINSTANCE module, MessageHook hook, void* context);
    void Destroy();
    HWND hwnd() const { return hwnd_; }
    bool shown() const { return shown_; }

    // Shows `content` under `caret` (screen coordinates), with `selection` highlighted: -1 for the
    // phrase row, otherwise a word index. Flips above the caret or slides left to stay on its monitor.
    void Show(const PopupContent& content, int selection, const RECT& caret);
    void SetSelection(int selection);
    void Hide();

private:
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    void Paint();
    void EnsureFont(UINT dpi);
    int RowHeight(HDC dc) const;

    HWND hwnd_ = nullptr;
    HINSTANCE module_ = nullptr;
    MessageHook hook_ = nullptr;
    void* context_ = nullptr;
    PopupContent content_;
    int selection_ = 0;
    HFONT font_ = nullptr;
    UINT fontDpi_ = 0;
    UINT dpi_ = 96;
    bool shown_ = false;
};

}  // namespace completionist
