// Logging to %LOCALAPPDATA%\Completionist\tip.log. Errors are always written; Debug lines only when the file
// %LOCALAPPDATA%\Completionist\verbose exists (checked when a text service activates, so no per-call cost).
#pragma once

namespace completionist {

void LogError(const wchar_t* format, ...);
void LogDebug(const wchar_t* format, ...);
void RefreshLogLevel();  // re-checks the verbose flag file

}  // namespace completionist
