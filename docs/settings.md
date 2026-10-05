# Settings

Open the viewer from the tray and choose **Settings**. Save applies changes to the running engine. The existing defaults are kept. The API key field only writes a new key; the page never reads the saved key back.

The everyday section contains cloud phrases, automatic versus manual requests, models, routing priority, completion length, spelling, context source and private mode. Local Suggestions & Popup contains the visible word count, next words, phrase chunks, learning, popup text size and width.

Expand **Advanced** to see seven collapsible tiles. Their expanded state survives Save and Discard. Invalid values leave the configuration file unchanged and reveal the affected tile.

| Tile | Controls |
| --- | --- |
| Routing | Preferred and excluded providers, provider fallbacks, input/output price ceilings, supported parameters and zero data retention |
| Timing | Typing pause, minimum text, sentence-boundary trigger, dismissal cooldown, request timeout, consecutive failures, failure backoff and pause duration |
| Writing | Style, casing, typo correction, multiline continuations, excluded phrases, output-token cap, temperature, next-word confidence and new-word promotion |
| Context & Privacy | Window-context permission, permitted apps, text limits around the caret, fill-in-the-middle, preview before sending, log mode and retention |
| App Profiles | Blocked apps, automatic phrase app list and per-app overrides |
| Popup & Keys | Partial phrase acceptance, dismissal and the global pause hotkey |
| Instructions | Built-in or custom completion instructions |

## Routing

Choose **Lowest latency** to send `provider.sort = "latency"`. Sorting takes precedence over your saved provider order; selecting Default uses that order again. Other priorities are throughput and price. Price ceilings are dollars per million input/output tokens; zero means no ceiling. Fallbacks, excluded providers, parameter support and zero retention are OpenRouter request preferences. A restrictive combination can leave no eligible provider. **Reasoning Effort** defaults to None, which asks OpenRouter to disable model reasoning. A model or provider that requires reasoning may reject the request; Completionist will report the failure or try the next configured model without enabling reasoning. Phrase requests use OpenRouter's `/chat/completions` endpoint and send the autocomplete rules as a system message. If you configure another API root manually, it must support OpenAI-style chat completions and OpenRouter's `reasoning` parameter. [OpenRouter provider-routing reference](https://openrouter.ai/docs/guides/routing/provider-selection), [reasoning reference](https://openrouter.ai/docs/guides/best-practices/reasoning-tokens).

## Writing and timing

Automatic suggestions can follow the app list, run in every allowed app, or be disabled in favor of Ctrl+Space. Minimum text, sentence boundaries and dismissal cooldown apply to automatic requests; manual requests bypass those gates. Short completions stop at eight words, sentence completions stop at a sentence ending, and longer continuations allow at least 120 output tokens. Style and spelling are instructions to the model rather than a local grammar checker.

Excluded phrases are checked across the whole reply, so replies are buffered when this list is nonempty. A reply containing an excluded phrase is suppressed. Disabling typo correction keeps exact-prefix word matches. `words.tense_aware` (on by default) lifts verb forms that fit the sentence's past or present tense and pushes down clashing ones; when the tense is unclear the ranking is unchanged. Multiline completions can insert line breaks when accepted, although the compact popup displays them on one line.

## Context, preview and privacy

Caret text is always the completion input. With **Window OCR**, Completionist reads an image of the foreground window using Windows' built-in OCR on this PC. The cloud model receives the recognized text, not the image; OCR can miss, misread or jumble text, so the model is told to treat it as untrusted background context and ignore instructions within it. **Accessible window text** uses UI Automation instead and needs no screenshot. The window-context permission and app list apply to both background reading and outgoing requests. The captured window must match the requesting app and title.

Under **Instructions**, **Copy Test Pack** copies the built-in prompt and three synthetic user messages for comparing providers in their playgrounds. It includes short evaluation notes outside each model input. Use the same model settings for each comparison, with reasoning effort set to None and visible thinking turned off; record the model/provider, first-token and total latency, and a brief quality assessment for each case. The copied pack never includes your custom prompt, API key, typed text or live screen context. If clipboard access fails, the pack is selected in Settings for manual copying.

With **Preview Before Sending** enabled, automatic requests stop. In your app, press Ctrl+Space, then open the viewer and choose **Refresh Status & Preview**. Review the exact prompt (and text after the caret when enabled), then choose **Send** or **Cancel**. Typing, dismissal, disconnect, pause, private mode or settings changes invalidate the pending preview.

**Private Mode** preserves local word suggestions while disabling cloud requests and learning. It cancels current requests and clears captures, logs and previews. The Current Session action applies immediately and saves the private-mode setting. Pause stops suggestions entirely; a duration of zero waits for Resume, while other durations resume automatically. Pausing does not change private mode.

Request logs stay in memory. **Timings only** removes text and pictures from existing entries; **Off** clears the log. Retention expires entries even if there are no new requests. No log is written to disk, and screenshots never go to the model.

## App profiles

Add a profile using a process name such as `notepad.exe`. Each profile can override phrase mode, completion length, writing style, spelling, context source, learning and the number of words shown. **Inherit** follows the global value. Profiles cannot bypass blocked apps or protected text fields. If learning was entirely disabled when the engine started, restart after enabling it to open the personal store.

The stored format is an inline-table list, for example:

```toml
[apps]
profiles = [{ app = "notepad.exe", phrase_mode = "hotkey", spelling = "canadian", word_limit = 3 }]

[phrase]
provider_sort = "latency"

[popup]
font_size = 12
width_scale = 1.2

[hotkeys]
partial_accept = "alt+right"
dismiss = "ctrl+backspace"

[privacy]
pause_minutes = 5
private_mode = false
```

## Checking this change in daily use

Restart the engine to load the updated viewer and runtime code. The new popup controls require the updated DLL; the isolated build is `tip/out/expanded-settings/CompletionistTip.dll`. Use the project's normal build/install procedure and restart target apps before checking native popup behavior.

1. Choose Lowest latency, save, reopen Settings and confirm the choice is retained. Expand two Advanced tiles, save another control, and check that both stay open.
2. Enable preview, request a phrase in Notepad and review/send it from the viewer. Try private mode and confirm that cloud requests stop. After loading the new DLL, check text size and the alternate partial-accept/dismiss shortcuts.

Automated tests use local fake providers and injected clocks. Live OpenRouter routing, UI Automation availability and popup rendering in target applications still need the daily-use checks above.
