#include "test_harness.h"

#include "../renderer/text.h"
#include "../src/popup_layout.h"

#include <cmath>

using completionist::render::Rect;
using completionist::render::Snapshot;
using completionist::render::Candidate;
using completionist::layout::ContentMetrics;
using completionist::layout::MeasuredRow;
using completionist::layout::WorkArea;

namespace {
Snapshot SnapshotWithRows(std::size_t count) {
    Snapshot value{};
    value.caret = {100, 100, 102, 120};
    value.words.resize(count);
    for (auto& word : value.words) word.text = L"candidate";
    value.selection = count ? 0 : -2;
    value.engineConnected = true;
    return value;
}

ContentMetrics MetricsFor(const Snapshot& snapshot, float rowHeight = 28.0f) {
    ContentMetrics metrics{};
    metrics.fontSizeDip = 12.0f;
    metrics.rowGapDip = 8.0f;
    metrics.measuredContentWidthDip = 170.0f;
    metrics.rows.resize(snapshot.words.size());
    for (auto& row : metrics.rows) {
        row.heightDip = rowHeight;
        row.widthDip = 70.0f;
    }
    metrics.phraseHeightDip = 34.0f;
    metrics.statusHeightDip = 18.0f;
    return metrics;
}

bool Contains(const Rect& outer, const Rect& inner) {
    return inner.left >= outer.left && inner.top >= outer.top &&
           inner.right <= outer.right && inner.bottom <= outer.bottom;
}
}

TEST(MenuAndDockUseTheCaretMonitorAtSupportedDpis) {
    for (UINT dpi : {96U, 144U, 192U}) {
        const int extent = MulDiv(1200, static_cast<int>(dpi), 96);
        WorkArea work{{-extent, -extent / 2, 0, extent / 2}, dpi};
        Snapshot snapshot = SnapshotWithRows(3);
        snapshot.caret = {-800, 0, -798, 24};
        const auto result = completionist::layout::Place(snapshot, work, MetricsFor(snapshot));
        CHECK(Contains(work.bounds, result.menuBounds));
        CHECK(Contains(work.bounds, result.dockBounds));
        CHECK(result.menuDip.width() <= 330.0f * snapshot.settings.width_scale + 0.01f);
        CHECK(result.dockBounds.left >= work.bounds.left);
        CHECK(result.dockBounds.bottom <= work.bounds.bottom);
    }
}

TEST(MenuKeepsCaretClearanceAndFlipsAboveAtTheBottomEdge) {
    WorkArea work{{0, 0, 1280, 720}, 96};
    Snapshot snapshot = SnapshotWithRows(5);
    snapshot.caret = {540, 680, 542, 700};
    const auto result = completionist::layout::Place(snapshot, work, MetricsFor(snapshot));
    CHECK(result.menuBounds.bottom <= snapshot.caret.top - 6);
    CHECK(result.menuBounds.bottom <= work.bounds.bottom);
}

TEST(MenuStaysInsideNegativePortraitWorkAreaAndHonorsConfiguredWidth) {
    WorkArea work{{-900, -1600, 0, 0}, 144};
    Snapshot snapshot = SnapshotWithRows(2);
    snapshot.settings.width_scale = 1.4;
    snapshot.caret = {-120, -20, -118, 0};
    const auto metrics = MetricsFor(snapshot);
    const auto result = completionist::layout::Place(snapshot, work, metrics);
    CHECK(Contains(work.bounds, result.menuBounds));
    CHECK(std::abs(result.menuDip.width() - std::min(330.0f * 1.4f,
          static_cast<float>(work.bounds.right - work.bounds.left) * 96.0f / work.dpi)) < 0.1f);
}

TEST(NarrowAndTinyWorkAreasClipRowsAndCollapseDockWithoutLeavingMonitor) {
    WorkArea work{{-30, 12, 260, 180}, 96};
    Snapshot snapshot = SnapshotWithRows(7);
    snapshot.caret = {120, 130, 122, 146};
    auto result = completionist::layout::Place(snapshot, work, MetricsFor(snapshot, 48.0f));
    CHECK(Contains(work.bounds, result.menuBounds));
    CHECK(Contains(work.bounds, result.dockBounds));
    CHECK(result.dockCollapsed);
    CHECK(result.rowOrder.size() == snapshot.words.size());
    CHECK(result.menuContent.height() <= result.menuDip.height());
}

