// Resilience scenarios for tsf_harness.cpp (included there): the engine dies while the app is in use, then
// comes back. tsf_e2e.ps1 plays the engine's part through flag files in a folder:
//   harness writes "kill"      -> script stops the engine, writes "killed"
//   harness writes "restart"   -> script starts the engine, writes "restarted"
#pragma once

static bool FlagExists(const std::wstring& path) { return GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES; }

static void RaiseFlag(const std::wstring& path) {
    HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file != INVALID_HANDLE_VALUE) CloseHandle(file);
}

static bool WaitForFlag(const std::wstring& path, DWORD timeoutMs) {
    ULONGLONG end = GetTickCount64() + timeoutMs;
    while (GetTickCount64() < end) {
        Pump(20);
        if (FlagExists(path)) return true;
    }
    return false;
}

static void ResilienceScenarios(auto& store, auto& keys, const std::wstring& flags) {
    std::printf("scenario R1: the engine is killed mid-use\n");
    bool up = false;
    for (int attempt = 0; attempt < 4 && !up; ++attempt) {  // the first keys can beat the pipe connection
        ResetText(store);
        store.Type(L"the recomm");
        up = WaitForPopup(true, 1500);
    }
    Check(up, "popup shows while the engine is up");
    RaiseFlag(flags + L"\\kill");
    Check(WaitForFlag(flags + L"\\killed", 30000), "the engine was stopped");
    Pump(300);

    ResetText(store);
    ULONGLONG start = GetTickCount64();
    store.Type(L"typing with no engine wor");
    ULONGLONG typingMs = GetTickCount64() - start;
    Check(typingMs < 1500, "typing with the engine gone doesn't hang the app");
    std::printf("    typed 25 characters in %llu ms\n", typingMs);
    Pump(600);
    Check(!PopupVisible(), "no popup while the engine is gone");
    bool testEaten = false;
    bool eaten = keys.Press(VK_TAB, &testEaten);
    Check(!eaten && !testEaten, "Tab passes through to the app while the engine is gone");
    Check(!keys.Press(VK_DOWN), "Down passes through too");

    std::printf("scenario R2: the engine comes back\n");
    RaiseFlag(flags + L"\\restart");
    Check(WaitForFlag(flags + L"\\restarted", 60000), "the engine was started again");
    // The DLL reconnects with a backoff of up to 2 s, and keys typed before that are simply not answered,
    // so keep typing the way a person would.
    start = GetTickCount64();
    bool back = false;
    for (int attempt = 0; attempt < 8 && !back; ++attempt) {
        ResetText(store);
        store.Type(L"the recomm");
        back = WaitForPopup(true, 1200);
    }
    Check(back, "the popup comes back after the restart, with no app restart");
    std::printf("    popup back %llu ms after typing\n", GetTickCount64() - start);
    Check(keys.Press(VK_TAB), "Tab accepts again");
    Pump(200);
    Check(store.text != L"the recomm" && store.text.rfind(L"the recomm", 0) == 0, "a word was inserted");
}
