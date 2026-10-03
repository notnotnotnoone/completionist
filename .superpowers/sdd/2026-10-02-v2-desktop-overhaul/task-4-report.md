# Task 4 report: native layout and typography

Task 4 now has a pure placement API, a measured DirectWrite text path, a native Evergreen palette, and authored geometry/Unicode tests. M10.17 remains **doing**. This is source and compile evidence only; runtime RED/GREEN, fixture rendering, and live acceptance remain **UNRUN**.

## Implementation

- `completionist::layout::Place(snapshot, workArea, measuredContent)` consumes the existing `render::Snapshot` and physical `render::Rect` without redefining protocol or settings types. It returns physical menu/dock bounds plus DIPs for content, clipped text rows, shelf and dock layout. The menu starts from 330 DIPs times `PopupSettings.width_scale`; the dock is 190 DIPs with a 20-DIP inset on the same supplied caret monitor. The preferred caret gap is 6 DIPs. Font points convert to DIPs with `font_size * 96 / 72`.
- `renderer::text::TextRenderer::Prepare` makes DirectWrite layouts for each candidate, phrase, status, origin and shortcut, and supplies their measured widths/heights to pure placement. It uses Segoe UI and Consolas, measured wrapping and DirectWrite ellipsis trimming. `Draw` paints only the selected surface's sharp text and selection shape over the glass/material layer. Correction marks from the Python engine are code-point indexes; the text path maps them to UTF-16 positions before DirectWrite hit testing. Invalid marks are ignored. Phrase acceptance ranges are normalized against the displayed lead-plus-phrase string without changing the snapshot's insertion text. `PreparedText::Reset()` releases retained layouts on owner hide/revocation; `TextRenderer` has no content cache.
- `renderer::renderFixtureMatrix(directory)` is wired to the same `Prepare`, `Place` and `Draw` path for six intended outputs: 96, 144 and 192 DPI in light and dark Evergreen themes. The executable exposes this as `--fixture-matrix`.
- `renderer/popup_palette.h` exports 30 named canonical roles for both themes. `tip/tests/check_popup_palette.py` checks those constants against actual `docs/roadmap/theme.css` values.
- The native suite in `tip/tests/test_popup_layout.cpp` covers 96/144/192 DPI, negative/portrait work areas, caret bottom edge and clearance, narrow/tiny work areas, configured width and font size, word-only and pending-only layouts, measured wrapping, invalid/code-point marks, UTF-16 range boundaries, phrase insertion preservation, and DirectWrite ellipsis behavior across a surrogate pair.

## Source API and files

- `tip/src/popup_layout.h/.cpp`: `WorkArea`, `ContentMetrics`, `Layout`, row/text clips and `Place`.
- `tip/renderer/text.h/.cpp`: `TextRenderer`, `PreparedText`, Unicode range and correction-mark helpers.
- `tip/renderer/popup_palette.h`: light/dark canonical role constants.
- `tip/renderer/fixture.h/.cpp` and `tip/renderer/main.cpp`: same-text-path fixture matrix entry point.
- `tip/renderer/build.cmd`: links the shared layout and DirectWrite source.
- `tip/tests/test_popup_layout.cpp`, `tip/tests/check_popup_palette.py`, and `tip/test.cmd`: authored tests, palette parity and compile-only test target.
- `docs/roadmap/roadmap.js`: M10.17 marked doing and this commit logged. Task 3's deferred `EncodeShow` and settings checks remain in their existing ledger entry.

## Compile and source checks

Command: `.\tip\test.cmd --compile-only`

Output:

```text
test_main.cpp
test_popup_model.cpp
test_protocol.cpp
test_render_protocol.cpp
protocol.cpp
render_protocol.cpp
Generating Code...
test_main.cpp
test_popup_layout.cpp
popup_layout.cpp
text.cpp
Generating Code...
exit_code=0
```

This compiled both the existing host regression target and the added layout/DirectWrite target. Neither test executable was run.

Command: `.\tip\renderer\build.cmd`

Output:

```text
compilation header save succeeded; see C:\projects\Experiments\completionist\tip\renderer\out\shaders.h
compilation header save succeeded; see C:\projects\Experiments\completionist\tip\renderer\out\blur_ps.h
main.cpp
fixture.cpp
surfaces.cpp
capture.cpp
material.cpp
text.cpp
popup_layout.cpp
Generating Code...
exit_code=0
```

The renderer build command compiled the executable but did not launch it.

Command: `python tip/tests/check_popup_palette.py`

Output: `Evergreen popup palette matches theme.css (30 roles, light/dark).` Exit code 0.

Command: `python scripts/check_roadmap.py`

Output: `roadmap ok: 96/108 tasks done, latest release 1.1.14, target 2.0.0, updated 2026-10-03` Exit code 0.

Command: `git -c core.safecrlf=false diff --check`

Output: no output (exit code 0).

## Unrun acceptance gates

- Native test executable and runtime RED/GREEN results: **UNRUN**.
- `--fixture-matrix`, generated PNGs, and visual image inspection: **UNRUN**.
- Native renderer/runtime, live windows, debugger, desktop capture, and the focus-stealing harness: **UNRUN**.
- Mixed-DPI owner checks and other M10.17 live/fixture acceptance: **UNRUN**.

The compile-only results establish that the added native source and test targets compile with the repository's x64 static CRT toolchain. They do not establish layout behavior at runtime or visual quality.
