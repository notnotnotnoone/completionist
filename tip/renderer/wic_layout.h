#pragma once

#include <cstddef>
#include <limits>

namespace renderer {
struct WicMemoryLayout {
    unsigned strideBytes = 0;
    unsigned bufferSizeBytes = 0;
};

constexpr bool makeWicMemoryLayout(unsigned width, unsigned height, unsigned rowPitch,
                                   std::size_t availableBytes, WicMemoryLayout& result) noexcept {
    constexpr std::size_t bytesPerPixel = 4;
    constexpr std::size_t maxSize = std::numeric_limits<std::size_t>::max();
    constexpr std::size_t maxWicSize = std::numeric_limits<unsigned>::max();
    if (width == 0 || height == 0 || rowPitch == 0 ||
        static_cast<std::size_t>(width) > maxSize / bytesPerPixel) return false;

    const std::size_t rowBytes = static_cast<std::size_t>(width) * bytesPerPixel;
    const std::size_t stride = rowPitch;
    if (stride < rowBytes || static_cast<std::size_t>(height) > maxSize / stride) return false;
    const std::size_t bufferBytes = stride * static_cast<std::size_t>(height);
    if (bufferBytes > availableBytes || bufferBytes > maxWicSize) return false;

    const std::size_t rowsBeforeLast = static_cast<std::size_t>(height) - 1;
    if (rowsBeforeLast > (maxSize - rowBytes) / stride) return false;
    const std::size_t requiredBytes = rowsBeforeLast * stride + rowBytes;
    if (requiredBytes > bufferBytes) return false;

    result.strideBytes = rowPitch;
    result.bufferSizeBytes = static_cast<unsigned>(bufferBytes);
    return true;
}

constexpr bool validFixtureWicLayout() noexcept {
    WicMemoryLayout layout{};
    return makeWicMemoryLayout(640, 360, 2560, 921600, layout) &&
        layout.strideBytes == 2560 && layout.bufferSizeBytes == 921600;
}

constexpr bool rejectsShortWicStride() noexcept {
    WicMemoryLayout layout{};
    return !makeWicMemoryLayout(640, 360, 2559, 921240, layout);
}

constexpr bool rejectsShortWicBuffer() noexcept {
    WicMemoryLayout layout{};
    return !makeWicMemoryLayout(640, 360, 2560, 921599, layout);
}

constexpr bool rejectsWicBufferSizeOverflow() noexcept {
    WicMemoryLayout layout{};
    return !makeWicMemoryLayout(1, 0xffffffffU, 4, std::numeric_limits<std::size_t>::max(), layout);
}

static_assert(validFixtureWicLayout());
static_assert(rejectsShortWicStride());
static_assert(rejectsShortWicBuffer());
static_assert(rejectsWicBufferSizeOverflow());
}
