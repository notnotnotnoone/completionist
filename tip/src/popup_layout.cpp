#include "popup_layout.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace completionist::layout {
namespace {

constexpr float kMenuWidthDip = 330.0f;
constexpr float kDockWidthDip = 190.0f;
constexpr float kWorkInsetDip = 20.0f;
constexpr float kCaretGapDip = 6.0f;
constexpr float kPaddingDip = 15.0f;
constexpr float kDockExpandedHeightDip = 54.0f;
constexpr float kDockCollapsedHeightDip = 30.0f;

float Scale(const WorkArea& work) { return static_cast<float>(work.dpi ? work.dpi : 96) / 96.0f; }
int Px(float dip, float scale) { return static_cast<int>(std::lround(dip * scale)); }
float Dip(int px, float scale) { return static_cast<float>(px) / scale; }

int Clamp(int value, int low, int high) {
    if (high < low) return low;
    return std::clamp(value, low, high);
}

bool Intersects(const render::Rect& a, const render::Rect& b) {
    return a.left < b.right && b.left < a.right && a.top < b.bottom && b.top < a.bottom;
}

bool HasStatus(const render::Snapshot& s) {
    using render::AiState;
    return s.ai == AiState::Manual || s.ai == AiState::Scheduled || s.ai == AiState::Working ||
           s.ai == AiState::Streaming || s.ai == AiState::Unavailable;
}

DipRect ToDip(const render::Rect& r, const render::Rect& origin, float scale) {
    return {Dip(r.left - origin.left, scale), Dip(r.top - origin.top, scale),
            Dip(r.right - origin.left, scale), Dip(r.bottom - origin.top, scale)};
}

render::Rect PlaceMenu(const render::Snapshot& s, const WorkArea& w, int width, int height, int gap) {
    const auto& area = w.bounds;
    const int availableWidth = std::max(1, area.right - area.left);
    const int availableHeight = std::max(1, area.bottom - area.top);
    width = std::clamp(width, 1, availableWidth);
    height = std::clamp(height, 1, availableHeight);
    const int x = Clamp(s.caret.left, area.left, area.right - width);
    const int belowY = s.caret.bottom + gap;
    const int aboveY = s.caret.top - gap - height;
    int y = belowY;
    if (belowY + height > area.bottom && aboveY >= area.top) y = aboveY;
    else if (belowY + height > area.bottom) {
        const int roomBelow = std::max(0, area.bottom - belowY);
        const int roomAbove = std::max(0, s.caret.top - gap - area.top);
        if (roomAbove > roomBelow) y = std::max(area.top, s.caret.top - gap - height);
        else y = belowY;
    }
    y = Clamp(y, area.top, area.bottom - height);
    return {x, y, x + width, y + height};
}

render::Rect PlaceDock(const WorkArea& work, float scale, bool collapsed) {
    const auto& area = work.bounds;
    const int inset = Px(kWorkInsetDip, scale);
    const int width = std::min(Px(kDockWidthDip, scale), std::max(1, area.right - area.left - inset * 2));
    const int height = std::min(Px(collapsed ? kDockCollapsedHeightDip : kDockExpandedHeightDip, scale),
                                std::max(1, area.bottom - area.top));
    const int left = Clamp(area.left + inset, area.left, area.right - width);
    const int bottom = Clamp(area.bottom - inset, area.top + height, area.bottom);
    return {left, bottom - height, left + width, bottom};
}

}  // namespace

