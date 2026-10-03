#pragma once

#define NOMINMAX
#include <windows.h>

#include "dock_state.h"

namespace renderer::dock {

constexpr bool CompletesMotionImmediately(UINT message) {
    return message == WM_THEMECHANGED || message == WM_DPICHANGED || message == WM_DISPLAYCHANGE;
}

inline void ApplySystemChange(State& state, UINT message, uint64_t nowMs) {
    if (CompletesMotionImmediately(message)) state.CompleteImmediately(nowMs);
}

}  // namespace renderer::dock
