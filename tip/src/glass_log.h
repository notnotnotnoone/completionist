// Always-on log of the glass popup handoff, shared by the TSF DLL and the renderer.
// Every line goes to %LOCALAPPDATA%\Completionist\glass.log; if that fails, to
// glass-<pid>.log beside it; and always to OutputDebugString. No typed text is logged.
#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <array>
#include <cstdarg>
#include <cstdio>
#include <cwchar>

namespace completionist {

namespace glass_detail {

struct RestoreLastError {
    DWORD value = GetLastError();
    ~RestoreLastError() { SetLastError(value); }
};

inline bool AppendUtf8(const wchar_t* path, const char* bytes, int length) {
    HANDLE file = CreateFileW(path, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                              nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;
    DWORD written = 0;
    const BOOL writeOk = WriteFile(file, bytes, static_cast<DWORD>(length), &written, nullptr);
    const DWORD writeError = writeOk ? ERROR_WRITE_FAULT : GetLastError();
    const BOOL closeOk = CloseHandle(file);
    if (!writeOk || written != static_cast<DWORD>(length)) {
        SetLastError(writeError);
        return false;
    }
    return closeOk != FALSE;
}

inline const wchar_t* ExeName() {
    static const auto name = [] {
        std::array<wchar_t, MAX_PATH> result{};
        wchar_t path[MAX_PATH] = {};
        GetModuleFileNameW(nullptr, path, MAX_PATH);
        const wchar_t* slash = wcsrchr(path, L'\\');
        wcsncpy_s(result.data(), result.size(), slash ? slash + 1 : path, _TRUNCATE);
        for (wchar_t& c : result) if (c == L' ') c = L'?';
        return result;
    }();
    return name.data();
}

}  // namespace glass_detail

inline void GlassLog(const wchar_t* format, ...) {
    glass_detail::RestoreLastError restoreLastError;
    wchar_t message[1200];
    va_list args;
    va_start(args, format);
    vswprintf_s(message, format, args);
    va_end(args);
    SYSTEMTIME t;
    GetLocalTime(&t);
    wchar_t line[1500];
    swprintf_s(line, L"%02u:%02u:%02u.%03u glass %s[%lu] %s\r\n", t.wHour, t.wMinute, t.wSecond, t.wMilliseconds,
               glass_detail::ExeName(), GetCurrentProcessId(), message);
    OutputDebugStringW(line);
    char utf8[4500];
    const int bytes = WideCharToMultiByte(CP_UTF8, 0, line, -1, utf8, sizeof(utf8), nullptr, nullptr);
    if (bytes <= 1) return;
    wchar_t dir[MAX_PATH];
    const DWORD length = GetEnvironmentVariableW(L"LOCALAPPDATA", dir, MAX_PATH);
    if (!length || length >= MAX_PATH - 64) return;
    wcscat_s(dir, L"\\Completionist");
    CreateDirectoryW(dir, nullptr);
    wchar_t path[MAX_PATH];
    swprintf_s(path, L"%s\\glass.log", dir);
    if (glass_detail::AppendUtf8(path, utf8, bytes - 1)) return;
    const DWORD error = GetLastError();
    swprintf_s(path, L"%s\\glass-%lu.log", dir, GetCurrentProcessId());
    static volatile LONG reported = 0;
    if (InterlockedExchange(&reported, 1) == 0) {
        char note[200];
        const int n = sprintf_s(note, "glass.log could not be appended (error %lu); writing here instead\r\n", error);
        wchar_t debugNote[200];
        swprintf_s(debugNote, L"glass.log could not be appended (error %lu); trying glass-%lu.log\r\n", error, GetCurrentProcessId());
        OutputDebugStringW(debugNote);
        glass_detail::AppendUtf8(path, note, n);
    }
    glass_detail::AppendUtf8(path, utf8, bytes - 1);
}

inline void GlassBanner(const wchar_t* component, const wchar_t* build, HMODULE module) {
    glass_detail::RestoreLastError restoreLastError;
    wchar_t path[MAX_PATH] = {};
    GetModuleFileNameW(module, path, MAX_PATH);
    wchar_t stamp[64] = {};
    wcsncpy_s(stamp, build, _TRUNCATE);
    for (wchar_t* c = path; *c; ++c) if (*c == L' ') *c = L'?';
    for (wchar_t* c = stamp; *c; ++c) if (*c == L' ') *c = L'_';
    GlassLog(L"step=loaded component=%s build=%s path=%s", component, stamp, path);
}

}  // namespace completionist

#define COMPLETIONIST_GLASS_BUILD (L"" __DATE__ L"_" __TIME__)