Layout Place(const render::Snapshot& snapshot, const WorkArea& work, const ContentMetrics& measured) {
    Layout out{};
    const float scale = Scale(work);
    const auto& area = work.bounds;
    const int workWidth = std::max(1, area.right - area.left);
    const int workHeight = std::max(1, area.bottom - area.top);
    const float workWidthDip = Dip(workWidth, scale);
    const float workHeightDip = Dip(workHeight, scale);
    const float widthScale = std::isfinite(snapshot.settings.width_scale) && snapshot.settings.width_scale > 0
                                 ? static_cast<float>(snapshot.settings.width_scale) : 1.0f;
    const float menuWidthDip = std::min(kMenuWidthDip * widthScale, workWidthDip);
    const int menuWidthPx = std::max(1, std::min(workWidth, Px(menuWidthDip, scale)));

    out.hasPhraseRow = !snapshot.phrase.empty();
    out.hasStatusShelf = HasStatus(snapshot) || measured.hasAuxiliaryShelf;
    out.selectableRows = snapshot.words.size() + (out.hasPhraseRow ? 1u : 0u);
    const float statusHeight = out.hasStatusShelf ? std::max(18.0f, measured.statusHeightDip) + 10.0f : 0.0f;
    const float phraseHeight = out.hasPhraseRow ? std::max(measured.phraseHeightDip, measured.fontSizeDip + 12.0f) : 0.0f;
    float rowsHeight = 0;
    for (std::size_t i = 0; i < snapshot.words.size(); ++i) {
        const float measuredHeight = i < measured.rows.size() ? measured.rows[i].heightDip : 0.0f;
        rowsHeight += std::max(measuredHeight, measured.fontSizeDip + 12.0f);
    }
    const float gaps = static_cast<float>(snapshot.words.size() + (out.hasPhraseRow ? 1u : 0u)) * 2.0f;
    const float headerHeight=std::max(0.0f,measured.headerHeightDip);
    const float desiredHeightDip = std::max(1.0f, kPaddingDip * 2.0f + headerHeight + statusHeight + phraseHeight + rowsHeight + gaps);
    const float menuHeightDip = std::min(desiredHeightDip, workHeightDip);
    const int menuHeightPx = std::max(1, std::min(workHeight, Px(menuHeightDip, scale)));
    const int gapPx = std::min(Px(kCaretGapDip, scale), std::max(0, workHeight / 2));
    out.menuBounds = PlaceMenu(snapshot, work, menuWidthPx, menuHeightPx, gapPx);
    out.menuDip = {0, 0, Dip(out.menuBounds.right - out.menuBounds.left, scale),
                   Dip(out.menuBounds.bottom - out.menuBounds.top, scale)};
    out.menuContent = {kPaddingDip, kPaddingDip,
                       std::max(kPaddingDip, out.menuDip.right - kPaddingDip),
                       std::max(kPaddingDip, out.menuDip.bottom - kPaddingDip)};

    const float contentWidth = std::max(0.0f, out.menuContent.width());
    float y = out.menuContent.top;
    if(headerHeight>0) {
        const float h=std::min(headerHeight,std::max(0.0f,out.menuContent.bottom-y));
        out.headerClip={out.menuContent.left,y,out.menuContent.right,y+h}; y+=h;
    }
    if (out.hasStatusShelf) {
        const float h = std::min(statusHeight, std::max(0.0f, out.menuContent.bottom - y));
        out.statusClip = {out.menuContent.left, y, out.menuContent.right, y + h};
        y += h;
    }
    if (out.hasPhraseRow) {
        const float h = std::min(phraseHeight, std::max(0.0f, out.menuContent.bottom - y));
        out.rowOrder.push_back({RowKind::Phrase, 0, measured.measuredContentWidthDip,
                                {out.menuContent.left, y, out.menuContent.right, y + h},
                                {out.menuContent.left, y, out.menuContent.right, y + h}});
        y += h + 2.0f;
    }
    for (std::size_t i = 0; i < snapshot.words.size(); ++i) {
        const float measuredHeight = i < measured.rows.size() ? measured.rows[i].heightDip : 0.0f;
        const float desired = std::max(measuredHeight, measured.fontSizeDip + 12.0f);
        const float h = std::min(desired, std::max(0.0f, out.menuContent.bottom - y));
        const float measuredWidth = i < measured.rows.size() ? measured.rows[i].widthDip : 0.0f;
        out.rowOrder.push_back({RowKind::Word, i, measuredWidth,
                                {out.menuContent.left, y, out.menuContent.right, y + h},
                                {out.menuContent.left, y, out.menuContent.left + contentWidth, y + h}});
        y += h + 2.0f;
    }

    out.dockCollapsed = workHeightDip < kDockExpandedHeightDip + 2.0f * kWorkInsetDip;
    out.dockBounds = PlaceDock(work, scale, out.dockCollapsed);
    if (Intersects(out.menuBounds, out.dockBounds)) {
        out.dockCollapsed = true;
        out.dockBounds = PlaceDock(work, scale, true);
    }
    out.dockDip = ToDip(out.dockBounds, out.dockBounds, scale);
    out.dockContent = {out.dockDip.left + 10.0f, out.dockDip.top + 6.0f,
                       out.dockDip.right - 10.0f, out.dockDip.bottom - 6.0f};
    return out;
}

}  // namespace completionist::layout
