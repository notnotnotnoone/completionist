# Liquid glass — throwaway native spike

Question: can a native shader reproduce the compact prototype's lens-like material with readable autocomplete text?

Run `tip\glass-spike\run.cmd` from the repository root. Requires the existing VS 2022 C++ build tools. No install, registration, Python, Electron, third-party library or engine connection.

- Drag the menu across the sample document to inspect refraction.
- Mouse wheel adjusts lens strength (0–60, default 18).
- **A** switches between liquid glass and a flat frosted comparison.
- **D** switches Evergreen light/dark colors.
- **M** pauses/resumes the simulated activity bar. Windows reduced-motion preference is respected at launch.
- Click the bottom-left dock to minimize/expand. **Esc** closes.

The D3D11/HLSL material uses a rounded-rectangle distance field, a curved meniscus displacement, subtle RGB dispersion, lightly blurred backdrop samples, paired specular highlights, rim shading, and pointer-responsive reflections. Foreground glyphs are sampled separately so text does not refract. Shader colors derive from Evergreen tokens.

**Simulation:** backdrop is a document drawn into this app's own texture, never a desktop capture. AI timer/origins/tense are fixtures. No TSF or production renderer changed. This probe does not establish desktop-capture reliability, multi-monitor behavior, production performance or final accessibility. Dock collapse is immediate in this material probe; production collapse motion remains separate.

Build then run `out\glass-spike.exe --export` (also `--dark` or `--frost`) to render a BMP without showing a window. Export reads only the app's own render target. Hardware D3D11 uses WARP software rendering if device creation fails.

Owner review: the first version works, but needs more polish and softness. The second pass broadens the rounded edge, uses a gentler Gaussian-profile shadow, softens the backdrop with nine weighted samples, reduces refraction/dispersion, and blends broader reflections rather than clipping them to white. Text remains sharp; selection, dividers, activity dots and progress edges are softened. Revised visual review remains pending. Keep the validated material decision; discard or deliberately adapt this code after the spike.

Third pass: owner rejected the sparse-sample background as pixelized. Replaced it with a true separable Gaussian convolution (sigma 6 px, radius 18 px) computed once on the controlled scene at startup/theme changes. Lens and dispersion now sample that blurred texture exclusively; foreground glyphs remain on their separate sharp texture. This CPU fixture precomputation is for the static throwaway spike; a live production backdrop would need GPU blur passes.
