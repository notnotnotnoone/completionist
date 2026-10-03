# V2 autocomplete menu — prototype and proposed design

Date: 2026-10-02. Status: design study for owner review, not an approved implementation spec.

## Current prototype: compact status and visible liquid glass

The owner approved a further simplification: the separate bottom-left surface is now a 190 px compact dock with a small connection light and a 9/11 px collapsible tense row. There are no explanatory paragraphs, context receipt or separate next-word display in it. The context receipt belongs to the viewer, which is outside this change. Tense remains manually simulated for V2.1. The connection light softly blinks while connected; offline is an unfilled ring with an accessible label, and reduced motion disables blinking.

The caret menu's AI section now owns the idle countdown and request activity. It uses short labels (Idle, Working, Receiving, Ready, Manual, Offline), a remaining-seconds timer and a thin idle track, replacing the former verbose corner-panel activity cards. Pending height is 76 logical px so arrival does not push word targets around. Suggestion origins, correction comparisons and underlining of the next partial-accept segment remain. Keyboard behavior is unchanged.

Palette was checked directly against `engine/src/completionist_engine/viewer.html` and `CONTEXT.md`: the same Evergreen pine-green, magenta selection, pale green light surfaces and deep green dark surfaces come from `docs/roadmap/theme.css`. The prior 86% opaque fill and 18 px blur concealed the glass. The current shell uses a 24% surface tint, a positive backdrop layer with SVG displacement and 2 px blur, sharp content above it, and a specular edge layer. Ruled paper beneath the menu provides visible detail for refraction. No new palette hex values were introduced. Opaque surfaces remain the unsupported-browser fallback. Computed-style inspection confirms the translucent fill and active backdrop refraction; native rendering and final visual approval remain unverified.

All sections below this point record previous iterations and native implementation considerations. This section supersedes their old panel contents, glass opacity, pending height and footprint.

## Revised direction: liquid glass with separate information panel

The owner subsequently requested a maximalist liquid-glass treatment based on the attached glass component guide, and authorized its prototype implementation. Crisp foreground text sits over layered translucent material, frosted tint, beveled highlights and restrained background refraction. The information panel is separate from the caret menu and anchored to the bottom-left of the browser viewport as the simulated screen, with a minimized state. This supersedes the earlier proposal to put information beside or beneath the caret menu. The prototype now implements this direction; the three original variants are historical proposals, and old variant links resolve to the new layout.

The lightweight caret menu retains suggestions, next-word acceptance preview, suggestion origin and correction comparison. Insertion preview was explicitly rejected. The separate information panel contains idle-trigger countdown, trigger explanation, AI request activity, context receipt, connection state and tense display. Tense is simulated in V2; real detection belongs to V2.1, with no detector or ranking changes in this prototype.

Minimizing the information panel leaves a compact status strip and a restore control at the bottom-left anchor. Expanded details should reveal with a pleasant short opacity/transform transition, without moving caret suggestions; reduced motion uses an instant state change. Countdown and request activity are distinct states, and model activity must not imply access to internal reasoning. Adaptive layout now applies separately to the caret menu and the anchored information panel rather than attaching the information panel to the menu.

The minimized strip retains current activity/countdown, selected simulated tense and connection status. An independent 360 px expanded glass body rests above that strip. Its pointer-triggered collapse uses 180 ms opacity and 220 ms transform transitions; keyboard activation, theme switching, adaptive minimization and reduced motion are instant. The activity symbol rotates only during a pending or streaming request and stops when that state ends. The information body is inert when minimized. Short viewports or overlap with the suggestions automatically minimize it; it restores when the constraint clears unless manually minimized.

