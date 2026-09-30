// Completionist roadmap data, read by index.html (the highway map) and tasks.html (the task board).
// Everything after the `=` is strict JSON: double quotes, no comments, no trailing commas.
// Update it with every change (see CLAUDE.md), then run: python scripts/check_roadmap.py
window.COMPLETIONIST_ROADMAP = {
  "schema": 3,
  "project": "Completionist",
  "tagline": "System-wide, VS Code-style English autocomplete for Windows",
  "updated": "2026-09-30",
  "prd": "https://github.com/notnotnotnoone/typer/issues/1",
  "repo": "https://github.com/notnotnotnoone/typer",
  "now": "0.4.0 is released. Next is 0.5.0 (N-gram suggestions and the personal log), the first milestone not yet built.",
  "highways": [
    {
      "id": "engine",
      "title": "Engine",
      "blurb": "Python: words, phrases, learning, metrics",
      "areas": [
        "engine",
        "data",
        "bench",
        "viewer"
      ]
    },
    {
      "id": "tsf",
      "title": "Text service",
      "blurb": "C++ TSF DLL inside every app",
      "areas": [
        "dll"
      ]
    },
    {
      "id": "tooling",
      "title": "Tooling",
      "blurb": "Install, tests, docs and the repo",
      "areas": [
        "install",
        "test",
        "docs",
        "repo",
        "toolchain"
      ]
    },
    {
      "id": "browser",
      "title": "Browser extension",
      "blurb": "Chrome, over Native Messaging",
      "areas": [
        "extension"
      ],
      "opens": "1.0.0"
    }
  ],
  "milestones": [
    {
      "id": "M0",
      "title": "Toolchain + TSF spike",
      "status": "done",
      "goal": "Prove a TSF text service fixes the three failures of the old hook-based attempt: popup position, text sync and key handling.",
      "done_when": "A throwaway TIP shows a popup at the caret, reads the real text and swallows Tab in the must-work apps.",
      "tasks": [
        {
          "id": "M0.1",
          "title": "Install VS 2022 Build Tools (MSVC v143, Windows SDK 10.0.26100)",
          "status": "done",
          "area": "toolchain",
          "stories": [],
          "notes": "CLI-only toolchain, no full Visual Studio IDE."
        },
        {
          "id": "M0.2",
          "title": "Build Microsoft's SampleIME unmodified",
          "status": "done",
          "area": "dll",
          "stories": [],
          "notes": "Link error LNK1295 fixed with `/p:WholeProgramOptimization=false`, retargeted to v143 and SDK 10.0.26100."
        },
        {
          "id": "M0.3",
          "title": "Spike TIP: popup at the caret, context length and input scope logging",
          "status": "done",
          "area": "dll",
          "stories": [
            47
          ],
          "notes": "`GetTextExt` never failed in ~830 inspections. Input scope must be read with `GetAppProperty`, not `GetProperty`.",
          "refs": [
            "tip/spike/CompletionistSpike.cpp",
            "m0-tsf-spike"
          ]
        },
        {
          "id": "M0.4",
          "title": "Spike: swallow Tab and replace the current word with ITfRange::SetText",
          "status": "done",
          "area": "dll",
          "stories": [
            47
          ],
          "notes": "Plain `SetText` worked everywhere; the composition fallback was never needed."
        },
        {
          "id": "M0.5",
          "title": "Manual app matrix, rounds 1 and 2",
          "status": "done",
          "area": "test",
          "stories": [
            47
          ],
          "notes": "Works in Notepad, Discord, Chrome, Edge and Teams, and stays silent in password fields. Discord exposed 2,239 characters before the caret. Google Docs and the Google search box are out of scope."
        },
        {
          "id": "M0.6",
          "title": "Write up spike findings and verdict",
          "status": "done",
          "area": "docs",
          "stories": [
            47
          ],
          "notes": "Verdict: TSF works for Completionist.",
          "refs": [
            "tip/spike/README.md"
          ]
        }
      ]
    },
    {
      "id": "M1",
      "title": "Word completion end-to-end",
      "status": "done",
      "goal": "Type in any supported app and get a word dropdown at the caret, fed by the engine, accepted with Tab.",
      "done_when": "The real DLL shows engine word suggestions at the caret in Notepad, Chrome, Edge and Discord, Tab/Up/Down/Esc behave as specified, Enter is never consumed, and gated fields stay silent.",
      "tasks": [
        {
          "id": "M1.1",
          "title": "Protocol codec: length-prefixed JSON frames",
          "status": "done",
          "area": "engine",
          "stories": [],
          "notes": "4-byte little-endian length, UTF-8 JSON, 1 MB cap. Streaming decoder handles split and merged frames.",
          "refs": [
            "engine/src/completionist_engine/protocol.py",
            "m1-engine-words"
          ]
        },
        {
          "id": "M1.2",
          "title": "Word completer over the wordfreq vocabulary",
          "status": "done",
          "area": "engine",
          "stories": [
            1,
            10,
            12,
            13
          ],
          "notes": "~146k words ranked by frequency. Matches case, excludes the exact typed word, capitalises \"i\". p95 lookup under 5 ms.",
          "refs": [
            "engine/src/completionist_engine/words.py",
            "engine/src/completionist_engine/vocabulary.py"
          ]
        },
        {
          "id": "M1.3",
          "title": "TOML config and policy gating",
          "status": "done",
          "area": "engine",
          "stories": [
            31,
            32,
            33,
            37
          ],
          "notes": "Block-list (editors, IDEs, terminals), silent input scopes (password, URL, email, number), allow-list for automatic phrases. Hot reload is M4.3.",
          "refs": [
            "engine/src/completionist_engine/config.py",
            "engine/src/completionist_engine/policy.py"
          ]
        },
        {
          "id": "M1.4",
          "title": "Named-pipe server, client and probe CLI",
          "status": "done",
          "area": "engine",
          "stories": [
            48,
            49
          ],
          "notes": "Real-pipe integration test asserts round-trip plus lookup p95 < 10 ms. 74 engine tests pass.",
          "refs": [
            "engine/src/completionist_engine/server.py",
            "engine/src/completionist_engine/probe.py"
          ]
        },
        {
          "id": "M1.5",
          "title": "Merge m1-engine-words and m0-tsf-spike into main",
          "status": "done",
          "area": "repo",
          "stories": [],
          "notes": "Fast-forwarded main to the engine branch, then merged the spike branch. Released as v0.0.4.",
          "refs": [
            "main",
            "v0.0.4"
          ]
        },
        {
          "id": "M1.6",
          "title": "TSF service shell from SampleIME: activation, key sink, edit sessions, registration",
          "status": "done",
          "area": "dll",
          "stories": [
            43
          ],
          "notes": "TSF shell written from the spike's proven code: activation, edit sessions, key sink, COM registration for en-US/CA/GB. Lifecycle smoke test (`tip/tests/load_test.ps1`) activates and tears it down 40 times on a real thread manager and the DLL stays unloadable.",
          "refs": [
            "tip/src/tsf_service.cpp",
            "m1-tsf-dll"
          ]
        },
        {
          "id": "M1.7",
          "title": "Context reader: text around the caret, caret rect, input scope, app and title",
          "status": "done",
          "area": "dll",
          "stories": [
            14,
            31,
            32
          ],
          "notes": "Reads up to 2,000 characters before and 500 after the caret, the caret rect, input scope and app/title. Keyboard-disabled and password contexts send nothing. `E_FAIL` from `RequestEditSession` is retried shortly instead of treated as an error.",
          "refs": [
            "tip/src/tsf_service.cpp"
          ]
        },
        {
          "id": "M1.8",
          "title": "Pure popup state + key router with native tests",
          "status": "done",
          "area": "dll",
          "stories": [
            4,
            5,
            6,
            7,
            8,
            9,
            51
          ],
          "notes": "`PopupModel` has no TSF dependency and 14 native tests cover every key in every state (closed, open, stale, modifiers held). Enter is never consumed. The tests also cover the frame codec and JSON reader (15 more).",
          "refs": [
            "tip/src/popup_model.h",
            "tip/tests/test_popup_model.cpp",
            "tip/test.cmd"
          ]
        },
        {
          "id": "M1.9",
          "title": "Engine client on a worker thread",
          "status": "done",
          "area": "dll",
          "stories": [
            34,
            35
          ],
          "notes": "Worker thread with overlapped I/O, request ids, stale replies dropped, reconnect with backoff (250 ms to 2 s), and a check that the engine runs as the same Windows user. `tip/tests/e2e.ps1` runs it against the real engine: no engine, late start, kill and restart all recover.",
          "refs": [
            "tip/src/engine_client.cpp",
            "tip/tests/e2e.ps1"
          ]
        },
        {
          "id": "M1.14",
          "title": "TSF harness: real DLL, real engine, simulated app",
          "status": "done",
          "area": "test",
          "stories": [
            1,
            2,
            4,
            5,
            6,
            7,
            8,
            31,
            51
          ],
          "notes": "`tip/tests/tsf_e2e.ps1` runs the unregistered DLL through real TSF against a stand-in text field (`ITextStoreACP`), with the real engine and n-gram data. 31 checks: popup under the caret, Down/Up/Tab/Esc, Enter never consumed, Tab left alone when the popup is closed or stale, no popup mid-word / with a selection / where keyboards are disabled, learned words offered, fast typing. Also passes as a DPI-unaware app. Key events go straight to the DLL's key interface because TSF only routes keys to a registered text service.",
          "refs": [
            "tip/tests/tsf_harness.cpp",
            "tip/tests/tsf_e2e.ps1"
          ]
        },
        {
          "id": "M1.10",
          "title": "Popup rendering at the caret",
          "status": "done",
          "area": "dll",
          "stories": [
            2,
            3,
            36
          ],
          "notes": "Built and checked in the harness: word rows, typed prefix highlighted in blue, highlight row moves, popup opens just under the caret (screenshots reviewed), never takes focus, click-through, per-window DPI. Second-monitor and 150% checks happen in daily use, not as a gate.",
          "refs": [
            "tip/src/popup.cpp"
          ]
        },
        {
          "id": "M1.11",
          "title": "Keyboard icon and an enable-keyboard step",
          "status": "done",
          "area": "install",
          "stories": [
            43
          ],
          "notes": "Icon (`tip/assets/completionist.ico`) is compiled into the DLL, and `tip/register.ps1` plus `tip/enable-keyboard.ps1` do the install. Confirming Completionist shows in Settings happens in daily use, not as a gate.",
          "refs": [
            "tip/register.ps1",
            "tip/enable-keyboard.ps1"
          ]
        },
        {
          "id": "M1.12",
          "title": "Manual app matrix for the real DLL",
          "status": "dropped",
          "area": "test",
          "stories": [
            2,
            31,
            36
          ],
          "notes": "Dropped: live checks no longer gate a milestone; it is a personal tool and daily use is the test. Notepad, Chrome (textarea, contenteditable, long text, password field, after `chrome://restart`), Edge, Discord, Slack, Obsidian, an older Electron app. 100% and 150% scaling. Enter still sends in Discord. The harness (M1.14) already covers the logic; this checks each app's own quirks. Switch to the Completionist keyboard first, so the old Completionist Spike keyboard isn't also active."
        },
        {
          "id": "M1.13",
          "title": "Remove the Completionist Spike keyboard and unregister it",
          "status": "dropped",
          "area": "install",
          "stories": [],
          "notes": "Moved to M4.11 so 0.1.0 does not wait on a cleanup only the user can do. Originally: the user removes the Spike keyboard from the language list, then runs `tip/spike/register.ps1 -Unregister`."
        },
        {
          "id": "M1.15",
          "title": "Roadmap as a highway map, in the Evergreen color scheme",
          "status": "done",
          "area": "docs",
          "stories": [],
          "notes": "Shipped the highway map page, the restyled task board with highway stripes and filter, the retired timeline page, and a CONTEXT.md holding the Evergreen scheme.",
          "refs": [
            "branch worktree-roadmap-map",
            "docs/roadmap/index.html",
            "docs/roadmap/map-layout.js",
            "docs/roadmap/map.js",
            "docs/roadmap/page.js",
            "docs/roadmap/tasks.html",
            "CONTEXT.md"
          ]
        },
        {
          "id": "M1.16",
          "title": "Rename the app from Typer to Completionist",
          "status": "done",
          "area": "repo",
          "stories": [],
          "notes": "Renamed everywhere: the engine package and commands, pipe, DLL and its files, folders, install scripts, tests, docs and the roadmap pages. The engine moves old Typer folders on first start. The GitHub repo and checkout folder keep the name typer until the owner renames them. After merging, run `scripts/install.ps1` again.",
          "refs": [
            "branch rename-completionist",
            "engine/src/completionist_engine/config.py",
            "scripts/install.ps1",
            "CONTEXT.md"
          ]
        },
        {
          "id": "M1.17",
          "title": "Make the roadmap easier to read: releases first, plain titles, tasks by status",
          "status": "done",
          "area": "docs",
          "stories": [],
          "refs": [
            "branch m1-roadmap-readability",
            "docs/roadmap/tasks.html",
            "v0.0.12"
          ],
          "notes": "The M-numbers confuse: M2 is 0.2.0 but M5 is 1.1.0, and task ids like M1.14 sit before M1.10. The pages should show each milestone as its release and name (\"0.2.0 · Smarter words\", found through the release's `milestone` field), lead task rows with the plain title and show the id only as a small grey tag (ids stay as stable keys, never renumbered), and sort tasks doing, next, todo, blocked, done, dropped. Then update the wording in CLAUDE.md and CONTEXT.md. Files: tasks.html, common.js, page.js."
        }
      ]
    },
    {
      "id": "M2",
      "title": "Smarter words",
      "status": "done",
      "goal": "Rank words by the preceding words and by the user's own habits.",
      "done_when": "After \"I'd like to\" the list shows \"know\" above \"knowledge\", and names or slang the user types a few times start appearing.",
      "tasks": [
        {
          "id": "M2.1",
          "title": "N-gram builder: bigram and trigram tables from a public corpus",
          "status": "done",
          "area": "data",
          "stories": [
            11
          ],
          "notes": "`completionist-build-ngrams` counts words, bigrams and trigrams from text files or folders (`.txt`/`.gz`), prunes rare entries while counting so memory stays bounded, and writes a SQLite file. Built from WikiText-103: 81M words, 100k vocabulary, 0.8M bigrams, 1.6M trigrams, 66 MB, 6.5 minutes.",
          "refs": [
            "engine/src/completionist_engine/ngrams.py",
            "engine/src/completionist_engine/build_ngrams.py"
          ]
        },
        {
          "id": "M2.2",
          "title": "N-gram re-ranking in the word completer",
          "status": "done",
          "area": "engine",
          "stories": [
            11
          ],
          "notes": "The completer mixes Zipf frequency with bigram and trigram counts (contexts seen under 5 times are ignored). \"What do you th\" now ranks \"think\" first. Pipe round trip stays fast: p95 1.2 ms uncached against the real table.",
          "refs": [
            "engine/src/completionist_engine/words.py"
          ]
        },
        {
          "id": "M2.3",
          "title": "Filter misspellings and junk out of the base vocabulary",
          "status": "done",
          "area": "engine",
          "stories": [
            10
          ],
          "notes": "Beyond the 20,000 most common words, a word must appear written lowercase at least 3 times in the corpus. \"tomorow\", \"tomorrowland\" and \"Updike\" no longer appear; the vocabulary went from 146k to 53k words.",
          "refs": [
            "engine/src/completionist_engine/vocabulary.py"
          ]
        },
        {
          "id": "M2.4",
          "title": "Personal store: accept and typed-word counts in SQLite",
          "status": "done",
          "area": "engine",
          "stories": [
            16,
            17
          ],
          "notes": "`PersonalStore` keeps word and word-pair counts in memory, saved to `personal.sqlite` every 10 s and on exit. Only plain lowercase English words are stored. Corrupt files are set aside; rare pairs are pruned past 300k.",
          "refs": [
            "engine/src/completionist_engine/personal.py"
          ]
        },
        {
          "id": "M2.5",
          "title": "Promote new words to the vocabulary after N uses",
          "status": "done",
          "area": "engine",
          "stories": [
            15
          ],
          "notes": "A word outside the dictionary is suggested once used 3 times (`[learning] promote_after`). Learning counts only real typing, one character at a time, so pasted text, caret jumps and backspacing never count. Silent fields and block-listed apps never teach it anything.",
          "refs": [
            "engine/src/completionist_engine/learning.py",
            "engine/src/completionist_engine/engine.py"
          ]
        },
        {
          "id": "M2.6",
          "title": "Accept events name the accepted item",
          "status": "done",
          "area": "dll",
          "stories": [
            16
          ],
          "notes": "The DLL sends an `accept` event with the inserted word, and the engine learns it and the word before it. The protocol field is covered by tests on both sides.",
          "refs": [
            "tip/src/tsf_service.cpp",
            "engine/src/completionist_engine/protocol.py"
          ]
        },
        {
          "id": "M2.7",
          "title": "Check learning and n-gram ranking live through the DLL",
          "status": "dropped",
          "area": "test",
          "stories": [
            11,
            15,
            16
          ],
          "notes": "Dropped: live checks no longer gate a milestone; it is a personal tool and daily use is the test. Needs you: with the engine running and the Completionist keyboard on, type a made-up word (like \"zorblax\") three or more times, then confirm it shows up as a suggestion. Also check \"I'd like to kn\" puts \"know\" first."
        }
      ]
    },
    {
      "id": "M3",
      "title": "Phrase suggestions",
      "status": "done",
      "goal": "A greyed phrase continuation from a cheap cloud FIM model as the top popup row, accepted with Tab.",
      "done_when": "Allow-listed apps show a streaming phrase row after a pause, Ctrl+Space works everywhere, and heavy use stays under $0.50 a day.",
      "tasks": [
        {
          "id": "M3.1",
          "title": "Provider benchmark: time-to-first-token and quality",
          "status": "dropped",
          "area": "bench",
          "stories": [
            45
          ],
          "notes": "Dropped: the benchmark tool was deleted. Model choice is read off the OpenRouter model cards and swapped in the config; completionist-stats shows real latency and cost."
        },
        {
          "id": "M3.2",
          "title": "Benchmark time-to-first-token against context size",
          "status": "dropped",
          "area": "bench",
          "stories": [
            46
          ],
          "notes": "Dropped: the benchmark tool was deleted. Model choice is read off the OpenRouter model cards and swapped in the config; completionist-stats shows real latency and cost."
        },
        {
          "id": "M3.3",
          "title": "Phrase provider: OpenAI-compatible /completions with optional FIM",
          "status": "done",
          "area": "engine",
          "stories": [
            22,
            27,
            28,
            29,
            39,
            40
          ],
          "notes": "`PhraseProvider` streams OpenAI-style `/completions` (plain or FIM with a suffix), reads usage from DeepSeek and OpenAI formats, and turns timeouts and HTTP errors into `ProviderError` without ever including the key. Settings live under `[phrase]`; the key is read from the environment variable named in `api_key_env`. Tested against a local fake server.",
          "refs": [
            "engine/src/completionist_engine/phrase_provider.py"
          ]
        },
        {
          "id": "M3.4",
          "title": "Cache-friendly anchored context window",
          "status": "done",
          "area": "engine",
          "stories": [
            59
          ],
          "notes": "`anchored_window` cuts the text before the caret at a paragraph or sentence boundary, so the start of the prompt only moves every sentence or so instead of every keystroke. 400 keystrokes move it 5 times, so consecutive requests share a cached prefix.",
          "refs": [
            "engine/src/completionist_engine/context.py"
          ]
        },
        {
          "id": "M3.5",
          "title": "Daily budget cap and per-request cost tracking (removed)",
          "status": "dropped",
          "area": "engine",
          "stories": [
            60,
            61
          ],
          "notes": "Removed on the user's call: Completionist no longer counts spend or prices. A spending limit is set on the OpenRouter key itself.",
          "refs": [
            "engine/src/completionist_engine/budget.py"
          ]
        },
        {
          "id": "M3.6",
          "title": "Phrase scheduler state machine",
          "status": "done",
          "area": "engine",
          "stories": [
            18,
            23,
            24,
            26
          ],
          "notes": "`PhraseScheduler` is a pure state machine with an injected clock: 350 ms pause in auto mode, hotkey mode never asks alone, typing along trims the phrase without a new request, anything else cancels and clears it, late chunks that contradict the typed text are dropped. 22 tests.",
          "refs": [
            "engine/src/completionist_engine/phrase_scheduler.py"
          ]
        },
        {
          "id": "M3.7",
          "title": "Phrase push messages over the pipe",
          "status": "done",
          "area": "engine",
          "stories": [
            22
          ],
          "notes": "Replies carry `phrase` and `phrase_mode`; streamed text is pushed as `{\"type\":\"phrase\",\"id\",\"text\",\"done\"}` keyed to the newest request. Tested over a real named pipe with a fake provider, including that password fields never reach the provider.",
          "refs": [
            "engine/src/completionist_engine/phrases.py",
            "engine/tests/test_phrases_pipe.py"
          ]
        },
        {
          "id": "M3.8",
          "title": "Phrase row in the popup",
          "status": "done",
          "area": "dll",
          "stories": [
            18,
            19,
            20,
            21
          ],
          "notes": "The popup gets a phrase row on top: typed part in blue, the phrase as ghost text, highlighted by default 150 ms after it appears so a late phrase can't steal a Tab (that Tab takes the word). Up/Down move through phrase and words. Checked in the TSF harness with a fake provider, in both auto and hotkey modes, and in a screenshot.",
          "refs": [
            "tip/src/popup_model.h",
            "tip/src/popup.cpp",
            "tip/tests/phrase_scenarios.h"
          ]
        },
        {
          "id": "M3.9",
          "title": "Ctrl+Space hotkey requests a phrase in any app",
          "status": "done",
          "area": "dll",
          "stories": [
            25
          ],
          "notes": "Ctrl+Space asks for a phrase (also right after a space, with no word yet), Ctrl+Right inserts the next phrase word and keeps the rest, Esc quiets the popup for the word and stops phrase requests. The key is only taken where the engine says phrases are available, so Word's own Ctrl+Space is safe elsewhere.",
          "refs": [
            "tip/src/tsf_service.cpp"
          ]
        },
        {
          "id": "M3.10",
          "title": "Fall back to words when the provider is slow or down",
          "status": "done",
          "area": "engine",
          "stories": [
            30
          ],
          "notes": "A provider error ends the phrase quietly (word suggestions carry on); three failures in a row pause phrases for 30 seconds; with no key, no failure pause or phrases switched off, nothing is ever requested.",
          "refs": [
            "engine/src/completionist_engine/phrases.py"
          ]
        },
        {
          "id": "M3.11",
          "title": "Check phrases live with a real provider and key",
          "status": "dropped",
          "area": "test",
          "stories": [
            18,
            19,
            25,
            59
          ],
          "notes": "Dropped: live checks no longer gate a milestone; it is a personal tool and daily use is the test. Needs you: tray menu > Open settings file, add `api_key = \"...\"` under `[phrase]` (never in chat; it applies within seconds), then type in Notepad (allow-listed, phrases appear on their own) and press Ctrl+Space in Discord. `spend.json` in `%LOCALAPPDATA%\\Completionist` shows what it cost."
        },
        {
          "id": "M3.12",
          "title": "OpenRouter without FIM: prefix-only phrases, held back when text follows the caret on the same line",
          "status": "done",
          "area": "engine",
          "stories": [],
          "notes": "The user only has an OpenRouter key and it documents no `suffix`. fim=false now also skips phrases while non-blank text follows the caret on the same line. Which OpenRouter models take /completions is untested; bench/providers.example.toml has candidates.",
          "refs": [
            "branch openrouter-phrases",
            "engine/src/completionist_engine/phrases.py"
          ]
        }
      ]
    },
    {
      "id": "M4",
      "title": "Daily-driver polish",
      "status": "done",
      "goal": "Completionist runs all day without attention and shows whether it's paying off.",
      "done_when": "One-command install and uninstall, autostart at logon, tray pause, hot-reloaded config and a stats summary.",
      "tasks": [
        {
          "id": "M4.1",
          "title": "Tray icon with pause/resume and a global hotkey",
          "status": "done",
          "area": "engine",
          "stories": [
            41
          ],
          "notes": "Code and tests done; the tray and hotkey need a manual look on a real desktop.",
          "refs": [
            "branch m3-phrases",
            "engine/src/completionist_engine",
            "scripts/",
            "tip/tests"
          ]
        },
        {
          "id": "M4.2",
          "title": "Start the engine at logon",
          "status": "done",
          "area": "install",
          "stories": [
            42
          ],
          "notes": "scripts/install.ps1 creates the logon task (pythonw, no console). Written and syntax-checked; not yet run, because it needs the user (UAC, keyboard setting). Scripts parse cleanly; running them (UAC) is left to the user.",
          "refs": [
            "scripts/install.ps1",
            "scripts/uninstall.ps1",
            "version 0.4.0"
          ]
        },
        {
          "id": "M4.3",
          "title": "Hot-reload the config file",
          "status": "done",
          "area": "engine",
          "stories": [
            38
          ],
          "notes": "ConfigWatcher polls mtime; a bad edit keeps the old config and logs why.",
          "refs": [
            "branch m3-phrases",
            "engine/src/completionist_engine",
            "scripts/",
            "tip/tests"
          ]
        },
        {
          "id": "M4.4",
          "title": "Install and uninstall scripts",
          "status": "done",
          "area": "install",
          "stories": [
            43,
            44
          ],
          "notes": "install.ps1 / uninstall.ps1 written and syntax-checked; the user runs them (UAC and language settings are not mine to change). Scripts parse cleanly; running them (UAC) is left to the user.",
          "refs": [
            "scripts/install.ps1",
            "scripts/uninstall.ps1",
            "version 0.4.0"
          ]
        },
        {
          "id": "M4.5",
          "title": "Metrics store: shown, accepted, keystrokes saved, provider latency",
          "status": "done",
          "area": "engine",
          "stories": [
            52,
            53,
            54,
            56
          ],
          "notes": "SQLite counts and timings per day/app/provider; never text.",
          "refs": [
            "branch m3-phrases",
            "engine/src/completionist_engine",
            "scripts/",
            "tip/tests"
          ]
        },
        {
          "id": "M4.6",
          "title": "Stats summary from the CLI or tray",
          "status": "done",
          "area": "engine",
          "stories": [
            55
          ],
          "notes": "completionist-stats CLI and tray \"Show stats\".",
          "refs": [
            "branch m3-phrases",
            "engine/src/completionist_engine",
            "scripts/",
            "tip/tests"
          ]
        },
        {
          "id": "M4.7",
          "title": "Engine logging of request timings and provider errors",
          "status": "done",
          "area": "engine",
          "stories": [
            50
          ],
          "notes": "engine.log rotates; requests over 25 ms and provider errors are logged.",
          "refs": [
            "branch m3-phrases",
            "engine/src/completionist_engine",
            "scripts/",
            "tip/tests"
          ]
        },
        {
          "id": "M4.8",
          "title": "Resilience pass: engine crashes and restarts",
          "status": "done",
          "area": "dll",
          "stories": [
            34,
            35
          ],
          "notes": "Harness scenarios R1/R2 kill and restart the engine: typing does not hang, keys pass through, the popup returns after reconnect. The harness needs the desktop foreground, so it is run only when the user is away.",
          "refs": [
            "branch m3-phrases",
            "engine/src/completionist_engine",
            "scripts/",
            "tip/tests"
          ]
        },
        {
          "id": "M4.9",
          "title": "Live check of install, tray, hotkey and logon start",
          "status": "dropped",
          "area": "install",
          "stories": [
            41,
            42,
            43,
            44
          ],
          "notes": "Dropped: live checks no longer gate a milestone; it is a personal tool and daily use is the test. User runs .\\scripts\\install.ps1, checks the tray icon and Ctrl+Alt+P, reboots once to see the engine start by itself."
        },
        {
          "id": "M4.10",
          "title": "README demo GIF and a small web demo of the popup",
          "status": "dropped",
          "area": "docs",
          "stories": [],
          "notes": "Moved to M9.7 (0.7.0, the last release before 1.0) so 0.4.0 does not wait on it. Originally: the public showcase, a short GIF of the popup in use and a plain HTML page with made-up text that shows the ranking."
        },
        {
          "id": "M4.11",
          "title": "Remove the Completionist Spike keyboard and unregister it",
          "status": "dropped",
          "area": "install",
          "stories": [],
          "notes": "Moved from M1.13. The user removes it from the language list, then runs `tip/spike/register.ps1 -Unregister`. Moved to M9.8 so 0.4.0 does not wait on it."
        }
      ]
    },
    {
      "id": "M5",
      "title": "Reply-aware phrases",
      "status": "planned",
      "goal": "Phrase suggestions know what you're replying to.",
      "done_when": "A phrase suggestion in a Discord reply reflects the last few messages in the channel.",
      "tasks": [
        {
          "id": "M5.1",
          "title": "Reply-aware context from the surrounding window",
          "status": "todo",
          "area": "engine",
          "stories": [
            57,
            58
          ],
          "notes": "Read-only UI Automation with Windows OCR as a fallback, refreshed on window switch. No AI or vision models."
        }
      ]
    },
    {
      "id": "M6",
      "title": "Web fields",
      "status": "planned",
      "goal": "Reach web text fields directly with a Chrome extension that talks to the same engine.",
      "done_when": "Inline ghost text works in a Chrome textarea through the extension.",
      "tasks": [
        {
          "id": "M6.1",
          "title": "Chrome extension over Native Messaging",
          "status": "todo",
          "area": "extension",
          "stories": [],
          "notes": "Inline ghost text and page context in web fields."
        }
      ]
    },
    {
      "id": "M7",
      "title": "N-gram suggestions and the personal log",
      "status": "active",
      "goal": "Suggest the next word after a space and two- or three-word chunks while typing, all in one popup box, and keep a clean personal log of the words and trigrams you type.",
      "done_when": "After a space the popup offers likely next words, chunks appear while typing, words, chunks and the phrase share one box, the popup stays quiet when nothing is likely, and the log holds only words and trigrams you typed, finished and used at least three times.",
      "tasks": [
        {
          "id": "M7.1",
          "title": "Personal log admission: learn only words you typed, finished without correcting and used 3 times",
          "status": "done",
          "area": "engine",
          "stories": [
            66
          ],
          "notes": "Builds on the existing PersonalStore and `promote_after`. Adds: a word counts only when you finish it with a space or punctuation and didn't backspace over it (so typos are never learned, which also keeps the typo feature from learning its own mistakes). Silent fields, block-listed apps, pasted text and caret jumps already teach nothing. Keep it simple: no per-day rule, decay or never-learn list until junk actually shows up.",
          "refs": [
            "engine/src/completionist_engine/learning.py",
            "branch m7-next-words"
          ]
        },
        {
          "id": "M7.2",
          "title": "Personal trigram counts next to the word and pair counts",
          "status": "done",
          "area": "engine",
          "stories": [
            66,
            17
          ],
          "notes": "A trigram is three words typed in a row within one sentence, never across punctuation, a newline or a window switch. Counts only, stored on disk locally. Admission follows M7.1.",
          "refs": [
            "engine/src/completionist_engine/personal.py",
            "branch m7-next-words"
          ]
        },
        {
          "id": "M7.3",
          "title": "`completionist-words` command: list and forget",
          "status": "done",
          "area": "engine",
          "stories": [
            67
          ],
          "notes": "`list` with search, `forget <word>` and `forget-recent 10m`. The smallest way to see and edit the log; the viewer (M8) shows the same data.",
          "refs": [
            "engine/src/completionist_engine/personal.py",
            "branch m7-kinds"
          ]
        },
        {
          "id": "M7.4",
          "title": "Next-word candidates after a space, with a confidence threshold",
          "status": "done",
          "area": "engine",
          "stories": [
            62,
            65
          ],
          "notes": "An empty prefix is allowed: rank continuations from the bigram and trigram tables and the personal pairs. Return nothing when no continuation is likely enough, and put the threshold in the config so it can be switched off. Built as WordCompleter.next_words, used by the engine after a space. Config: [words] next (default false until M7.8 lets the popup open after a space safely) and next_threshold (default 0.05; 0 offers the best few whatever the odds).",
          "refs": [
            "engine/src/completionist_engine/words.py",
            "engine/src/completionist_engine/config.py",
            "branch m7-next-words"
          ]
        },
        {
          "id": "M7.5",
          "title": "Multi-word chunks from the n-gram tables",
          "status": "done",
          "area": "engine",
          "stories": [
            63
          ],
          "notes": "Extend the top continuation one word at a time while its probability stays above a cutoff, up to 3 words. Offer a chunk only when it reads coherently. Built as WordCompleter.chunks (cutoff 0.3, at most 3 words, never ends on the/a/of/to/and). Not yet in replies: M7.6 carries the kind and wires it in. Quality is limited by the WikiText corpus; judge it in M7.9.",
          "refs": [
            "engine/src/completionist_engine/words.py",
            "branch m7-next-words"
          ]
        },
        {
          "id": "M7.6",
          "title": "Protocol: each suggestion carries its kind (word, chunk or next)",
          "status": "done",
          "area": "engine",
          "stories": [
            64
          ],
          "notes": "The field is covered by tests on both the engine and DLL sides, like the `accept` event was.",
          "refs": [
            "engine/src/completionist_engine/protocol.py",
            "tip/src/protocol.cpp",
            "branch m7-kinds"
          ]
        },
        {
          "id": "M7.7",
          "title": "Popup shows words, chunks and the phrase in one box",
          "status": "next",
          "area": "dll",
          "stories": [
            64
          ],
          "notes": "The phrase stays on top. First guess at the order: phrase, then chunks, then words. The row order gets polished in the UI redesign (M10). Checked in the TSF harness with a screenshot."
        },
        {
          "id": "M7.8",
          "title": "Open the popup after a space without stealing Tab or Enter",
          "status": "todo",
          "area": "dll",
          "stories": [
            62,
            7,
            8
          ],
          "notes": "Today the popup closes on space (story 9), so this changes the key router and needs native tests for every key. A next-word row is not highlighted by default until you press Down, so Tab still indents or moves on, like the 150 ms no-steal idea for phrases."
        },
        {
          "id": "M7.9",
          "title": "Check next words and chunks live in real apps",
          "status": "todo",
          "area": "test",
          "stories": [
            62,
            63,
            64,
            65
          ],
          "notes": "Needs you: type in Notepad, Discord and Chrome and judge whether it helps or is noisy. If the base tables (0.8M bigrams, 1.6M trigrams from WikiText-103) feel thin, rebuild them with a looser prune and a chat-style corpus such as subtitles. Don't do that before this check says it's needed."
        }
      ]
    },
    {
      "id": "M8",
      "title": "Personal viewer",
      "status": "planned",
      "goal": "Let you see your personal dictionary and your stats, opened from the tray.",
      "done_when": "The tray's Open viewer writes a local HTML page with your learned words, trigrams and stats and opens it in the browser, without a server, and it holds no typed-text history.",
      "tasks": [
        {
          "id": "M8.1",
          "title": "Snapshot writer: personal dictionary and stats into one local HTML file",
          "status": "todo",
          "area": "viewer",
          "stories": [
            68,
            69
          ],
          "notes": "Written into `%LOCALAPPDATA%\\Completionist` each time, from `personal.sqlite` and `metrics.sqlite`. No server and no open port: like the roadmap pages, it is a plain file opened from disk."
        },
        {
          "id": "M8.2",
          "title": "Viewer page: searchable, sortable dictionary and the stats, in the Evergreen theme",
          "status": "todo",
          "area": "viewer",
          "stories": [
            68,
            69
          ],
          "notes": "Dictionary entries (words and trigrams) sortable by count and last seen, marked as learned or base words you use often. Stats per day and per app: shown, accepted, keystrokes saved, latency. Read-only, with the `completionist-words forget` command shown beside each entry."
        },
        {
          "id": "M8.3",
          "title": "Tray item: Open viewer",
          "status": "todo",
          "area": "engine",
          "stories": [
            70
          ],
          "notes": "Next to the existing \"Show stats\" item."
        },
        {
          "id": "M8.4",
          "title": "Tests: the snapshot holds counts and dictionary entries only",
          "status": "todo",
          "area": "test",
          "stories": [
            17,
            56
          ],
          "notes": "No typed-text history, no text from silent fields or blocked apps."
        },
        {
          "id": "M8.5",
          "title": "Check the viewer live",
          "status": "todo",
          "area": "test",
          "stories": [
            68,
            69,
            70
          ],
          "notes": "Needs you: open it from the tray after some typing and check that the dictionary and stats look right."
        }
      ]
    },
    {
      "id": "M9",
      "title": "Typo-tolerant words",
      "status": "planned",
      "goal": "Still suggest the right word when the typed part has a typo, and show which letters were guessed.",
      "done_when": "Typing \"moutian\" offers \"mountain\" with the guessed letters in a different colour, exact prefix matches still rank first, and lookup p95 stays under 10 ms.",
      "tasks": [
        {
          "id": "M9.1",
          "title": "Fuzzy index with SymSpell over the vocabulary and personal words",
          "status": "todo",
          "area": "engine",
          "stories": [
            71
          ],
          "notes": "Use the existing `symspellpy` library, not a hand-made matcher. Try it first; only if it can't handle half-typed words (it corrects whole words, but we complete prefixes), compare `rapidfuzz` or a trie with a Levenshtein automaton, and record the result in `decisions`."
        },
        {
          "id": "M9.2",
          "title": "Fuzzy candidates fill the rows after exact prefix matches",
          "status": "todo",
          "area": "engine",
          "stories": [
            71,
            73
          ],
          "notes": "Exact matches always come first. Corrections fill the remaining rows, or all of them when nothing matches. Edit distance 1 for short fragments and 2 for longer ones, and no correcting 1 to 2 letter fragments, names or learned words. Tests: moutian, definately, recieve."
        },
        {
          "id": "M9.3",
          "title": "Protocol marks the guessed letters of each suggestion",
          "status": "todo",
          "area": "engine",
          "stories": [
            72
          ],
          "notes": "Each item carries the positions it corrected. Tests on both sides. Builds on the suggestion kind from M7.6."
        },
        {
          "id": "M9.4",
          "title": "Popup draws guessed letters in a third colour",
          "status": "todo",
          "area": "dll",
          "stories": [
            72
          ],
          "notes": "New colour token, validated as CONTEXT.md describes. Checked in the TSF harness with a screenshot."
        },
        {
          "id": "M9.5",
          "title": "Latency check with the fuzzy index on",
          "status": "todo",
          "area": "test",
          "stories": [
            71
          ],
          "notes": "Lookup p95 stays under 10 ms over the real pipe."
        },
        {
          "id": "M9.6",
          "title": "Check typo completion live in real apps",
          "status": "todo",
          "area": "test",
          "stories": [
            71,
            72,
            73
          ],
          "notes": "Needs you: type moutian, definately, recieve and similar, and check that correct rare words aren't pushed aside."
        },
        {
          "id": "M9.7",
          "title": "README demo GIF and a small web demo of the popup",
          "status": "todo",
          "area": "docs",
          "stories": [],
          "notes": "Moved here from M4.10. The public showcase: a short GIF of the popup in use, and a plain HTML page with made-up text that shows the ranking (no engine, no server, no real data). Needs your OK before anything is published."
        },
        {
          "id": "M9.8",
          "title": "Remove the Completionist Spike keyboard and unregister it",
          "status": "todo",
          "area": "install",
          "stories": [],
          "notes": "Moved from M4.11 (and M1.13). The user removes it from the language list, then runs `tip/spike/register.ps1 -Unregister`."
        }
      ]
    },
    {
      "id": "M10",
      "title": "Major UI redesign",
      "status": "planned",
      "goal": "Give the suggestion popup, and whatever else the audit says belongs with it, one deliberate look.",
      "done_when": "The popup, with all its row types, reads well at 100% and 200% scaling and on a second monitor, in the Evergreen colours, and the rewritten drawing code passes the harness.",
      "tasks": [
        {
          "id": "M10.1",
          "title": "Scope the redesign: audit the popup in real apps and decide what else is included",
          "status": "todo",
          "area": "dll",
          "stories": [],
          "notes": "Assumed to mean the popup. Decide whether the tray menu and the viewer's look come too. Mockups and Evergreen design tokens follow; split into tasks when scoped."
        },
        {
          "id": "M10.2",
          "title": "Redesign and rewrite the popup drawing",
          "status": "todo",
          "area": "dll",
          "stories": [
            74
          ],
          "notes": "Split into mockups, tokens, the drawing rewrite and harness screenshots once M10.1 is done. A bad popup must fall back to the current look, never crash the host app."
        }
      ]
    }
  ],
  "releases": [
    {
      "version": "0.0.1",
      "status": "released",
      "date": "2026-09-29",
      "title": "Repository scaffold",
      "highways": [
        "tooling"
      ],
      "text": "The Completionist repository started with a README, a gitignore and a standalone uv project for the Python engine."
    },
    {
      "version": "0.0.2",
      "status": "released",
      "date": "2026-09-29",
      "title": "Engine word completion",
      "highways": [
        "engine"
      ],
      "text": "The engine completes words from a 146,000-word frequency list and answers over a named pipe in under 10 ms at the 95th percentile, backed by 74 tests."
    },
    {
      "version": "0.0.3",
      "status": "released",
      "date": "2026-09-29",
      "title": "TSF spike",
      "highways": [
        "tsf"
      ],
      "milestone": "M0",
      "text": "A throwaway text service proved that the Text Services Framework gives Completionist the exact caret position, the real surrounding text and clean key handling in Notepad, Discord, Chrome, Edge and Teams."
    },
    {
      "version": "0.0.4",
      "status": "released",
      "date": "2026-09-29",
      "title": "Roadmap pages",
      "highways": [
        "tooling"
      ],
      "text": "The engine and spike branches were merged into main, and the project gained a task board and a left-to-right release timeline that agents keep current under the rules in CLAUDE.md."
    },
    {
      "version": "0.0.5",
      "status": "released",
      "title": "Text service, phrases and daily-driver polish",
      "highways": [
        "engine",
        "tsf",
        "tooling"
      ],
      "text": "The real Completionist text service, context-aware and learning word ranking, streamed cloud phrases with a daily budget, a tray icon, pause hotkey, stats and install scripts are all built and covered by automated tests, with checks in real apps still to do.",
      "date": "2026-09-29"
    },
    {
      "version": "0.0.6",
      "status": "released",
      "date": "2026-09-29",
      "title": "OpenRouter phrases",
      "highways": [
        "engine"
      ],
      "text": "Phrases now come only through OpenRouter from a list of model names in the config, DeepSeek and the benchmark tool were removed, and phrases are held back when text follows the caret on the same line."
    },
    {
      "version": "0.0.7",
      "status": "released",
      "date": "2026-09-29",
      "title": "Highway map and Completionist",
      "highways": [
        "engine",
        "tsf",
        "tooling"
      ],
      "text": "The roadmap became a highway map in the new Evergreen color scheme with a restyled task board and a CONTEXT.md, and the app was renamed from Typer to Completionist, with old data folders moved on first start."
    },
    {
      "version": "0.0.8",
      "status": "released",
      "date": "2026-09-29",
      "title": "Continue-only phrases",
      "highways": [
        "engine",
        "tsf"
      ],
      "text": "The phrase prompt now tells models to continue the text instead of answering it, and the daily budget and price counting were removed in favour of a spending limit on the OpenRouter key."
    },
    {
      "version": "0.0.9",
      "status": "released",
      "date": "2026-09-30",
      "title": "Stricter phrase prompt and the M7 to M10 plan",
      "highways": [
        "engine",
        "tooling"
      ],
      "text": "The phrase model now gets a longer autocomplete prompt with thirty numbered rules and two worked examples, and the roadmap gained milestones M7 to M10 for n-gram suggestions, a personal viewer, typo tolerance and a UI redesign."
    },
    {
      "version": "0.0.10",
      "status": "released",
      "date": "2026-09-30",
      "title": "Groq default, key in the config",
      "highways": [
        "engine",
        "tooling"
      ],
      "text": "The phrase prompt now tells the model not to over-complete, the default model is Llama 3.3 70B on Groq through OpenRouter with a provider_order setting, and the API key moved from an environment variable into the config file."
    },
    {
      "version": "0.0.11",
      "status": "released",
      "date": "2026-09-30",
      "title": "One-line launcher",
      "highways": [
        "tooling"
      ],
      "text": "The engine now starts with `cd engine && uv run completionist.py`, a small launcher script beside the project file."
    },
    {
      "version": "0.0.12",
      "status": "released",
      "date": "2026-09-30",
      "title": "Task board by release",
      "highways": [
        "tooling"
      ],
      "text": "The task board now shows each milestone as its release and name, leads task rows with the plain title and a small grey id, and sorts tasks by status."
    },
    {
      "version": "0.1.0",
      "status": "released",
      "title": "Words everywhere",
      "milestone": "M1",
      "text": "This was the first release you could type with. The real text service replaced the spike and connected to the engine, so a list of word completions appears under the caret in Notepad, Chrome, Edge, Discord and other Electron apps. Tab accepts the highlighted word, the arrow keys move through the list and Esc closes it. Enter is never intercepted, so chat messages still send. Password, URL, email and number fields stay silent, as do code editors and terminals. If the engine stops, apps carry on as if Completionist were not installed and reconnect when it returns. A keyboard icon lets Windows list Completionist in Settings, so it can be switched on without PowerShell. The text service, the popup and the engine link are tested natively and through a harness that drives the real DLL; checks in each app are left to daily use.",
      "date": "2026-09-30"
    },
    {
      "version": "0.2.0",
      "status": "released",
      "title": "Smarter words",
      "milestone": "M2",
      "text": "Word suggestions now take the sentence and the writer into account. Bigram and trigram tables built from a public corpus re-rank completions by the previous one or two words, so after “I’d like to” the list offers “know” before “knowledge”. Common misspellings such as “tomorow” are filtered out of the base vocabulary. A personal store boosts the words you actually use and promotes new ones, such as names and slang, into the vocabulary after a few uses. It keeps word counts only and never the text itself. Accept events now name the accepted item, so the engine learns from what you took. The n-gram file is optional: without it the engine ranks by word frequency alone. Checks through the real text service are left to daily use.",
      "date": "2026-09-30"
    },
    {
      "version": "0.3.0",
      "status": "released",
      "title": "Phrase suggestions",
      "milestone": "M3",
      "text": "The popup gained a greyed phrase row at the top, a continuation of your sentence written by a cheap and fast cloud completion model. In allow-listed apps it appears after a short pause, it is highlighted by default so Tab takes the whole phrase, and Ctrl+Right takes it one word at a time. In any other app, Ctrl+Space asks for a phrase on demand. Phrases come through OpenRouter from a list of cheap models you name in the config, each tried in turn if the one before fails. Models without fill-in-the-middle support get the text before the caret only, and a phrase is held back when text follows the caret on the same line. An anchored context window keeps requests cache-friendly. Completionist does not count spend, so the limit is set on your OpenRouter key. When the provider is slow or unavailable it quietly falls back to words. Checks against a live provider are left to daily use.",
      "date": "2026-09-30"
    },
    {
      "version": "0.4.0",
      "status": "released",
      "title": "Daily driver",
      "milestone": "M4",
      "text": "This release made Completionist something to leave running all day. The engine starts at logon through a scheduled task, a tray icon pauses and resumes it with Ctrl+Alt+P, and edits to the config file apply without a restart. One script installs Completionist for your Windows user and another removes it, including the keyboard registration. A local metrics store records how often suggestions are shown and accepted, how many keystrokes they save, and how each phrase provider performs. It keeps counts and timings only, never text. A stats summary, from the command line or the tray, shows whether Completionist is paying off. The engine logs request timings and provider errors, and it recovers when it crashes or restarts. Running the install scripts and removing the old Spike keyboard are left to you.",
      "date": "2026-09-30"
    },
    {
      "version": "0.4.1",
      "status": "released",
      "title": "Groundwork for smarter suggestions",
      "highways": [
        "engine"
      ],
      "date": "2026-09-30",
      "text": "The engine no longer learns words you backspaced over, keeps personal trigram counts, and can rank next words and two- or three-word chunks, all switched off until the popup can show them."
    },
    {
      "version": "0.5.0",
      "status": "next",
      "title": "N-gram suggestions",
      "milestone": "M7",
      "text": "Suggestions start to look ahead. After you type a space, Completionist predicts the next word from the last one or two words before you have typed a letter, and while you type it can offer two- or three-word chunks from the same bigram and trigram tables. Single words, chunks and the cloud phrase share one popup box, and the popup stays quiet when nothing is likely. Your personal log grows to include trigrams, but it only learns words and trigrams you typed yourself, finished without correcting and used at least three times. A small command lists what Completionist has learned and forgets anything you don’t want it to keep."
    },
    {
      "version": "0.6.0",
      "status": "planned",
      "title": "Personal viewer",
      "milestone": "M8",
      "text": "You can now see what Completionist has learned. An Open viewer item in the tray writes a snapshot of your personal dictionary and your usage stats to a local HTML page and opens it in the browser. The dictionary lists the words and trigrams you use, searchable and sortable by how often and how recently you typed them. The stats show how often suggestions were shown and accepted, how many keystrokes they saved and how fast they were, by day and by app. The page is read-only, opens straight from disk with no server, and is never published. Forgetting a word stays a command-line job."
    },
    {
      "version": "0.7.0",
      "status": "planned",
      "title": "Typo-tolerant words",
      "milestone": "M9",
      "text": "Suggestions now survive typos. Type a misspelled word such as “moutian” and the popup still offers “mountain”, with the letters it guessed drawn in a different colour so you can see what was corrected. Exact prefix matches always rank first, and corrections only fill the remaining rows or step in when nothing matches, so a real word is never pushed aside by a guess. The matching comes from an existing spell-correction library, SymSpell, built over the vocabulary and your personal words, and lookups stay under ten milliseconds at the 95th percentile. Words you fix yourself are never learned as new vocabulary."
    },
    {
      "version": "1.0.0",
      "status": "planned",
      "title": "Autocomplete for everything you type",
      "essay": [
        "Completionist 1.0 puts the suggestions of a code editor into every place you write English on Windows. Type a few letters in Discord, a browser text box or an email and a short list of completions appears just under the caret, ranked by the words before it and by the words you actually use. Tab takes the top one. In apps you choose, and on Ctrl+Space everywhere else, a greyed phrase appears above the list, a continuation of your sentence that you take whole with Tab or one word at a time with Ctrl+Right. Enter is never touched, and nothing in a password field is read or sent.",
        "The unusual decision is that Completionist is a Text Services Framework text service, the mechanism Windows uses for input methods, and not a keyboard hook with a screen-scraping overlay. That choice cost a milestone of throwaway spiking and a C++ DLL that lives inside every app, but it is why the caret position, the surrounding text, the key handling and the insertion of accepted text all come from Windows itself and work the same in Notepad, Chrome, Edge, Discord and Teams. The DLL stays deliberately thin: it draws the popup, decides which keys it consumes, and talks over a named pipe to a Python engine, reconnecting quietly if the engine is gone. The key router is pure logic with native tests, and the whole DLL is exercised through real TSF against a simulated text field, including the engine being killed and restarted mid-typing.",
        "Word ranking is local and instant. A 146,000-word frequency list gives the base order, bigram and trigram counts built from WikiText-103 re-rank by context, and a personal store of word and word-pair counts, never text, boosts what you use and adds names and slang after a few uses. The same tables predict the next word after a space and offer two- and three-word chunks, all in the one popup box, and a typo still finds its word, with the guessed letters drawn in a different colour. A viewer opened from the tray shows the personal dictionary and the stats. Lookups take about a millisecond at the 95th percentile. Phrases are the only thing that leaves the machine, and they are built to be cheap: a plain completion request through OpenRouter to a low-cost model you choose from a list in the config, seeing only the text before the caret and held back when other text follows it on the same line, with the context cut at paragraph boundaries so the provider's prefix cache does the work, a debounce so it asks once per pause, and no spend counting of its own: you cap the key in OpenRouter, and when the cap is hit phrases quietly fall back to words. The key is typed into the config file by the user and is never logged or shown in errors.",
        "It is not finished in every way, and this release is honest about that. The automated tests pass, but the checks that need a person at a real desktop are still open: registering the DLL and using it in real apps, a live phrase request with a real key, the tray icon and pause hotkey, and starting at logon after a reboot. Which OpenRouter models accept plain completions, and cleanly, is untested, so the default model is a starting guess to be swapped after real use. The n-gram corpus is encyclopedic, which suits formal writing more than chat. The harness that tests the real DLL needs the foreground window, so it only runs when nobody is using the machine.",
        "What 1.0 leaves out is as deliberate as what it includes. It does not read the conversation you are replying to, so a Discord phrase knows your sentence but not the message above it; that is the next release. It has no browser extension, no macOS or Linux, no languages other than English, and no cloud account, sync or telemetry: the only network traffic is the phrase request you configured, and the only record of your typing is a set of counts on your own disk. Those limits keep the promise small enough to keep, which is that Completionist starts with Windows, stays out of the way, and shows you the numbers on whether it is paying for itself."
      ]
    },
    {
      "version": "1.1.0",
      "status": "planned",
      "title": "Reply-aware phrases",
      "milestone": "M5",
      "text": "Phrase suggestions learn what you are replying to. When you switch to a window, Completionist reads the surrounding conversation in the background through UI Automation, falling back to Windows’ built-in text recognition for apps that expose little text. That context travels with each phrase request, so a reply in Discord or Slack can follow the last few messages and an email reply can respond to the message it quotes. Reading the screen involves no AI or vision model. The phrase request remains the only model call."
    },
    {
      "version": "1.2.0",
      "status": "planned",
      "title": "Web fields",
      "milestone": "M6",
      "text": "A Chrome extension connects web pages to the same engine through Native Messaging. Inside the browser it can show true inline ghost text instead of a popup row, and it can read context a text service never sees, such as the post you are commenting on. Desktop apps keep using the text service, so the extension adds to what already works and replaces nothing."
    },
    {
      "version": "2.0.0",
      "status": "planned",
      "title": "A popup with a deliberate look",
      "milestone": "M10",
      "essay": [
        "Completionist 2.0 is a redesign of the thing you actually look at. The popup that appears under the caret began as a way to prove that the plumbing worked, and by now it carries single words, next-word predictions, multi-word chunks, letters it guessed after a typo, and a greyed cloud phrase, all in one box. This release gives that box one deliberate look: a single set of Evergreen colours, one type scale, clear rules for which kind of row sits where and how it is marked, and a version that reads well at 100% and 200% scaling and on a second monitor. The goal is that you can tell at a glance, without reading, which row is a word, which is a chunk, which is the cloud phrase and which letters were a guess, and that the popup still feels like a quiet part of whatever app you are in, not a foreign window sitting on top of it.",
        "The reason to do it late is that a design made before the features exist is a guess. By 2.0 the row types and the colour for guessed letters are real, so the redesign can begin from an audit of what actually feels wrong in Notepad, Chrome and Discord, instead of from a blank page. The popup stays what it has always been: a thin C++ window that draws what the engine sends, never takes focus and never consumes a key it shouldn’t. The redesign changes how it looks and reads, not what it is allowed to do. Mockups come first, then design tokens checked against the colour rules in CONTEXT.md, then a rewrite of the drawing code, then harness screenshots at several scales and on more than one monitor.",
        "It costs a rewrite of the most delicate rendering code in the project. That code lives inside every application the user types in, so each change is proved in the harness before it is tried in a real app, and a popup that can’t draw the new design has to fall back to the current one and never crash the host. It also costs restraint. A redesign is an invitation to add themes, settings and animation, and for a typing tool each of those makes it slower to read and easier to distract. The audit will say how far the scope goes, and whether the tray menu and the personal viewer deserve the same treatment or are fine as they are.",
        "What 2.0 leaves out is as deliberate as what it includes. It adds no new suggestion features, no themes or user-editable styles, no settings window, no dashboard and no animation beyond whatever helps the eye follow the highlighted row. It does not change how suggestions are ranked, what leaves the machine, or which keys are taken. Its whole scope is how the existing suggestions look and read, which keeps the release small enough to finish and to test properly. This essay is a plan for now, and it becomes a retrospective of what actually shipped when the release is done."
      ]
    }
  ],
  "stories": {
    "1": "Word list at the caret",
    "2": "Popup exactly at the caret",
    "3": "Popup never takes focus",
    "4": "Tab accepts the highlighted word",
    "5": "Up/Down move the highlight",
    "6": "Esc dismisses the popup",
    "7": "Enter is never consumed",
    "8": "Keys normal when the popup is closed",
    "9": "Close on space or punctuation",
    "10": "Rank by word frequency",
    "11": "Re-rank by the previous words",
    "12": "Match my capitalisation",
    "13": "Never suggest the word I typed",
    "14": "Follow the caret on click and arrows",
    "15": "Learn new words after a few uses",
    "16": "Boost words I use often",
    "17": "Learning stores counts only",
    "18": "Phrase row after a ~350 ms pause",
    "19": "Phrase row highlighted by default",
    "20": "Ctrl+Right accepts the next phrase word",
    "21": "150 ms no-steal window",
    "22": "Phrase streams in",
    "23": "Trim the phrase as I type along",
    "24": "Cancel when I diverge",
    "25": "Ctrl+Space phrase hotkey",
    "26": "No cloud calls outside the allow-list",
    "27": "FIM: the model sees text after the caret",
    "28": "App name and title sent to the model",
    "29": "Configurable context cap",
    "30": "Quiet fallback to words",
    "31": "Silent in password fields",
    "32": "Silent in URL, email and number fields",
    "33": "Silent in editors and terminals",
    "34": "Never hang or crash the app",
    "35": "Survive engine restarts",
    "36": "Correct at any DPI and on any monitor",
    "37": "One TOML config file",
    "38": "Config changes apply live",
    "39": "Switch provider or model in config",
    "40": "API keys from env vars",
    "41": "Pause and resume",
    "42": "Start at login",
    "43": "Registered English input method",
    "44": "Install and uninstall scripts",
    "45": "Provider benchmark",
    "46": "Time-to-first-token vs context size",
    "47": "Spike findings note",
    "48": "Command-line probe",
    "49": "Pipe + lookup under 10 ms p95",
    "50": "Timing and error logs",
    "51": "Pure, tested key router",
    "52": "Shown and accepted per app",
    "53": "Keystrokes saved",
    "54": "Provider latency and errors",
    "55": "Stats summary",
    "56": "Metrics hold counts only",
    "57": "Reply-aware context",
    "58": "Context read in the background",
    "59": "Heavy use under $0.50 a day",
    "60": "Daily spending cap",
    "61": "Token and cost tracking",
    "62": "Next word after a space",
    "63": "Multi-word chunks",
    "64": "Everything in one popup box",
    "65": "Stay quiet when unsure",
    "66": "Learn only what I typed and finished",
    "67": "Forget a word",
    "68": "See my personal dictionary",
    "69": "See my stats in a viewer",
    "70": "Open the viewer from the tray",
    "71": "Suggest through typos",
    "72": "Mark the guessed letters",
    "73": "A guess never outranks a real prefix match",
    "74": "One deliberate popup design"
  },
  "decisions": [
    {
      "date": "2026-09-30",
      "text": "Hands-on live checks (app matrix, live provider, logon reboot, ranking checks) no longer gate a milestone. Completionist is a personal tool, so it counts as shipped once it works in the owner's daily use; the four pure check tasks were dropped."
    },
    {
      "date": "2026-09-30",
      "text": "The API key moved from an environment variable into the config file (`[phrase] api_key`), on the owner's request; this reverses the earlier rule that the config only names a variable. It is plain text in `%APPDATA%\\Completionist\\config.toml`, outside the repo, and is kept out of logs, errors and printed settings. Editing it applies on hot reload. The key is still never asked for in chat. The default phrase model is meta-llama/llama-3.3-70b-instruct with provider_order [\"Groq\"]."
    },
    {
      "date": "2026-09-30",
      "text": "The phrase prompt changed again, to a version that adds a \"do not over-complete\" section: stop before details only the writer knows, so Tab saves typing without writing the person's thoughts. It supersedes the 30-rule prompt from earlier today. Provider preference is `[phrase] provider_order`, sent to OpenRouter as `provider.order`; fallbacks stay on, so it prefers a provider and never requires one."
    },
    {
      "date": "2026-09-30",
      "text": "The phrase system prompt is the owner's own 30-rule version with two worked examples, kept at the start of every request so providers can cache it. It is much longer than before, so a model that still answers instead of continuing, or a provider that charges by prompt length, is worth checking in the live phrase test (M3.11)."
    },
    {
      "date": "2026-09-30",
      "text": "The personal log stores words, word pairs and trigrams as counts on the local disk, never sent anywhere. A word or trigram is learned only when it was typed (not pasted), finished without correcting, and used 3 times; trigrams never span punctuation or a window switch. No per-day rule, decay or never-learn list until junk shows up."
    },
    {
      "date": "2026-09-30",
      "text": "Typo tolerance uses an existing library (SymSpell, `symspellpy`), not a hand-made matcher. Try it first and swap to `rapidfuzz` or a trie only if it can't handle half-typed words."
    },
    {
      "date": "2026-09-30",
      "text": "No TUI and no dashboard: a writing tool should stay out of the way. Instead a read-only viewer for the personal dictionary and stats, opened from the tray as a local HTML snapshot with no server. Forgetting words stays in the `completionist-words` command. Any public demo uses made-up data and needs the owner's OK before publishing; real data is never published."
    },
    {
      "date": "2026-09-30",
      "text": "Milestone numbers don't match release order (M5 is 1.1.0; the new M7 to M10 are 0.5.0, 0.6.0, 0.7.0 and 2.0.0). The pages will read releases first (M1.17), with the M-number as a small tag."
    },
    {
      "date": "2026-09-30",
      "text": "The base n-gram tables (0.8M bigrams, 1.6M trigrams from WikiText-103) are small for next-word prediction. Rebuild them with a looser prune and a chat-style corpus only if the live check (M7.9) says they feel thin."
    },
    {
      "date": "2026-09-29",
      "text": "The app is named Completionist (it was Typer, a working name). The rename covers code identifiers as well as display text, so existing installs need `scripts/install.ps1` run again, and the engine moves the old `Typer` data and config folders on first start."
    },
    {
      "date": "2026-09-29",
      "text": "The roadmap is drawn as a highway map. Each part of the app is a highway (a task's area picks it), major releases are interchanges where every highway meets, minor releases are stations across the highways their milestone touched, patches are stops, and bug-fix patches (`\"kind\": \"fix\"`) are cul-de-sacs. The official color scheme is Evergreen: pine-green signs and highways, with magenta marking now and next (values in the design spec, and in CONTEXT.md once it is written)."
    },
    {
      "date": "2026-09-29",
      "text": "Provider: OpenRouter only. Models are a list in the config (tried in order), prefix-only with a same-line guard, no FIM. DeepSeek was removed as an option. No benchmark tool: read the model cards."
    },
    {
      "date": "2026-09-29",
      "text": "N-gram corpus is WikiText-103 (encyclopedic English, from Hugging Face; the original S3 link is gone). It helps formal writing most; a chat-style corpus such as subtitles would help casual text and can be added later."
    },
    {
      "date": "2026-09-29",
      "text": "Releases follow semantic versioning, and each gets writing that matches its size: one sentence for a patch, a paragraph for a minor release (one per milestone), an essay for a major release."
    },
    {
      "date": "2026-09-29",
      "text": "The roadmap pages are plain HTML files opened from disk, reading `roadmap.js`. They are not published anywhere."
    },
    {
      "date": "2026-09-29",
      "text": "Build Completionist as a TSF text service (an IME). The spike confirmed caret rect, real context, key capture and insertion in Notepad and Chromium/Electron apps."
    },
    {
      "date": "2026-09-29",
      "text": "Thin C++ DLL, all intelligence in one Python engine process, named pipe with length-prefixed JSON between them."
    },
    {
      "date": "2026-09-29",
      "text": "Phrases come from cheap models through OpenRouter (the only key the user has), never premium ones. Typer does not count spend; the user sets a limit on the key."
    },
    {
      "date": "2026-09-29",
      "text": "No AI or vision for caret or screen tracking. TSF gives the caret; UI Automation reads context after v1."
    },
    {
      "date": "2026-09-29",
      "text": "Google Docs and the Google search box are out of scope: canvas rendering and Google's own suggestion dropdown."
    }
  ],
  "risks": [
    {
      "text": "The OpenRouter key is plain text in config.toml, so anyone who can read the user's profile, a backup or a shared copy of the file can use it.",
      "status": "open",
      "mitigation": "The file lives outside the repo, the key is never logged or printed, and the key on OpenRouter has a spending limit. Revoke and replace it if the file is ever shared."
    },
    {
      "text": "A popup after every space could be noisy, and Tab could be stolen right after a space.",
      "status": "open",
      "mitigation": "A confidence threshold that can be switched off, and next-word rows not highlighted by default (M7.4, M7.8)."
    },
    {
      "text": "Typo matches could push out correct rare words or slow lookups.",
      "status": "open",
      "mitigation": "Exact prefix matches always rank first, no fuzzing of short fragments, names or learned words, and a p95 under 10 ms check (M9.2, M9.5)."
    },
    {
      "text": "The personal log holds trigrams, which read more like your writing than single words do.",
      "status": "open",
      "mitigation": "Local disk only, silent fields and blocked apps teach nothing, a forget command, and no real data in any public demo."
    },
    {
      "text": "A crash in the DLL takes down the app the user is typing in.",
      "status": "open",
      "mitigation": "Keep the DLL minimal, catch everything at COM boundaries, debug in a throwaway app first."
    },
    {
      "text": "Older Electron apps may use the legacy IMM32 path with weaker context access.",
      "status": "open",
      "mitigation": "Include an older Electron app in the M1 manual matrix."
    },
    {
      "text": "The keyboard doesn't appear in Settings without an icon.",
      "status": "open",
      "mitigation": "M1.11: add an icon and an enable-keyboard step."
    },
    {
      "text": "RequestEditSession returns E_FAIL in Chromium apps during focus changes.",
      "status": "mitigated",
      "mitigation": "Treat it as transient and retry on the next edit."
    },
    {
      "text": "Chromium might expose only a small window of text through TSF.",
      "status": "retired",
      "mitigation": "Discord exposed 2,239 characters and Notepad the whole document."
    }
  ],
  "log": [
    {
      "date": "2026-09-30",
      "text": "M7.3 done: PersonalStore.words(search) lists learned words and forget(word) removes a word from the counts, pairs, triples and the saved file (built on request; no command line, the M8 viewer will call these)."
    },
    {
      "date": "2026-09-30",
      "text": "M7.6 done: replies carry a kind per suggestion (word, chunk or next) and accept events may name chunk or next; engine and DLL both tested. Chunks are behind [words] chunks (off until M7.7)."
    },
    {
      "date": "2026-09-30",
      "text": "Pushed main and tag v0.4.1 to origin; engine tests pass on main (422)."
    },
    {
      "date": "2026-09-30",
      "text": "Release 0.4.1: merged m7-next-words and m7-words-command into main; .commandcode is now gitignored and untracked."
    },
    {
      "date": "2026-09-30",
      "text": "M7.5 done: WordCompleter.chunks extends the top words into two- or three-word chunks while the next word is likely; wiring into replies is M7.6."
    },
    {
      "date": "2026-09-30",
      "text": "M7.4 done: next-word candidates after a space with a confidence threshold, behind [words] next (off until M7.8). M7.3 is left to the user, who is doing it hands-on."
    },
    {
      "date": "2026-09-30",
      "text": "M7.3 started as a guided learning task on branch m7-words-command: the user writes the `completionist-words` command with tutoring."
    },
    {
      "date": "2026-09-30",
      "text": "M7.2 done: the personal store counts trigrams (two words of context, same sentence only) next to words and pairs; counts only, saved locally."
    },
    {
      "date": "2026-09-30",
      "text": "M7.1 done: a word backspaced over while typing is never learned (the 3-use admission already existed as promote_after)."
    },
    {
      "date": "2026-09-30",
      "text": "Pushed main and tag v0.4.0 to origin."
    },
    {
      "date": "2026-09-30",
      "text": "Release 0.4.0 (Daily driver): M4 done; M4.2 and M4.4 set done (scripts parse, tests pass); Spike keyboard removal moved to M9.8; M7 is active (0.5.0 next; M5 comes after 1.0)."
    },
    {
      "date": "2026-09-30",
      "text": "Pushed main and tag v0.3.0 to origin."
    },
    {
      "date": "2026-09-30",
      "text": "Release 0.3.0 (Phrase suggestions): M3 done, tests pass; live provider checks left to daily use; M4 is active."
    },
    {
      "date": "2026-09-30",
      "text": "Pushed main and tag v0.2.0 to origin."
    },
    {
      "date": "2026-09-30",
      "text": "Release 0.2.0 (Smarter words): M2 done, reviewed against the real n-gram file (know ranks above knowledge after I'd like to, misspellings filtered); 0.3.0 is next."
    },
    {
      "date": "2026-09-30",
      "text": "Deleted all merged branches, local and on origin; main is the only branch."
    },
    {
      "date": "2026-09-30",
      "text": "Pushed main and tag v0.1.0. All other branches are merged into main and ready to delete."
    },
    {
      "date": "2026-09-30",
      "text": "Released 0.1.0 (M1 done): finished M1.10 and M1.11 as built and harness-tested, moved the Spike-keyboard cleanup to M4.11, made M2 active and 0.2.0 next."
    },
    {
      "date": "2026-09-30",
      "text": "Moved the README demo GIF and web demo from M4.10 to M9.7 (0.7.0, the last release before 1.0), so M4 has no open tasks besides the install run."
    },
    {
      "date": "2026-09-30",
      "text": "Decision: live checks no longer gate milestones (personal tool). Dropped M1.12, M2.7, M3.11 and M4.9; M2 and M3 now have nothing left open."
    },
    {
      "date": "2026-09-30",
      "text": "Released 0.0.12 and finished M1.17: the task board names milestones by release, leads rows with the title, sorts by status; CLAUDE.md and CONTEXT.md reworded. Every remaining M1 to M4 task needs the user's hands-on checks."
    },
    {
      "date": "2026-09-30",
      "text": "Released 0.0.11: engine/completionist.py, so the engine starts with `cd engine && uv run completionist.py`."
    },
    {
      "date": "2026-09-30",
      "text": "Released 0.0.10 (merged into main): the \"do not over-complete\" phrase prompt (ends with a TEXT TO CONTINUE label; build_prompt adds the header and text), `[phrase] provider_order`, default model meta-llama/llama-3.3-70b-instruct on Groq, and `[phrase] api_key` in the config instead of the OPENROUTER_API_KEY environment variable (`api_key_env` removed; docs, tray template, install script and harness updated)."
    },
    {
      "date": "2026-09-30",
      "text": "Released 0.0.9: replaced the phrase system prompt (`INSTRUCTIONS` in context.py) with the owner's 30-rule prompt and two examples, without the trailing header line since build_prompt adds it; merged the M7 to M10 plan into main."
    },
    {
      "date": "2026-09-30",
      "text": "Planned M7 (n-gram suggestions and personal log, 0.5.0), M8 (personal viewer, 0.6.0), M9 (typo-tolerant words, 0.7.0) and M10 (UI redesign, 2.0.0), plus M1.17 (easier roadmap reading) and M4.10 (demo GIF); stories 62 to 74, decisions and risks added."
    },
    {
      "date": "2026-09-29",
      "text": "Released 0.0.8: continue-only phrase prompt and no price counting, merged into main after the Completionist rename."
    },
    {
      "date": "2026-09-29",
      "text": "Removed the daily budget and price counting; the user limits the OpenRouter key instead. Metrics drop the cost column."
    },
    {
      "date": "2026-09-29",
      "text": "Released 0.0.7: merged main (0.0.6, OpenRouter phrases) into the highway-map and Completionist rename work, and released it."
    },
    {
      "date": "2026-09-29",
      "text": "Renamed the app from Typer to Completionist across code, scripts, tests, docs and the roadmap pages (M1.16), with a one-time move of old data folders."
    },
    {
      "date": "2026-09-29",
      "text": "M1.15 done: the highway map, the Evergreen task board and CONTEXT.md."
    },
    {
      "date": "2026-09-29",
      "text": "CONTEXT.md: product, architecture, glossary, release and map rules, and the official Evergreen color scheme and typography."
    },
    {
      "date": "2026-09-29",
      "text": "Retired releases.html (its writing now lives in the map's travel guide and itinerary); README and CLAUDE.md point at the map and the task board."
    },
    {
      "date": "2026-09-29",
      "text": "Map page sections: masthead sign, now sign, mile-marker posts, highway spotlight, legend, travel guide, road work and itinerary."
    },
    {
      "date": "2026-09-29",
      "text": "Map drawing (map.js): highways with casing, built/construction/planned/idle road, barricades, stations, exit and gantry signs, shields, cul-de-sacs, you-are-here pin, lane labels."
    },
    {
      "date": "2026-09-29",
      "text": "Map geometry (map-layout.js) with node tests: events, lanes, interchange bundles, highway paths, road pieces by milestone progress, barricades, sign rows."
    },
    {
      "date": "2026-09-29",
      "text": "Task board moved to tasks.html and restyled in Evergreen, with highway stripes, a highway filter and highway strips on milestone cards."
    },
    {
      "date": "2026-09-29",
      "text": "Started the highway-map roadmap (M1.15): highways in the data and on patch releases, checker rules for them, the Evergreen theme.css, shared common.js, a CLAUDE.md rule that every commit updates this file, and a spec plus plan for the map page, restyled task board and CONTEXT.md."
    },
    {
      "date": "2026-09-29",
      "text": "Released 0.0.6: OpenRouter-only phrases merged into main."
    },
    {
      "date": "2026-09-29",
      "text": "Removed DeepSeek and the completionist-bench tool; phrase models are now a list of OpenRouter model names. M3.1/M3.2 dropped."
    },
    {
      "date": "2026-09-29",
      "text": "Phrases work without FIM: prefix-only requests are held back when text follows the caret on the same line. OpenRouter candidates added to the benchmark file."
    },
    {
      "date": "2026-09-29",
      "text": "Released 0.0.5: merged m3-phrases (M1-M4 code) into main. Live checks in real apps remain open."
    },
    {
      "date": "2026-09-29",
      "text": "READMEs rewritten (install, keys, phrases, tray, config) and the 1.0.0 essay redrafted as a retrospective, with the open live checks stated plainly."
    },
    {
      "date": "2026-09-29",
      "text": "M4 built: metrics, tray, pause hotkey, hot-reloaded config, engine.log, completionist-stats, phrase accept kinds, install/uninstall scripts, and an engine kill/restart harness scenario. Live install check (M4.9) is the user's."
    },
    {
      "date": "2026-09-29",
      "text": "M3 DLL side done: phrase row, Ctrl+Space, Ctrl+Right, no-steal window, Esc quieting. The TSF harness now runs both phrase modes against a fake provider and all checks pass. Live check with a real provider key is M3.11."
    },
    {
      "date": "2026-09-29",
      "text": "M3 engine side done on branch m3-phrases: provider, anchored context, daily budget, scheduler, phrase service, pushes over the pipe and completionist-bench. 292 engine tests pass. Next: the phrase row in the DLL (M3.8) and the Ctrl+Space hotkey (M3.9). Benchmark runs are blocked on your API keys."
    },
    {
      "date": "2026-09-29",
      "text": "Added a TSF harness that runs the real DLL through real TSF with the real engine and a simulated text field: 31 checks pass, popup rendering reviewed from screenshots. Real-app checks (M1.12, M2.7) still need the DLL registered, which needs your UAC click."
    },
    {
      "date": "2026-09-29",
      "text": "M2 engine work done on branch m1-tsf-dll: n-gram tables built from WikiText-103, bigram/trigram ranking, misspelling filter, personal learning and accept events. 197 engine tests pass; p95 lookup 1.2 ms. Waiting on your live checks (M1.10-M1.12, M2.7) before merging."
    },
    {
      "date": "2026-09-29",
      "text": "M1 code complete on branch m1-tsf-dll: real DLL, key router, engine client, popup, icon and install scripts. Native tests, the engine end-to-end check and the lifecycle smoke test pass. Popup look and the manual app matrix (M1.10-M1.12) still need you."
    },
    {
      "date": "2026-09-29",
      "text": "Released v0.0.4: merged the engine and spike branches into main (M1.5 done). Added the release timeline, switched the data to `roadmap.js` so both pages open from disk, and split the post-v1 work into M5 and M6."
    },
    {
      "date": "2026-09-29",
      "text": "Roadmap created from PRD #1 and the M0 spike verdict. M1 is active: next are the branch merge, the TSF service shell and the key router."
    },
    {
      "date": "2026-09-29",
      "text": "M0 done: TSF works for Completionist (spike round 2 confirmed long Electron context and input scope)."
    },
    {
      "date": "2026-09-29",
      "text": "Engine word completion served over a named pipe, 74 tests passing (branch m1-engine-words)."
    }
  ]
};
