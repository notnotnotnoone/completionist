window.COMPLETIONIST_ROADMAP = {
  "schema": 4,
  "project": "Completionist",
  "tagline": "System-wide, VS Code-style English autocomplete for Windows",
  "updated": "2026-10-02",
  "prd": "https://github.com/notnotnotnoone/typer/issues/1",
  "repo": "https://github.com/notnotnotnoone/typer",
  "now": "Building toward 2.0.0: nine native desktop stages plus the viewer and scope tracks. Live soft-glass feasibility is next. Latest recorded shipped release is 1.1.14; tense-aware ranking targets 2.1.0 and the browser client targets 3.0.0.",
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
      "opens": "3.0.0"
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
      "title": "Stricter phrase prompt and the 0.5.0 to 2.0.0 plan",
      "highways": [
        "engine",
        "tooling"
      ],
      "text": "The phrase model now gets a longer autocomplete prompt with thirty numbered rules and two worked examples, and the roadmap gained milestones 0.5.0 to 2.0.0 for n-gram suggestions, a personal viewer, typo tolerance and a UI redesign."
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
      "version": "0.0.13",
      "status": "released",
      "date": "2026-09-30",
      "title": "Protocol codec",
      "highways": [
        "engine"
      ],
      "text": "The engine and the text service now speak length-prefixed JSON frames, with a streaming decoder that handles split and merged frames."
    },
    {
      "version": "0.0.14",
      "status": "released",
      "date": "2026-09-30",
      "title": "Word completer",
      "highways": [
        "engine"
      ],
      "text": "The engine ranks about 146,000 words by frequency and completes the word being typed in under 5 ms."
    },
    {
      "version": "0.0.15",
      "status": "released",
      "date": "2026-09-30",
      "title": "Config and policy",
      "highways": [
        "engine"
      ],
      "text": "A TOML config now blocks editors, terminals and silent fields such as passwords, and decides where phrases may appear."
    },
    {
      "version": "0.0.16",
      "status": "released",
      "date": "2026-09-30",
      "title": "Pipe server and probe",
      "highways": [
        "engine"
      ],
      "text": "The engine serves a named pipe with a probe command, and a real-pipe test checks the 95th percentile lookup stays under 10 ms."
    },
    {
      "version": "0.0.17",
      "status": "released",
      "date": "2026-09-30",
      "title": "Engine and spike merged",
      "highways": [
        "tooling"
      ],
      "text": "The word-completion engine branch and the spike branch were merged into main."
    },
    {
      "version": "0.0.18",
      "status": "released",
      "date": "2026-09-30",
      "title": "Text service shell",
      "highways": [
        "tsf"
      ],
      "text": "The real text service shell came up from the spike code, with activation, edit sessions, a key sink and registration for English keyboards."
    },
    {
      "version": "0.0.19",
      "status": "released",
      "date": "2026-09-30",
      "title": "Context reader",
      "highways": [
        "tsf"
      ],
      "text": "The text service now reads the text around the caret, the caret rectangle, the input scope and the app, and sends nothing from password fields."
    },
    {
      "version": "0.0.20",
      "status": "released",
      "date": "2026-09-30",
      "title": "Popup state and key router",
      "highways": [
        "tsf"
      ],
      "text": "A pure popup model and key router with native tests now decide every key in every state, and Enter is never consumed."
    },
    {
      "version": "0.0.21",
      "status": "released",
      "date": "2026-09-30",
      "title": "Engine client",
      "highways": [
        "tsf"
      ],
      "text": "The text service talks to the engine from a worker thread with stale replies dropped and reconnect with backoff."
    },
    {
      "version": "0.0.22",
      "status": "released",
      "date": "2026-09-30",
      "title": "TSF test harness",
      "highways": [
        "tooling"
      ],
      "text": "A harness runs the real DLL and the real engine against a simulated text field and checks the popup, the keys and the output."
    },
    {
      "version": "0.0.23",
      "status": "released",
      "date": "2026-09-30",
      "title": "Popup at the caret",
      "highways": [
        "tsf"
      ],
      "text": "The popup draws word rows just under the caret with the typed part highlighted, without taking focus."
    },
    {
      "version": "0.0.24",
      "status": "released",
      "date": "2026-09-30",
      "title": "Keyboard icon and enable step",
      "highways": [
        "tooling"
      ],
      "text": "The keyboard got an icon compiled into the DLL and a script that turns it on."
    },
    {
      "version": "0.0.27",
      "status": "released",
      "date": "2026-09-30",
      "title": "Highway map",
      "highways": [
        "tooling"
      ],
      "text": "The roadmap became a highway map in the Evergreen color scheme, with a restyled task board and a CONTEXT.md."
    },
    {
      "version": "0.0.28",
      "status": "released",
      "date": "2026-09-30",
      "title": "Typer renamed",
      "highways": [
        "tooling"
      ],
      "text": "The app was renamed from Typer to Completionist everywhere, and the engine moves old data folders on first start."
    },
    {
      "version": "0.0.29",
      "status": "released",
      "date": "2026-09-30",
      "title": "Roadmap easier to read",
      "highways": [
        "tooling"
      ],
      "text": "The roadmap pages now lead with releases, plain task titles and tasks sorted by status."
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
      "version": "0.1.1",
      "status": "released",
      "date": "2026-09-30",
      "title": "N-gram builder",
      "highways": [
        "engine"
      ],
      "text": "A command now builds bigram and trigram tables from a public corpus, and the tables were built from WikiText-103."
    },
    {
      "version": "0.1.2",
      "status": "released",
      "date": "2026-09-30",
      "title": "N-gram ranking",
      "highways": [
        "engine"
      ],
      "text": "The word completer now mixes word frequency with bigram and trigram counts, so the word that fits the sentence ranks first."
    },
    {
      "version": "0.1.3",
      "status": "released",
      "date": "2026-09-30",
      "title": "Vocabulary cleanup",
      "highways": [
        "engine"
      ],
      "text": "Misspellings and junk were filtered out of the base vocabulary, which shrank from 146,000 to 53,000 words."
    },
    {
      "version": "0.1.4",
      "status": "released",
      "date": "2026-09-30",
      "title": "Personal store",
      "highways": [
        "engine"
      ],
      "text": "A personal store now counts the words you accept and type in SQLite, saved every ten seconds and on exit."
    },
    {
      "version": "0.1.5",
      "status": "released",
      "date": "2026-09-30",
      "title": "New words promoted",
      "highways": [
        "engine"
      ],
      "text": "A word outside the dictionary is now suggested after you type it three times."
    },
    {
      "version": "0.1.6",
      "status": "released",
      "date": "2026-09-30",
      "title": "Accept events",
      "highways": [
        "tsf"
      ],
      "text": "The text service now tells the engine which item you accepted so it can learn it and the word before it."
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
      "version": "0.2.3",
      "status": "released",
      "date": "2026-09-30",
      "title": "Phrase provider",
      "highways": [
        "engine"
      ],
      "text": "The engine can stream phrases from an OpenAI-style completions endpoint, with optional fill-in-the-middle, and never includes the key in errors."
    },
    {
      "version": "0.2.4",
      "status": "released",
      "date": "2026-09-30",
      "title": "Anchored context window",
      "highways": [
        "engine"
      ],
      "text": "Phrase prompts now start at a paragraph or sentence boundary so consecutive requests share a cacheable start."
    },
    {
      "version": "0.2.6",
      "status": "released",
      "date": "2026-09-30",
      "title": "Phrase scheduler",
      "highways": [
        "engine"
      ],
      "text": "A phrase scheduler now asks after a 350 ms pause, trims a phrase as you type along and cancels it on anything else."
    },
    {
      "version": "0.2.7",
      "status": "released",
      "date": "2026-09-30",
      "title": "Phrase pushes",
      "highways": [
        "engine"
      ],
      "text": "Streamed phrase text now reaches the text service as push messages over the pipe, and never for password fields."
    },
    {
      "version": "0.2.8",
      "status": "released",
      "date": "2026-09-30",
      "title": "Phrase row",
      "highways": [
        "tsf"
      ],
      "text": "The popup gained a greyed phrase row on top that Tab accepts."
    },
    {
      "version": "0.2.9",
      "status": "released",
      "date": "2026-09-30",
      "title": "Ctrl+Space phrases",
      "highways": [
        "tsf"
      ],
      "text": "Ctrl+Space now asks for a phrase in any app, Ctrl+Right inserts the next phrase word and Esc quiets the popup."
    },
    {
      "version": "0.2.10",
      "status": "released",
      "date": "2026-09-30",
      "title": "Fall back to words",
      "highways": [
        "engine"
      ],
      "text": "A slow or failing phrase provider now ends the phrase quietly while word suggestions carry on."
    },
    {
      "version": "0.2.12",
      "status": "released",
      "date": "2026-09-30",
      "title": "Prefix-only phrases",
      "highways": [
        "engine"
      ],
      "text": "Without fill-in-the-middle, phrases are now held back when text follows the caret on the same line."
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
      "version": "0.3.1",
      "status": "released",
      "date": "2026-09-30",
      "title": "Tray and pause hotkey",
      "highways": [
        "engine"
      ],
      "text": "A tray icon now pauses and resumes Completionist, and Ctrl+Alt+P does the same from the keyboard."
    },
    {
      "version": "0.3.2",
      "status": "released",
      "date": "2026-09-30",
      "title": "Start at logon",
      "highways": [
        "tooling"
      ],
      "text": "A logon task now starts the engine without a console window."
    },
    {
      "version": "0.3.3",
      "status": "released",
      "date": "2026-09-30",
      "title": "Config hot reload",
      "highways": [
        "engine"
      ],
      "text": "The engine now reloads its config file when it changes and keeps the old settings if an edit is bad."
    },
    {
      "version": "0.3.4",
      "status": "released",
      "date": "2026-09-30",
      "title": "Install scripts",
      "highways": [
        "tooling"
      ],
      "text": "Install and uninstall scripts now set up and remove the keyboard, the logon task and the DLL registration."
    },
    {
      "version": "0.3.5",
      "status": "released",
      "date": "2026-09-30",
      "title": "Metrics store",
      "highways": [
        "engine"
      ],
      "text": "A local metrics store now counts suggestions shown and accepted, keystrokes saved and provider latency, never text."
    },
    {
      "version": "0.3.6",
      "status": "released",
      "date": "2026-09-30",
      "title": "Stats summary",
      "highways": [
        "engine"
      ],
      "text": "The completionist-stats command and the tray now show a usage summary."
    },
    {
      "version": "0.3.7",
      "status": "released",
      "date": "2026-09-30",
      "title": "Engine logging",
      "highways": [
        "engine"
      ],
      "text": "The engine now logs slow requests and provider errors to a rotating file."
    },
    {
      "version": "0.3.8",
      "status": "released",
      "date": "2026-09-30",
      "title": "Resilience",
      "highways": [
        "tsf"
      ],
      "text": "Tests that kill and restart the engine now show typing never hangs and the popup returns after reconnect."
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
      "version": "0.4.2",
      "status": "released",
      "title": "Next words and chunks in the popup",
      "highways": [
        "engine",
        "tsf"
      ],
      "date": "2026-09-30",
      "text": "After a space the popup can offer likely next words with nothing highlighted, chunks of two or three words appear while typing, and the personal store can list and forget words."
    },
    {
      "version": "0.5.0",
      "status": "released",
      "title": "N-gram suggestions",
      "milestone": "M7",
      "text": "Suggestions now look ahead. After you type a space, Completionist offers the likeliest next words from the last one or two words before you have typed a letter, in a short list with nothing highlighted, so Tab and Enter still behave as the app expects until you press Up or Down. While you type, it can extend the top word into a chunk of two or three words, shown above the plain words and below the cloud phrase, all in one popup. The list stays quiet when nothing is likely, and the threshold, or either feature, can be changed in the config. Chunks come from encyclopedic text, so some read oddly. The personal log now keeps trigrams as well as words, never learns a word you backspaced over, and can list and forget words. Checks in real apps are left to daily use.",
      "date": "2026-09-30"
    },
    {
      "version": "0.5.2",
      "status": "released",
      "date": "2026-09-30",
      "title": "Viewer page",
      "highways": [
        "engine"
      ],
      "text": "The engine now serves a viewer page with a searchable dictionary, a settings form and usage stats."
    },
    {
      "version": "0.5.3",
      "status": "released",
      "date": "2026-09-30",
      "title": "Tray Open viewer",
      "highways": [
        "engine"
      ],
      "text": "The tray gained an Open viewer item that opens the viewer with its token."
    },
    {
      "version": "0.5.4",
      "status": "released",
      "date": "2026-09-30",
      "title": "Viewer privacy tests",
      "highways": [
        "tooling"
      ],
      "text": "Tests now check the viewer serves only counts and dictionary entries and refuses other websites."
    },
    {
      "version": "0.5.6",
      "status": "released",
      "date": "2026-09-30",
      "title": "Local viewer server",
      "highways": [
        "engine"
      ],
      "text": "The engine now runs a viewer server on this machine only, behind a secret token and Host and Origin checks."
    },
    {
      "version": "0.5.7",
      "status": "released",
      "date": "2026-09-30",
      "title": "Settings form",
      "highways": [
        "engine"
      ],
      "text": "The viewer can now show the config as a form and write changes back, with the API key write-only."
    },
    {
      "version": "0.6.0",
      "status": "released",
      "title": "Personal viewer",
      "milestone": "M8",
      "text": "You can now see what Completionist has learned and change it. An Open viewer item in the tray opens a page in your browser, served to your own machine only by the running engine. The dictionary lists the words you use, searchable and sortable by how often you typed them, and any word can be removed from there, which forgets it along with the pairs and triples it belonged to. A settings panel shows the main options as a form and writes changes back into your settings file line by line, so your comments stay; you can type the phrase key there too, but the page can only save it and never shows it back. The stats show how often suggestions were shown and accepted and how many keystrokes they saved, by day and by app, with phrase provider speed. The page holds counts only, never your typed text, and needs a secret address the tray opens, so another website can't drive it. It has no trigram list yet.",
      "date": "2026-09-30"
    },
    {
      "version": "0.6.1",
      "status": "released",
      "title": "Phrases in the viewer",
      "highways": [
        "engine"
      ],
      "date": "2026-09-30",
      "text": "The viewer gains a Phrases tab to list and remove three-word phrases, and the whole page is redesigned with usage bars, a daily chart, a save bar for settings, a two-step confirm before removing anything, and motion that respects reduced-motion settings."
    },
    {
      "version": "0.6.2",
      "status": "released",
      "title": "Versions replace milestones",
      "highways": [
        "tooling"
      ],
      "date": "2026-09-30",
      "text": "The roadmap pages now show every task by the version it ships in instead of an M-number, and the checker enforces it, so each patch counts up to the next release."
    },
    {
      "version": "0.6.3",
      "status": "released",
      "title": "Fuzzy index over the vocabulary",
      "highways": [
        "engine"
      ],
      "date": "2026-10-01",
      "text": "The engine builds a typo-tolerant index over the vocabulary and your personal words, ready for lookups."
    },
    {
      "version": "0.6.4",
      "status": "released",
      "title": "Typo-tolerant ranking",
      "highways": [
        "engine"
      ],
      "date": "2026-10-01",
      "text": "Typo corrections now fill the rows left after exact prefix matches, so a real word is never pushed aside by a guess."
    },
    {
      "version": "0.6.5",
      "status": "released",
      "title": "Guessed letters marked in replies",
      "highways": [
        "engine"
      ],
      "date": "2026-10-01",
      "text": "Each suggestion in the engine's reply now says which of its letters were guessed, parsed on both sides of the pipe."
    },
    {
      "version": "0.6.6",
      "status": "released",
      "title": "Guessed letters drawn in amber",
      "highways": [
        "tsf"
      ],
      "date": "2026-10-01",
      "text": "The popup draws the guessed letters of a suggestion in amber so you can see what was corrected."
    },
    {
      "version": "0.6.7",
      "status": "released",
      "title": "Latency check with the fuzzy index on",
      "highways": [
        "engine"
      ],
      "date": "2026-10-01",
      "text": "A new pipe test types typos as well as plain text and holds the 95th percentile under 10 ms, since the fuzzy scan only runs when rows are unfilled."
    },
    {
      "version": "0.7.0",
      "status": "released",
      "title": "Typo-tolerant words",
      "milestone": "M9",
      "date": "2026-10-01",
      "text": "Suggestions now survive typos. Type a misspelled word such as “moutian” and the popup still offers “mountain”, with the letters it guessed drawn in a different colour so you can see what was corrected. Exact prefix matches always rank first, and corrections only fill the remaining rows or step in when nothing matches, so a real word is never pushed aside by a guess. The matching comes from an existing edit-distance library, rapidfuzz, over the vocabulary and your personal words, and lookups stay under ten milliseconds at the 95th percentile over the real pipe. Words you fix yourself are never learned as new vocabulary. The live check, the demo GIF and the Spike keyboard removal are dropped to daily use."
    },
    {
      "version": "0.7.1",
      "status": "released",
      "date": "2026-10-01",
      "title": "Every task is a stop",
      "highways": [
        "tooling"
      ],
      "text": "Every shipped task now has its own patch number and a stop on the map, the numbering rule and the prompt-rewriting rule are in CLAUDE.md, and the checker lets shipped work sit below the previous group's release."
    },
    {
      "version": "0.7.2",
      "status": "released",
      "date": "2026-10-01",
      "title": "Review fixes before 1.0",
      "highways": [
        "engine",
        "tsf"
      ],
      "text": "Fifteen fixes from a full code review: Esc and phrase suppression survive the sliding text window, queued DLL edit sessions keep the service alive, pause cancels phrases, config and viewer errors are handled, and uninstall removes the roaming config."
    },
    {
      "version": "1.0.0",
      "status": "released",
      "date": "2026-10-01",
      "title": "Autocomplete for everything you type",
      "highways": [
        "engine",
        "tsf",
        "tooling"
      ],
      "essay": [
        "Completionist 1.0 put the suggestions of a code editor into the places you write English on Windows. Type a few letters in Discord, a browser text box or an email and a short list of completions appears just under the caret, ranked by the words before it and by the words you actually use. Tab takes the top one. In apps you choose, and on Ctrl+Space everywhere else, a greyed phrase appears above the list, a continuation of your sentence that you take whole with Tab or one word at a time with Ctrl+Right. Enter is never touched, and nothing in a password field is read or sent. It starts with Windows, lives in the tray, and can be paused with Ctrl+Alt+P.",
        "The unusual decision was that Completionist is a Text Services Framework text service, the mechanism Windows uses for input methods, and not a keyboard hook with a screen-scraping overlay. That choice cost a milestone of throwaway spiking and a C++ DLL that lives inside every app, but it is why the caret position, the surrounding text, the key handling and the insertion of accepted text all come from Windows itself and work the same across apps. The DLL stayed deliberately thin: it draws the popup, decides which keys it consumes, and talks over a named pipe to a Python engine, reconnecting quietly if the engine is gone. The key router is pure logic with native tests, and the DLL was exercised through real TSF against a simulated text field, including the engine being killed and restarted mid-typing. Before the tag, a full code review produced fifteen fixes in the last patch, among them keeping the service alive for queued edit sessions and keeping Esc and phrase suppression correct as the text window slides along a long field.",
        "Word ranking is local and instant. A 146,000-word frequency list gives the base order, bigram and trigram counts built from WikiText-103 re-rank by context, and a personal store of word and word-pair counts, never text, boosts what you use and adds names and slang after a few uses. The same tables predict the next word after a space and offer two- and three-word chunks in the one popup box, and a typo still finds its word, with the guessed letters drawn in a different colour and exact prefix matches always ranked first. A viewer opened from the tray shows the personal dictionary and the stats, lets you forget a word, and edits the settings. Lookups took about a millisecond at the 95th percentile over the real pipe. Phrases are the only thing that leaves the machine, and they were built to be cheap: a plain completion request through OpenRouter to a low-cost model chosen from a list in the config, seeing only the text before the caret, with the context cut at paragraph boundaries so the provider's prefix cache does the work, a debounce so it asks once per pause, and no spend counting of its own. You cap the key in OpenRouter, and when the cap is hit phrases quietly fall back to words. The key is typed into the config file by the user and is never logged or shown in errors.",
        "The release was judged the way a personal tool should be: by using it. The scripted live checks, a manual matrix of apps, a real-key phrase check, an install and logon check, were dropped from the gates in favour of daily use, and after daily use in real apps the verdict was that it works well. That is a weaker kind of evidence than a written matrix, and this essay says so. The default phrase model is still a starting guess to be swapped after more use, and which OpenRouter models accept plain completions cleanly was never surveyed. The n-gram corpus is encyclopedic, which suits formal writing more than chat. The API key sits in plain text in a file outside the repository, protected only by the account and by the spending limit on the key. The harness that tests the real DLL needs the foreground window, so it only runs when nobody is using the machine, and the old Spike keyboard is still registered until the user removes it.",
        "What 1.0 leaves out is as deliberate as what it includes. It does not read the conversation you are replying to, so a Discord phrase knows your sentence but not the message above it; that is the next release. It has no browser extension, no popup redesign, no macOS or Linux, no languages other than English, and no cloud account, sync or telemetry: the only network traffic is the phrase request you configured, and the only record of your typing is a set of counts on your own disk. Those limits keep the promise small enough to keep, which is that Completionist starts with Windows, stays out of the way, and shows you the numbers on whether it is paying for itself."
      ]
    },
    {
      "version": "1.0.1",
      "status": "released",
      "date": "2026-10-01",
      "title": "Evergreen icon",
      "highways": [
        "engine",
        "tsf"
      ],
      "text": "The tray and keyboard icon is now a pine-green square with a white C and a pink caret instead of the old blue T from the Typer days, grey when paused."
    },
    {
      "version": "1.0.2",
      "status": "released",
      "date": "2026-10-01",
      "title": "Preferred Providers setting",
      "highways": [
        "engine"
      ],
      "text": "The viewer's settings page now has a Preferred Providers box under Cloud Phrases for provider_order (Groq by default), so you can change which OpenRouter provider is tried first without editing the file."
    },
    {
      "version": "1.0.3",
      "status": "released",
      "date": "2026-10-01",
      "title": "Requests log",
      "highways": [
        "engine"
      ],
      "text": "The viewer has a new Requests tab, modelled on the flexrouter request log, listing the latest phrase requests live with their text, timings, result and the models tried, kept in memory only."
    },
    {
      "version": "1.0.4",
      "status": "released",
      "date": "2026-10-01",
      "title": "Advanced phrase settings",
      "highways": [
        "engine"
      ],
      "text": "The viewer's Cloud Phrases settings gain an Advanced section (output length, temperature, seconds before asking, timeout, context sizes) and an Instructions section where the model's system prompt can be edited or restored."
    },
    {
      "version": "1.0.5",
      "status": "released",
      "date": "2026-10-01",
      "title": "Reading the screen for replies",
      "highways": [
        "engine"
      ],
      "text": "Completionist now reads the window you are typing in with the text recognition built into Windows, in the background, and sends that text with your own so phrases can fit what you are replying to; a new setting switches it off."
    },
    {
      "version": "1.0.6",
      "status": "released",
      "date": "2026-10-01",
      "title": "Screenshots in the Requests page",
      "highways": [
        "engine"
      ],
      "text": "The Requests page now shows the screenshot each request was shown and the text read from it, next to what was sent to the model."
    },
    {
      "version": "1.1.0",
      "status": "released",
      "date": "2026-10-01",
      "title": "Reply-aware phrases",
      "milestone": "M5",
      "text": "Phrase suggestions learned what you are replying to. When you switched windows, Completionist read the one in front in the background with the text recognition built into Windows, and sent that text with each phrase request, so a reply in Discord or Slack could follow the last few messages and an email reply could answer the message it quotes. UI Automation was measured first and dropped: its cost grew with each app and ran from about a tenth of a second to nineteen, in C++ as much as in Python, while reading the picture took about a tenth of a second whatever the window. Every request in the viewer now shows the screenshot and text it was given, a setting turns the reading off, nothing is saved to disk, and the phrase request is still the only model call."
    },
    {
      "version": "1.1.1",
      "status": "released",
      "date": "2026-10-01",
      "title": "Expanded settings",
      "highways": [
        "engine",
        "tsf"
      ],
      "text": "Added all 30 approved settings, including latency routing, writing preferences, privacy controls, app profiles and popup shortcuts, with niche controls grouped in collapsible Advanced tiles."
    },
    {
      "version": "1.1.2",
      "status": "released",
      "date": "2026-10-01",
      "title": "Current viewer icon",
      "highways": [
        "engine"
      ],
      "text": "Replaced the viewer's leftover T-shaped mark with the Evergreen C and caret used by the app icon."
    },
    {
      "version": "1.1.3",
      "status": "released",
      "date": "2026-10-01",
      "title": "Clean personal words",
      "highways": [
        "engine"
      ],
      "text": "Personal learning now skips one-letter words and removes old one-letter entries while first-letter suggestions continue to work."
    },
    {
      "version": "1.1.4",
      "status": "released",
      "date": "2026-10-01",
      "title": "Full request details",
      "highways": [
        "engine"
      ],
      "text": "The Requests detail panel now shows complete in-memory prompts and replies, sent suffixes, non-secret settings and an ordered event history."
    },
    {
      "version": "1.1.5",
      "status": "released",
      "date": "2026-10-01",
      "title": "Exact request events",
      "highways": [
        "engine",
        "tooling"
      ],
      "text": "The Requests event timeline now includes outgoing JSON and HTTP status, the config watcher test is stable, and the repo has an agent-guide pointer."
    },
    {
      "version": "1.1.6",
      "status": "released",
      "date": "2026-10-01",
      "title": "Proportional map lanes",
      "highways": [
        "tooling"
      ],
      "text": "The roadmap highway map now gives busy highways taller lanes with stops stacked in rows, and the Browser extension lane opens at 3.0.0."
    },
    {
      "version": "1.1.7",
      "status": "released",
      "date": "2026-10-01",
      "title": "Agent roadmap brief",
      "highways": [
        "tooling"
      ],
      "text": "Agents now read a short generated roadmap brief and use a helper script for routine roadmap edits, instead of opening the huge roadmap.js."
    },
    {
      "version": "1.1.11",
      "status": "released",
      "date": "2026-10-02",
      "title": "Cloud model OCR context",
      "highways": [
        "engine",
        "tooling"
      ],
      "text": "The phrase prompt now explains the local Windows OCR source and treats recognized screen text as imperfect, untrusted background context."
    },
    {
      "version": "1.1.12",
      "status": "released",
      "date": "2026-10-02",
      "title": "Prompt playground pack and reasoning control",
      "highways": [
        "engine",
        "tooling"
      ],
      "text": "Settings now copies the OCR-aware system prompt with three comparison cases, and cloud requests use chat completions with configurable reasoning defaulted to none."
    },
    {
      "version": "1.1.13",
      "status": "released",
      "date": "2026-10-02",
      "title": "Merge OCR-aware prompt playground",
      "highways": [
        "engine",
        "tooling"
      ],
      "text": "The OCR-aware cloud prompt, three provider-playground scenarios and no-reasoning default are now released on main."
    },
    {
      "version": "1.1.14",
      "status": "released",
      "date": "2026-10-02",
      "title": "Roadmap brief detail levels",
      "highways": [
        "tooling"
      ],
      "text": "Roadmap briefs now show current work in minimal, current work and recent completions in medium, and full context in maximum."
    },
    {
      "version": "2.0.0",
      "status": "next",
      "title": "Desktop overhaul",
      "milestone": "M10",
      "essay": [
        "Completionist 2.0 gives the desktop autocomplete menu and the viewer one deliberate Evergreen identity. The owner has chosen to begin with the native desktop surfaces: a compact suggestion menu beside the caret and a separate lightweight information dock at the bottom-left of the typing monitor. The material should feel like soft liquid glass, with a genuinely blurred background, restrained edge refraction, broad highlights and sharp foreground words. Plain Acrylic is a fallback candidate rather than the target. The dock contains a small connection light and a smaller tense field; countdown and AI activity belong inside the caret menu. Descriptions, insertion previews and duplicated next-word displays are left out.",
        "The desktop implementation is tracked through nine stages, each with its own scope, dependencies and verification gate, all targeting the same 2.0.0 release. First prove live glass in an isolated native renderer. Then supply truthful AI status and candidate origins, implement the display protocol and focus leases, build layout and typography, finish the glass windows and dock behavior, redraw the dependable host fallback, connect TSF snapshots with failover, integrate renderer lifecycle and packaging, and finally validate and stage the result. The written specification and implementation plan describe the interfaces and checks. A working static spike is a useful visual reference, but cannot prove desktop capture, transparent composition or behavior across monitors.",
        "Python remains the engine for ranking, learning and cloud phrases. A separate C++ rendering process owns the graphics so a graphics failure does not crash the application receiving typing. The existing text-service DLL retains caret tracking, keyboard rules and text insertion. It sends asynchronous display snapshots and keeps an opaque Evergreen popup ready when the renderer is missing or unhealthy. Live refraction requires local GPU desktop sampling, used only for drawing: no captured pixels are sent to the engine or model or saved to disk. Capture exclusion prevents the glass from sampling itself, with a screenshot-visibility tradeoff that needs review. High contrast, disabled transparency, unsupported displays and device or session failures use opaque surfaces or hide the UI.",
        "The final result must preserve Enter passthrough, after-space Tab behavior, the phrase selection guard, explicit selection and exact partial acceptance. Real scheduler data drives idle countdowns and request state; labels distinguish local vocabulary, personally promoted words and AI phrases without inventing provenance. Tense detection remains in 2.1, so V2 displays an unavailable dash. Verification covers engine and native tests, generated renderer fixtures, owner-run checks in Notepad, Chrome and Electron, mixed DPI and negative monitor coordinates, focus races, process restarts, accessibility and reduced motion. The viewer still needs its separately approved layout overhaul, with context receipt in Requests and existing settings as the authority. Desktop completion alone does not release all of V2; both tracks and the release checks must be finished."
      ]
    },
    {
      "version": "2.1.0",
      "status": "planned",
      "title": "Tense-aware suggestions",
      "milestone": "M11",
      "text": "This release will make the tense of the current sentence one signal in word suggestion ranking. When the surrounding text indicates past or present tense, Completionist can favour suggestions that fit the sentence while still weighing the typed prefix and nearby word context. If the sentence is ambiguous or mixes tenses, the current ranking remains in place."
    },
    {
      "version": "3.0.0",
      "status": "planned",
      "title": "Autocomplete in the browser",
      "milestone": "M6",
      "highways": [
        "engine",
        "tsf",
        "tooling",
        "browser"
      ],
      "essay": [
        "Completionist 3.0 takes the autocomplete into the one place a Windows text service can only reach halfway: the web page. A Chrome extension talks to the same Python engine through Native Messaging, the browser's own way of letting an extension speak to a local program, so the words, the personal dictionary and the cloud phrases you already have follow you into every web text field. Inside the browser the extension can do what a text service cannot. It draws true inline ghost text in the field itself, instead of a popup row under the caret, and it reads context that only the page knows, such as the post you are commenting on or the email thread you are answering.",
        "The reason this is a major release, and the reason it comes after the redesign, is that it adds a second front end to a product that has had one. Until now the text service was the only thing that touched the screen: the engine ranked, the DLL drew. With the extension there are two clients with two different ideas of where the caret is and how a suggestion looks, and the engine has to serve both without confusing them. It was placed after the dashboard and popup redesign on purpose, so the look of a suggestion is settled before a second client has to match it, and the extension can borrow the same Evergreen colours, the same row types and the same marks for guessed letters instead of inventing its own.",
        "It costs a new toolchain, a new kind of code and a new way to break. The extension is JavaScript running in someone else's browser, shipped with a small native host that Windows registers for Chrome, and every web page is a different and sometimes hostile environment, with rich editors, shadow roots, iframes and fields that rewrite themselves as you type. Each of those has to be proved on real pages before it is trusted. It also raises the privacy bar. A browser extension can see far more than a text service, so it reads only the field you are typing in and the page context needed for a phrase, stays silent in password fields, and sends nothing anywhere except to the local engine, which still makes the one phrase request you configured.",
        "What 3.0 leaves out is as deliberate as what it includes. It does not replace the text service: desktop apps keep working through it exactly as before, and Chrome itself still gets word suggestions from the text service in any field the extension cannot handle. It targets Chrome first, with no Firefox or Safari version, no store listing or sync, and no accounts. It adds no new suggestion features and no new places for your typing to leave the machine. The aim is narrow and testable: inline ghost text working in a Chrome textarea, then in the rich editors people actually write in, with the engine unchanged from the user's point of view. This essay is a plan for now, and it becomes a retrospective of what actually shipped when the release is done."
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
    "74": "One deliberate popup design",
    "75": "Change my settings in the viewer"
  },
  "decisions": [
    {
      "date": "2026-10-02",
      "text": "Adopted Release atlas concept A: versions are coherent destinations, tasks share targetRelease, completion is distinct from explicit validated shipping, and region widths follow broad scope rather than task count; all 91 shipped records were preserved."
    },
    {
      "date": "2026-10-01",
      "text": "Reply-aware context reads the screen with Windows OCR, not UI Automation, reversing the same-day decision below after measuring both on the owner's machine. Reading a whole window with UI Automation cost 5 to 10 ms per element (about 100 ms to 19 s per window, the same in C++ and Python, so the language was not the problem), and Chrome and Gmail gave little or no text. OCR of a window-sized picture took about 60 to 150 ms on any app and read ordinary text almost perfectly (99 to 100 percent of characters at 12 px and up, 96 percent in dark mode at 11 px, but only about 32 percent at 10 px). So it runs in Python in the background, never on the typing path, with no C++. It is local, uses no AI or vision model and sends nothing by itself; what reaches OpenRouter is the text, placed in the prompt as background. Screenshots stay in memory with the Requests log."
    },
    {
      "date": "2026-10-01",
      "text": "Reading the screen is on by default but has its own switch, [phrase] screen_context, because it sends other people's messages to the cloud model. It never runs without an API key, while paused, in blocked apps, in Completionist's own windows or in programs running as administrator, and a capture is handed to a request only if its window is still in front. Lines the person typed are removed from it. The setting was added without being asked for, on privacy grounds, and can be dropped."
    },
    {
      "date": "2026-10-01",
      "text": "(Reversed later the same day: see the entry above.) Reply-aware context (M5.1, 1.0.5) reads the surrounding window with read-only UI Automation only. OCR is deliberately left out: it is a heavy, noisy fallback (it reads menus and sidebars along with the thread) that would only help the few apps that expose no UIA text, and it sits close to the 'no AI or vision for screen tracking' rule. When an app exposes nothing the suggestion falls back to caret context, as today. The engine will log per app whether context was found, so OCR can be reconsidered later as its own task if real misses justify it. Selecting all and copying (Ctrl+A / Ctrl+C) was also ruled out: it would copy the wrong text, overwrite the clipboard and disturb the user's selection."
    },
    {
      "date": "2026-10-01",
      "text": "The owner reordered the plan: the dashboard (viewer) overhaul is prioritised and now leads the 2.0.0 redesign, and the Chrome extension moves from 1.2.0 to its own major release, 3.0.0, after the redesign so a second client can match a settled look. This supersedes the 2026-09-30 'no dashboard' line only in that the viewer gets a full redesign; it is still a local page, not a server or a TUI."
    },
    {
      "date": "2026-10-01",
      "text": "The endpoint (base_url) is deliberately not editable from the viewer: a page that could change it could send the API key and the text being typed to another host. Everything else about phrases (output length, temperature, wait, timeout, context sizes, and the instructions the model is given) is. The instructions are stored in config.toml as [phrase] instructions; saving blank or the built-in text stores blank, so improvements to the built-in prompt still reach anyone who has not customised it."
    },
    {
      "date": "2026-10-01",
      "text": "The Requests log stores the text sent and the suggestion returned (the owner chose this over metadata only, to debug bad phrases). To keep the 'your typing is never saved' promise as far as possible it is held in memory only, capped at the last 200 requests, trimmed to the end of the prompt, cleared when the engine stops and by a Clear Log button, and every other number stays counts only. Writing it to disk would be a separate, deliberate change."
    },
    {
      "date": "2026-10-01",
      "text": "Typo tolerance uses rapidfuzz (OSA edit distance with an early cutoff over a first-letter bucket of the vocabulary), not SymSpell: SymSpell corrects whole words and can't complete half-typed fragments (\"moutia\" finds \"mouth\", never \"mountain\"), and its whole-word deletes index costs ~580 MB in memory. Only fragments of 3+ letters are corrected, exact prefix matches always rank first, and corrections only fill unfilled rows, so short fragments, names and learned words are never corrected."
    },
    {
      "date": "2026-09-30",
      "text": "Milestones are no longer a thing you read on the roadmap; versions are. A task shows the version it ships in: shipped work names its real release (several tasks can share one), unshipped work gets one patch number each, counting up from the last release so 0.6.10 comes before 0.7.0. The M-ids stay in roadmap.js as internal keys (and in the old log lines and git history) and are never renumbered. Planned releases are ordered by version, so the reply-aware and web-field work (1.1.0, 1.2.0) now sits after 0.7.0."
    },
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
      "text": "The phrase system prompt is the owner's own 30-rule version with two worked examples, kept at the start of every request so providers can cache it. It is much longer than before, so a model that still answers instead of continuing, or a provider that charges by prompt length, is worth checking in the live phrase test (0.3.0)."
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
      "text": "Milestone numbers don't match release order (1.1.0 is 1.1.0; the new 0.5.0 to 2.0.0 are 0.5.0, 0.6.0, 0.7.0 and 2.0.0). The pages will read releases first (0.1.0), with the M-number as a small tag."
    },
    {
      "date": "2026-09-30",
      "text": "The base n-gram tables (0.8M bigrams, 1.6M trigrams from WikiText-103) are small for next-word prediction. Rebuild them with a looser prune and a chat-style corpus only if the live check (0.5.0) says they feel thin."
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
    },
    {
      "date": "2026-09-30",
      "text": "The viewer is no longer a read-only file opened from disk. It is served by the engine on 127.0.0.1, can remove words and edit settings, and the `completionist-words` command is dropped; this replaces the earlier 'no server, forgetting stays a command' decision. The phrase API key can be typed into the settings page but is write-only: never sent back to the page, never logged."
    },
    {
      "date": "2026-09-30",
      "text": "The viewer is started only with the tray (not with --no-tray), on a random port, with its token in memory only; trigram listing is left for later because the store has no per-trigram list or remove."
    },
    {
      "date": "2026-10-01",
      "text": "1.0.0 shipped without the scripted live checks: the user used it daily in real apps and judged that it works great, and the 1.0.0 essay records that as weaker evidence than a written matrix."
    }
  ],
  "risks": [
    {
      "text": "Reading the screen sends text from the window in front, such as other people's messages or anything else visible, to the cloud model.",
      "status": "mitigated",
      "mitigation": "A [phrase] screen_context switch, no reading without a key, while paused, in blocked apps or in elevated programs, only the window in front, text only (the picture never leaves the PC), and screenshots kept in memory and cleared with the Requests log (1.0.5, 1.0.6)."
    },
    {
      "text": "Text on the screen could contain instructions aimed at the model, or OCR could misread a window with tiny or busy text.",
      "status": "open",
      "mitigation": "The screen text is framed as background that is not to be continued or obeyed, and the text to continue always comes last. Text under about 11 px is not read reliably; upscaling before reading is untried."
    },
    {
      "text": "The OpenRouter key is plain text in config.toml, so anyone who can read the user's profile, a backup or a shared copy of the file can use it.",
      "status": "open",
      "mitigation": "The file lives outside the repo, the key is never logged or printed, and the key on OpenRouter has a spending limit. Revoke and replace it if the file is ever shared."
    },
    {
      "text": "A popup after every space could be noisy, and Tab could be stolen right after a space.",
      "status": "open",
      "mitigation": "A confidence threshold that can be switched off, and next-word rows not highlighted by default (0.4.1, 0.4.2)."
    },
    {
      "text": "Typo matches could push out correct rare words or slow lookups.",
      "status": "open",
      "mitigation": "Exact prefix matches always rank first, no fuzzing of short fragments, names or learned words, and a p95 under 10 ms check (0.6.2, 0.6.5)."
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
      "mitigation": "Include an older Electron app in the 0.1.0 manual matrix."
    },
    {
      "text": "The keyboard doesn't appear in Settings without an icon.",
      "status": "open",
      "mitigation": "0.1.0: add an icon and an enable-keyboard step."
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
    },
    {
      "text": "A local server that can delete words and edit config could be driven by any web page the user visits.",
      "status": "mitigated",
      "mitigation": "Bind 127.0.0.1 only, require a secret token, check Host and Origin, and tests for each (0.6.0)."
    },
    {
      "text": "The API key passes through the viewer's local server when typed on the settings page.",
      "status": "mitigated",
      "mitigation": "Write-only: saved straight to config.toml, never returned, logged or printed, and the page is covered by the token and Origin checks (0.6.0)."
    }
  ],
  "log": [
    {
      "date": "2026-10-02",
      "text": "Task 1b: added process-local crash-dialog suppression and DirectComposition presentation scaffolding; fixture WIC runtime fault is unverified after compile-only COM lifetime fix; M10.14 remains doing."
    },
    {
      "date": "2026-10-02",
      "text": "Strengthened M10.14 WARP tests to compare production blur output with the normalized sigma-6 radius-18 Gaussian profile."
    },
    {
      "date": "2026-10-02",
      "text": "Document the conservative V2 viewer scope and baseline/native live verification boundaries."
    },
    {
      "date": "2026-10-02",
      "text": "Added partial D3D11 Gaussian blur and WARP fixture foundation for M10.14; live composition and acceptance remain pending."
    },
    {
      "date": "2026-10-02",
      "text": "V2 execution started on codex/v2-overhaul with Luna subagents; native live acceptance stays pending and viewer scope is documented separately."
    },
    {
      "date": "2026-10-02",
      "text": "Started M10.14 toward 2.0.0."
    },
    {
      "date": "2026-10-02",
      "text": "Commit label readability fix and push codex/roadmap-release-atlas to origin as requested; includes previously committed release atlas and V2 prototype work."
    },
    {
      "date": "2026-10-02",
      "text": "Completed atlas-label-readability toward 2.0.0; no release was created."
    },
    {
      "date": "2026-10-02",
      "text": "Started atlas-label-readability toward 2.0.0."
    },
    {
      "date": "2026-10-02",
      "text": "Added task atlas-label-readability toward 2.0.0: Keep atlas labels clear of construction lines"
    },
    {
      "date": "2026-10-02",
      "text": "Commit and push all current workspace changes on codex/roadmap-release-atlas: release atlas implementation and concept prototypes, roadmap tooling and documentation, V2 design plans, native glass spike and UI prototypes."
    },
    {
      "date": "2026-10-02",
      "text": "Completed roadmap-release-atlas toward 2.0.0; no release was created."
    },
    {
      "date": "2026-10-02",
      "text": "Updated active release writing and the V2 execution plan to remove obsolete task patch numbers; strengthened invalid-source rejection for trustworthy briefs."
    },
    {
      "date": "2026-10-02",
      "text": "Migrated roadmap to release destinations and implemented the approved atlas; task completion no longer manufactures releases and shipped history remains intact."
    },
    {
      "date": "2026-10-02",
      "text": "Started roadmap-release-atlas toward 2.0.0."
    },
    {
      "date": "2026-10-02",
      "text": "Added task roadmap-release-atlas toward 2.0.0: Separate release destinations from roadwork and build the Release atlas"
    },
    {
      "date": "2026-10-02",
      "text": "Added three read-only roadmap concept prototypes: a release atlas, a release roadbook and a focused interchange, using live task statuses and preserving shipped history."
    },
    {
      "date": "2026-10-02",
      "text": "Inspected the roadmap redesign: task completion currently creates patch releases and map width follows release count; proposed separate target releases, completed work and explicit shipping while preserving released history."
    },
    {
      "date": "2026-10-02",
      "text": "Corrected brief scope after review: minimal only doing/next/blocked tasks; medium adds the five latest released versions and matching completed tasks; maximum alone retains full history. Updated CLAUDE.md, regenerated files and passed eight CLI tests."
    },
    {
      "date": "2026-10-02",
      "text": "Released 1.1.14: Roadmap briefs now show current work in minimal, current work and recent completions in medium, and full context in maximum."
    },
    {
      "date": "2026-10-02",
      "text": "Added task M10.23 (1.1.23): Generate roadmap briefs at three detail levels"
    },
    {
      "date": "2026-10-02",
      "text": "Added all nine V2 desktop stages as planned highway-map stops 1.1.14-1.1.22 before the 2.0.0 interchange, assigned to text-service, engine and tooling highways with stage summaries; task-board scope and validation details remain linked through the matching versions, and no stop is marked shipped."
    },
    {
      "date": "2026-10-02",
      "text": "Expanded the V2 desktop overhaul into all nine individually tracked stages M10.14-M10.22 (planned patches 1.1.14-1.1.22), each with scope, dependencies, verification gates and spec/plan references. Live material feasibility is next; the duplicate broad popup task is retained as dropped history, viewer remains a separate V2 track, and no production stage is marked started or complete."
    },
    {
      "date": "2026-10-02",
      "text": "Added task M10.22 (1.1.22): Stage 9/9: Validate and stage the desktop overhaul"
    },
    {
      "date": "2026-10-02",
      "text": "Added task M10.21 (1.1.21): Stage 8/9: Integrate renderer lifecycle, packaging and viewer contracts"
    },
    {
      "date": "2026-10-02",
      "text": "Added task M10.20 (1.1.20): Stage 7/9: Connect TSF snapshots with renderer failover"
    },
    {
      "date": "2026-10-02",
      "text": "Added task M10.19 (1.1.19): Stage 6/9: Ship matching host fallback and keyboard regressions"
    },
    {
      "date": "2026-10-02",
      "text": "Added task M10.18 (1.1.18): Stage 5/9: Complete glass windows, dock motion and recovery"
    },
    {
      "date": "2026-10-02",
      "text": "Added task M10.17 (1.1.17): Stage 4/9: Build native layout, typography and Evergreen palette"
    },
    {
      "date": "2026-10-02",
      "text": "Added task M10.16 (1.1.16): Stage 3/9: Implement renderer protocol, focus leases and arbitration"
    },
    {
      "date": "2026-10-02",
      "text": "Added task M10.15 (1.1.15): Stage 2/9: Supply truthful AI lifecycle and suggestion origins"
    },
    {
      "date": "2026-10-02",
      "text": "Added task M10.14 (1.1.14): Stage 1/9: Prove live soft glass in a standalone native renderer"
    },
    {
      "date": "2026-10-02",
      "text": "Prepared the V2 desktop overhaul spec and nine-stage implementation plan: separate native renderer, true GPU Gaussian blur and live-capture feasibility gate, focus/IPC failover, real AI/origin metadata, compact accessible dock, host fallback, lifecycle and rollout. Documents supersede the earlier Acrylic proposal; viewer redesign remains a separate V2 track. Owner requested both documents together; production execution and live capture have not started."
    },
    {
      "date": "2026-10-02",
      "text": "Corrected the native glass spike's blocky sparse-sample background: true separable Gaussian blur now supplies every lens/dispersion sample while foreground text stays sharp. Build completed without warnings; light/dark/frost offscreen renders succeeded. Blur is precomputed for the static fixture, not a production live-capture implementation."
    },
    {
      "date": "2026-10-02",
      "text": "Owner verified the C++ glass spike works and requested a softer finish. Refined curvature, nine-sample backdrop softness, reduced refraction and dispersion, broader non-clipping highlights, gentler shadows and antialiased status/selection edges. Rebuilt without warnings and exported light/dark/frost successfully; preserved the running first-pass executable alongside the revision."
    },
    {
      "date": "2026-10-02",
      "text": "Built a throwaway standalone C++/D3D11 liquid-glass material spike with a curved refraction shader, rim highlights, sharp text, Evergreen themes, drag/strength controls and a frost comparison. Successfully compiled without warnings and rendered light/dark/frost fixtures offscreen; controlled app-owned backdrop only, no desktop capture or installed TSF changes. Owner visual review and live-desktop feasibility remain pending."
    },
    {
      "date": "2026-10-02",
      "text": "Researched premade Windows liquid-glass renderers for V2: electron-liquid-glass is a packaged MIT native Electron addon; liquidDX11 is the closest native C++ source candidate; LiquidGlassWinUI depends on internal hooks pinned to one SDK. Recorded framework, desktop capture and fallback constraints in the native spec; no candidate installed or quality verified."
    },
    {
      "date": "2026-10-02",
      "text": "The owner rejected plain Acrylic as too simple for the production menu. Researched richer native material options: custom transparent layering, specular edge highlights and composition lighting are candidates; genuine backdrop displacement requires separate feasibility validation and is not promised by the default DWM Acrylic path. Native production design remains unapproved pending the material choice."
    },
    {
      "date": "2026-10-02",
      "text": "Prepared the native V2 autocomplete production design after the owner's promotion request: real scheduler/status/origin data, compact non-activating information dock, Tense dash until V2.1, and a documented Acrylic-versus-exact-refraction material decision. Production interface/rendering changes await written-design review under the requested brainstorming workflow; no native code or installed DLL was changed."
    },
    {
      "date": "2026-10-02",
      "text": "Started M10.2."
    },
    {
      "date": "2026-10-02",
      "text": "Verified the compact glass revision in headless Chrome: countdown stays in the caret AI section, corner panel has no descriptions/context/duplicate preview, connection indicator and reduced motion work, correction acceptance and request lifecycle pass, and 27 responsive edge/scale combinations stay within bounds. Computed styles confirm 24% surface tint, active refraction and the actual viewer's Evergreen tokens; visual review remains with the owner."
    },
    {
      "date": "2026-10-02",
      "text": "Refined the approved prototype toward glanceable status: removed info descriptions, context receipt and duplicate next-word display; moved countdown/activity into the caret AI section; reduced corner panel to connection light and small collapsible tense label. Verified Evergreen palette against the actual viewer and CONTEXT.md; replaced opaque glass with transparent backdrop refraction and stronger bevel highlights."
    },
    {
      "date": "2026-10-02",
      "text": "Built the approved liquid-glass autocomplete prototype in tip/prototype: lightweight caret menu, separate bottom-left minimizable information, simulated idle/AI/tense/context/connection displays, origins, corrections and next-word preview. Headless Chrome checks passed lifecycle, acceptance, explicit selection, countdown reset, panel animation states, adaptive minimize/restore, retained preference, reduced motion and responsive edge/scale bounds. Native drawing and live telemetry remain unimplemented."
    },
    {
      "date": "2026-10-02",
      "text": "Started implementing the approved liquid-glass prototype: separate minimizable bottom-left information panel, caret suggestions with origins and correction comparisons, simulated idle/AI lifecycle, next-word preview and simulated V2.1 tense controls."
    },
    {
      "date": "2026-10-02",
      "text": "The owner selected a separate information panel anchored to the screen's bottom-left corner, with a minimized state; the lightweight suggestions menu remains at the caret. This supersedes information attached beside or beneath the menu. Tense remains simulated for V2, with real detection planned for V2.1."
    },
    {
      "date": "2026-10-02",
      "text": "The owner confirmed real tense detection belongs to V2.1; the V2 liquid-glass autocomplete prototype will simulate past, present, future and uncertain/mixed displays without adding a detector or changing ranking."
    },
    {
      "date": "2026-10-02",
      "text": "Refined the liquid-glass prototype brief: bottom-left collapsible information beside lightweight suggestions; keep next-word preview, suggestion origin, correction comparison, trigger explanations, context receipt and connection state; remove insertion preview. Adaptive footprint should preserve readable sizing and selected-row stability while relocating or collapsing information near edges; exact bottom-left anchoring remains to be clarified."
    },
    {
      "date": "2026-10-02",
      "text": "The owner approved the revised brief for a maximalist liquid-glass autocomplete prototype using the attached glass guide, with an AI activity display, idle-trigger countdown and past/present/future tense display; brainstorming layout and additional features before changing prototype code, with real engine telemetry and native glass deferred to implementation planning."
    },
    {
      "date": "2026-10-02",
      "text": "Applied frontend-design and ui-animation to the V2 popup study: Phrase shelf now reserves streaming space, menu updates remain instant, and headless browser checks passed acceptance, correction, dismissal, seven scenarios, themes, reduced motion and responsive edge/scale combinations."
    },
    {
      "date": "2026-10-02",
      "text": "Started the V2 autocomplete design study ahead of the dashboard at the owner's request: rich and expressive visual character, three local throwaway menu variants, and a proposed native design; M10.1 awaits layout review and live-app audit."
    },
    {
      "date": "2026-10-02",
      "text": "Released 1.1.13: The OCR-aware cloud prompt, three provider-playground scenarios and no-reasoning default are now released on main."
    },
    {
      "date": "2026-10-02",
      "text": "Added task M10.13 (1.1.13): Ship OCR-aware prompt playground to main"
    },
    {
      "date": "2026-10-02",
      "text": "Started M10.1."
    },
    {
      "date": "2026-10-02",
      "text": "Released 1.1.12: Settings now copies the OCR-aware system prompt with three comparison cases, and cloud requests use chat completions with configurable reasoning defaulted to none."
    },
    {
      "date": "2026-10-02",
      "text": "Started M10.12."
    },
    {
      "date": "2026-10-02",
      "text": "Started M10.12."
    },
    {
      "date": "2026-10-02",
      "text": "Added task M10.12 (1.1.12): Copy the prompt test pack and disable reasoning by default"
    },
    {
      "date": "2026-10-02",
      "text": "Released 1.1.11: The phrase prompt now explains the local Windows OCR source and treats recognized screen text as imperfect, untrusted background context."
    },
    {
      "date": "2026-10-02",
      "text": "Added task M10.11 (1.1.11): Explain Windows OCR context to the cloud model"
    },
    {
      "date": "2026-10-01",
      "text": "Released 1.1.7: scripts/roadmap.py writes a short agent brief and makes the routine roadmap edits, CLAUDE.md tells agents to avoid opening roadmap.js, and the planned M10 tasks moved to 1.1.8-1.1.10."
    },
    {
      "date": "2026-10-01",
      "text": "Released 1.1.6: the highway map gives busy highways taller lanes with patch stops stacked in rows, the Browser extension lane opens at 3.0.0 not 1.0.0, and the planned M10 tasks moved to 1.1.7-1.1.9."
    },
    {
      "date": "2026-10-01",
      "text": "Planned M11 (2.1.0): sentence past or present tense will become a ranking signal for word suggestions, with ambiguous or mixed-tense context falling back to current ranking; moved M6.1 to 2.1.1 to keep the release order."
    },
    {
      "date": "2026-10-01",
      "text": "Released 1.1.5: Requests shows outgoing JSON and HTTP status without the authorization header, the config watcher test uses monotonic file times, and the agent-guide pointer is tracked."
    },
    {
      "date": "2026-10-01",
      "text": "Prepared the approved push to main: expanded settings remains done, and releases 1.1.1 through 1.1.4 include the completed viewer icon, personal-learning and request-detail fixes; dashboard overhaul remains next."
    },
    {
      "date": "2026-10-01",
      "text": "Completed patch fixes 1.1.2 to 1.1.4: the viewer mark matches the app icon, personal learning excludes one-letter words, and Requests shows full detail with an event timeline."
    },
    {
      "date": "2026-10-01",
      "text": "Completed expanded settings (M10.4, 1.1.1): all 30 controls wired through configuration, runtime and popup; 626 Python, 77 native and four settings-form tests pass, with live Windows/provider checks documented for daily use."
    },
    {
      "date": "2026-10-01",
      "text": "Added three small fixes after expanded settings: the viewer uses the current C icon, personal learning excludes one-letter words, and Requests shows full in-memory detail with an event history. Dashboard and popup versions move three patches later."
    },
    {
      "date": "2026-10-01",
      "text": "Started expanded settings (M10.4, 1.1.1) after approval of all brainstormed controls and grouped collapsible Advanced tiles; dashboard and popup tasks move one patch later."
    },
    {
      "date": "2026-10-01",
      "text": "Investigated the requested latency routing setting: Cloud Phrases currently sends provider.order but not provider.sort; awaiting clarification of classifier scope before implementation."
    },
    {
      "date": "2026-10-01",
      "text": "Released 1.1.0: the last task of M5 is done, so the reply-aware phrases group is released and M10 (dashboard and popup redesign) becomes active with 2.0.0 next."
    },
    {
      "date": "2026-10-01",
      "text": "Released 1.0.6 (new task M5.7): the Requests page shows the screenshot each request was shown, the text read from it and what was sent to the model, served by a token-guarded endpoint and held in memory only."
    },
    {
      "date": "2026-10-01",
      "text": "Released 1.0.5 (M5.1): phrases read the window in front with Windows OCR in the background and send that text with the typed text; a [phrase] screen_context switch turns it off. Chosen over UI Automation after measuring both."
    },
    {
      "date": "2026-10-01",
      "text": "Picked up M5.1 (1.0.5, now doing) and removed the OCR fallback as a deliberate decision: reply-aware context is read-only UI Automation only, falling back to caret context, with a per-app found/empty log to revisit OCR later if needed."
    },
    {
      "date": "2026-10-01",
      "text": "Replanned: the Chrome extension (M6.1) moves from 1.1.1 to 2.0.1 and becomes the major 3.0.0 release (new essay); the dashboard overhaul is a new first task M10.3 (1.1.1) leading 2.0.0, and the popup tasks shift to 1.1.2 and 1.1.3; the 1.2.0 release is removed."
    },
    {
      "date": "2026-10-01",
      "text": "Released 1.0.4 (new task M5.6): Advanced and Instructions sections in the viewer's Cloud Phrases settings (max output, temperature, seconds before asking, timeout, context sizes, editable system prompt); base_url stays uneditable on purpose; reply-aware context (M5.1) moves to 1.0.5."
    },
    {
      "date": "2026-10-01",
      "text": "Released 1.0.3 (new task M5.5): Requests tab in the viewer, modelled on the flexrouter request log, with a live table, a detail sheet and Clear Log; the text is in memory only; reply-aware context (M5.1) moves to 1.0.4."
    },
    {
      "date": "2026-10-01",
      "text": "Released 1.0.2 (new task M5.4): Preferred Providers (provider_order) option in the viewer's Cloud Phrases settings; reply-aware context (M5.1) moves to 1.0.3."
    },
    {
      "date": "2026-10-01",
      "text": "Released 1.0.1 (new task M5.3): new tray and keyboard icon in the Evergreen colours with a C instead of the Typer T; reply-aware context (M5.1) moves to 1.0.2."
    },
    {
      "date": "2026-10-01",
      "text": "Released 1.0.0: rewrote the planned essay as a retrospective, marked it released on all four highways and made 1.1.0 the next release; the live checks stay dropped because daily use was the test and it works well."
    },
    {
      "date": "2026-10-01",
      "text": "Released 0.7.2, 15 code-review fixes (new task M5.2): phrase and Esc suppression survive the sliding 8000-character window, tray stats run on the engine loop, personal-word promotion and chunk seeds, config encoding, DLL edit sessions hold the service, viewer 500s, pause cancels phrases, uninstall removes the roaming config, learning-off stops ranking."
    },
    {
      "date": "2026-10-01",
      "text": "Released 0.7.1: committed the per-task numbering, the 0.0.x stops and the CLAUDE.md rules; tagged v0.7.1 and pushed."
    },
    {
      "date": "2026-10-01",
      "text": "Gave every task in the 0.1.0 to 0.4.0 and 0.6.0 groups its own patch number counting up from the release before it (0.0.13 to 0.0.29, 0.1.x, 0.2.x, 0.3.x, 0.5.x), added a released patch entry for each shipped task, kept dropped tasks numbered but without a stop, moved M7.9 to 0.4.3, and wrote the numbering rule into CLAUDE.md."
    },
    {
      "date": "2026-10-01",
      "text": "Moved the M1 to M4 tasks onto the 0.0.x release they actually shipped in (0.0.2, 0.0.4 to 0.0.8, 0.0.12), removed the 0.0.5 umbrella task, and let the checker accept a shipped patch earlier than its group's minor."
    },
    {
      "date": "2026-10-01",
      "text": "Added a done task for every 0.0.x release that had none (0.0.1, 0.0.2, 0.0.4 to 0.0.12) to the first group, so each stop on the map has work on the task board; renamed that group Foundations and TSF spike."
    },
    {
      "date": "2026-10-01",
      "text": "Pushed main and tagged v0.7.0, plus v0.6.3 to v0.6.7 (0.6.3 and 0.6.4 share one commit); all are on origin. Added the missing 0.6.3 to 0.6.6 release entries so they show on the highway map."
    },
    {
      "date": "2026-10-01",
      "text": "Release 0.7.0 (Typo-tolerant words): merged m9-fuzzy into main; M9 done; the live check, demo GIF and Spike removal are dropped to daily use; M5 is active and 1.1.0 is next."
    },
    {
      "date": "2026-10-01",
      "text": "Release 0.6.7 on branch m9-fuzzy: the p95 latency check with the fuzzy index on; 0.7.0 is next once the merge lands."
    },
    {
      "date": "2026-10-01",
      "text": "0.6.3 to 0.6.7 built on branch m9-fuzzy (fuzzy index, ranking, marks, amber popup, p95 check); 0.6.8 is next and needs you at a real desktop."
    },
    {
      "date": "2026-10-01",
      "text": "0.6.7 done on branch m9-fuzzy: p95 stays under 10 ms over the real pipe with the fuzzy index on (new test types typos too); 0.6.8 needs you."
    },
    {
      "date": "2026-10-01",
      "text": "0.6.6 done on branch m9-fuzzy: the popup draws guessed letters in amber (validated contrast) from the reply marks; native tests pass and the DLL builds; M9.5 is doing."
    },
    {
      "date": "2026-10-01",
      "text": "0.6.5 done on branch m9-fuzzy: replies carry guessed-letter marks per suggestion, parsed on both sides; M9.4 is doing."
    },
    {
      "date": "2026-10-01",
      "text": "0.6.3 and 0.6.4 done on branch m9-fuzzy: rapidfuzz fuzzy index (SymSpell tried and dropped, see the decision) and corrections filling rows after exact matches; M9.3 is doing."
    },
    {
      "date": "2026-09-30",
      "text": "0.6.2: versions replace milestones on the roadmap. Every task has a version, pages show it instead of the M-id, milestones sort by release, and the checker enforces the numbering. Rebased after the viewer took 0.6.1."
    },
    {
      "date": "2026-09-30",
      "text": "Release 0.6.1: viewer trigram list and remove (M8.8) and a full redesign of the page (M8.9); 486 engine tests."
    },
    {
      "date": "2026-09-30",
      "text": "Release 0.6.0 (M8 done): the personal viewer. M9 is now active and 0.7.0 is next."
    },
    {
      "date": "2026-09-30",
      "text": "M8 built on branch m8-viewer: engine-served viewer (viewer.py and viewer.html), settings writer (settings.py), Metrics.daily, tray Open viewer, 38 new tests; M8.5 dropped."
    },
    {
      "date": "2026-09-30",
      "text": "M8 replanned on main from the other session's draft: an engine-served viewer that removes words and edits settings (M8.1 dropped, M8.6 and M8.7 added, story 75, two risks). M8 active; M8.6 started. Trigram listing is left out: the store has no method for it."
    },
    {
      "date": "2026-09-30",
      "text": "Pushed main and tag v0.5.0 to origin. Next is 0.6.0 (Personal viewer); its plan is being reworked in another session."
    },
    {
      "date": "2026-09-30",
      "text": "Release 0.5.0 (N-gram suggestions): M7 done; M7.9 dropped (live checks no longer gate); smoke-tested through the real engine; M8 is active and 0.6.0 is next."
    },
    {
      "date": "2026-09-30",
      "text": "Pushed main and tag v0.4.2 to origin; engine (442) and native (70) tests pass on main."
    },
    {
      "date": "2026-09-30",
      "text": "Release 0.4.2: merged m7-kinds into main (M7.3, M7.6 to M7.8); M7.9 live check still to do."
    },
    {
      "date": "2026-09-30",
      "text": "M7.9 prepared: the live-check checklist is in the task. It needs you to run it; nothing else in M7 is left."
    },
    {
      "date": "2026-09-30",
      "text": "M7.7 and M7.8 done: next words and chunks show in the one popup box; next-word rows open with nothing highlighted so Tab and Enter pass through; [words] next and chunks default on. 70 native and 442 engine tests pass; the DLL builds. M7.9 (live check) is next."
    },
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
  ],
  "activeRelease": "2.0.0",
  "releasePlans": {
    "0.0.3": {
      "title": "Foundations and TSF spike",
      "goal": "Prove a TSF text service fixes the three failures of the old hook-based attempt: popup position, text sync and key handling.",
      "done_when": "A throwaway TIP shows a popup at the caret, reads the real text and swallows Tab in the must-work apps.",
      "scope": "medium"
    },
    "0.1.0": {
      "title": "Word completion end-to-end",
      "goal": "Type in any supported app and get a word dropdown at the caret, fed by the engine, accepted with Tab.",
      "done_when": "The real DLL shows engine word suggestions at the caret in Notepad, Chrome, Edge and Discord, Tab/Up/Down/Esc behave as specified, Enter is never consumed, and gated fields stay silent.",
      "scope": "medium"
    },
    "0.2.0": {
      "title": "Smarter words",
      "goal": "Rank words by the preceding words and by the user's own habits.",
      "done_when": "After \"I'd like to\" the list shows \"know\" above \"knowledge\", and names or slang the user types a few times start appearing.",
      "scope": "medium"
    },
    "0.3.0": {
      "title": "Phrase suggestions",
      "goal": "A greyed phrase continuation from a cheap cloud FIM model as the top popup row, accepted with Tab.",
      "done_when": "Allow-listed apps show a streaming phrase row after a pause, Ctrl+Space works everywhere, and heavy use stays under $0.50 a day.",
      "scope": "medium"
    },
    "0.4.0": {
      "title": "Daily-driver polish",
      "goal": "Completionist runs all day without attention and shows whether it's paying off.",
      "done_when": "One-command install and uninstall, autostart at logon, tray pause, hot-reloaded config and a stats summary.",
      "scope": "medium"
    },
    "0.5.0": {
      "title": "N-gram suggestions and the personal log",
      "goal": "Suggest the next word after a space and two- or three-word chunks while typing, all in one popup box, and keep a clean personal log of the words and trigrams you type.",
      "done_when": "After a space the popup offers likely next words, chunks appear while typing, words, chunks and the phrase share one box, the popup stays quiet when nothing is likely, and the log holds only words and trigrams you typed, finished and used at least three times.",
      "scope": "medium"
    },
    "0.6.0": {
      "title": "Personal viewer",
      "goal": "Let you see your personal dictionary and stats, remove words and change settings, from a page opened from the tray.",
      "done_when": "The tray's Open viewer opens a page served by the engine on this machine. It lists learned words, trigrams and stats; removing a word there forgets it; changes in the settings panel reach config.toml and take effect without a restart; the API key can be typed in but is never shown back; it holds no typed-text history; and another website can't drive it.",
      "scope": "medium"
    },
    "0.7.0": {
      "title": "Typo-tolerant words",
      "goal": "Still suggest the right word when the typed part has a typo, and show which letters were guessed.",
      "done_when": "Typing \"moutian\" offers \"mountain\" with the guessed letters in a different colour, exact prefix matches still rank first, and lookup p95 stays under 10 ms.",
      "scope": "medium"
    },
    "1.1.0": {
      "title": "Reply-aware phrases",
      "goal": "Phrase suggestions know what you're replying to.",
      "done_when": "A phrase suggestion in a Discord reply reflects the last few messages in the channel.",
      "scope": "medium"
    },
    "2.0.0": {
      "title": "Desktop overhaul",
      "goal": "Deliver the native soft-glass autocomplete and information dock through nine verified stages, alongside the separately designed Evergreen viewer overhaul.",
      "done_when": "All nine desktop stages pass their checks: live soft glass, truthful status, safe focus and failover, readable layout, accessible dock, packaging and real-app validation. The separately designed Evergreen viewer is also complete. Native testing covers mixed DPI and both monitors; no full V2 release is claimed from desktop work alone.",
      "scope": "large"
    },
    "2.1.0": {
      "title": "Tense-aware suggestions",
      "goal": "Use whether the current sentence is in past or present tense to rank suggestions that fit what the person is writing.",
      "done_when": "Past and present sentence context can influence suggestion ranking in representative examples, while ambiguous or mixed-tense context leaves the existing ranking unchanged.",
      "scope": "contained"
    },
    "3.0.0": {
      "title": "Browser autocomplete",
      "goal": "Reach web text fields directly with a Chrome extension that talks to the same engine.",
      "done_when": "Inline ghost text works in a Chrome textarea through the extension.",
      "scope": "large"
    }
  },
  "tasks": [
    {
      "id": "M0.1",
      "title": "Install VS 2022 Build Tools (MSVC v143, Windows SDK 10.0.26100)",
      "status": "done",
      "area": "toolchain",
      "stories": [],
      "notes": "CLI-only toolchain, no full Visual Studio IDE.",
      "targetRelease": "0.0.3",
      "shippedIn": "0.0.3"
    },
    {
      "id": "M0.2",
      "title": "Build Microsoft's SampleIME unmodified",
      "status": "done",
      "area": "dll",
      "stories": [],
      "notes": "Link error LNK1295 fixed with `/p:WholeProgramOptimization=false`, retargeted to v143 and SDK 10.0.26100.",
      "targetRelease": "0.0.3",
      "shippedIn": "0.0.3"
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
      ],
      "targetRelease": "0.0.3",
      "shippedIn": "0.0.3"
    },
    {
      "id": "M0.4",
      "title": "Spike: swallow Tab and replace the current word with ITfRange::SetText",
      "status": "done",
      "area": "dll",
      "stories": [
        47
      ],
      "notes": "Plain `SetText` worked everywhere; the composition fallback was never needed.",
      "targetRelease": "0.0.3",
      "shippedIn": "0.0.3"
    },
    {
      "id": "M0.5",
      "title": "Manual app matrix, rounds 1 and 2",
      "status": "done",
      "area": "test",
      "stories": [
        47
      ],
      "notes": "Works in Notepad, Discord, Chrome, Edge and Teams, and stays silent in password fields. Discord exposed 2,239 characters before the caret. Google Docs and the Google search box are out of scope.",
      "targetRelease": "0.0.3",
      "shippedIn": "0.0.3"
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
      ],
      "targetRelease": "0.0.3",
      "shippedIn": "0.0.3"
    },
    {
      "id": "M0.7",
      "title": "Scaffold the repository",
      "status": "done",
      "area": "repo",
      "stories": [],
      "notes": "README, gitignore and a standalone uv project for the engine.",
      "targetRelease": "0.0.3",
      "shippedIn": "0.0.1"
    },
    {
      "id": "M0.8",
      "title": "Engine word completion over the named pipe",
      "status": "done",
      "area": "engine",
      "stories": [],
      "notes": "146,000-word frequency list, p95 under 10 ms, 74 tests.",
      "targetRelease": "0.0.3",
      "shippedIn": "0.0.2"
    },
    {
      "id": "M0.9",
      "title": "Roadmap pages and agent guide",
      "status": "done",
      "area": "docs",
      "stories": [],
      "notes": "Task board, release timeline and the CLAUDE.md rules.",
      "targetRelease": "0.0.3",
      "shippedIn": "0.0.4"
    },
    {
      "id": "M0.11",
      "title": "OpenRouter-only phrases",
      "status": "done",
      "area": "engine",
      "stories": [],
      "notes": "Models list in the config; DeepSeek and the benchmark tool removed; phrases held back when text follows the caret.",
      "targetRelease": "0.0.3",
      "shippedIn": "0.0.6"
    },
    {
      "id": "M0.12",
      "title": "Highway map, Evergreen colors and the Completionist rename",
      "status": "done",
      "area": "docs",
      "stories": [],
      "notes": "CONTEXT.md added; old data folders move on first start.",
      "targetRelease": "0.0.3",
      "shippedIn": "0.0.7"
    },
    {
      "id": "M0.13",
      "title": "Continue-only phrase prompt",
      "status": "done",
      "area": "engine",
      "stories": [],
      "notes": "Daily budget and price counting removed in favour of a spending limit on the OpenRouter key.",
      "targetRelease": "0.0.3",
      "shippedIn": "0.0.8"
    },
    {
      "id": "M0.14",
      "title": "Stricter phrase prompt and the 0.5.0 to 2.0.0 plan",
      "status": "done",
      "area": "engine",
      "stories": [],
      "notes": "Thirty numbered rules and two worked examples in the prompt.",
      "targetRelease": "0.0.3",
      "shippedIn": "0.0.9"
    },
    {
      "id": "M0.15",
      "title": "Groq default model and API key in the config",
      "status": "done",
      "area": "engine",
      "stories": [],
      "notes": "Llama 3.3 70B on Groq through OpenRouter, with a provider_order setting.",
      "targetRelease": "0.0.3",
      "shippedIn": "0.0.10"
    },
    {
      "id": "M0.16",
      "title": "One-line launcher",
      "status": "done",
      "area": "install",
      "stories": [],
      "notes": "cd engine && uv run completionist.py",
      "targetRelease": "0.0.3",
      "shippedIn": "0.0.11"
    },
    {
      "id": "M0.17",
      "title": "Task board by release",
      "status": "done",
      "area": "docs",
      "stories": [],
      "notes": "Milestones shown as releases, plain titles, tasks sorted by status.",
      "targetRelease": "0.0.3",
      "shippedIn": "0.0.12"
    },
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
      ],
      "targetRelease": "0.1.0",
      "shippedIn": "0.0.13"
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
      ],
      "targetRelease": "0.1.0",
      "shippedIn": "0.0.14"
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
      "notes": "Block-list (editors, IDEs, terminals), silent input scopes (password, URL, email, number), allow-list for automatic phrases. Hot reload is 0.4.0.",
      "refs": [
        "engine/src/completionist_engine/config.py",
        "engine/src/completionist_engine/policy.py"
      ],
      "targetRelease": "0.1.0",
      "shippedIn": "0.0.15"
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
      ],
      "targetRelease": "0.1.0",
      "shippedIn": "0.0.16"
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
      ],
      "targetRelease": "0.1.0",
      "shippedIn": "0.0.17"
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
      ],
      "targetRelease": "0.1.0",
      "shippedIn": "0.0.18"
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
      ],
      "targetRelease": "0.1.0",
      "shippedIn": "0.0.19"
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
      ],
      "targetRelease": "0.1.0",
      "shippedIn": "0.0.20"
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
      ],
      "targetRelease": "0.1.0",
      "shippedIn": "0.0.21"
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
      ],
      "targetRelease": "0.1.0",
      "shippedIn": "0.0.22"
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
      ],
      "targetRelease": "0.1.0",
      "shippedIn": "0.0.23"
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
      ],
      "targetRelease": "0.1.0",
      "shippedIn": "0.0.24"
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
      "notes": "Dropped: live checks no longer gate a milestone; it is a personal tool and daily use is the test. Notepad, Chrome (textarea, contenteditable, long text, password field, after `chrome://restart`), Edge, Discord, Slack, Obsidian, an older Electron app. 100% and 150% scaling. Enter still sends in Discord. The harness (0.1.0) already covers the logic; this checks each app's own quirks. Switch to the Completionist keyboard first, so the old Completionist Spike keyboard isn't also active.",
      "targetRelease": "0.1.0"
    },
    {
      "id": "M1.13",
      "title": "Remove the Completionist Spike keyboard and unregister it",
      "status": "dropped",
      "area": "install",
      "stories": [],
      "notes": "Moved to 0.4.0 so 0.1.0 does not wait on a cleanup only the user can do. Originally: the user removes the Spike keyboard from the language list, then runs `tip/spike/register.ps1 -Unregister`.",
      "targetRelease": "0.1.0"
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
      ],
      "targetRelease": "0.1.0",
      "shippedIn": "0.0.27"
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
      ],
      "targetRelease": "0.1.0",
      "shippedIn": "0.0.28"
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
      "notes": "The M-numbers confuse: M2 is 0.2.0 but M5 is 1.1.0, and task ids like M1.14 sit before M1.10. The pages should show each milestone as its release and name (\"0.2.0 · Smarter words\", found through the release's `milestone` field), lead task rows with the plain title and show the id only as a small grey tag (ids stay as stable keys, never renumbered), and sort tasks doing, next, todo, blocked, done, dropped. Then update the wording in CLAUDE.md and CONTEXT.md. Files: tasks.html, common.js, page.js.",
      "targetRelease": "0.1.0",
      "shippedIn": "0.0.29"
    },
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
      ],
      "targetRelease": "0.2.0",
      "shippedIn": "0.1.1"
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
      ],
      "targetRelease": "0.2.0",
      "shippedIn": "0.1.2"
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
      ],
      "targetRelease": "0.2.0",
      "shippedIn": "0.1.3"
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
      ],
      "targetRelease": "0.2.0",
      "shippedIn": "0.1.4"
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
      ],
      "targetRelease": "0.2.0",
      "shippedIn": "0.1.5"
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
      ],
      "targetRelease": "0.2.0",
      "shippedIn": "0.1.6"
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
      "notes": "Dropped: live checks no longer gate a milestone; it is a personal tool and daily use is the test. Needs you: with the engine running and the Completionist keyboard on, type a made-up word (like \"zorblax\") three or more times, then confirm it shows up as a suggestion. Also check \"I'd like to kn\" puts \"know\" first.",
      "targetRelease": "0.2.0"
    },
    {
      "id": "M3.1",
      "title": "Provider benchmark: time-to-first-token and quality",
      "status": "dropped",
      "area": "bench",
      "stories": [
        45
      ],
      "notes": "Dropped: the benchmark tool was deleted. Model choice is read off the OpenRouter model cards and swapped in the config; completionist-stats shows real latency and cost.",
      "targetRelease": "0.3.0"
    },
    {
      "id": "M3.2",
      "title": "Benchmark time-to-first-token against context size",
      "status": "dropped",
      "area": "bench",
      "stories": [
        46
      ],
      "notes": "Dropped: the benchmark tool was deleted. Model choice is read off the OpenRouter model cards and swapped in the config; completionist-stats shows real latency and cost.",
      "targetRelease": "0.3.0"
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
      ],
      "targetRelease": "0.3.0",
      "shippedIn": "0.2.3"
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
      ],
      "targetRelease": "0.3.0",
      "shippedIn": "0.2.4"
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
      ],
      "targetRelease": "0.3.0"
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
      ],
      "targetRelease": "0.3.0",
      "shippedIn": "0.2.6"
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
      ],
      "targetRelease": "0.3.0",
      "shippedIn": "0.2.7"
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
      ],
      "targetRelease": "0.3.0",
      "shippedIn": "0.2.8"
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
      ],
      "targetRelease": "0.3.0",
      "shippedIn": "0.2.9"
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
      ],
      "targetRelease": "0.3.0",
      "shippedIn": "0.2.10"
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
      "notes": "Dropped: live checks no longer gate a milestone; it is a personal tool and daily use is the test. Needs you: tray menu > Open settings file, add `api_key = \"...\"` under `[phrase]` (never in chat; it applies within seconds), then type in Notepad (allow-listed, phrases appear on their own) and press Ctrl+Space in Discord. `spend.json` in `%LOCALAPPDATA%\\Completionist` shows what it cost.",
      "targetRelease": "0.3.0"
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
      ],
      "targetRelease": "0.3.0",
      "shippedIn": "0.2.12"
    },
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
      ],
      "targetRelease": "0.4.0",
      "shippedIn": "0.3.1"
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
      ],
      "targetRelease": "0.4.0",
      "shippedIn": "0.3.2"
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
      ],
      "targetRelease": "0.4.0",
      "shippedIn": "0.3.3"
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
      ],
      "targetRelease": "0.4.0",
      "shippedIn": "0.3.4"
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
      ],
      "targetRelease": "0.4.0",
      "shippedIn": "0.3.5"
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
      ],
      "targetRelease": "0.4.0",
      "shippedIn": "0.3.6"
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
      ],
      "targetRelease": "0.4.0",
      "shippedIn": "0.3.7"
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
      ],
      "targetRelease": "0.4.0",
      "shippedIn": "0.3.8"
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
      "notes": "Dropped: live checks no longer gate a milestone; it is a personal tool and daily use is the test. User runs .\\scripts\\install.ps1, checks the tray icon and Ctrl+Alt+P, reboots once to see the engine start by itself.",
      "targetRelease": "0.4.0"
    },
    {
      "id": "M4.10",
      "title": "README demo GIF and a small web demo of the popup",
      "status": "dropped",
      "area": "docs",
      "stories": [],
      "notes": "Moved to 0.6.7 (0.7.0, the last release before 1.0) so 0.4.0 does not wait on it. Originally: the public showcase, a short GIF of the popup in use and a plain HTML page with made-up text that shows the ranking.",
      "targetRelease": "0.4.0"
    },
    {
      "id": "M4.11",
      "title": "Remove the Completionist Spike keyboard and unregister it",
      "status": "dropped",
      "area": "install",
      "stories": [],
      "notes": "Moved from 0.1.0. The user removes it from the language list, then runs `tip/spike/register.ps1 -Unregister`. Moved to 0.6.8 so 0.4.0 does not wait on it.",
      "targetRelease": "0.4.0"
    },
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
      ],
      "targetRelease": "0.5.0",
      "shippedIn": "0.4.1"
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
      "notes": "A trigram is three words typed in a row within one sentence, never across punctuation, a newline or a window switch. Counts only, stored on disk locally. Admission follows 0.4.1.",
      "refs": [
        "engine/src/completionist_engine/personal.py",
        "branch m7-next-words"
      ],
      "targetRelease": "0.5.0",
      "shippedIn": "0.4.1"
    },
    {
      "id": "M7.3",
      "title": "`completionist-words` command: list and forget",
      "status": "done",
      "area": "engine",
      "stories": [
        67
      ],
      "notes": "`list` with search, `forget <word>` and `forget-recent 10m`. The smallest way to see and edit the log; the viewer (0.6.0) shows the same data.",
      "refs": [
        "engine/src/completionist_engine/personal.py",
        "branch m7-kinds"
      ],
      "targetRelease": "0.5.0",
      "shippedIn": "0.4.2"
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
      "notes": "An empty prefix is allowed: rank continuations from the bigram and trigram tables and the personal pairs. Return nothing when no continuation is likely enough, and put the threshold in the config so it can be switched off. Built as WordCompleter.next_words, used by the engine after a space. Config: [words] next (default false until 0.4.2 lets the popup open after a space safely) and next_threshold (default 0.05; 0 offers the best few whatever the odds).",
      "refs": [
        "engine/src/completionist_engine/words.py",
        "engine/src/completionist_engine/config.py",
        "branch m7-next-words"
      ],
      "targetRelease": "0.5.0",
      "shippedIn": "0.4.1"
    },
    {
      "id": "M7.5",
      "title": "Multi-word chunks from the n-gram tables",
      "status": "done",
      "area": "engine",
      "stories": [
        63
      ],
      "notes": "Extend the top continuation one word at a time while its probability stays above a cutoff, up to 3 words. Offer a chunk only when it reads coherently. Built as WordCompleter.chunks (cutoff 0.3, at most 3 words, never ends on the/a/of/to/and). Not yet in replies: 0.4.2 carries the kind and wires it in. Quality is limited by the WikiText corpus; judge it in 0.5.0.",
      "refs": [
        "engine/src/completionist_engine/words.py",
        "branch m7-next-words"
      ],
      "targetRelease": "0.5.0",
      "shippedIn": "0.4.1"
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
      ],
      "targetRelease": "0.5.0",
      "shippedIn": "0.4.2"
    },
    {
      "id": "M7.7",
      "title": "Popup shows words, chunks and the phrase in one box",
      "status": "done",
      "area": "dll",
      "stories": [
        64
      ],
      "notes": "The phrase stays on top. First guess at the order: phrase, then chunks, then words. The row order gets polished in the UI redesign (2.0.0). Checked in the TSF harness with a screenshot. Built: the popup already drew the phrase row above the word rows, so chunks (engine-ordered first) and next words appear in the same box; the service now remembers each row's kind and sends it on accept. Not checked in the TSF harness: it steals focus, so it runs when you say you're away.",
      "refs": [
        "tip/src/tsf_service.cpp",
        "tip/src/popup.cpp",
        "branch m7-kinds"
      ],
      "targetRelease": "0.5.0",
      "shippedIn": "0.4.2"
    },
    {
      "id": "M7.8",
      "title": "Open the popup after a space without stealing Tab or Enter",
      "status": "done",
      "area": "dll",
      "stories": [
        62,
        7,
        8
      ],
      "notes": "Today the popup closes on space (story 9), so this changes the key router and needs native tests for every key. A next-word row is not highlighted by default until you press Down, so Tab still indents or moves on, like the 150 ms no-steal idea for phrases. Built: next-word rows open with nothing highlighted, so Tab and Enter stay the app's until Up or Down highlights a row; Esc dismisses. 12 native tests cover every key. [words] next and chunks now default to on.",
      "refs": [
        "tip/src/popup_model.h",
        "tip/tests/test_popup_model.cpp",
        "tip/src/tsf_service.cpp",
        "branch m7-kinds"
      ],
      "targetRelease": "0.5.0",
      "shippedIn": "0.4.2"
    },
    {
      "id": "M7.9",
      "title": "Check next words and chunks live in real apps",
      "status": "dropped",
      "area": "test",
      "stories": [
        62,
        63,
        64,
        65
      ],
      "notes": "Dropped for the release: live checks no longer gate a milestone (see the 2026-09-30 decision); daily use is the test. The checklist below stays for when you want to run it. Needs you. Checklist: (1) rebuild and register the DLL from the merged code (tip\build.cmd, then tip\register.ps1; restart Chrome with chrome://restart) and restart the engine. (2) In Notepad, Discord and Chrome type 'I would like to ' and check: a few next words appear with nothing highlighted; Tab still indents or moves on; Down then Tab takes one; Enter still sends or adds a line. (3) Type 'thank you for yo' and check a chunk can show above the words and Tab takes it. (4) Judge: helpful, or noisy? If noisy, raise [words] next_threshold (for example 0.15) or set next = false or chunks = false in config.toml. (5) If the base tables feel thin, the next step is a looser prune and a chat-style corpus, and only then. Logs are in %LOCALAPPDATA%\\Completionist (engine.log, tip.log); completionist-stats shows accepts.",
      "targetRelease": "0.5.0"
    },
    {
      "id": "M8.1",
      "title": "Snapshot writer: personal dictionary and stats into one local HTML file",
      "status": "dropped",
      "area": "viewer",
      "stories": [
        68,
        69
      ],
      "notes": "Dropped 2026-09-30: a page opened from disk can't remove words or save settings, so the viewer is served by the engine instead (0.6.0).",
      "targetRelease": "0.6.0"
    },
    {
      "id": "M8.2",
      "title": "Viewer page: searchable, sortable dictionary with remove, a settings form and the stats, in the Evergreen theme",
      "status": "done",
      "area": "viewer",
      "stories": [
        67,
        68,
        69,
        75
      ],
      "notes": "Built as one self-contained page the engine serves: a searchable, sortable dictionary of words with counts and a Remove button, a settings form (only changed fields are saved), and stats by day, app and provider speed, in the Evergreen colours. Left out: a trigram list, because the store has no way to list or remove a single trigram; forgetting a word still removes the pairs and triples it was in.",
      "refs": [
        "m8-viewer",
        "engine/src/completionist_engine/viewer.html",
        "0.6.0"
      ],
      "targetRelease": "0.6.0",
      "shippedIn": "0.5.2"
    },
    {
      "id": "M8.3",
      "title": "Tray item: Open viewer",
      "status": "done",
      "area": "engine",
      "stories": [
        70
      ],
      "notes": "Next to the existing \"Show stats\" item; opens the engine's local address with its token.",
      "refs": [
        "m8-viewer",
        "engine/src/completionist_engine/tray.py",
        "engine/src/completionist_engine/app.py",
        "0.6.0"
      ],
      "targetRelease": "0.6.0",
      "shippedIn": "0.5.3"
    },
    {
      "id": "M8.4",
      "title": "Tests: the page serves counts and dictionary entries only, and other websites can't drive it",
      "status": "done",
      "area": "test",
      "stories": [
        17,
        56
      ],
      "notes": "No typed-text history, no text from silent fields or blocked apps. Requests without the token or from another origin are refused.",
      "refs": [
        "m8-viewer",
        "engine/tests/test_viewer.py",
        "engine/tests/test_settings.py",
        "0.6.0"
      ],
      "targetRelease": "0.6.0",
      "shippedIn": "0.5.4"
    },
    {
      "id": "M8.5",
      "title": "Check the viewer live",
      "status": "dropped",
      "area": "test",
      "stories": [
        68,
        69,
        70
      ],
      "notes": "Dropped for the release: live checks no longer gate a milestone (see the 2026-09-30 decision); the viewer was driven in the browser pane instead. Checklist for when you want it: restart the engine, choose Open viewer in the tray, type for a while, check the dictionary and stats, remove a word, change the words limit and watch it apply within a couple of seconds, and type your key once to see the page say it is set but never show it.",
      "targetRelease": "0.6.0"
    },
    {
      "id": "M8.6",
      "title": "Local viewer server in the engine, on this machine only",
      "status": "done",
      "area": "engine",
      "stories": [
        69,
        70
      ],
      "notes": "Binds 127.0.0.1 only and needs a secret token that the tray's Open viewer puts in the address. Checks the Host and Origin headers so another website can't call it. Serves the page and a small JSON interface: list words, forget a word, read and write settings.",
      "refs": [
        "m8-viewer",
        "engine/src/completionist_engine/viewer.py",
        "0.6.0"
      ],
      "targetRelease": "0.6.0",
      "shippedIn": "0.5.6"
    },
    {
      "id": "M8.7",
      "title": "Settings: show the config as a form and write changes back to config.toml",
      "status": "done",
      "area": "engine",
      "stories": [
        75
      ],
      "notes": "Only known options, validated, written back without losing the user's comments if practical. The phrase api_key is typed into the page by the user and saved to config.toml, but it is write-only: the engine never sends it to the page, the page shows only \"set\" or \"not set\", and it is never logged or printed in errors. The existing config hot reload applies changes.",
      "refs": [
        "m8-viewer",
        "engine/src/completionist_engine/settings.py",
        "engine/tests/test_settings.py",
        "0.6.0"
      ],
      "targetRelease": "0.6.0",
      "shippedIn": "0.5.7"
    },
    {
      "id": "M8.8",
      "title": "Trigrams: list and remove three-word phrases, in the store, the server and the viewer",
      "status": "done",
      "area": "engine",
      "stories": [
        67,
        68
      ],
      "notes": "PersonalStore.trigrams(search) and forget_trigram(a, b, c); /api/trigrams and /api/forget-trigram; a Phrases tab. Removing one phrase leaves its words and two-word pairs, since other sentences use them.",
      "refs": [
        "m8-trigrams",
        "engine/src/completionist_engine/personal.py",
        "engine/src/completionist_engine/viewer.py",
        "0.6.1"
      ],
      "targetRelease": "0.6.0",
      "shippedIn": "0.6.1"
    },
    {
      "id": "M8.9",
      "title": "Viewer redesign: clearer hierarchy, motion and accessibility",
      "status": "done",
      "area": "viewer",
      "stories": [
        68,
        69,
        75
      ],
      "notes": "Rebuilt viewer.html: four tabs kept in the address hash, usage bars, a daily chart, switches and a save bar for settings, a two-step confirm before any remove, toasts, empty and loading states, dark mode, phone width, reduced motion, keyboard-initiated changes not animated. Checked against the ui-ux-designer, ui-animation and web-design-guidelines skills.",
      "refs": [
        "m8-trigrams",
        "engine/src/completionist_engine/viewer.html",
        "0.6.1"
      ],
      "targetRelease": "0.6.0",
      "shippedIn": "0.6.1"
    },
    {
      "id": "M9.9",
      "title": "Versions replace milestones on the roadmap",
      "status": "done",
      "area": "docs",
      "stories": [],
      "refs": [
        "branch roadmap-versions",
        "scripts/check_roadmap.py",
        "docs/roadmap/tasks.html",
        "docs/roadmap/page.js",
        "0.6.2"
      ],
      "notes": "Every task shows the version it ships in instead of an M-id. Shipped work shows the release it went out in (several tasks can share one); unshipped work gets one patch number each, counting up to the next release, so 0.6.10 comes before 0.7.0. The M-ids stay in the file as internal keys and the old log lines keep them. The checker enforces the version rule.",
      "targetRelease": "0.7.0",
      "shippedIn": "0.6.2"
    },
    {
      "id": "M9.1",
      "title": "Fuzzy index over the vocabulary and personal words",
      "status": "done",
      "area": "engine",
      "stories": [
        71
      ],
      "refs": [
        "m9-fuzzy",
        "engine/src/completionist_engine/fuzzy.py",
        "engine/tests/test_fuzzy.py",
        "0.6.3"
      ],
      "notes": "Tried SymSpell first as planned, but it corrects whole words and can't handle half-typed fragments (see the decision). Uses rapidfuzz instead: a first-letter bucket scan with OSA edit distance and an early cutoff (see the decision).",
      "targetRelease": "0.7.0",
      "shippedIn": "0.6.3"
    },
    {
      "id": "M9.2",
      "title": "Fuzzy candidates fill the rows after exact prefix matches",
      "status": "done",
      "area": "engine",
      "stories": [
        71,
        73
      ],
      "refs": [
        "m9-fuzzy",
        "engine/src/completionist_engine/words.py",
        "engine/src/completionist_engine/assemble.py",
        "0.6.4"
      ],
      "notes": "Exact matches always come first. Corrections fill the remaining rows, or all of them when nothing matches. Edit distance 1 for short fragments and 2 for longer ones, and no correcting 1 to 2 letter fragments, names or learned words. Tests: moutian, definately, recieve.",
      "targetRelease": "0.7.0",
      "shippedIn": "0.6.4"
    },
    {
      "id": "M9.3",
      "title": "Protocol marks the guessed letters of each suggestion",
      "status": "done",
      "area": "engine",
      "stories": [
        72
      ],
      "refs": [
        "m9-fuzzy",
        "engine/src/completionist_engine/protocol.py",
        "engine/src/completionist_engine/words.py",
        "engine/src/completionist_engine/engine.py",
        "tip/src/protocol.cpp",
        "0.6.5"
      ],
      "notes": "Each item carries the positions it corrected. Tests on both sides. Builds on the suggestion kind from 0.4.2.",
      "targetRelease": "0.7.0",
      "shippedIn": "0.6.5"
    },
    {
      "id": "M9.4",
      "title": "Popup draws guessed letters in a third colour",
      "status": "done",
      "area": "dll",
      "stories": [
        72
      ],
      "refs": [
        "m9-fuzzy",
        "tip/src/popup.cpp",
        "tip/src/popup.h",
        "tip/src/tsf_service.cpp",
        "0.6.6"
      ],
      "notes": "Guessed letters draw in amber (validated: 10.6 on the popup background, 5.0 on the highlight row). Native tests pass and the DLL builds; the TSF harness screenshot runs when you say you are away.",
      "targetRelease": "0.7.0",
      "shippedIn": "0.6.6"
    },
    {
      "id": "M9.5",
      "title": "Latency check with the fuzzy index on",
      "status": "done",
      "area": "test",
      "stories": [
        71
      ],
      "refs": [
        "m9-fuzzy",
        "engine/tests/test_server.py",
        "0.6.7"
      ],
      "notes": "Lookup p95 stays under 10 ms over the real pipe (5 ms with typos typed; the fuzzy scan only runs when rows are unfilled).",
      "targetRelease": "0.7.0",
      "shippedIn": "0.6.7"
    },
    {
      "id": "M9.6",
      "title": "Check typo completion live in real apps",
      "status": "dropped",
      "area": "test",
      "stories": [
        71,
        72,
        73
      ],
      "notes": "Dropped for the release: live checks no longer gate a milestone (see the 2026-09-30 decision); daily use is the test. Checklist for when you want it: type moutian, definately, recieve and similar, and check that correct rare words aren't pushed aside.",
      "targetRelease": "0.7.0"
    },
    {
      "id": "M9.7",
      "title": "README demo GIF and a small web demo of the popup",
      "status": "dropped",
      "area": "docs",
      "stories": [],
      "notes": "Dropped for the 0.7.0 release: a public showcase with made-up data needs the owner's OK before publishing, and daily use is the test. Checklist: a short GIF of the popup in use and a plain HTML page showing the ranking (no engine, no server, no real data).",
      "targetRelease": "0.7.0"
    },
    {
      "id": "M9.8",
      "title": "Remove the Completionist Spike keyboard and unregister it",
      "status": "dropped",
      "area": "install",
      "stories": [],
      "notes": "Dropped for the 0.7.0 release: only the user can do it at a real desktop, and daily use is the test. Checklist: remove it from the language list, then run `tip/spike/register.ps1 -Unregister`.",
      "targetRelease": "0.7.0"
    },
    {
      "id": "M5.2",
      "title": "Code-review fixes before 1.0",
      "status": "done",
      "area": "engine",
      "stories": [
        34,
        35
      ],
      "refs": [
        "release-1.0",
        "engine/src/completionist_engine/phrase_scheduler.py",
        "tip/src/tsf_service.cpp",
        "0.7.2"
      ],
      "notes": "15 fixes from a full review: phrase and Esc suppression survive the sliding 8000-character window, DLL edit sessions hold the service alive, tray stats run on the engine loop, pause cancels phrases, personal-word promotion and chunk seeds, config encoding, viewer 500s, uninstall removes the roaming config, learning-off stops ranking.",
      "targetRelease": "1.1.0",
      "shippedIn": "0.7.2"
    },
    {
      "id": "M5.3",
      "title": "Evergreen tray and keyboard icon (no more Typer T)",
      "status": "done",
      "area": "engine",
      "stories": [],
      "refs": [
        "tray-icon-evergreen",
        "engine/src/completionist_engine/tray.py",
        "tip/assets/make_icon.py",
        "1.0.1"
      ],
      "notes": "The old icon was a blue square with a T for Typer. Now a pine-green square (Evergreen --sign) with a white C and a pink caret bar (--vms-ink); grey (--todo) when paused. Same drawing in the tray (Pillow) and the keyboard .ico (make_icon.py).",
      "targetRelease": "1.1.0",
      "shippedIn": "1.0.1"
    },
    {
      "id": "M5.4",
      "title": "Preferred Providers option in the viewer settings",
      "status": "done",
      "area": "viewer",
      "stories": [],
      "refs": [
        "tray-icon-evergreen",
        "engine/src/completionist_engine/settings.py",
        "engine/src/completionist_engine/viewer.html",
        "1.0.2"
      ],
      "notes": "provider_order (default Groq) is now a list box under Cloud Phrases in the viewer settings, one provider per line; an empty list lets OpenRouter choose.",
      "targetRelease": "1.1.0",
      "shippedIn": "1.0.2"
    },
    {
      "id": "M5.5",
      "title": "Requests log tab in the viewer",
      "status": "done",
      "area": "viewer",
      "stories": [],
      "refs": [
        "tray-icon-evergreen",
        "engine/src/completionist_engine/request_log.py",
        "engine/src/completionist_engine/viewer.html",
        "engine/src/completionist_engine/viewer.py",
        "1.0.3"
      ],
      "notes": "Modelled on the flexrouter request log: a live table (time, app, answered by, suggestion, first text, took, result) with search and a result filter, a side sheet with the text sent, the suggestion and each model tried (and why it failed), and a two-step Clear Log. Results are OK, Failover, Failed and Cancelled (the user kept typing). Text is held in memory only, last 200.",
      "targetRelease": "1.1.0",
      "shippedIn": "1.0.3"
    },
    {
      "id": "M5.6",
      "title": "Advanced phrase settings and editable instructions in the viewer",
      "status": "done",
      "area": "viewer",
      "stories": [],
      "refs": [
        "tray-icon-evergreen",
        "engine/src/completionist_engine/settings.py",
        "engine/src/completionist_engine/config.py",
        "engine/src/completionist_engine/context.py",
        "engine/src/completionist_engine/viewer.html",
        "1.0.4"
      ],
      "notes": "Cloud Phrases gains an expandable Advanced section (max output length, temperature, seconds before asking, give-up timeout, text before and after the caret, send-text-after) and an expandable Instructions section holding the system prompt, new as [phrase] instructions with a Restore Built-in button. The wait is shown in seconds but the file keeps debounce_ms. base_url stays out of the page on purpose: it would let a page redirect the key and typed text.",
      "targetRelease": "1.1.0",
      "shippedIn": "1.0.4"
    },
    {
      "id": "M5.1",
      "title": "Reply-aware context from the surrounding window",
      "status": "done",
      "area": "engine",
      "stories": [
        57,
        58
      ],
      "refs": [
        "reply-aware-context",
        "engine/src/completionist_engine/screen_context.py",
        "engine/src/completionist_engine/screen_windows.py",
        "engine/src/completionist_engine/context.py",
        "engine/src/completionist_engine/phrases.py",
        "1.0.5"
      ],
      "notes": "Windows OCR of the window in front, read in the background after a window or title has stayed in front for two looks (twice a second), never on the typing path. The text (the last 2000 characters, whole lines) goes first in the prompt as background; lines the person typed are removed. A capture is only handed over if its window is still in front. Skipped for blocked apps, while paused, without an API key, for the engine's own windows and for elevated programs. New [phrase] screen_context switch, on by default. UI Automation was measured and dropped: see decisions.",
      "targetRelease": "1.1.0",
      "shippedIn": "1.0.5"
    },
    {
      "id": "M5.7",
      "title": "Show the screenshot and extracted text in the Requests page",
      "status": "done",
      "area": "viewer",
      "stories": [],
      "refs": [
        "reply-aware-context",
        "engine/src/completionist_engine/viewer.py",
        "engine/src/completionist_engine/viewer.html",
        "engine/src/completionist_engine/request_log.py",
        "1.0.6"
      ],
      "notes": "A request's side sheet gains Screen it was shown (the JPEG, fetched with the token and shown from an in-memory blob, click for full size), Text read from the screen, and Sent to the model when lines were removed. The screenshot is served by a token-guarded endpoint, the page may show blob images and nothing else, and the pictures live in memory with the log and are cleared with it.",
      "targetRelease": "1.1.0",
      "shippedIn": "1.0.6"
    },
    {
      "id": "M10.4",
      "title": "Expanded settings and collapsible Advanced tiles",
      "status": "done",
      "area": "viewer",
      "stories": [],
      "notes": "All 30 approved controls: routing, timing, writing, context/privacy, app profiles, popup and keys; defaults retained and behavior tested. Verified with 626 passing Python tests (one skip), 77 native tests, four settings-form tests and a clean x64 DLL build; live popup and provider checks remain manual.",
      "refs": [
        "main",
        "codex/expanded-settings",
        "1.1.1",
        "docs/settings.md",
        "engine/src/completionist_engine/settings.py",
        "engine/src/completionist_engine/viewer.html",
        "tip/src/popup_settings.h"
      ],
      "targetRelease": "2.0.0",
      "shippedIn": "1.1.1"
    },
    {
      "id": "M10.5",
      "title": "Use the current C icon in the viewer header",
      "status": "done",
      "area": "viewer",
      "stories": [],
      "refs": [
        "engine/src/completionist_engine/viewer.html"
      ],
      "notes": "Replace the leftover Typer T header SVG with the Evergreen C and pink caret used by the tray and keyboard icons.",
      "targetRelease": "2.0.0",
      "shippedIn": "1.1.2"
    },
    {
      "id": "M10.6",
      "title": "Keep single-letter words out of personal learning",
      "status": "done",
      "area": "engine",
      "stories": [],
      "refs": [
        "engine/src/completionist_engine/personal.py",
        "engine/tests/test_personal.py"
      ],
      "notes": "Typed or accepted single-letter words are not learned; old single-letter entries are removed on open. Word suggestions still start at the first letter.",
      "targetRelease": "2.0.0",
      "shippedIn": "1.1.3"
    },
    {
      "id": "M10.7",
      "title": "Show full phrase request details and events",
      "status": "done",
      "area": "viewer",
      "stories": [],
      "refs": [
        "engine/src/completionist_engine/request_log.py",
        "engine/src/completionist_engine/phrases.py",
        "engine/src/completionist_engine/viewer.py",
        "engine/src/completionist_engine/viewer.html"
      ],
      "notes": "Keep full prompts and replies in memory and show the suffix, non-secret request settings and an event timeline beside the existing context, screenshot, outcome, timings and model attempts. Timings-only mode scrubs the added detail.",
      "targetRelease": "2.0.0",
      "shippedIn": "1.1.4"
    },
    {
      "id": "M10.8",
      "title": "Show exact outgoing request and HTTP status",
      "status": "done",
      "area": "viewer",
      "stories": [],
      "refs": [
        "engine/src/completionist_engine/phrase_provider.py",
        "engine/tests/test_phrases.py",
        "engine/tests/test_config_watch.py",
        "AGENTS.MD"
      ],
      "notes": "The in-memory event timeline shows the exact outgoing JSON without the authorization header, followed by the HTTP status; the config watcher test uses monotonic file times, and the repository has a small AGENTS.MD pointer to CLAUDE.md.",
      "targetRelease": "2.0.0",
      "shippedIn": "1.1.5"
    },
    {
      "id": "M10.9",
      "title": "Proportional highway lanes and a 3.0.0 browser lane",
      "status": "done",
      "area": "docs",
      "stories": [],
      "refs": [
        "docs/roadmap/map-layout.js",
        "docs/roadmap/map.js",
        "scripts/map_layout.test.mjs"
      ],
      "notes": "The highway map's lane height now follows how many patch stops a highway carries, and a tall lane deals its stops into slot rows above and below the road. The Browser extension highway opens at 3.0.0, not 1.0.0.",
      "targetRelease": "2.0.0",
      "shippedIn": "1.1.6"
    },
    {
      "id": "M10.10",
      "title": "Agent roadmap brief and edit helper",
      "status": "done",
      "area": "docs",
      "stories": [],
      "refs": [
        "scripts/roadmap.py",
        "scripts/test_roadmap_cli.py",
        "CLAUDE.md"
      ],
      "notes": "scripts/roadmap.py brief writes a short git-ignored docs/roadmap/roadmap-brief.md for agents, and start, done, log and add-task make the routine roadmap edits without opening roadmap.js, undoing themselves if the checker rejects the result. CLAUDE.md tells agents to run the brief first and avoid roadmap.js.",
      "targetRelease": "2.0.0",
      "shippedIn": "1.1.7"
    },
    {
      "id": "M10.3",
      "title": "Overhaul the dashboard (the viewer)",
      "status": "next",
      "area": "viewer",
      "stories": [],
      "notes": "Independent V2 viewer overhaul track. Desktop autocomplete scope now has a separate spec and nine-stage implementation plan, prioritised by the owner. The viewer still needs its own approved Words, Stats, Requests and Settings layout in Evergreen tokens. Existing settings remain authoritative; context receipt stays in the viewer. Native desktop completion alone does not finish the full 2.0.0 release.",
      "targetRelease": "2.0.0",
      "label": "Dashboard overhaul"
    },
    {
      "id": "M10.1",
      "title": "Scope the popup redesign: audit the popup in real apps and decide what else is included",
      "status": "doing",
      "area": "dll",
      "stories": [],
      "refs": [
        "docs/superpowers/specs/2026-10-02-v2-desktop-overhaul.md",
        "docs/superpowers/plans/2026-10-02-v2-desktop-overhaul.md",
        "codex/native-autocomplete-v2"
      ],
      "notes": "Scope and document review for the V2 native desktop surfaces. The owner verified the C++ glass spike works and requested softer true Gaussian blur. The final spec and nine-stage plan are drafted; latest material review, joint document approval and live desktop capture/focus/DPI verification remain pending. No production renderer or installed DLL has been changed. M10.14 is the next feasibility stage; viewer redesign remains an independent V2 track.",
      "targetRelease": "2.0.0",
      "label": "Popup scope"
    },
    {
      "id": "M10.2",
      "title": "Redesign and rewrite the popup drawing",
      "status": "dropped",
      "area": "dll",
      "stories": [
        74
      ],
      "refs": [
        "docs/superpowers/specs/2026-10-02-v2-desktop-overhaul.md",
        "docs/superpowers/plans/2026-10-02-v2-desktop-overhaul.md",
        "codex/native-autocomplete-v2"
      ],
      "notes": "Superseded by nine individually tracked desktop overhaul stages M10.14 through M10.22, matching Tasks 1 through 9 of the implementation plan. This broad drawing task is retained as history rather than duplicated implementation work; its patch number is preserved and no release is created. The separate renderer, real data, focus and IPC, layout, dock and fallback, TSF integration, packaging and validation now have their own scope and gates.",
      "targetRelease": "2.0.0",
      "label": "Superseded popup"
    },
    {
      "id": "M10.11",
      "title": "Explain Windows OCR context to the cloud model",
      "status": "done",
      "area": "engine",
      "stories": [],
      "refs": [
        "engine/src/completionist_engine/context.py",
        "docs/settings.md"
      ],
      "notes": "Update the phrase prompt to identify OCR text as locally recognized, imperfect background context and treat it as untrusted data; document that only recognized text is sent.",
      "targetRelease": "2.0.0",
      "shippedIn": "1.1.11"
    },
    {
      "id": "M10.12",
      "title": "Copy the prompt test pack and disable reasoning by default",
      "status": "done",
      "area": "engine",
      "stories": [],
      "refs": [
        "engine/src/completionist_engine/context.py",
        "engine/src/completionist_engine/phrase_provider.py",
        "engine/src/completionist_engine/viewer.html",
        "docs/settings.md"
      ],
      "notes": "Add a Settings action that copies the built-in system prompt plus three provider-playground scenarios; switch phrase requests to chat completions so reasoning effort defaults to none and no thinking output is requested.",
      "targetRelease": "2.0.0",
      "shippedIn": "1.1.12"
    },
    {
      "id": "M10.13",
      "title": "Ship OCR-aware prompt playground to main",
      "status": "done",
      "area": "engine",
      "stories": [],
      "refs": [
        "engine/src/completionist_engine/context.py",
        "engine/src/completionist_engine/phrase_provider.py",
        "engine/src/completionist_engine/viewer.html"
      ],
      "notes": "Merge the Windows OCR-aware system prompt, three synthetic provider comparison scenarios, copy action and no-reasoning default into main.",
      "targetRelease": "2.0.0",
      "shippedIn": "1.1.13"
    },
    {
      "id": "M10.23",
      "title": "Generate roadmap briefs at three detail levels",
      "status": "done",
      "area": "docs",
      "stories": [],
      "refs": [
        "scripts/roadmap.py",
        "scripts/test_roadmap_cli.py",
        "CLAUDE.md"
      ],
      "notes": "Generated focused briefs: minimal shows doing/next/blocked tasks on one line each; medium adds full current notes and the five most recently shipped versions and their completed tasks; maximum alone retains full context. The default brief aliases minimal. Scope tests verify exclusions, recent history limits and lossless maximum data.",
      "targetRelease": "2.0.0",
      "shippedIn": "1.1.14"
    },
    {
      "id": "M10.14",
      "title": "Prove live soft glass in a standalone native renderer",
      "status": "doing",
      "area": "dll",
      "stories": [
        74
      ],
      "refs": [
        "docs/superpowers/specs/2026-10-02-v2-desktop-overhaul.md",
        "docs/superpowers/plans/2026-10-02-v2-desktop-overhaul.md",
        "codex/native-autocomplete-v2"
      ],
      "notes": "Dependency: reviewed V2 spec and plan. Prove two non-activating windows with real desktop refraction, proper GPU Gaussian blur, sharp foreground text and no capture feedback. Test exclusion failure, device loss, HDR/unsupported-session fallback, rotation and mixed DPI; verify focus stays in the typing app. Measure GPU/paint timing and idle repaint behavior. Owner-run live material review is the gate before integration; the static spike is not production proof.",
      "targetRelease": "2.0.0",
      "stage": 1,
      "label": "Live glass proof"
    },
    {
      "id": "M10.15",
      "title": "Supply truthful AI lifecycle and suggestion origins",
      "status": "todo",
      "area": "engine",
      "stories": [
        74
      ],
      "refs": [
        "docs/superpowers/specs/2026-10-02-v2-desktop-overhaul.md",
        "docs/superpowers/plans/2026-10-02-v2-desktop-overhaul.md",
        "codex/native-autocomplete-v2"
      ],
      "notes": "Dependency: Stage 1 feasibility gate; engine work may be developed independently after document review. Publish actual scheduled/working/streaming/ready/unavailable state, scheduler-derived wait and elapsed durations, trigger reason and aligned Local/Learned origins. Keep ranking unchanged and tense unavailable until V2.1. Fake-clock/provider tests cover reset, cancel, failure, manual/off modes and stale updates; native parsing tests cover legacy clients and malformed optional metadata.",
      "targetRelease": "2.0.0",
      "stage": 2,
      "label": "AI lifecycle"
    },
    {
      "id": "M10.16",
      "title": "Implement renderer protocol, focus leases and arbitration",
      "status": "todo",
      "area": "dll",
      "stories": [
        74
      ],
      "refs": [
        "docs/superpowers/specs/2026-10-02-v2-desktop-overhaul.md",
        "docs/superpowers/plans/2026-10-02-v2-desktop-overhaul.md",
        "codex/native-autocomplete-v2"
      ],
      "notes": "Dependency: Stage 1; establishes the shared interfaces used by later stages. Add versioned full-snapshot/acknowledgement framing, same-user/session pipe validation, actual peer PID checks, foreground eligibility and revision/generation arbitration. A 500 ms heartbeat expires after 1500 ms; revoked focus cannot be revived by old messages. Test malformed/oversize frames, negative coordinates, competing apps, host exit and disconnect; both surfaces must hide without stale content.",
      "targetRelease": "2.0.0",
      "stage": 3,
      "label": "Renderer IPC"
    },
    {
      "id": "M10.17",
      "title": "Build native layout, typography and Evergreen palette",
      "status": "todo",
      "area": "dll",
      "stories": [
        74
      ],
      "refs": [
        "docs/superpowers/specs/2026-10-02-v2-desktop-overhaul.md",
        "docs/superpowers/plans/2026-10-02-v2-desktop-overhaul.md",
        "codex/native-autocomplete-v2"
      ],
      "notes": "Dependency: Stages 2 and 3. Implement pure layout for the 330-DIP caret menu and 190-DIP bottom-left dock, measured font/width settings, origins, correction comparison, dotted guessed letters and exact partial-accept underline. Keep foreground sharp and pending AI targets stable. Test 100/150/200% DPI, negative/portrait monitors, every edge, small work areas and long Unicode content. Verify palette parity and generated light/dark fixture images.",
      "targetRelease": "2.0.0",
      "stage": 4,
      "label": "Native layout"
    },
    {
      "id": "M10.18",
      "title": "Complete glass windows, dock motion and recovery",
      "status": "todo",
      "area": "dll",
      "stories": [
        74
      ],
      "refs": [
        "docs/superpowers/specs/2026-10-02-v2-desktop-overhaul.md",
        "docs/superpowers/plans/2026-10-02-v2-desktop-overhaul.md",
        "codex/native-autocomplete-v2"
      ],
      "notes": "Dependency: Stages 1, 3 and 4. Compose final glass/text layers; implement the non-activating dock toggle, interruptible 180/220 ms collapse, auto-minimize/restore, manual preference, reduced motion and accessible states. Connection dot indicates the engine pipe only; V2 displays Tense dash. Test high contrast, transparency disabled, exclusion/device/capture failure and cleanup. Successful-current-presentation acknowledgements are required; no frozen captured background or focus theft.",
      "targetRelease": "2.0.0",
      "stage": 5,
      "label": "Glass & motion"
    },
    {
      "id": "M10.19",
      "title": "Ship matching host fallback and keyboard regressions",
      "status": "todo",
      "area": "dll",
      "stories": [
        74
      ],
      "refs": [
        "docs/superpowers/specs/2026-10-02-v2-desktop-overhaul.md",
        "docs/superpowers/plans/2026-10-02-v2-desktop-overhaul.md",
        "codex/native-autocomplete-v2"
      ],
      "notes": "Dependency: Stages 2 and 4. Redraw the in-host opaque fallback with shared Evergreen layout, corrections/origins, partial underline and truthful compact AI status. Preserve the existing popup HWND for engine messages/timers. Add worker-to-UI connection notifications. Test status-only Tab passthrough, unchanged Enter, after-space unselected Tab, the 150 ms phrase guard, explicit-selection persistence, stale keys and configured shortcuts. Build a review artifact without installing it.",
      "targetRelease": "2.0.0",
      "stage": 6,
      "label": "Host fallback"
    },
    {
      "id": "M10.20",
      "title": "Connect TSF snapshots with renderer failover",
      "status": "todo",
      "area": "dll",
      "stories": [
        74
      ],
      "refs": [
        "docs/superpowers/specs/2026-10-02-v2-desktop-overhaul.md",
        "docs/superpowers/plans/2026-10-02-v2-desktop-overhaul.md",
        "codex/native-autocomplete-v2"
      ],
      "notes": "Dependency: Stages 3, 5 and 6. Add asynchronous renderer client, canonical physical-pixel caret conversion and current-focus full snapshots; the DLL remains insertion/keyboard authority. Coalesce shows, prioritize hides and cap the queue at 32. Suppress fallback only for matching presented acknowledgements; restore after disconnect/unavailable or 250 ms without acknowledgement. Fake-peer tests cover old revisions, bad peers, delays, renderer kill and focus races; manually verify host DPI-awareness modes.",
      "targetRelease": "2.0.0",
      "stage": 7,
      "label": "TSF integration"
    },
    {
      "id": "M10.21",
      "title": "Integrate renderer lifecycle, packaging and viewer contracts",
      "status": "todo",
      "area": "install",
      "stories": [
        74
      ],
      "refs": [
        "docs/superpowers/specs/2026-10-02-v2-desktop-overhaul.md",
        "docs/superpowers/plans/2026-10-02-v2-desktop-overhaul.md",
        "codex/native-autocomplete-v2"
      ],
      "notes": "Dependency: Stage 7. Python supervises one hidden native renderer from a known local path, with at most three crash restarts in 60 seconds and owned-child shutdown. Test missing binaries, duplicate session instance, retry exhaustion and clean exit. Package paired DLL/renderer artifacts and preserve rollback versions; installation/UAC remains user-run. Existing viewer settings stay authoritative and context receipt stays in Requests; dashboard layout remains a separate V2 track.",
      "targetRelease": "2.0.0",
      "stage": 8,
      "label": "Packaging"
    },
    {
      "id": "M10.22",
      "title": "Validate and stage the desktop overhaul",
      "status": "todo",
      "area": "test",
      "stories": [
        74
      ],
      "refs": [
        "docs/superpowers/specs/2026-10-02-v2-desktop-overhaul.md",
        "docs/superpowers/plans/2026-10-02-v2-desktop-overhaul.md",
        "codex/native-autocomplete-v2"
      ],
      "notes": "Dependency: all preceding stages. Run engine/native/renderer tests and both production builds; then owner-run Notepad, Chrome and Electron checks at 100/150/200%, both monitors/all edges, themes, accessibility, motion, real AI lifecycle, correction/partial acceptance and engine/renderer failure. Measure timing and idle targets, document actual results and paired-artifact rollback. Run focus-stealing harness only while the owner is away. Enable by default only after gates pass; full V2 also requires the viewer track.",
      "targetRelease": "2.0.0",
      "stage": 9,
      "label": "Validation"
    },
    {
      "id": "roadmap-release-atlas",
      "targetRelease": "2.0.0",
      "title": "Separate release destinations from roadwork and build the Release atlas",
      "status": "done",
      "area": "docs",
      "stories": [],
      "notes": "Implemented concept A: release destinations, independent completion and explicit shipping, flat tasks with stable IDs and local stages, scope-based atlas regions, expandable shipping history, migrated task board and briefs. Verified all 91 released records and all 122 original task identities and historical facts survive. Validation: 23 Python tests, 8 Node geometry tests, JavaScript syntax checks and browser task selection, historical writing, expansion, search, future destinations and 390px/1440px layouts. Local HTTP preview verified; direct file navigation is blocked by the test browser protocol policy. No release or tag created.",
      "label": "Release atlas",
      "completed": "2026-10-02",
      "refs": [
        "codex/roadmap-release-atlas",
        "scripts/roadmap.py",
        "scripts/check_roadmap.py",
        "scripts/migrate_roadmap.py",
        "docs/roadmap/map-layout.js",
        "docs/roadmap/map.js",
        "docs/roadmap/page.js",
        "docs/roadmap/tasks.html"
      ]
    },
    {
      "id": "atlas-label-readability",
      "targetRelease": "2.0.0",
      "title": "Keep atlas labels clear of construction lines",
      "status": "done",
      "area": "docs",
      "stories": [],
      "notes": "Construction connectors now use side gutters and render before marker text. Stage, title and status text masks underlying lines with a small background stroke; the current-position line renders behind labels. Browser screenshot and task selection verified. JavaScript syntax, 8 layout tests and 23 Python tests pass.",
      "label": "Readable map labels",
      "completed": "2026-10-02",
      "refs": [
        "docs/roadmap/map.js",
        "codex/roadmap-release-atlas"
      ]
    },
    {
      "id": "M11.1",
      "title": "Use sentence tense when ranking word suggestions",
      "status": "todo",
      "area": "engine",
      "stories": [],
      "notes": "Detect whether the current sentence is in past or present tense from available context and use it as a ranking signal. Keep typed-prefix and context evidence in consideration; if tense is ambiguous or mixed, do not force a tense-based change.",
      "targetRelease": "2.1.0",
      "label": "Tense-aware ranking"
    },
    {
      "id": "M6.1",
      "title": "Chrome extension over Native Messaging",
      "status": "todo",
      "area": "extension",
      "stories": [],
      "notes": "Inline ghost text and page context in web fields. Moved from 1.1.1 to 2.0.1 (2026-10-01), then to 2.1.1 (2026-10-01) to leave room for the tense-aware suggestions milestone; it still leads the 3.0.0 major release after the dashboard and popup redesign.",
      "targetRelease": "3.0.0",
      "label": "Chrome extension"
    }
  ]
};