The caret menu is 330 logical px wide, with a reserved 88 px two-line continuation area during scheduled requests, simulated origin labels and an explicit correction comparison. The next accepted continuation segment is underlined; there is no insertion preview. A 1.2-second demo idle delay makes the lifecycle easy to inspect; controls also offer the engine's 350 ms default and a two-second delay. Typing resets the schedule. A fixed 950 ms simulated request wait precedes streaming, with no claim to measure actual model reasoning. Manual mode requires Ctrl+Space, offline mode retains local words, and paused fixtures run no countdown. Native telemetry, interaction/focus behavior and glass rendering need a separate implementation plan.

The prototype remains plain HTML/CSS/JavaScript: the supplied React component is a visual reference, not a reason to introduce React, Tailwind or shadcn into the native product. The SVG filter is adapted with low displacement and applied only to a backdrop layer. Existing Evergreen tokens drive both themes. Below are the original alternatives and native requirements retained as design history; their older shelf dimensions and layout selection do not describe the current prototype.

Verification: headless Chrome passed the idle/request/stream/ready lifecycle, manual requests, offline local suggestions, simulated tense, origin labels, correction replacement, next-word acceptance, after-space Tab pass-through, explicit word selection across phrase arrival, countdown reset after typing, unchanged menu placement during manual minimization, inert collapsed details, automatic minimize/restore and retained manual preference. Responsive checks covered three viewport widths, three edge positions and three preview scales; normal-placement checks also confirmed caret clearance. Reduced-motion checks confirmed no activity rotation or panel transition. JavaScript syntax, roadmap validation and whitespace checks pass. These are browser checks, not native Windows tests or a visual approval from the owner.

## Intent and scope

The owner wants an extremely polished autocomplete menu and chose **rich and expressive** as its visual character. Start V2 with a runnable, throwaway prototype; use it to settle the menu before implementing native drawing. This advances the popup work ahead of the dashboard's previously recorded priority. This study covers the autocomplete surface only. Dashboard and tray redesign are separate work.

Prototype: `tip/prototype/index.html`. Open directly from disk; no server or build. `?variant=A`, `?variant=B`, and `?variant=C` select layouts. The lead proposal is **B, Phrase shelf**. Its hierarchy gives sentence continuations room while keeping the word list precise. A is the compact baseline; C explores a split preview at the cost of more document coverage. No layout has been selected by the owner yet.

## Existing behavior audited in source

`tip/src/popup.cpp` draws a rectangular, dark list using GDI, a blue highlight and amber guessed letters. Width is measured from content; phrases are capped at 520 logical pixels before the configurable width multiplier. The window stays on the caret's monitor and opens above when necessary. It never takes focus and is click-through (`popup.h`); mouse selection is therefore not part of this proposal.

`tip/src/popup_model.h` owns keyboard behavior separately from drawing. Phrase rows sit above words. A phrase arriving alongside words has a 150 ms guard before becoming the default selection; an explicit arrow choice prevents automatic takeover. A phrase-only popup selects its phrase immediately. After-space word suggestions open without a selection, preserving Tab until an arrow chooses a row. Up/Down wrap. Enter always passes through. Modified keys pass through except configured actions. A stale reply consumes nothing. Existing configurable dismissal and partial-accept shortcuts must be preserved; hints must reflect actual settings.

This is a source audit. No live app audit, native DPI verification, accessibility measurement, or TSF harness run has been performed during this design study.

## Visual directions

| Direction | Structure | Benefit | Cost |
|---|---|---|---|
| A — Quiet list | One 292 px column, phrase separated by a rule | Small footprint, quick scanning | Long phrases truncate early; least expressive |
| B — Phrase shelf | 356 px surface, wrapped continuation above a compact list | Strong hierarchy, fuller continuation, generous but controlled | More space than A; shelf arrival changes height |
| C — Context rail | 450 px split view, list left and selected preview right | Long selected content is easier to inspect | Largest footprint; more visual attention and implementation complexity |

All widths are prototype defaults at 96 DPI, not fixed physical pixel sizes. Do not offer these as three production settings. Choose and refine one design.

## Lead direction: Phrase shelf

