// Pure placement and logical content geometry for the V2 surfaces.
#pragma once

#include <cstddef>
#include <vector>

#include "render_protocol.h"

namespace completionist::layout {

struct WorkArea {
    render::Rect bounds{};  // physical virtual-desktop pixels, from the caret monitor
    unsigned dpi = 96;
};

struct DipRect {
    float left = 0, top = 0, right = 0, bottom = 0;
    float width() const { return right - left; }
    float height() const { return bottom - top; }
};

struct MeasuredRow {
    float widthDip = 0;
    float heightDip = 0;
};

// Filled from measured GDI or DirectWrite text metrics. Geometry never estimates glyph widths.
struct ContentMetrics {
    float fontSizeDip = 12;
    float rowGapDip = 8;
    float measuredContentWidthDip = 0;
    float phraseHeightDip = 0;
    float statusHeightDip = 0;  // measured height of the AI/correction shelf content
    bool hasAuxiliaryShelf = false;  // for selected correction context without an AI status
    std::vector<MeasuredRow> rows;
};

enum class RowKind { Phrase, Word };
struct RowLayout {
    RowKind kind = RowKind::Word;
    std::size_t wordIndex = 0;
    float measuredTextWidthDip = 0;
    DipRect bounds{};
    DipRect textClip{};
};

struct Layout {
    render::Rect menuBounds{};
    render::Rect dockBounds{};
    DipRect menuDip{};
    DipRect dockDip{};
    DipRect menuContent{};
    DipRect dockContent{};
    std::vector<RowLayout> rowOrder;  // spatial and keyboard word order; phrase first
    DipRect statusClip{};
    bool hasPhraseRow = false;
    bool hasStatusShelf = false;
    bool dockCollapsed = false;
    std::size_t selectableRows = 0;  // phrase plus words, never status-only content
};

Layout Place(const render::Snapshot& snapshot, const WorkArea& work, const ContentMetrics& measured);

}  // namespace completionist::layout
