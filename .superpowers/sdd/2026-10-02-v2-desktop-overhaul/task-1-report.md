# Task 1 renderer feasibility report

Status: **BLOCKED / partial foundation**. M10.14 remains `doing`. The implementation below is not a live-glass feasibility result and must not be treated as passing the release gate.

## Implemented

- Added a focused D3D11 separable Gaussian pass with sigma 6 px, radius 18 px and full-resolution textures in `tip/renderer/material.*`.
- Added SDK `fxc` shader compilation and an explicit generated `/Fh` header included at build time. Runtime has no HLSL source-file lookup.
- Added WARP fixtures for an opaque generated step and an isolated point source. The test reads back only its generated GPU fixture and checks normalized weights, kernel symmetry, monotonic multi-pixel edge response and symmetric impulse response.
- Added a Desktop Duplication acquisition wrapper. Every successful acquisition uses a scope guard to call `ReleaseFrame`; the acquired surface is copied to a GPU texture and released before later work. Access loss, device removal and unsupported format/HDR return failure for a caller to handle.
- Added two nonactivating surface-window creation with `WDA_EXCLUDEFROMCAPTURE`; creation fails closed when either exclusion call fails.

## Verification evidence

Initial RED attempt: `cmd /c tip\renderer\test.cmd` failed during native compilation (compiler output: D8036 malformed object-output path; subsequent compiler diagnostics found the Windows header name collision, missing DXGI Output6 declarations and generated shader symbol mistakes). This was a build red, not a demonstrated pre-blur assertion failure. No live windows were opened.

GREEN command: `cmd /c tip\renderer\test.cmd`

Output ended with:

```text
PASS: WARP Gaussian symmetry, normalized weights, smooth monotonic edge, generated-only fixture
```

This command compiled both embedded HLSL shaders using the installed Windows SDK `fxc`, compiled the executable and WARP test using VS 2022 Build Tools, and ran the WARP GPU test. `python scripts/check_roadmap.py` reported `roadmap ok: 94/107 tasks done ... updated 2026-10-02`.

## Remaining blocking work

The current `main.cpp` intentionally does nothing by default and returns `ERROR_NOT_SUPPORTED` for `--live`, preventing accidental capture until a real presentation path exists. There is no opt-in `--live` capture/composition loop, no DirectComposition swapchain/visual attached to either HWND, no lens pass, no sharp foreground glyph layer, no generated image export, and no usable two-window demo. Capture crop/padding, rotation and desktop origin mapping, dirty/move-region invalidation, resize/shutdown coordination, capture-resource-count fault injection and the required opaque/hidden next-draw state machine are not complete. There are no measurements of GPU time, presentation latency or static repaint behavior. No live checks were attempted, as required.

Implementing and reviewing the missing composition and failure-state path is a distinct substantial unit. Until it exists and owner-run visual checks pass, the renderer cannot establish whether the requested live material is feasible; do not switch production behavior. The headless test proves the convolution shader on WARP fixtures only, not hardware performance, capture correctness or rendered glass appearance. The fixture currently does not export an image and does not render a separate sharp glyph layer.

## Changed paths

- `tip/renderer/main.cpp`
- `tip/renderer/surfaces.h`, `tip/renderer/surfaces.cpp`
- `tip/renderer/capture.h`, `tip/renderer/capture.cpp`
- `tip/renderer/material.h`, `tip/renderer/material.cpp`
- `tip/renderer/blur.hlsl`, `tip/renderer/build.cmd`, `tip/renderer/test.cmd`
- `tip/renderer/tests/test_blur.cpp`

`docs/roadmap/roadmap.js` was updated through `python scripts/roadmap.py start M10.14`; task remains doing. Other currently untracked/modified paths in the worktree are unrelated and were left untouched.
