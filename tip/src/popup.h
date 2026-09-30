// The word list drawn next to the caret. It's a topmost, click-through window that never takes focus,
// drawn in the host app's process (so it inherits that app's DPI awareness and stacks correctly).
#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <string>
#include <vector>

namespace typer {

class Popup {
public:
    // Called for messages the popup doesn't handle itself (used to receive engine replies).
    using MessageHook = bool (*)(void* context, UINT message, WPARAM wParam, LPARAM lParam);

    ~Popup() { Destroy(); }

    bool Create(HINSTANCE module, MessageHook hook, void* context);
    void Destroy();
    HWND hwnd() const { return hwnd_; }
    bool shown() const { return shown_; }

    // Shows `words` under `caret` (screen coordinates). The first `typedChars` characters of each word
    // are drawn as already typed. Flips above the caret or slides left to stay on the caret's monitor.
    void Show(const std::vector<std::wstring>& words, std::size_t highlight, int typedChars, const RECT& caret);
    void SetHighlight(std::size_t highlight);
    void Hide();

private:
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    void Paint();
    void EnsureFont(UINT dpi);

    HWND hwnd_ = nullptr;
    HINSTANCE module_ = nullptr;
    MessageHook hook_ = nullptr;
    void* context_ = nullptr;
    std::vector<std::wstring> words_;
    std::size_t highlight_ = 0;
    int typedChars_ = 0;
    HFONT font_ = nullptr;
    UINT fontDpi_ = 0;
    UINT dpi_ = 96;
    bool shown_ = false;
};

}  // namespace typer
