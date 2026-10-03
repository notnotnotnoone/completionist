#pragma once

#define NOMINMAX
#include <windows.h>

#include <functional>

#include "session.h"

namespace renderer {

// Runs blocking named-pipe work on its caller's worker thread. The presenter owns the
// renderer's UI/GPU thread and returns true only after both current surfaces were drawn.
class ProductionRendererPipe {
public:
    using Present = std::function<bool(const completionist::render::Snapshot&)>;
    using Hide = std::function<void()>;

    ProductionRendererPipe(Present present, Hide hide) : present_(std::move(present)), hide_(std::move(hide)) {}
    DWORD Run(HANDLE stopEvent);

private:
    bool ServeClient(HANDLE pipe, HANDLE stopEvent);
    Present present_;
    Hide hide_;
    Session session_;
};

}  // namespace renderer
