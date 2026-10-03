// Tokens invalidate already-queued pipe-state notifications after a popup is unregistered.
#pragma once

#include <cstdint>

namespace completionist {

constexpr bool IsCurrentConnectionObserver(std::uint64_t registration, std::uintptr_t messageToken) {
    return registration != 0 && registration == messageToken;
}

}  // namespace completionist
