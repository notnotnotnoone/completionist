// Completionist roadmap data, read by index.html (the highway map) and tasks.html (the task board).
// Everything after the `=` is strict JSON: double quotes, no comments, no trailing commas.
// Update it with every change (see CLAUDE.md), then run: python scripts/check_roadmap.py
window.COMPLETIONIST_ROADMAP = {
  "schema": 3,
  "project": "Completionist",
  "tagline": "System-wide, VS Code-style English autocomplete for Windows",
  "updated": "2026-09-29",
  "prd": "https://github.com/notnotnotnoone/typer/issues/1",
  "repo": "https://github.com/notnotnotnoone/typer",
  "now": "Try the real text service and learning in real apps (your checklist), then merge branch m1-tsf-dll to main as 0.0.5",
  "highways": [
    {
      "id": "engine",
      "title": "Engine",
      "blurb": "Python: words, phrases, learning, metrics",
      "areas": [
        "engine",
        "data",
        "bench"
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
      "status": "active",
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
          "status": "doing",
          "area": "dll",
          "stories": [
            2,
            3,
            36
          ],
          "notes": "Built and checked in the harness: word rows, typed prefix highlighted in blue, highlight row moves, popup opens just under the caret (screenshots reviewed), never takes focus, click-through, per-window DPI. Still needs your look on a second monitor and in a real app at 150% scaling.",
          "refs": [
            "tip/src/popup.cpp"
          ]
        },
        {
          "id": "M1.11",
          "title": "Keyboard icon and an enable-keyboard step",
          "status": "doing",
          "area": "install",
          "stories": [
            43
          ],
          "notes": "Icon (`tip/assets/completionist.ico`) is compiled into the DLL, and `tip/register.ps1` plus `tip/enable-keyboard.ps1` do the install. Waiting for you to confirm Completionist shows up in Settings and can be switched to.",
          "refs": [
            "tip/register.ps1",
            "tip/enable-keyboard.ps1"
          ]
        },
        {
          "id": "M1.12",
          "title": "Manual app matrix for the real DLL",
          "status": "next",
          "area": "test",
          "stories": [
            2,
            31,
            36
          ],
          "notes": "Notepad, Chrome (textarea, contenteditable, long text, password field, after `chrome://restart`), Edge, Discord, Slack, Obsidian, an older Electron app. 100% and 150% scaling. Enter still sends in Discord. The harness (M1.14) already covers the logic; this checks each app's own quirks. Switch to the Completionist keyboard first, so the old Completionist Spike keyboard isn't also active."
        },
        {
          "id": "M1.13",
          "title": "Remove the Completionist Spike keyboard and unregister it",
          "status": "todo",
          "area": "install",
          "stories": [],
          "notes": "The user removes it from the language list, then runs `tip/spike/register.ps1 -Unregister`."
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
        }
      ]
    },
    {
      "id": "M2",
      "title": "Smarter words",
      "status": "planned",
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
          "status": "next",
          "area": "test",
          "stories": [
            11,
            15,
            16
          ],
          "notes": "Needs you: with the engine running and the Completionist keyboard on, type a made-up word (like \"zorblax\") three or more times, then confirm it shows up as a suggestion. Also check \"I'd like to kn\" puts \"know\" first."
        }
      ]
    },
    {
      "id": "M3",
      "title": "Phrase suggestions",
      "status": "planned",
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
          "title": "Daily budget cap and per-request cost tracking",
          "status": "done",
          "area": "engine",
          "stories": [
            60,
            61
          ],
          "notes": "`DailyBudget` prices each request from the provider's cached, uncached and output token counts, saves the day's total in `spend.json`, resets at midnight and stops requests at `daily_budget_usd` (default 0.50). Only totals are stored.",
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
          "notes": "A provider error ends the phrase quietly (word suggestions carry on); three failures in a row pause phrases for 30 seconds; with no key, no budget or phrases switched off, nothing is ever requested.",
          "refs": [
            "engine/src/completionist_engine/phrases.py"
          ]
        },
        {
          "id": "M3.11",
          "title": "Check phrases live with a real provider and key",
          "status": "next",
          "area": "test",
          "stories": [
            18,
            19,
            25,
            59
          ],
          "notes": "Needs you: `setx OPENROUTER_API_KEY \"...\"` in a terminal (never in chat), restart the engine, then type in Notepad (allow-listed, phrases appear on their own) and press Ctrl+Space in Discord. `spend.json` in `%LOCALAPPDATA%\\Completionist` shows what it cost."
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
      "status": "planned",
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
          "status": "doing",
          "area": "install",
          "stories": [
            42
          ],
          "notes": "scripts/install.ps1 creates the logon task (pythonw, no console). Written and syntax-checked; not yet run, because it needs the user (UAC, keyboard setting).",
          "refs": [
            "branch m3-phrases",
            "engine/src/completionist_engine",
            "scripts/",
            "tip/tests"
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
          "status": "doing",
          "area": "install",
          "stories": [
            43,
            44
          ],
          "notes": "install.ps1 / uninstall.ps1 written and syntax-checked; the user runs them (UAC and language settings are not mine to change).",
          "refs": [
            "branch m3-phrases",
            "engine/src/completionist_engine",
            "scripts/",
            "tip/tests"
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
          "status": "next",
          "area": "install",
          "stories": [
            41,
            42,
            43,
            44
          ],
          "notes": "User runs .\\scripts\\install.ps1, checks the tray icon and Ctrl+Alt+P, reboots once to see the engine start by itself."
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
      "version": "0.1.0",
      "status": "next",
      "title": "Words everywhere",
      "milestone": "M1",
      "text": "This is the first release you can type with. The real text service replaces the spike and connects to the engine, so a list of word completions appears at the caret in Notepad, Chrome, Edge, Discord and other Electron apps. Tab accepts the highlighted word, the arrow keys move through the list and Esc closes it. Enter is never intercepted, so chat messages still send. Password, URL, email and number fields stay silent, as do code editors and terminals. If the engine stops, apps carry on as if Completionist weren’t installed and reconnect when it returns. A keyboard icon lets Windows list Completionist in Settings, so it can be switched on without PowerShell."
    },
    {
      "version": "0.2.0",
      "status": "planned",
      "title": "Smarter words",
      "milestone": "M2",
      "text": "Word suggestions start to take the sentence and the writer into account. Bigram and trigram tables built from a public corpus re-rank completions by the previous one or two words, so after “I’d like to” the list offers “know” before “knowledge”. Common misspellings such as “tomorow” are filtered out of the base vocabulary. A personal store boosts the words you actually use and adds new ones, such as names and slang, after a few uses. It keeps word counts only and never the text itself."
    },
    {
      "version": "0.3.0",
      "status": "planned",
      "title": "Phrase suggestions",
      "milestone": "M3",
      "text": "The popup gains a greyed phrase row at the top, a continuation of your sentence written by a cheap and fast cloud completion model. In allow-listed apps it appears after a short pause and streams in as it is generated. It is highlighted by default, so Tab takes the whole phrase, and Ctrl+Right takes it one word at a time. In any other app, Ctrl+Space asks for a phrase on demand. Phrases come through OpenRouter from a list of cheap models you name in the config, each tried in turn if the one before fails. An anchored context window keeps requests cache-friendly so heavy use costs under fifty cents a day, and a daily cap enforces the limit. When the provider is slow or unavailable, Completionist quietly falls back to words."
    },
    {
      "version": "0.4.0",
      "status": "planned",
      "title": "Daily driver",
      "milestone": "M4",
      "text": "This release covers what it takes to leave Completionist running all day. The engine starts at logon, a tray icon pauses and resumes it, and edits to the config file apply without a restart. One script installs Completionist and another removes it. A local metrics store records how often suggestions are shown and accepted, how many keystrokes they save and how each phrase provider performs. It keeps counts and timings only. A stats summary shows whether Completionist is paying off."
    },
    {
      "version": "1.0.0",
      "status": "planned",
      "title": "Autocomplete for everything you type",
      "essay": [
        "Completionist 1.0 puts the suggestions of a code editor into every place you write English on Windows. Type a few letters in Discord, a browser text box or an email and a short list of completions appears just under the caret, ranked by the words before it and by the words you actually use. Tab takes the top one. In apps you choose, and on Ctrl+Space everywhere else, a greyed phrase appears above the list, a continuation of your sentence that you take whole with Tab or one word at a time with Ctrl+Right. Enter is never touched, and nothing in a password field is read or sent.",
        "The unusual decision is that Completionist is a Text Services Framework text service, the mechanism Windows uses for input methods, and not a keyboard hook with a screen-scraping overlay. That choice cost a milestone of throwaway spiking and a C++ DLL that lives inside every app, but it is why the caret position, the surrounding text, the key handling and the insertion of accepted text all come from Windows itself and work the same in Notepad, Chrome, Edge, Discord and Teams. The DLL stays deliberately thin: it draws the popup, decides which keys it consumes, and talks over a named pipe to a Python engine, reconnecting quietly if the engine is gone. The key router is pure logic with native tests, and the whole DLL is exercised through real TSF against a simulated text field, including the engine being killed and restarted mid-typing.",
        "Word ranking is local and instant. A 146,000-word frequency list gives the base order, bigram and trigram counts built from WikiText-103 re-rank by context, and a personal store of word and word-pair counts, never text, boosts what you use and adds names and slang after a few uses. Lookups take about a millisecond at the 95th percentile. Phrases are the only thing that leaves the machine, and they are built to be cheap: a plain completion request through OpenRouter to a low-cost model you choose from a list in the config, seeing only the text before the caret and held back when other text follows it on the same line, with the context cut at paragraph boundaries so the provider's prefix cache does the work, a debounce so it asks once per pause, and a hard daily budget of fifty cents after which phrases simply stop. The key lives in an environment variable and never in a config file.",
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
    "61": "Token and cost tracking"
  },
  "decisions": [
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
      "text": "Phrases come from cheap completion models through OpenRouter (the only key the user has), never premium chat models. Budget under $0.50 a day via a daily cap."
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
