#pragma once

#define NOMINMAX
#include <windows.h>
#include <werapi.h>

namespace renderer {
inline bool suppressFaultDialogsForCurrentProcess() noexcept {
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
    return SUCCEEDED(WerSetFlags(WER_FAULT_REPORTING_NO_UI));
}
}