The surface uses Evergreen `--surface`, `--line-strong`, `--ink` and `--muted`. Selection uses `--accent-soft` with a short `--accent-ink` rail and a Tab hint. The rail provides a shape cue alongside color. No highway colors or new palette values are introduced. The prototype imports `docs/roadmap/theme.css` instead of copying its tokens. Light and dark are explicit parallel palettes.

Use Overpass for menu text, Overpass Mono for shortcuts and small labels. The browser loads the official font families and falls back to Segoe UI and monospace offline. Fraunces belongs to the surrounding design-study page, not candidate text. Native delivery must bundle or privately load the chosen UI fonts; do not depend on a network request or install fonts system-wide. This requires a feasibility check before promising identical browser/native metrics.

Proposed dimensions in logical pixels:

| Element | Proposal |
|---|---|
| Menu width | 356, clamped to the monitor work area |
| Menu corner radius | 12 |
| Word row | 29 minimum height; 5 px vertical and 9 px horizontal padding |
| Word list inset | 5 |
| Candidate text | 13 px default in the study; honor configured font size in production |
| Phrase shelf | 116 px reserved height; 14 px vertical / 15 px horizontal padding |
| Shelf label | 9 px mono, “CONTINUE THE THOUGHT” |
| Phrase text | 14 px, 1.55 line height, up to three lines |
| Footer | 9 px type, 8 px vertical / 14 px horizontal padding |
| Caret gap | 6 |
| Border | 1 logical px, snapped for crisp device rendering |

The typed prefix has stronger weight and muted color; when selected it uses accent ink. The continuation remains ordinary readable text, not low-opacity ghost text. Keep the Tab hint on the selected row only. Corrections use dotted underlines on the guessed character positions plus the word “correction”; never depend on an amber color to identify them. Draw exact engine-provided marks, not an inferred edit distance.

Width stays fixed within a suggestion session. The phrase shelf reserves its three-line height when a request is actually pending, then replaces the waiting message in place. Word targets stay at the same screen coordinates throughout streaming. The prototype implements this reservation using its fixture's pending state. Production needs an explicit pending-state signal; current `PopupContent` only exposes phrase text. Word-only sessions have no shelf or reserved gap.

Above-caret layout needs special care: expanding content must grow away from the text, and an explicitly selected row should remain at a stable screen position. Measure and position the whole surface before painting, rather than moving individual rows through an animation. Any change needed to pin the selected word must remain in layout and not alter key routing.

Long shelf text must finish with a real ellipsis at the three-line boundary in native drawing. Accept always inserts the original full suggestion, not the ellipsized display. The browser study uses a three-line clamp with an ellipsis. C shows the longer text in its side preview.

## Interaction and state coverage

| Moment | Required production behavior | Study coverage |
|---|---|---|
| Typed prefix | Top word selected; Tab completes the word | Fixed scenarios and small in-memory vocabulary |
| Phrase requested | Words remain usable, no animated loading indicator | Simulated 650 ms arrival; no provider connection |
| First phrase text | Retain word target for 150 ms; do not overwrite an explicit choice | Simulated guard and streaming |
| Phrase selected | Tab accepts full phrase; configured partial shortcut takes next word | Tab and default Ctrl+Right simulated |
| After a space | No selection; Tab remains the app's | After-space fixture and typing |
| Correction | Show guessed positions and readable correction cue | One representative fixture |
| Arrow choice | Wrap through phrase then words; no automatic takeover | Simulated |
| Escape | Dismiss until the next word | Simulated |
| No matches | No popup; no empty menu or warning | Fixture and typing |
| Paused or silent | No popup and no interception | Paused fixture |
| Focus/caret changed | Hide immediately or recompute; never accept stale content | Must be validated natively; browser study uses last measured caret |
| Provider failure | Keep available local words; no error toast in typing flow | Native design requirement, not simulated |
| Near screen edge | Clamp to work area, flip above caret; do not overlap caret | Simulated within note window |
| Scale or monitor change | Measure using the active monitor's DPI; preserve readable text and bounds | CSS magnification only; native testing still required |

