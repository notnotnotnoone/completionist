#pragma once

#define NOMINMAX
#include <windows.h>
#include <UIAutomation.h>

#include <memory>

#include "../src/popup_layout.h"

namespace completionist::render { struct Snapshot; }

namespace renderer::accessibility {

struct Trees {
    IRawElementProviderSimple* menu = nullptr;
    IRawElementProviderSimple* dock = nullptr;
};

Trees CreateTrees(HWND menuWindow, HWND dockWindow,
                  const completionist::render::Snapshot& snapshot,
                  const completionist::layout::Layout& layout, unsigned dpi, bool dockExpanded);
void Release(Trees* trees);

}  // namespace renderer::accessibility
