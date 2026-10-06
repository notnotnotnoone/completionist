# Glass spike and production comparison

The reference is `tip/glass-spike/glass.hlsl`, with the true Gaussian blur in
`tip/glass-spike/main.cpp`. The supplied photos show different backdrops and
lighting, so photographs alone cannot establish pixel alignment or color values.
The production review mode uses the actual `HostRenderer`, desktop capture,
material shader, layout, and sharp text compositor on a generated document.

## Findings

| Detail | Spike | Production before this correction | Cause / intended correction |
|---|---|---|---|
| Backdrop position | Samples the pixel underneath the panel | Enlarges the crop by 40 DIP on every side and scales it into the panel | Use the unpadded panel for lens geometry and presentation; blur expands its own sampling region |
| Lens shape | Rounded-box distance gradient, curved rim, almost flat center | Normalized vector toward the center, maximum bend through the face | Adapt the spike's meniscus and signed-distance gradient |
| Bend direction | Follows each edge and corner | Radial oval, with a hard-coded vertical bias | Use the surface normal rather than the center vector |
| Dispersion | Very subtle, follows the edge normal in pixels | Fixed diagonal UV vector, varies with monitor resolution | Use the spike's normal-based dispersion and DIP scaling |
| Reflections | Directional rim reflections blended toward white | Broad horizontal additive band, clipped highlights | Use the spike's blended Fresnel and inner-rim highlights |
| Tint | Surface token, 24% light / 35% dark | Fixed saturated green, 18% | Use Evergreen surface colors with the spike's proportions |
| Text contrast | Theme-matched ink | Light-theme ink even over a dark desktop | Choose the foreground and tint together from the captured local backdrop |
| Selection | Soft magenta wash, dark/light ink | Opaque magenta with white text and an abrupt left stripe | Keep the wash translucent and reserve solid selection for opaque fallback |
| Blur | Gaussian radius 18, sigma 6, normalized inside radius | Normalizes all 73 taps then discards taps outside active radius | Normalize the active kernel and support the same DPI range as presentation |
| Dock geometry | Glass is sampled at its displayed location | Captured pixels are shifted by composition during collapse | Animate the actual rectangle before sampling; keep the collapsed shell visible |
| Dock fade | Shell remains visible | Entire dock fades to zero, including its restore control | Keep shell/control visible while the body collapses |
| Text and origins | Separate aligned origin labels | Origin appended to word, can collide with Tab | Measure origin separately and reserve a shortcut column |
| Labels | Completionist masthead, sample AI/progress and tense | No masthead; real phrase status, connection state, correction marks and tense | Restored the masthead; preserve real state rather than simulated activity |
| Size | Fixed 350×286 menu and 190×78 dock | Dynamic 330-DIP menu, 54-DIP dock, font/width preferences | Preserve production sizing and preferences; scale material by DIP |
| Row order | Words, then simulated AI phrase | Phrase/status first, matching production keyboard behavior | Preserve product behavior and identify this as an intentional difference |
| Typeface | Segoe UI glyph atlas | DirectWrite Segoe UI, configured font size | Preserve configured size; keep foreground glyphs outside refraction |
| Background source | Simulated static texture | Live captured monitor with rotation and virtual-desktop origin | Keep production capture and fallbacks; never treat the spike as evidence of capture reliability |
| Pointer sheen | Simulated pointer position | None | Adapt a subtle pointer glint using the actual cursor, without moving the backdrop |
| Shadow | Soft shadow outside panel | Hard-clipped shader shadow with no exterior HWND area | Keep the material edge clean; the spike's exterior shadow needs additional window margins |
| Capture exclusion | No captured desktop | Glass HWNDs excluded to prevent feedback | Preserve exclusion; diagnostic PNGs read the final renderer surface instead |
| Text measurement | Static fixture pixels | Physical width passed as DIP, including display scaling | Measure against the actual logical menu width, including width preference; reserve phrase/status shortcut columns |
| Static desktop | Static background remains available | A capture timeout switches to opaque mode despite a valid cached frame | Reuse the last valid frame on timeout; real capture failures still retire it |

## Screenshot loop

Build with `tip\renderer\build.cmd`, then run:

```powershell
tip\renderer\out\CompletionistRenderer.exe --review --capture-dir tip\renderer\out\glass-review
```

This opens a generated document and presents the production material over it.
`D` changes only that document's light/dark backdrop; `C` runs the production
dock toggle path. Drag its title bar to check movement. `Esc` closes it.
`menu.png` and `dock.png` are overwritten when a new
snapshot is presented. Normal `--serve` never writes images. For an explicitly
requested real-app capture, `--serve --capture-dir DIRECTORY` uses the same opt-in
surface export. `menu-backdrop.png` and `dock-backdrop.png` record the matching
unmodified local backdrop crops. The PNGs contain the local background underneath
the popup, and export is disabled in the normally installed service.

Inspect each newly captured PNG after changing one root cause. Confirm that
background landmarks stay in position, the center does not converge, all four
edges bend, text remains crisp, and selection leaves the glass visible. Repeat
on light/dark backdrops and after moving the review window. Only report the
checks actually performed; the photos are visual references, not a numeric
pixel-diff oracle.

## Verified on 2026-10-06

- Final shader and C++ build succeeded with `/W4 /WX`; no native test suite was run.
- Pre-merge engine verification passed 655 of 656 tests. The existing fuzzy
  pipe latency check exceeded its 10 ms p95 limit: 27.58 ms in the full suite
  and 17.89 ms when rerun alone after the renderer build finished. The engine
  tree is identical to local `main`; no engine behavior or test threshold was
  changed. The unresolved performance failure is tracked as
  `fuzzy-pipe-latency-investigation` in the roadmap.
- The actual production renderer was inspected through the generated review
  window at the current 144 DPI (150% display scale), on light and dark backdrops.
- The initial production screenshot reproduced the wrong scale/position and
  solid selection. The crop-only build corrected the background alignment before
  the material changes were applied.
- Final screenshots show neutral glass, separate origin/Tab columns, a soft
  selection, sharp text, and the restored masthead. The center no longer uses
  radial convergence; the refraction and rim equations follow the spike.
- Moving the generated document preserved an identical backdrop crop: mean
  difference 0 in all four channels, with no changed pixels.
- Dock collapse changed its screenshot from 285×81 to 285×48 pixels. The shell,
  connection label, and plus control stayed visible; restoration returned the
  tense line and minus control.
- The verified executable was copied to `tip/out/CompletionistRenderer.exe` and
  restarted with normal `--serve`, with identical build/installed SHA-256 hashes.
  The previous executable is preserved as
  `tip/out/CompletionistRenderer.before-glass-parity-20261006.exe`.

Images are under `tip/renderer/out/glass-review-final/`, with baseline captures
in `glass-review-before/` and crop-only captures in `glass-review-aligned/`.
These generated directories are ignored by Git.

The exterior shadow, fixed spike dimensions, simulated progress, and sample row
order are deliberately not copied. An exterior shadow requires additional HWND
margins and interaction geometry; the current correction keeps the actual
backdrop rectangle and caret layout aligned. Rotation, other display scales,
high-contrast/HDR fallbacks, and real-app typing were not exercised in this visual
pass. Those limits do not change which renderer executable is installed.
