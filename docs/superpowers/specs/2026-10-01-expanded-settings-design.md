# Expanded settings

The user approved all 30 settings from the brainstorm and asked for niche settings in Advanced, grouped into collapsible tiles. Preserve existing defaults and API-key secrecy. Every setting must affect behavior, persist, validate and reload. The speculative separate classifier is outside this list.

## Contract

Extend ProviderSettings with provider_sort (empty, latency, throughput, price; default empty), allow_fallbacks (true), provider_ignore (empty tuple), max_price_input/max_price_output (0 means no cap, dollars per million tokens), require_parameters (false), zdr (false). Send OpenRouter provider preferences. Explicit sorting overrides the preferred provider order, retaining that saved order for switching back.

Extend PhraseConfig with mode (apps/auto/hotkey; apps default), min_chars (0), trigger (pause/sentence; pause default), dismiss_cooldown (0 seconds), failure_limit (3), failure_pause (30 seconds), completion_length (default/short/sentence/long), writing_style (match/neutral/casual/professional), spelling (auto/canadian/american/british), preserve_casing (true), multiline (false), avoid_phrases (empty tuple), context_source (ocr/caret/accessible; ocr default), context_apps (empty frozenset means unrestricted except blocked apps), preview_context (false), log_mode (full/timings/off; full default), log_retention_minutes (0 means until exit).

Config adds typo_correction (true; stored words.typo_correction), PopupConfig font_size (9 points, 7..24), width_scale (1, .5..2), hotkeys.partial_accept (ctrl+right/alt+right/ctrl+tab), hotkeys.dismiss (escape/ctrl+backspace/alt+backspace), privacy.private_mode (false), privacy.pause_minutes (0 indefinite), apps.profiles (list of inline TOML tables). Profiles identify app and optionally set phrase_mode, completion_length, writing_style, spelling, context_source, learning (boolean), word_limit (1..20). Omitted values inherit global settings. Profiles never override blocked/silent-field protections.

Everyday controls: phrases enabled/mode, routing priority, completion length, spelling, context source, popup size/text, visible words and private mode. Advanced tiles: Routing, Timing, Writing, Context & Privacy, App Profiles, Popup & Keys, Instructions. Use native details/summary, semantic labels and descriptions, save/discard all values, preserve expanded state through saving and reveal all collapsed ancestors on validation errors. App profiles use an accessible row editor, not raw JSON.

Manual context preview holds the exact outgoing prompt locally, exposes it in the token-protected viewer and sends only on explicit Send. Editing, dismissal, disconnect, private mode or config changes invalidate it. Automatic requests are disabled while preview is enabled. No keys or text enter disk logs. Log privacy changes erase existing text/captures immediately. Retention expires on reads as well as writes.

Accessible context uses local Windows UI Automation instead of OCR with safe empty fallback; context restrictions apply before capture and before sending. Private mode disables learning/cloud and clears captures/logs/previews. Timed pause starts from tray/hotkey toggle and automatically resumes; private mode persists separately.

Native popup/font/shortcut changes travel as optional popup settings on words replies, with bounds checked on both ends and compatibility with older DLLs. Native tests can run; the focus-stealing TSF harness cannot run while the user is working.

Writing preferences are instructions, with local enforcement for multiline, sentence/short length and excluded phrases. Typo-correction toggle disables fuzzy candidates before ranking. No new paid services or dependencies.
