#include "text.h"

#include <algorithm>
#include <cmath>
#include <cwchar>
#include <string_view>

namespace renderer::text {
namespace {
using completionist::layout::DipRect;
using completionist::render::AiState;
using completionist::render::Snapshot;
using Microsoft::WRL::ComPtr;

bool High(wchar_t c) { return c >= 0xD800 && c <= 0xDBFF; }
bool Low(wchar_t c) { return c >= 0xDC00 && c <= 0xDFFF; }
bool InsidePair(const std::wstring& text, UINT32 i) {
    return i > 0 && i < text.size() && High(text[i - 1]) && Low(text[i]);
}

std::wstring StatusText(const Snapshot& snapshot) {
    switch (snapshot.ai) {
        case AiState::Off: return {};
        case AiState::Manual: return L"Ctrl+Space for a phrase";
        case AiState::Scheduled: {
            const double seconds = static_cast<double>(snapshot.waitMs) / 1000.0;
            wchar_t value[64]{};
            swprintf_s(value, L"%.1fs until AI", seconds);
            return value;
        }
        case AiState::Working: {
            const double seconds = static_cast<double>(snapshot.elapsedMs) / 1000.0;
            wchar_t value[64]{};
            swprintf_s(value, L"Working · %.1fs", seconds);
            return value;
        }
        case AiState::Streaming: {
            const double seconds = static_cast<double>(snapshot.elapsedMs) / 1000.0;
            wchar_t value[64]{};
            swprintf_s(value, L"Streaming · %.1fs", seconds);
            return value;
        }
        case AiState::Ready: return {};
        case AiState::Unavailable:
            if (snapshot.triggerReason == "paused") return L"AI unavailable · paused";
            if (snapshot.triggerReason == "provider") return L"AI unavailable · provider";
            return L"AI unavailable";
    }
    return {};
}

std::wstring PartialHint(const completionist::PopupSettings& settings) {
    switch (settings.partial_accept) {
        case completionist::PartialAccept::CtrlRight: return L"Ctrl+→";
        case completionist::PartialAccept::AltRight: return L"Alt+→";
        case completionist::PartialAccept::CtrlTab: return L"Ctrl+Tab";
    }
    return {};
}

bool MakeFormat(IDWriteFactory* factory, const wchar_t* family, float size, ComPtr<IDWriteTextFormat>* format) {
    return factory && SUCCEEDED(factory->CreateTextFormat(family, nullptr, DWRITE_FONT_WEIGHT_NORMAL,
        DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, size, L"en-us", format->GetAddressOf())) &&
        SUCCEEDED((*format)->SetWordWrapping(DWRITE_WORD_WRAPPING_WRAP));
}

bool MakeLayout(IDWriteFactory* factory, const std::wstring& value, IDWriteTextFormat* format,
                float width, float height, ComPtr<IDWriteTextLayout>* layout) {
    if (!factory || !format || width <= 0 || height <= 0 || value.size() > UINT32_MAX) return false;
    if (FAILED(factory->CreateTextLayout(value.data(), static_cast<UINT32>(value.size()), format,
            width, height, layout->GetAddressOf()))) return false;
    DWRITE_TRIMMING trimming{DWRITE_TRIMMING_GRANULARITY_CHARACTER, 0, 0};
    ComPtr<IDWriteInlineObject> ellipsis;
    return SUCCEEDED(factory->CreateEllipsisTrimmingSign(format, &ellipsis)) &&
           SUCCEEDED((*layout)->SetTrimming(&trimming, ellipsis.Get()));
}

D2D1_COLOR_F D2d(palette::Color color, float alpha = 1.0f) {
    return D2D1::ColorF(static_cast<float>(color.r) / 255.0f, static_cast<float>(color.g) / 255.0f,
                        static_cast<float>(color.b) / 255.0f, alpha);
}

bool MakeBrush(ID2D1DeviceContext* context, palette::Color color, ComPtr<ID2D1SolidColorBrush>* brush) {
    return SUCCEEDED(context->CreateSolidColorBrush(D2d(color), brush->GetAddressOf()));
}

bool DrawLayout(ID2D1DeviceContext* context, IDWriteTextLayout* layout, float x, float y,
                ID2D1Brush* brush, const D2D1_RECT_F& clip) {
    if (!layout || !brush) return true;
    context->PushAxisAlignedClip(clip, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
    context->DrawTextLayout(D2D1::Point2F(x, y), layout, brush, D2D1_DRAW_TEXT_OPTIONS_CLIP);
    context->PopAxisAlignedClip();
    return true;
}

D2D1_RECT_F Rect(const DipRect& value) { return D2D1::RectF(value.left, value.top, value.right, value.bottom); }

bool DrawDottedRange(ID2D1DeviceContext* context, IDWriteTextLayout* textLayout, Utf16Range range,
                     float originX, float originY, palette::Color color) {
    if (!range.length) return true;
    std::vector<D2D1_RECT_F> rects;
    if (!HitTestRangeRects(textLayout, range, originX, originY, &rects)) return false;
    if (rects.empty()) return true;
    ComPtr<ID2D1SolidColorBrush> brush;
    if (!MakeBrush(context, color, &brush)) return false;
    for (const auto& part : rects) {
        const float end = part.right;
        const float y = part.top + std::max(1.0f, (part.bottom - part.top) - 2.0f);
        for (float x = part.left + 0.5f; x < end; x += 3.0f)
            context->FillEllipse(D2D1::Ellipse(D2D1::Point2F(x, y), 0.7f, 0.7f), brush.Get());
    }
    return true;
}

bool DrawMarks(ID2D1DeviceContext* context, IDWriteTextLayout* textLayout,
               const std::vector<UINT32>& marks, float x, float y, palette::Color color) {
    if (marks.empty()) return true;
    ComPtr<ID2D1SolidColorBrush> brush;
    if (!MakeBrush(context, color, &brush)) return false;
    for (UINT32 mark : marks) {
        FLOAT hitX = 0, hitY = 0;
        DWRITE_HIT_TEST_METRICS metric{};
        if (FAILED(textLayout->HitTestTextPosition(mark, FALSE, &hitX, &hitY, &metric))) continue;
        const float left = x + hitX;
        const float right = left + std::max(1.0f, metric.width);
        const float baseline = y + hitY + std::max(1.0f, metric.height - 2.0f);
        for (float dot = left + 0.5f; dot < right; dot += 3.0f)
            context->FillEllipse(D2D1::Ellipse(D2D1::Point2F(dot, baseline), 0.65f, 0.65f), brush.Get());
    }
    return true;
}

}  // namespace

Utf16Range NormalizeUtf16Range(const std::wstring& text, UINT32 begin, UINT32 length) {
    const UINT32 size = static_cast<UINT32>(std::min<std::size_t>(text.size(), UINT32_MAX));
    begin = std::min(begin, size);
    const UINT32 end = static_cast<UINT32>(std::min<std::uint64_t>(size,
        static_cast<std::uint64_t>(begin) + length));
    if (InsidePair(text, begin)) ++begin;
    UINT32 safeEnd = end;
    if (InsidePair(text, safeEnd)) --safeEnd;
    if (safeEnd < begin) safeEnd = begin;
    return {begin, safeEnd - begin};
}

std::vector<UINT32> ValidCorrectionMarks(const std::wstring& text, const std::vector<int>& marks) {
    std::vector<UINT32> result;
    result.reserve(std::min(text.size(), marks.size()));
    for (int mark : marks) {
        if (mark < 0) continue;
        std::size_t codePoint = 0;
        for (UINT32 unit = 0; unit < text.size();) {
            if (codePoint == static_cast<std::size_t>(mark)) {
                result.push_back(unit);
                break;
            }
            if (High(text[unit]) && unit + 1 < text.size() && Low(text[unit + 1])) unit += 2;
            else ++unit;
            ++codePoint;
        }
    }
    std::sort(result.begin(), result.end());
    result.erase(std::unique(result.begin(), result.end()), result.end());
    return result;
}

Utf16Range PhraseAcceptanceRange(const completionist::render::Snapshot& snapshot) {
    const std::wstring displayed = snapshot.phraseLead + snapshot.phrase;
    return NormalizeUtf16Range(displayed, snapshot.partialBegin, snapshot.partialLength);
}

bool HitTestRangeRects(IDWriteTextLayout* textLayout, Utf16Range range, float originX, float originY,
                       std::vector<D2D1_RECT_F>* out) {
    if (!out) return false;
    out->clear();
    if (!range.length) return true;
    if (!textLayout) return false;
    UINT32 count = 0;
    const HRESULT countResult = textLayout->HitTestTextRange(range.begin, range.length, 0.0f, 0.0f,
                                                              nullptr, 0, &count);
    if (countResult != E_NOT_SUFFICIENT_BUFFER && FAILED(countResult)) return false;
    if (!count) return true;
    std::vector<DWRITE_HIT_TEST_METRICS> metrics(count);
    if (FAILED(textLayout->HitTestTextRange(range.begin, range.length, 0.0f, 0.0f,
                                            metrics.data(), count, &count))) return false;
    out->reserve(count);
    for (const auto& metric : metrics) {
        out->push_back(D2D1::RectF(originX + metric.left, originY + metric.top,
                                   originX + metric.left + metric.width,
                                   originY + metric.top + metric.height));
    }
    return true;
}

bool TextRenderer::Prepare(const Snapshot& snapshot, float availableWidthDip, PreparedText* out) const {
    if (!out || !factory_ || !std::isfinite(availableWidthDip) || availableWidthDip <= 0) return false;
    PreparedText next{};
    const float points = static_cast<float>(snapshot.settings.font_size > 0 ? snapshot.settings.font_size : 9);
    const float fontSize = points * 96.0f / 72.0f;
    const float contentWidth = std::max(1.0f, availableWidthDip - 30.0f);
    ComPtr<IDWriteTextFormat> body, smallFormat, mono;
    if (!MakeFormat(factory_, L"Segoe UI", fontSize, &body) ||
        !MakeFormat(factory_, L"Segoe UI", 10.5f, &smallFormat) ||
        !MakeFormat(factory_, L"Consolas", 10.0f, &mono)) return false;

    next.metrics.fontSizeDip = fontSize;
    next.metrics.rowGapDip = 8.0f;
    next.metrics.rows.reserve(snapshot.words.size());
    next.words.reserve(snapshot.words.size());
    next.correctionComparisons.reserve(snapshot.words.size());
    next.correctionMarks.reserve(snapshot.words.size());
    float widest = 0;
    for (const auto& word : snapshot.words) {
        std::wstring displayed = word.text;
        UINT32 originBegin = static_cast<UINT32>(displayed.size());
        if (!word.origin.empty()) {
            displayed += L"  ";
            originBegin = static_cast<UINT32>(displayed.size());
            displayed.append(word.origin.begin(), word.origin.end());
        }
        ComPtr<IDWriteTextLayout> layout;
        if (!MakeLayout(factory_, displayed, body.Get(), contentWidth, fontSize * 2.5f + 14.0f, &layout)) return false;
        if (originBegin < displayed.size()) {
            const auto begin = originBegin;
            const auto length = static_cast<UINT32>(displayed.size() - begin);
            if (FAILED(layout->SetFontSize(10.5f, {begin, length}))) return false;
        }
        DWRITE_TEXT_METRICS textMetrics{};
        if (FAILED(layout->GetMetrics(&textMetrics))) return false;
        ComPtr<IDWriteTextLayout> comparison;
        float comparisonHeight = 0;
        if (!word.marks.empty() && !snapshot.typedFragment.empty()) {
            const std::wstring comparisonText = snapshot.typedFragment + L" → " + word.text;
            if (!MakeLayout(factory_, comparisonText, smallFormat.Get(), contentWidth, 20.0f, &comparison)) return false;
            DWRITE_TEXT_METRICS comparisonMetrics{};
            if (FAILED(comparison->GetMetrics(&comparisonMetrics))) return false;
            comparisonHeight = comparisonMetrics.height + 2.0f;
            widest = std::max(widest, comparisonMetrics.width);
        }
        next.metrics.rows.push_back({textMetrics.width, comparisonHeight +
            std::max(textMetrics.height, fontSize + 2.0f) + 8.0f});
        widest = std::max(widest, textMetrics.width);
        next.words.push_back(std::move(layout));
        next.correctionComparisons.push_back(std::move(comparison));
        next.correctionMarks.push_back(ValidCorrectionMarks(word.text, word.marks));
    }

    next.displayPhrase = snapshot.phraseLead + snapshot.phrase;
    next.acceptance = PhraseAcceptanceRange(snapshot);
    if (!next.displayPhrase.empty()) {
        if (!MakeLayout(factory_, next.displayPhrase, body.Get(), contentWidth, fontSize * 2.5f + 14.0f, &next.phrase)) return false;
        DWRITE_TEXT_METRICS phraseMetrics{};
        if (FAILED(next.phrase->GetMetrics(&phraseMetrics))) return false;
        next.metrics.phraseHeightDip = std::min(phraseMetrics.height + 10.0f, fontSize * 2.5f + 14.0f);
        widest = std::max(widest, phraseMetrics.width);
    }

    const std::wstring statusText = StatusText(snapshot);
    if (!statusText.empty()) {
        if (!MakeLayout(factory_, statusText, smallFormat.Get(), contentWidth, fontSize * 3.0f + 20.0f, &next.status)) return false;
        DWRITE_TEXT_METRICS statusMetrics{};
        if (FAILED(next.status->GetMetrics(&statusMetrics))) return false;
        next.metrics.statusHeightDip = std::min(statusMetrics.height, 2.0f * 14.0f);
        widest = std::max(widest, statusMetrics.width);
    }

    const std::wstring connectionText = snapshot.engineConnected ? L"Connected" : L"Engine offline";
    if (!MakeLayout(factory_, connectionText, smallFormat.Get(), 150.0f, 24.0f, &next.connection) ||
        !MakeLayout(factory_, completionist::render::TenseText(snapshot), smallFormat.Get(), 120.0f, 24.0f, &next.tense) ||
        !MakeLayout(factory_, L"Tab", mono.Get(), 42.0f, 20.0f, &next.shortcut)) return false;
    next.metrics.measuredContentWidthDip = widest;
    *out = std::move(next);
    return true;
}

bool TextRenderer::Draw(ID2D1DeviceContext* context, const Snapshot& snapshot,
                        const completionist::layout::Layout& layout, const PreparedText& prepared,
                        const palette::Theme& colors, Surface surface, float connectionOpacity) const {
    if (!context || !factory_) return false;
    ComPtr<ID2D1SolidColorBrush> ink, muted, ghost, accent, accentInk, onAccent, connected, outline;
    if (!MakeBrush(context, colors.ink, &ink) || !MakeBrush(context, colors.muted, &muted) ||
        !MakeBrush(context, colors.ghost, &ghost) || !MakeBrush(context, colors.accent, &accent) ||
        !MakeBrush(context, colors.accentInk, &accentInk) || !MakeBrush(context, colors.onAccent, &onAccent) ||
        !MakeBrush(context, colors.ok, &connected) || !MakeBrush(context, colors.lineStrong, &outline)) return false;

    if (surface == Surface::Menu) for (const auto& row : layout.rowOrder) {
        const bool isPhrase = row.kind == completionist::layout::RowKind::Phrase;
        const bool selected = isPhrase ? snapshot.selection == -1 : snapshot.selection == static_cast<int>(row.wordIndex);
        if (selected) {
            const D2D1_ROUNDED_RECT shape{Rect(row.bounds), 7.0f, 7.0f};
            context->FillRoundedRectangle(shape, accent.Get());
            context->FillRectangle(D2D1::RectF(row.bounds.left, row.bounds.top, row.bounds.left + 3.0f,
                                               row.bounds.bottom), onAccent.Get());
        }
        IDWriteTextLayout* textLayout = nullptr;
        if (isPhrase) textLayout = prepared.phrase.Get();
        else if (row.wordIndex < prepared.words.size()) textLayout = prepared.words[row.wordIndex].Get();
        if (textLayout && isPhrase && !snapshot.phraseLead.empty()) {
            textLayout->SetDrawingEffect(selected ? static_cast<IUnknown*>(onAccent.Get())
                                                  : static_cast<IUnknown*>(ink.Get()),
                                        {0, static_cast<UINT32>(snapshot.phraseLead.size())});
            if (!snapshot.phrase.empty())
                textLayout->SetDrawingEffect(selected ? static_cast<IUnknown*>(onAccent.Get())
                                                      : static_cast<IUnknown*>(ghost.Get()),
                                            {static_cast<UINT32>(snapshot.phraseLead.size()),
                                             static_cast<UINT32>(snapshot.phrase.size())});
        } else if (textLayout && row.wordIndex < snapshot.words.size()) {
            const auto& word = snapshot.words[row.wordIndex];
            if (!word.origin.empty()) {
                const UINT32 begin = static_cast<UINT32>(word.text.size() + 2);
                const UINT32 length = static_cast<UINT32>(word.origin.size());
                textLayout->SetDrawingEffect(selected ? static_cast<IUnknown*>(onAccent.Get())
                                                      : static_cast<IUnknown*>(muted.Get()), {begin, length});
            }
        }
        const float x = row.textClip.left;
        float y = row.textClip.top + std::max(0.0f, (row.textClip.height() - prepared.metrics.fontSizeDip) / 2.0f);
        ID2D1SolidColorBrush* textBrush = selected ? onAccent.Get() : (isPhrase ? ghost.Get() : ink.Get());
        if (!isPhrase && row.wordIndex < prepared.correctionComparisons.size() &&
            prepared.correctionComparisons[row.wordIndex]) {
            auto* comparison = prepared.correctionComparisons[row.wordIndex].Get();
            DWRITE_TEXT_METRICS comparisonMetrics{};
            if (SUCCEEDED(comparison->GetMetrics(&comparisonMetrics))) {
                const float comparisonY = row.textClip.top;
                DrawLayout(context, comparison, x, comparisonY,
                           selected ? onAccent.Get() : muted.Get(), Rect(row.textClip));
                y = comparisonY + comparisonMetrics.height;
            }
        }
        if (!DrawLayout(context, textLayout, x, y, textBrush, Rect(row.textClip))) return false;

        if (isPhrase) {
            if (!DrawDottedRange(context, textLayout, prepared.acceptance, x, y,
                                 selected ? colors.onAccent : colors.accentInk)) return false;
        } else if (row.wordIndex < prepared.correctionMarks.size()) {
            if (!DrawMarks(context, textLayout, prepared.correctionMarks[row.wordIndex], x, y,
                           selected ? colors.onAccent : colors.accentInk)) return false;
        }
        if (selected && prepared.shortcut) {
            DWRITE_TEXT_METRICS hint{};
            prepared.shortcut->GetMetrics(&hint);
            const float hintX = row.textClip.right - hint.width;
            DrawLayout(context, prepared.shortcut.Get(), hintX, row.bounds.top + 4.0f,
                       selected ? onAccent.Get() : muted.Get(), Rect(row.textClip));
        }
    }

    if (surface == Surface::Menu && layout.hasStatusShelf && prepared.status) {
        const bool selected = false;
        DrawLayout(context, prepared.status.Get(), layout.statusClip.left, layout.statusClip.top,
                   selected ? onAccent.Get() : muted.Get(), Rect(layout.statusClip));
        const std::wstring hint = PartialHint(snapshot.settings);
        ComPtr<IDWriteTextFormat> hintFormat;
        ComPtr<IDWriteTextLayout> hintLayout;
        if (!hint.empty() && MakeFormat(factory_, L"Consolas", 10.0f, &hintFormat) &&
            MakeLayout(factory_, hint, hintFormat.Get(), 60.0f, 18.0f, &hintLayout)) {
            DWRITE_TEXT_METRICS hintMetrics{};
            hintLayout->GetMetrics(&hintMetrics);
            DrawLayout(context, hintLayout.Get(), layout.statusClip.right - hintMetrics.width,
                       layout.statusClip.top, muted.Get(), Rect(layout.statusClip));
        }
    }

    if (surface == Surface::Dock && prepared.connection) {
        const float dockY = layout.dockContent.top;
        const D2D1_ELLIPSE connectionMark = D2D1::Ellipse(
            D2D1::Point2F(layout.dockContent.left + 3.0f, dockY + 7.0f), 3.0f, 3.0f);
        if (snapshot.engineConnected) {
            connected->SetOpacity(std::clamp(connectionOpacity,0.0f,1.0f));
            context->FillEllipse(connectionMark, connected.Get());
        }
        else context->DrawEllipse(connectionMark, outline.Get(), 1.4f);
        DrawLayout(context, prepared.connection.Get(), layout.dockContent.left + 12.0f, dockY,
                   snapshot.engineConnected ? accentInk.Get() : muted.Get(), Rect(layout.dockContent));
        if (prepared.tense && !layout.dockCollapsed) {
            DWRITE_TEXT_METRICS tenseMetrics{};
            prepared.tense->GetMetrics(&tenseMetrics);
            DrawLayout(context, prepared.tense.Get(), layout.dockContent.left,
                       dockY + tenseMetrics.height + 2.0f, muted.Get(), Rect(layout.dockContent));
        }
    }
    return true;
}

}  // namespace renderer::text
