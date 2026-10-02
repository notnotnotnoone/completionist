# completionist-engine

The background process behind Completionist. It serves word completions and phrase continuations to the TSF text service over the named pipe `\\.\pipe\completionist-engine`, learns your vocabulary, and keeps usage numbers. See the [PRD](https://github.com/notnotnotnoone/typer/issues/1).

```bash
uv sync
uv run pytest
uv run completionist-engine                         # serve, with the tray icon and pause hotkey
uv run completionist-engine --no-tray               # serve without them (tests, servers)
uv run completionist-probe "I'd like to recomm"     # ask a running engine, without the DLL
uv run completionist-stats --days 7                 # what Completionist has saved you, and how the phrase provider is doing
uv run completionist-build-ngrams corpus.txt        # build the n-gram file (see below)
```

Normally `..\scripts\install.ps1` starts it at logon, so you don't run it by hand.

## How words are ranked

1. **Frequency.** `wordfreq` gives the base vocabulary (~146k words), ranked Zipf-style.
2. **Context.** With `ngrams.sqlite` in the data folder, bigram and trigram counts re-rank by the previous one or two words ("I'd like to" -> "know" over "knowledge"). The same corpus stats drop misspellings and surnames from the rare end of the vocabulary.
3. **Habits.** The personal store boosts words and word pairs you use, and adds words outside the dictionary (names, slang) after 3 uses. It holds counts of single words and word pairs only, never text. Only real typing counts (one character at a time); pasted text, caret jumps and backspacing do not. Nothing is learned in password/URL/email/number fields or block-listed apps.

## How phrases work

A phrase is a continuation of your sentence from a cheap cloud completion model, streamed into the popup's top row.

- **Provider:** OpenRouter (`https://openrouter.ai/api/v1`), key in `[phrase] api_key`. The default model is `meta-llama/llama-3.3-70b-instruct`, preferring the Groq provider. Name other models in `[phrase] models = ["a/model", "b/model"]`; the first is used, and the next only if it fails. Any model that accepts plain completions works. `provider_order = ["Groq", "DeepInfra"]` lists the OpenRouter providers (the companies hosting the model) to try first, in order; OpenRouter still falls back to the others if they fail, and `provider_order = []` leaves the choice to OpenRouter.
- **Context sent:** up to 8,000 characters before the caret (and 2,000 after, only if `fim = true`), cut at paragraph/sentence boundaries so the provider's prefix cache hits.
- **Text after the caret:** OpenRouter has no fill-in-the-middle, so the model sees only the text before the caret, and phrases are held back while other text follows the caret on the same line. A provider that does take a `suffix` can be used by setting `fim = true`.
- **When:** in apps listed under `[apps] allow`, a phrase is requested 350 ms after you pause. Everywhere else, only when you press Ctrl+Space.
- **Cost:** Completionist does not count spend. Set a limit on the key in OpenRouter. Three provider failures in a row (a spent limit shows up as HTTP errors) pause phrases for 30 s.
- **Privacy:** nothing is sent from password fields or block-listed apps. Text goes only to the provider you configured.
- **Key:** `api_key` under `[phrase]` in the config file, as plain text. It is never logged or shown in errors. With no key, phrases stay off and words still work.

## Tray, hotkey, stats and logs

- **Tray icon:** pause/resume, open the viewer, show stats, open the settings file, open the log folder, quit. Blue when on, grey when paused.
- **Viewer:** the tray's "Open viewer" opens a page served by the engine on `127.0.0.1` (a port chosen at each start). The address carries a secret token held in memory only; requests without it, with another Host, or from another website are refused. The page lists learned words and three-word phrases (each with a two-step Remove), stats by day and app, and a settings form that edits `config.toml` line by line, keeping your comments. The phrase key can be typed there but is write-only: the page only learns whether one is set. It is off with `--no-tray`.
- **Pause hotkey:** Ctrl+Alt+P by default (`[hotkeys] pause`, or `""` to turn it off). While paused, replies are empty and nothing is learned.
- **Stats:** `metrics.sqlite` holds counts and timings only (shown, accepted, keystrokes saved, provider latency per day, app and provider), never typed text. `completionist-stats` prints a summary.
- **Logs:** `engine.log` (rotating). Requests slower than 25 ms and provider errors are logged.
- **Hot reload:** edits to `config.toml` apply within a couple of seconds. A bad edit keeps the old settings and logs why. Changing `[data] dir` needs a restart.

## Data files

All live in `%LOCALAPPDATA%\Completionist` (change with `[data] dir` in the config).

| File | Made by | What |
|---|---|---|
| `ngrams.sqlite` | `completionist-build-ngrams` | Word counts and pruned bigram/trigram tables from a text corpus. Optional. |
| `personal.sqlite` | the engine | Your word and word-pair counts. Saved every 10 s and on exit. |
| `metrics.sqlite` | the engine | Usage counts and timings. |
| `engine.log` | the engine | Log. |

Build the n-gram file from any plain text (`.txt`, `.gz`, or folders of them):

```bash
uv run completionist-build-ngrams corpus1.txt corpus2.txt
```

The current file was built from WikiText-103 (81M words): 100k-word vocabulary, 0.8M bigrams, 1.6M trigrams, 66 MB, ~6.5 minutes. It is encyclopedic English, so it helps most on formal text; a chat corpus would suit Discord better.

## Config

The viewer now groups everyday controls separately from collapsible Advanced tiles. See [the settings guide](../docs/settings.md) for routing, writing, timing, per-app profiles, privacy and popup controls.

`%APPDATA%\Completionist\config.toml`, all optional (the tray's "Open settings file" creates a commented template):

```toml
[apps]
block = ["code.exe"]        # no suggestions at all
allow = ["obsidian.exe"]    # phrases appear on their own here

[words]
limit = 5

[learning]
enabled = true
promote_after = 3

[phrase]
enabled = true
base_url = "https://openrouter.ai/api/v1"
models = ["meta-llama/llama-3.3-70b-instruct"]
provider_order = ["Groq"]   # OpenRouter providers to try first; [] lets OpenRouter choose
api_key = "sk-or-..."       # your OpenRouter key (plain text in this file)
fim = false
max_tokens = 40
temperature = 0.2
timeout = 4.0
debounce_ms = 350
context_before = 6000
context_after = 2000

[hotkeys]
pause = "ctrl+alt+p"

[data]
dir = "C:/somewhere"
```
