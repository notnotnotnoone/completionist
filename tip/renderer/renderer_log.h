// Appends one timestamped line to %LOCALAPPDATA%\Completionist\renderer.log.
// The renderer is otherwise silent; this records why a popup was or wasn't drawn.
#pragma once

#include <windows.h>

#include <cstdarg>
#include <cstdio>
#include <mutex>

namespace renderer {

inline void Log(const wchar_t* format, ...) {
    static std::mutex mutex;
    wchar_t line[1024];
    SYSTEMTIME now;
    GetLocalTime(&now);
    int used = swprintf_s(line, L"%02u:%02u:%02u.%03u ", now.wHour, now.wMinute, now.wSecond, now.wMilliseconds);
    if (used < 0) return;
    va_list args;
    va_start(args, format);
    vswprintf_s(line + used, _countof(line) - used, format, args);
    va_end(args);
    wchar_t path[MAX_PATH];
    const DWORD length = GetEnvironmentVariableW(L"LOCALAPPDATA", path, MAX_PATH);
    if (!length || length >= MAX_PATH - 40) return;
    wcscat_s(path, L"\\Completionist\\renderer.log");
    std::lock_guard lock(mutex);
    FILE* file = nullptr;
    if (_wfopen_s(&file, path, L"a, ccs=UTF-8") || !file) return;
    fwprintf(file, L"%s\n", line);
    fclose(file);
}

}  // namespace renderer
