// Validated settings shared by the wire reader, key router and popup renderer.
#pragma once

namespace completionist {

enum class PartialAccept { CtrlRight, AltRight, CtrlTab };
enum class DismissShortcut { Escape, CtrlBackspace, AltBackspace };

struct PopupSettings {
    int font_size = 9;
    double width_scale = 1.0;
    PartialAccept partial_accept = PartialAccept::CtrlRight;
    DismissShortcut dismiss = DismissShortcut::Escape;
};

}  // namespace completionist
