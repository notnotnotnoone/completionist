// Phrase scenarios for tsf_harness.cpp (included there). The engine is pointed at a fake provider that
// always streams "ld", " is", " big", so after "Hello wor" the phrase completes to "Hello world is big".
#pragma once

static int PopupHeight() {
    HWND popup = PopupWindow();
    RECT rc = {};
    if (!popup || !IsWindowVisible(popup) || !GetWindowRect(popup, &rc)) return 0;
    return rc.bottom - rc.top;
}

// Waits until the popup is at least `taller` pixels taller than `baseline` (the phrase row appeared).
static bool WaitForPhraseRow(int baseline, DWORD timeoutMs = 4000) {
    ULONGLONG end = GetTickCount64() + timeoutMs;
    while (GetTickCount64() < end) {
        Pump(5);
        if (PopupHeight() > baseline + 10) return true;
    }
    return false;
}

static void ResetText(auto& store) {
    if (g_refocus) g_refocus();
    store.SetTextDirectly(L"", 0);
    Pump(150);
}

static void HotkeyPhraseScenarios(auto& store, auto& keys, const std::wstring& shots) {
    std::printf("scenario H1: Ctrl+Space asks for a phrase, Tab takes all of it\n");
    ResetText(store);
    store.Type(L"Hello wor");
    Check(WaitForPopup(true), "words popup appears");
    Pump(400);
    int words_only = PopupHeight();
    Check(WaitForPopup(true) && words_only > 0, "popup measured");
    Pump(800);
    Check(PopupHeight() == words_only, "no phrase without asking, in hotkey mode");
    bool eaten = false;
    Check(keys.Press(VK_SPACE, &eaten, true) && eaten, "Ctrl+Space is consumed");
    Check(WaitForPhraseRow(words_only), "a phrase row appears above the words");
    Pump(300);
    SaveScreenshot(shots, L"4-phrase");
    Check(keys.Press(VK_TAB), "Tab is consumed");
    Pump(300);
    Check(store.text == L"Hello world is big", "Tab inserted the whole phrase");
    std::printf("    text: %ls\n", store.text.c_str());

    std::printf("scenario H2: Ctrl+Right takes the phrase one word at a time\n");
    ResetText(store);
    store.Type(L"Hello wor");
    WaitForPopup(true);
    Pump(300);
    int base = PopupHeight();
    keys.Press(VK_SPACE, nullptr, true);
    Check(WaitForPhraseRow(base), "phrase appears");
    Pump(300);
    Check(keys.Press(VK_RIGHT, nullptr, true), "Ctrl+Right is consumed");
    Pump(400);
    Check(store.text == L"Hello world", "first word inserted");
    Check(PopupVisible(), "popup stays open with the rest of the phrase");
    Check(keys.Press(VK_RIGHT, nullptr, true), "Ctrl+Right again");
    Pump(400);
    Check(store.text == L"Hello world is", "second word inserted");
    Check(keys.Press(VK_TAB), "Tab takes what is left");
    Pump(300);
    Check(store.text == L"Hello world is big", "the rest was inserted");

    std::printf("scenario H3: a Tab just as the phrase arrives takes the word, not the phrase\n");
    ResetText(store);
    store.Type(L"Hello wor");
    WaitForPopup(true);
    Pump(300);
    base = PopupHeight();
    keys.Press(VK_SPACE, nullptr, true);
    ULONGLONG deadline = GetTickCount64() + 4000;
    while (PopupHeight() <= base + 10 && GetTickCount64() < deadline) Pump(2);
    keys.Press(VK_TAB);  // within a few ms of the phrase row appearing
    Pump(300);
    Check(store.text.rfind(L"Hello wor", 0) == 0 && store.text.find(L"is big") == std::wstring::npos,
          "the word was taken, not the phrase");
    std::printf("    text: %ls\n", store.text.c_str());

    std::printf("scenario H4: Esc closes the phrase; typing on brings nothing back for the word\n");
    ResetText(store);
    store.Type(L"Hello wor");
    WaitForPopup(true);
    Pump(300);
    base = PopupHeight();
    keys.Press(VK_SPACE, nullptr, true);
    WaitForPhraseRow(base);
    Check(keys.Press(VK_ESCAPE), "Esc is consumed");
    Check(!PopupVisible(), "everything closed");
    store.Type(L"l");
    Pump(600);
    Check(!PopupVisible(), "stays closed while typing the same word");

    std::printf("scenario H5: Ctrl+Space right after a space (no word yet) shows just the phrase\n");
    ResetText(store);
    store.Type(L"Hello ");
    Pump(500);
    Check(!PopupVisible(), "nothing shown after a space by itself");
    Check(keys.Press(VK_SPACE, nullptr, true), "Ctrl+Space is consumed");
    Check(WaitForPopup(true, 4000), "a phrase-only popup appears");
    Pump(300);
    Check(keys.Press(VK_TAB), "Tab is consumed");
    Pump(300);
    Check(store.text == L"Hello ld is big", "the phrase was inserted at the caret");
}

static void AutoPhraseScenarios(auto& store, auto& keys, const std::wstring& shots) {
    std::printf("scenario A1: in an allow-listed app a phrase appears after a pause, with no key pressed\n");
    ResetText(store);
    store.Type(L"Hello wor");
    Check(WaitForPopup(true), "words popup appears");
    int words_only = PopupHeight();
    Check(WaitForPhraseRow(words_only), "the phrase row arrives on its own");
    Pump(300);
    SaveScreenshot(shots, L"5-auto-phrase");
    Check(keys.Press(VK_TAB), "Tab is consumed");
    Pump(300);
    Check(store.text == L"Hello world is big", "Tab took the phrase (it is highlighted by default)");

    std::printf("scenario A2: typing along with the phrase trims it instead of asking again\n");
    ResetText(store);
    store.Type(L"Hello wor");
    WaitForPopup(true);
    int base = PopupHeight();
    Check(WaitForPhraseRow(base), "phrase arrives");
    Pump(300);
    store.Type(L"ld");
    Pump(600);
    Check(PopupVisible(), "popup still there after typing part of the phrase");
    Check(keys.Press(VK_TAB), "Tab is consumed");
    Pump(300);
    Check(store.text == L"Hello world is big", "the remaining phrase was inserted");

    std::printf("scenario A3: typing something else drops the phrase at once\n");
    ResetText(store);
    store.Type(L"Hello wor");
    WaitForPopup(true);
    base = PopupHeight();
    WaitForPhraseRow(base);
    Pump(300);
    store.Type(L"k");
    Pump(40);  // before the 100 ms pause is over, so no new phrase can have arrived (the fake gives the same one)
    Check(PopupHeight() <= base + 10, "the phrase row is gone");
    keys.Press(VK_TAB);
    Pump(300);
    Check(store.text.find(L"is big") == std::wstring::npos, "Tab did not insert the old phrase");
}
