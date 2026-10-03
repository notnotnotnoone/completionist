#pragma once

namespace renderer {

constexpr bool SupportsLiveWindowsBuild(unsigned major, unsigned minor, unsigned build) {
    return major > 10 || (major == 10 && (minor > 0 || (minor == 0 && build >= 19041)));
}

}  // namespace renderer