No mouse interaction in the menu. It remains attached to typing, while the editor owns focus. The bottom direction switcher, scenario controls and live-state panel belong only to the study. No branding, status dashboard, extra settings or confidence scores enter the production menu.

## Motion and accessibility

Menu entry, selection, dismissal and keyboard-driven updates are instant: autocomplete repeats hundreds of times a day, so an entrance animation would introduce repeated friction. Text streams without a pulsing shimmer, typing animation, size spring, or caret slide. Keep the shell steady through redraws. The study's occasional pointer control feedback uses a 160 ms background-color transition with `cubic-bezier(.22,1,.36,1)`; keyboard input, theme switching and reduced motion disable it. No layout property is animated.

The owner explicitly requested the frontend-design and ui-animation skills. The design pass kept the existing Evergreen palette (surface #FBFCF8 / #141C18; ink #16221C / #E4EBE6; sign #0A5A3D / #0F6B48; accent #AB3F84 / #C0188D), and retained Overpass, Fraunces and Overpass Mono in their established roles. The signature is the reserved continuation shelf, with larger phrase typography above precise word rows. The surrounding page was simplified to put that interactive menu first, and arbitrary numbered decoration was removed.

| Before | After | Why |
|---|---|---|
| Phrase arrival adds a shelf and shifts words | Pending shelf reserves 116 px from request start | Keep keyboard targets visually stable |
| Replayed menu fades in with a keyframe | Menu appears instantly | This is a high-frequency typing interaction |
| Same typography across phrase and words | Phrase gets 14 px type and more leading | Give the continuation a distinct, expressive hierarchy |
| Generic two-line headline and numbered principles | Direct “Finish the thought” title and unnumbered principles | Put the actual typing task ahead of decoration |

Readable text uses the existing Evergreen ink pairs. The palette's prior validation is not a substitute for checking every final combination at the menu's actual small sizes. Validate contrast for ink/surface, muted/surface, ink/accent-soft and accent-ink/accent-soft in both themes. Provide a Windows high-contrast mapping using system colors. Keep selection visible through its rail and text emphasis as well as fill.

The browser study exposes options and active selection for inspection. Native screen-reader compatibility is a release requirement: expose suggestion names and selection without stealing the host application's focus, and avoid announcing every streamed fragment. Determine the suitable TSF/UI Automation integration in the implementation plan. HTML semantics do not prove native accessibility.

## Implementation boundary and verification

Do not promote prototype code into the DLL. Keep `PopupModel` as the behavior authority. Separate palette/font metrics, content layout and drawing inside the native implementation. Consume the current wire content and configuration; add pending-state data only if stable shelf reservation is selected. Favor the smallest drawing change that provides clear text, rounded surfaces and stable repainting. Evaluate DirectWrite/Direct2D versus retaining GDI against these requirements; the prototype does not select a renderer.

The DLL executes in host applications. Native drawing or font failures must fall back to the existing functioning popup, with no exception crossing a COM boundary. Layout work must avoid blocking pipe activity or the host UI. Preserve font-size/width controls and configurable key hints; do not quietly replace them with prototype defaults.

Before release: keep key-router behavior tests passing; check layout boundaries for short/long content, phrase-only, after-space, marks, empty results, and changed selection. Manually inspect Notepad, Chromium and Electron at 100%, 150% and 200%, on both monitors and at bottom/right edges. Check both themes, high contrast, reduced motion, font failure and stale replies. Run the focus-stealing TSF harness only when the owner has said they are away.

## Review and next step

Ask the owner to try B and compare A/C. Settle structure, density, maximum phrase height, and whether the shelf label adds value. Then revise this document into the approved design and write the native implementation plan. Popup scope task M10.1 remains doing; drawing task M10.2 is not complete. No release is declared for an unapproved design study.

Once the design is approved, retain its decisions in the spec and remove or archive losing prototype variants. Remove the prototype when the native result supersedes it.
