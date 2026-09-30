// Typer roadmap data, read by index.html (tasks) and releases.html (release timeline).
// Everything after the `=` is strict JSON: double quotes, no comments, no trailing commas.
// Update it with every change (see CLAUDE.md), then run: python scripts/check_roadmap.py
window.TYPER_ROADMAP = {
  "schema": 2,
  "project": "Typer",
  "tagline": "System-wide, VS Code-style English autocomplete for Windows",
  "updated": "2026-09-29",
  "prd": "https://github.com/notnotnotnoone/typer/issues/1",
  "repo": "https://github.com/notnotnotnoone/typer",
  "now": "Build the real TSF text service and its key router",
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
            "tip/spike/TyperSpike.cpp",
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
          "notes": "Verdict: TSF works for Typer.",
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
            "engine/src/typer_engine/protocol.py",
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
            "engine/src/typer_engine/words.py",
            "engine/src/typer_engine/vocabulary.py"
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
            "engine/src/typer_engine/config.py",
            "engine/src/typer_engine/policy.py"
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
            "engine/src/typer_engine/server.py",
            "engine/src/typer_engine/probe.py"
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
          "id": "M1.10",
          "title": "Popup rendering at the caret",
          "status": "doing",
          "area": "dll",
          "stories": [
            2,
            3,
            36
          ],
          "notes": "Built: word rows, typed prefix highlighted, never takes focus, click-through, per-window DPI, flips above the caret or slides left to stay on the caret's monitor. Waiting for your look at 100% and 150% scaling and on a second monitor.",
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
          "notes": "Icon (`tip/assets/typer.ico`) is compiled into the DLL, and `tip/register.ps1` plus `tip/enable-keyboard.ps1` do the install. Waiting for you to confirm Typer shows up in Settings and can be switched to.",
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
          "notes": "Notepad, Chrome (textarea, contenteditable, long text, password field, after `chrome://restart`), Edge, Discord, Slack, Obsidian, an older Electron app. 100% and 150% scaling. Enter still sends in Discord. The user tests; give a checklist and read the logs."
        },
        {
          "id": "M1.13",
          "title": "Remove the Typer Spike keyboard and unregister it",
          "status": "todo",
          "area": "install",
          "stories": [],
          "notes": "The user removes it from the language list, then runs `tip/spike/register.ps1 -Unregister`."
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
          "status": "todo",
          "area": "data",
          "stories": [
            11
          ],
          "notes": "Wikipedia plus subtitles, stored compactly (SQLite or marisa-trie), not as in-memory dicts."
        },
        {
          "id": "M2.2",
          "title": "N-gram re-ranking in the word completer",
          "status": "todo",
          "area": "engine",
          "stories": [
            11
          ]
        },
        {
          "id": "M2.3",
          "title": "Filter misspellings and junk out of the base vocabulary",
          "status": "todo",
          "area": "engine",
          "stories": [
            10
          ],
          "notes": "wordfreq includes entries like \"tomorow\"."
        },
        {
          "id": "M2.4",
          "title": "Personal store: accept and typed-word counts in SQLite",
          "status": "todo",
          "area": "engine",
          "stories": [
            16,
            17
          ],
          "notes": "Counts only, never raw text."
        },
        {
          "id": "M2.5",
          "title": "Promote new words to the vocabulary after N uses",
          "status": "todo",
          "area": "engine",
          "stories": [
            15
          ]
        },
        {
          "id": "M2.6",
          "title": "Accept events name the accepted item",
          "status": "todo",
          "area": "dll",
          "stories": [
            16
          ],
          "notes": "Protocol change on both sides so the engine can learn from accepts."
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
          "status": "todo",
          "area": "bench",
          "stories": [
            45
          ],
          "notes": "Candidates: DeepSeek-Flash FIM (leading), Codestral FIM, Qwen 3.5 Flash, Groq gpt-oss-20b, Ministral 3B, Gemini 3.1 Flash-Lite. Cheap models only, never premium chat models."
        },
        {
          "id": "M3.2",
          "title": "Benchmark time-to-first-token against context size",
          "status": "todo",
          "area": "bench",
          "stories": [
            46
          ],
          "notes": "Sets the default context cap (starting at 8k characters before, 2k after)."
        },
        {
          "id": "M3.3",
          "title": "Phrase provider: OpenAI-compatible /completions with optional FIM",
          "status": "todo",
          "area": "engine",
          "stories": [
            22,
            27,
            28,
            29,
            39,
            40
          ],
          "notes": "Streaming. API key read from an env var named in config; the user sets it with `setx`, never in chat."
        },
        {
          "id": "M3.4",
          "title": "Cache-friendly anchored context window",
          "status": "todo",
          "area": "engine",
          "stories": [
            59
          ],
          "notes": "The window start moves only in large steps so consecutive requests share a prefix and hit the provider cache (about $0.31 vs $1.56 a day at peak)."
        },
        {
          "id": "M3.5",
          "title": "Daily budget cap and per-request cost tracking",
          "status": "todo",
          "area": "engine",
          "stories": [
            60,
            61
          ],
          "notes": "`daily_budget_usd` defaults to 0.50. Spend comes from provider usage data."
        },
        {
          "id": "M3.6",
          "title": "Phrase scheduler state machine",
          "status": "todo",
          "area": "engine",
          "stories": [
            18,
            23,
            24,
            26
          ],
          "notes": "350 ms debounce, cancel on divergence, typeahead trim, no requests when gated. Tested with a fake clock."
        },
        {
          "id": "M3.7",
          "title": "Phrase push messages over the pipe",
          "status": "todo",
          "area": "engine",
          "stories": [
            22
          ],
          "notes": "Keyed by the originating request id, carrying the text so far and a streaming flag."
        },
        {
          "id": "M3.8",
          "title": "Phrase row in the popup",
          "status": "todo",
          "area": "dll",
          "stories": [
            18,
            19,
            20,
            21
          ],
          "notes": "Greyed top row, highlighted by default, Ctrl+Right accepts the next word, 150 ms no-steal window."
        },
        {
          "id": "M3.9",
          "title": "Ctrl+Space hotkey requests a phrase in any app",
          "status": "todo",
          "area": "dll",
          "stories": [
            25
          ]
        },
        {
          "id": "M3.10",
          "title": "Fall back to words when the provider is slow or down",
          "status": "todo",
          "area": "engine",
          "stories": [
            30
          ]
        }
      ]
    },
    {
      "id": "M4",
      "title": "Daily-driver polish",
      "status": "planned",
      "goal": "Typer runs all day without attention and shows whether it's paying off.",
      "done_when": "One-command install and uninstall, autostart at logon, tray pause, hot-reloaded config and a stats summary.",
      "tasks": [
        {
          "id": "M4.1",
          "title": "Tray icon with pause/resume and a global hotkey",
          "status": "todo",
          "area": "engine",
          "stories": [
            41
          ]
        },
        {
          "id": "M4.2",
          "title": "Start the engine at logon",
          "status": "todo",
          "area": "install",
          "stories": [
            42
          ]
        },
        {
          "id": "M4.3",
          "title": "Hot-reload the config file",
          "status": "todo",
          "area": "engine",
          "stories": [
            38
          ]
        },
        {
          "id": "M4.4",
          "title": "Install and uninstall scripts",
          "status": "todo",
          "area": "install",
          "stories": [
            43,
            44
          ],
          "notes": "Register the TIP, enable the keyboard, create the logon task. Admin steps need the user's UAC approval."
        },
        {
          "id": "M4.5",
          "title": "Metrics store: shown, accepted, keystrokes saved, provider latency",
          "status": "todo",
          "area": "engine",
          "stories": [
            52,
            53,
            54,
            56
          ],
          "notes": "Counts and timings only, never typed text."
        },
        {
          "id": "M4.6",
          "title": "Stats summary from the CLI or tray",
          "status": "todo",
          "area": "engine",
          "stories": [
            55
          ]
        },
        {
          "id": "M4.7",
          "title": "Engine logging of request timings and provider errors",
          "status": "todo",
          "area": "engine",
          "stories": [
            50
          ]
        },
        {
          "id": "M4.8",
          "title": "Resilience pass: engine crashes and restarts",
          "status": "todo",
          "area": "dll",
          "stories": [
            34,
            35
          ],
          "notes": "Kill the engine mid-typing: apps behave as if Typer weren't installed, then reconnect."
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
      "text": "The Typer repository started with a README, a gitignore and a standalone uv project for the Python engine."
    },
    {
      "version": "0.0.2",
      "status": "released",
      "date": "2026-09-29",
      "title": "Engine word completion",
      "text": "The engine completes words from a 146,000-word frequency list and answers over a named pipe in under 10 ms at the 95th percentile, backed by 74 tests."
    },
    {
      "version": "0.0.3",
      "status": "released",
      "date": "2026-09-29",
      "title": "TSF spike",
      "milestone": "M0",
      "text": "A throwaway text service proved that the Text Services Framework gives Typer the exact caret position, the real surrounding text and clean key handling in Notepad, Discord, Chrome, Edge and Teams."
    },
    {
      "version": "0.0.4",
      "status": "released",
      "date": "2026-09-29",
      "title": "Roadmap pages",
      "text": "The engine and spike branches were merged into main, and the project gained a task board and a left-to-right release timeline that agents keep current under the rules in CLAUDE.md."
    },
    {
      "version": "0.1.0",
      "status": "next",
      "title": "Words everywhere",
      "milestone": "M1",
      "text": "This is the first release you can type with. The real text service replaces the spike and connects to the engine, so a list of word completions appears at the caret in Notepad, Chrome, Edge, Discord and other Electron apps. Tab accepts the highlighted word, the arrow keys move through the list and Esc closes it. Enter is never intercepted, so chat messages still send. Password, URL, email and number fields stay silent, as do code editors and terminals. If the engine stops, apps carry on as if Typer weren’t installed and reconnect when it returns. A keyboard icon lets Windows list Typer in Settings, so it can be switched on without PowerShell."
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
      "text": "The popup gains a greyed phrase row at the top, a continuation of your sentence written by a cheap and fast cloud completion model. In allow-listed apps it appears after a short pause and streams in as it is generated. It is highlighted by default, so Tab takes the whole phrase, and Ctrl+Right takes it one word at a time. In any other app, Ctrl+Space asks for a phrase on demand. A benchmark of fill-in-the-middle models picks the provider, and DeepSeek-Flash is the current favourite. An anchored context window keeps requests cache-friendly so heavy use costs under fifty cents a day, and a daily cap enforces the limit. When the provider is slow or unavailable, Typer quietly falls back to words."
    },
    {
      "version": "0.4.0",
      "status": "planned",
      "title": "Daily driver",
      "milestone": "M4",
      "text": "This release covers what it takes to leave Typer running all day. The engine starts at logon, a tray icon pauses and resumes it, and edits to the config file apply without a restart. One script installs Typer and another removes it. A local metrics store records how often suggestions are shown and accepted, how many keystrokes they save and how each phrase provider performs. It keeps counts and timings only. A stats summary shows whether Typer is paying off."
    },
    {
      "version": "1.0.0",
      "status": "planned",
      "title": "Autocomplete for everything you type",
      "essay": [
        "Typer 1.0 brings the suggestions of a code editor to every place you write English on Windows. Type a few letters in Discord, a browser text box or an email, and a short list of completions appears at the caret. Pause for a moment, and a continuation of your sentence appears above them. Tab accepts the highlighted suggestion and Enter still sends the message. When Typer has nothing useful to offer, every key behaves exactly as it did before it was installed.",
        "An earlier attempt at this tool failed because of its plumbing. It located the caret through accessibility APIs, rebuilt the text from a keyboard hook and inserted completions by simulating keystrokes. Each of those methods is an approximation, and the approximations drifted apart. The popup appeared in the wrong place, the tool’s copy of the text fell out of step after a click or a paste, and Tab either leaked through to the app or disappeared. Typer avoids all three problems by registering as a text service, the mechanism Windows provides for Chinese and Japanese input methods. A text service receives the exact caret rectangle, the real text around the caret, key events it is allowed to consume, and a proper channel for inserting text. The spike released as 0.0.3 confirmed that this works in the apps that matter most: Chrome, Edge, Discord and Teams.",
        "Suggestions arrive at two speeds. Words are local and instant. A frequency list of about 146,000 English words, re-ranked by the words before the caret and by your own habits, answers within a few milliseconds of each keystroke. Phrases come from the cloud, from a small and inexpensive fill-in-the-middle model that sees the text before and after the caret along with the app name and window title. The split follows from the job each part does. Word completion must keep pace with typing, and only a local index is fast enough. Phrase quality depends on a real language model, and a cloud model that costs a fraction of a cent per request writes much better text than anything that runs comfortably on a laptop.",
        "Cost shaped the design from the start. Heavy daily use has to stay under fifty cents, so phrases appear automatically only in apps you have allow-listed, and everywhere else they wait for Ctrl+Space. The context sent to the model is anchored, which means consecutive requests begin with the same text and are served from the provider’s cache at about a fiftieth of the normal price. A daily spending cap backs this up. Once the cap is reached, Typer offers words only until the next day. The same caution applies to privacy and safety. Typer stays silent in password, URL, email and number fields and in code editors and terminals, and its learning and metrics stores keep counts and timings rather than anything you typed.",
        "Version 1.0 is measured as well as built. Typer records how often suggestions are shown and accepted in each app, how many keystrokes they save, and how quickly each provider responds. Those numbers set the defaults, including which provider to use, how long to wait before asking for a phrase and how much context to send. A stats summary shows the same figures, so the question of whether Typer is worth running has a clear answer.",
        "Some things are left out on purpose. Google Docs draws its text on a canvas that no text service can read, so it remains out of reach. Phrases appear as the top row of the popup instead of inline ghost text, because other apps’ text fields have no way to display ghost text. There is no settings window, since a single config file covers every option. After 1.0, phrase suggestions will learn what you are replying to by reading the surrounding conversation, and a Chrome extension will reach web text fields directly. Both build on the engine that 1.0 ships."
      ]
    },
    {
      "version": "1.1.0",
      "status": "planned",
      "title": "Reply-aware phrases",
      "milestone": "M5",
      "text": "Phrase suggestions learn what you are replying to. When you switch to a window, Typer reads the surrounding conversation in the background through UI Automation, falling back to Windows’ built-in text recognition for apps that expose little text. That context travels with each phrase request, so a reply in Discord or Slack can follow the last few messages and an email reply can respond to the message it quotes. Reading the screen involves no AI or vision model. The phrase request remains the only model call."
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
      "text": "Releases follow semantic versioning, and each gets writing that matches its size: one sentence for a patch, a paragraph for a minor release (one per milestone), an essay for a major release."
    },
    {
      "date": "2026-09-29",
      "text": "The roadmap pages are plain HTML files opened from disk, reading `roadmap.js`. They are not published anywhere."
    },
    {
      "date": "2026-09-29",
      "text": "Build Typer as a TSF text service (an IME). The spike confirmed caret rect, real context, key capture and insertion in Notepad and Chromium/Electron apps."
    },
    {
      "date": "2026-09-29",
      "text": "Thin C++ DLL, all intelligence in one Python engine process, named pipe with length-prefixed JSON between them."
    },
    {
      "date": "2026-09-29",
      "text": "Phrases come from cheap FIM/completion models (DeepSeek-Flash leads), never premium chat models. Budget under $0.50 a day via anchored context and a daily cap."
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
      "text": "M0 done: TSF works for Typer (spike round 2 confirmed long Electron context and input scope)."
    },
    {
      "date": "2026-09-29",
      "text": "Engine word completion served over a named pipe, 74 tests passing (branch m1-engine-words)."
    }
  ]
};
