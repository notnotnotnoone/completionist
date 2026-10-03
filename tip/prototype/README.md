# V2 autocomplete design study — throwaway

Question: how can Completionist's caret menu feel rich, expressive and extremely polished without interrupting typing?

Open `index.html` directly in a browser. From the repository root on Windows:

```powershell
Start-Process tip/prototype/index.html
```

No build or server. This now shows the selected **liquid-glass direction**, replacing the earlier three layout variants. Old `?variant=` links open this same revised prototype.

Latest revision: the bottom-left panel is a compact 190 px dock with a blinking connection light and small collapsible tense row. No descriptions, context receipt or duplicate next-word display. Countdown and AI activity now live directly in the caret menu's AI section. Its lower-opacity material, light blur, refraction and beveled edges reveal the paper beneath. Colors match the actual viewer's Evergreen palette.

Try these:

1. Click the typing text. Use Up/Down and Tab; replay to restore it.
2. Choose “A sentence continuation”; press Down before it arrives to keep your word choice. The caret menu's AI section counts down, shows AI working, and follows streamed text.
3. Try “After a space”: Tab passes through until you choose a row.
4. Compare the typo, long phrase, and edge-position examples in both themes.
5. Minimize and expand the fixed information panel. Its glass body fades and settles above a persistent status strip; caret suggestions stay stationary.
6. Choose a simulated tense, offline connection, or manual trigger. Ctrl+Space requests a phrase in manual mode.

The menu itself is click-through, matching the native popup. Ctrl+Right partially accepts a phrase; its next segment is underlined. Enter inserts a newline as usual. Origin labels, learned vocabulary, connection and AI activity are all fixtures. Nothing is sent to an engine or saved. Tense is manually simulated; real detection is planned for V2.1. External Google Fonts are optional; offline, system fonts replace them.

Scale controls magnify CSS in the simulated note window. They do not verify Windows DPI behavior. Custom typing uses a small fixture vocabulary. At very narrow window sizes the zoom is constrained to keep the preview usable.

The compact glass continuation now uses a two-line ellipsis. Editing resets the countdown and starts a new sample continuation where the vocabulary permits one. The information panel is anchored to the browser viewport, independently of the caret menu; it automatically minimizes on short viewports or when its expanded body would overlap suggestions. It restores after that constraint clears unless manually minimized. The collapse transition uses only opacity and transform, and reduced motion or keyboard activation uses an instant change. The glass SVG distorts only the backdrop, never the text; opaque surfaces are the fallback where backdrop filtering is unsupported.

Design proposal: `docs/superpowers/specs/2026-10-02-v2-autocomplete-design.md`.

Verdict: owner selected liquid glass, a separate bottom-left minimizable information panel, and a lightweight caret menu. This implementation is for hands-on visual review. Native rendering and engine telemetry remain separate work. This directory is isolated from the C++ build and must not ship with production assets.
