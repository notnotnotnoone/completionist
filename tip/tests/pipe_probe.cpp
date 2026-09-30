// Drives the real EngineClient against a real engine, without TSF. Used by e2e.ps1.
//
//   pipe_probe.exe <pipe-name> <seconds> [before-text]
//
// Sends a keystroke request every 100 ms for <seconds> and prints one line per attempt:
//   t=<ms> connected=<0|1> reply=<words joined by ,>|none
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <cstdio>
#include <memory>
#include <string>

#include "../src/engine_client.h"

namespace {
LRESULT CALLBACK Proc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == typer::WM_TYPER_REPLY) {
        std::unique_ptr<typer::protocol::WordReply> reply(reinterpret_cast<typer::protocol::WordReply*>(lParam));
        std::string words;
        for (auto& w : reply->words) words += (words.empty() ? "" : ",") + typer::protocol::ToUtf8(w);
        std::printf("  reply id=%u replace=%d words=%s\n", reply->id, reply->replace, words.c_str());
        std::fflush(stdout);
        return 0;
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}
}  // namespace

int wmain(int argc, wchar_t** argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: pipe_probe <pipe-name> <seconds> [before-text]\n");
        return 2;
    }
    std::wstring before = argc > 3 ? argv[3] : L"I would recomm";
    int seconds = _wtoi(argv[2]);

    WNDCLASSW wc = {};
    wc.lpfnWndProc = Proc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"TyperProbe";
    RegisterClassW(&wc);
    HWND window = CreateWindowExW(0, L"TyperProbe", L"", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, wc.hInstance, nullptr);

    auto& client = typer::EngineClient::Instance();
    client.SetPipeName(argv[1]);
    client.Acquire();

    ULONGLONG start = GetTickCount64();
    ULONGLONG nextSend = start;
    while (GetTickCount64() - start < static_cast<ULONGLONG>(seconds) * 1000) {
        MSG msg;
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) DispatchMessageW(&msg);
        if (GetTickCount64() >= nextSend) {
            typer::protocol::Request request;
            request.id = client.NextId();
            request.event = "keystroke";
            request.app = L"probe.exe";
            request.before = before;
            std::printf("t=%llu connected=%d id=%u\n", GetTickCount64() - start, client.connected() ? 1 : 0, request.id);
            std::fflush(stdout);
            client.Send(std::move(request), window);
            nextSend += 100;
        }
        MsgWaitForMultipleObjects(0, nullptr, FALSE, 10, QS_ALLINPUT);
    }
    client.Release();
    return 0;
}
