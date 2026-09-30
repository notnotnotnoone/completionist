#include "log.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <strsafe.h>

#include <cstdarg>

namespace completionist {

namespace {

volatile LONG g_verbose = 0;

bool DataPath(const wchar_t* file, wchar_t (&path)[MAX_PATH], bool createDirectory) {
    DWORD len = GetEnvironmentVariableW(L"LOCALAPPDATA", path, MAX_PATH);
    if (len == 0 || len >= MAX_PATH) return false;
    if (FAILED(StringCchCatW(path, MAX_PATH, L"\\Completionist"))) return false;
    if (createDirectory) CreateDirectoryW(path, nullptr);
    return SUCCEEDED(StringCchCatW(path, MAX_PATH, L"\\")) && SUCCEEDED(StringCchCatW(path, MAX_PATH, file));
}

void Write(const wchar_t* level, const wchar_t* format, va_list args) {
    wchar_t message[1500];
    StringCchVPrintfW(message, ARRAYSIZE(message), format, args);

    wchar_t exe[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, exe, MAX_PATH);
    const wchar_t* name = wcsrchr(exe, L'\\');
    name = name ? name + 1 : exe;

    SYSTEMTIME t;
    GetLocalTime(&t);
    wchar_t line[1800];
    StringCchPrintfW(line, ARRAYSIZE(line), L"%02d:%02d:%02d.%03d %s %s[%lu] %s\r\n", t.wHour, t.wMinute, t.wSecond,
                     t.wMilliseconds, level, name, GetCurrentProcessId(), message);

    char utf8[5000];
    int bytes = WideCharToMultiByte(CP_UTF8, 0, line, -1, utf8, sizeof(utf8), nullptr, nullptr);
    if (bytes <= 1) return;

    wchar_t path[MAX_PATH];
    if (!DataPath(L"tip.log", path, true)) return;
    HANDLE file = CreateFileW(path, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                              OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return;
    DWORD written;
    WriteFile(file, utf8, static_cast<DWORD>(bytes - 1), &written, nullptr);
    CloseHandle(file);
}

}  // namespace

void RefreshLogLevel() {
    wchar_t path[MAX_PATH];
    bool verbose = DataPath(L"verbose", path, false) && GetFileAttributesW(path) != INVALID_FILE_ATTRIBUTES;
    InterlockedExchange(&g_verbose, verbose ? 1 : 0);
}

void LogError(const wchar_t* format, ...) {
    va_list args;
    va_start(args, format);
    Write(L"ERROR", format, args);
    va_end(args);
}

void LogDebug(const wchar_t* format, ...) {
    if (!g_verbose) return;
    va_list args;
    va_start(args, format);
    Write(L"debug", format, args);
    va_end(args);
}

}  // namespace completionist
