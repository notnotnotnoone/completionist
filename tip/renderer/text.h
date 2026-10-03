#pragma once

#define NOMINMAX
#include <windows.h>
#include <d2d1_1.h>
#include <dwrite.h>
#include <wrl/client.h>

#include <string>
#include <vector>

#include "popup_palette.h"
#include "../src/popup_layout.h"

namespace renderer::text {

using Microsoft::WRL::ComPtr;

struct Utf16Range { UINT32 begin = 0; UINT32 length = 0; };
enum class Surface { Menu, Dock };

Utf16Range NormalizeUtf16Range(const std::wstring& text, UINT32 begin, UINT32 length);
std::vector<UINT32> ValidCorrectionMarks(const std::wstring& text, const std::vector<int>& marks);
Utf16Range PhraseAcceptanceRange(const completionist::render::Snapshot& snapshot);
bool HitTestRangeRects(IDWriteTextLayout* textLayout, Utf16Range range, float originX, float originY,
                       std::vector<D2D1_RECT_F>* out);

struct PreparedText {
    completionist::layout::ContentMetrics metrics;
    std::vector<ComPtr<IDWriteTextLayout>> words;
    std::vector<ComPtr<IDWriteTextLayout>> correctionComparisons;
    ComPtr<IDWriteTextLayout> phrase;
    ComPtr<IDWriteTextLayout> status;
    ComPtr<IDWriteTextLayout> connection;
    ComPtr<IDWriteTextLayout> tense;
    ComPtr<IDWriteTextLayout> shortcut;
    std::vector<std::vector<UINT32>> correctionMarks;
    Utf16Range acceptance{};
    std::wstring displayPhrase;

    // Call when the owning session hides/revokes; no text cache is retained by TextRenderer.
    void Reset() {
        words.clear();
        correctionComparisons.clear();
        correctionMarks.clear();
        phrase.Reset(); status.Reset(); connection.Reset(); tense.Reset(); shortcut.Reset();
        displayPhrase.clear();
        acceptance = {};
        metrics = {};
    }
};

class TextRenderer {
public:
    explicit TextRenderer(IDWriteFactory* factory) : factory_(factory) {}

    // Creates actual DirectWrite layouts and returns their measured dimensions to pure geometry.
    bool Prepare(const completionist::render::Snapshot& snapshot, float availableWidthDip, PreparedText* out) const;
    // Draws crisp foreground text and selection shapes over a material surface.
    bool Draw(ID2D1DeviceContext* context, const completionist::render::Snapshot& snapshot,
              const completionist::layout::Layout& layout, const PreparedText& prepared,
              const palette::Theme& colors, Surface surface = Surface::Menu,
              float connectionOpacity = 1.0f) const;

private:
    IDWriteFactory* factory_ = nullptr;  // owned by the renderer surface lifetime
};

}  // namespace renderer::text