TEST(WordOnlyAndPendingOnlyContentHaveCorrectRows) {
    WorkArea work{{0, 0, 1200, 800}, 96};
    Snapshot words = SnapshotWithRows(2);
    const auto wordLayout = completionist::layout::Place(words, work, MetricsFor(words));
    CHECK(wordLayout.rowOrder.size() == 2);
    CHECK(!wordLayout.hasStatusShelf);

    Snapshot pending{};
    pending.caret = {300, 200, 302, 218};
    pending.ai = completionist::render::AiState::Scheduled;
    pending.waitMs = 400;
    pending.selection = -2;
    const auto pendingLayout = completionist::layout::Place(pending, work, MetricsFor(pending));
    CHECK(pendingLayout.rowOrder.empty());
    CHECK(pendingLayout.hasStatusShelf);
    CHECK(pendingLayout.selectableRows == 0);
}

TEST(CustomFontMetricsAndMeasuredWrappingDriveLogicalHeight) {
    WorkArea work{{0, 0, 1400, 900}, 192};
    Snapshot snapshot = SnapshotWithRows(2);
    auto metricsSmall = MetricsFor(snapshot, 27.0f);
    auto metricsLarge = MetricsFor(snapshot, 57.0f);
    snapshot.settings.font_size = 18;
    const auto smallLayout = completionist::layout::Place(snapshot, work, metricsSmall);
    const auto largeLayout = completionist::layout::Place(snapshot, work, metricsLarge);
    CHECK(largeLayout.menuDip.height() > smallLayout.menuDip.height());
    CHECK(std::abs(metricsLarge.fontSizeDip - 18.0f * 96.0f / 72.0f) < 0.01f);
}

TEST(DirectWriteMeasuresConfiguredPointSizeAndWrappedCandidateHeight) {
    Microsoft::WRL::ComPtr<IDWriteFactory> factory;
    CHECK(SUCCEEDED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
        reinterpret_cast<IUnknown**>(factory.GetAddressOf()))));
    if (!factory) return;
    Snapshot snapshot = SnapshotWithRows(0);
    snapshot.words.push_back({std::wstring(240, L'W'), "local", {}});
    snapshot.selection = 0;
    snapshot.settings.font_size = 18;
    renderer::text::TextRenderer renderer(factory.Get());
    renderer::text::PreparedText prepared;
    CHECK(renderer.Prepare(snapshot, 330.0f, &prepared));
    CHECK(std::abs(prepared.metrics.fontSizeDip - 24.0f) < 0.01f);
    CHECK(prepared.metrics.rows.size() == 1);
    if (!prepared.metrics.rows.empty()) CHECK(prepared.metrics.rows[0].heightDip > 24.0f);
    prepared.Reset();
}

TEST(DirectWriteEllipsisKeepsBothUnitsOfASurrogatePairTogether) {
    Microsoft::WRL::ComPtr<IDWriteFactory> factory;
    CHECK(SUCCEEDED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
        reinterpret_cast<IUnknown**>(factory.GetAddressOf()))));
    if (!factory) return;
    Snapshot snapshot = SnapshotWithRows(0);
    snapshot.words.push_back({L"WWWWWWWW\U0001F600 plus a long tail to force measured ellipsis", "local", {}});
    snapshot.selection = 0;
    renderer::text::TextRenderer renderer(factory.Get());
    renderer::text::PreparedText prepared;
    CHECK(renderer.Prepare(snapshot, 100.0f, &prepared));
    if (prepared.words.empty()) return;
    FLOAT highX = 0, highY = 0, lowX = 0, lowY = 0;
    DWRITE_HIT_TEST_METRICS high{}, low{};
    CHECK(SUCCEEDED(prepared.words[0]->HitTestTextPosition(8, FALSE, &highX, &highY, &high)));
    CHECK(SUCCEEDED(prepared.words[0]->HitTestTextPosition(9, FALSE, &lowX, &lowY, &low)));
    CHECK(high.isTrimmed == low.isTrimmed);
    prepared.Reset();
}

TEST(InvalidCorrectionMarksAndSurrogateSplittingAreIgnored) {
    const std::wstring text = L"A\U0001F600BC";
    const auto marks = renderer::text::ValidCorrectionMarks(text, {0, 1, 2, 3, 4, 500});
    CHECK((marks == std::vector<UINT32>{0, 1, 3, 4}));
    const auto range = renderer::text::NormalizeUtf16Range(text, 2, 1);
    CHECK(range.begin == 3);
    CHECK(range.length == 0);
}

TEST(PartialAcceptanceRangeTracksDisplayedPhraseWithoutChangingInsertionText) {
    Snapshot snapshot{};
    snapshot.phrase = L"\U0001F600 house at the end";
    snapshot.phraseLead = L"hello ";
    snapshot.partialBegin = 9;
    snapshot.partialLength = 5;
    const auto range = renderer::text::PhraseAcceptanceRange(snapshot);
    CHECK(range.begin == 9);
    CHECK(range.length == 5);
    CHECK(snapshot.phrase == L"\U0001F600 house at the end");
}
