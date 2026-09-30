Reference for anyone working on this project. CLAUDE.md is how agents work; this file is what the product is.

## 1. Name

The product is called **Completionist**. It was first called Typer (the working name through 0.0.5); the engine moves a leftover `%LOCALAPPDATA%\Typer` and `%APPDATA%\Typer` folder to the new name on its first start. The name appears in:

- the `COMPLETIONIST_ROADMAP` global in `roadmap.js`, and the engine package `completionist_engine`
- the CLI commands `completionist-engine`, `completionist-probe`, `completionist-stats` and `completionist-build-ngrams`
- the pipe `\.\pipe\completionist-engine`, the folder `%LOCALAPPDATA%\Completionist\`, the config `%APPDATA%\Completionist\config.toml` and `CompletionistTip.dll`
- the keyboard name in Windows Settings

The GitHub repository and the local checkout folder are still named `typer`; renaming them is up to the owner.

## 2. What it is

System-wide, VS Code-style English autocomplete for Windows. In any app with a text box, a small word popup appears at the caret, and above it a greyed phrase row continues your sentence. You accept with Tab. It works in Notepad, Chrome, Edge, Discord, Teams and other Electron apps, and it is silent where it shouldn't act (see Privacy and safety).

It is built as a **Text Services Framework (TSF) text service**, the same mechanism IMEs use, so the caret position, the surrounding text, key handling and text insertion all come from Windows itself instead of keyboard hooks and screen guessing. The spec is the PRD in [issue #1](https://github.com/notnotnotnoone/typer/issues/1).

## 3. Keys

| Key | What |
|---|---|
| Tab | Accept the highlighted row (the phrase if one is showing, else the top word) |
| Up / Down | Move the highlight |
| Ctrl+Right | Accept the next word of the phrase |
| Ctrl+Space | Ask for a phrase now (in apps where phrases aren't automatic) |
| Esc | Dismiss until the next word |
| Ctrl+Alt+P | Pause or resume Completionist everywhere |

**Enter is never used**, so chat messages still send.

## 4. Architecture

- **The DLL** (`tip/`, C++, `CompletionistTip.dll`) is loaded by Windows into every app with text input. It is thin: the popup window, the key router, the context reader and the pipe client. Pipe I/O is on a worker thread, so it never blocks the app's UI thread. No exception crosses a COM boundary. If the engine isn't running, nothing happens and the DLL reconnects on its own.
- **The pipe** is `\\.\pipe\completionist-engine`. Messages are length-prefixed JSON: a 4-byte little-endian length, then UTF-8, with a 1 MB cap. The DLL checks that the engine runs as the same user.
- **The engine** (`engine/`, Python, a standalone uv project) does word ranking, phrases, learning, metrics, the tray icon, the pause hotkey and config hot reload.
- **Data** lives in `%LOCALAPPDATA%\Completionist` (change it with `[data] dir`):

| File | What |
|---|---|
| `ngrams.sqlite` | Word counts and pruned bigram and trigram tables from a text corpus (optional) |
| `personal.sqlite` | Your word and word-pair counts, saved every 10 seconds and on exit |
| `metrics.sqlite` | Usage counts and timings |
| `spend.json` | Today's phrase spend |
| `engine.log` | Engine log (rotating) |
| `tip.log` | DLL log. An empty file named `verbose` next to it turns on debug lines |

- **The config** is `%APPDATA%\Completionist\config.toml`. All of it is optional, and edits apply within a couple of seconds. A bad edit keeps the old settings and logs why.

## 5. How words are ranked

1. **Word frequency.** `wordfreq` gives the base vocabulary (about 146k words), ranked Zipf-style.
2. **Context.** With `ngrams.sqlite` present, bigram and trigram counts re-rank by the previous one or two words ("I'd like to" ranks "know" above "knowledge"). The n-gram file was built from WikiText-103 (81M words). It is encyclopedic English, so it helps formal writing most.
3. **Your habits.** The personal store boosts words and word pairs you use, and adds words outside the dictionary (names, slang) after **3 uses**. It holds counts only, never text. Only real typing counts, one character at a time. Pasted text, caret jumps and backspacing do not.

## 6. How phrases work

- A phrase is a continuation of your sentence from a cheap cloud completion model, streamed into the popup's top row.
- **Models:** cheap completion models only, never premium chat models. The provider is OpenRouter, and the models are named in `[phrase] models = [...]`. The first is used, and the next only if it fails. Swapping a model is a config edit.
- **Anchored context:** the engine sends up to 8,000 characters before the caret and 2,000 after, cut at paragraph and sentence boundaries so the provider's prefix cache hits. OpenRouter has no fill-in-the-middle, so the model sees only the text before the caret, and phrases are held back while other text follows the caret on the same line.
- **Cost:** spend is counted per day. Phrases stop for the day at a **$0.50 daily cap**. Three provider failures in a row pause phrases for 30 seconds.
- **When:** in apps listed under `[apps] allow`, a phrase is requested 350 ms after you pause. Everywhere else, only when you press Ctrl+Space.
- **The API key** is `api_key` under `[phrase]` in `%APPDATA%\Completionist\config.toml`, typed there by the user. It is plain text in that file, never shown in logs, errors or printed settings. Never ask for a key in chat.

## 7. Privacy and safety

- Password, URL, email and number fields are silent, and so are code editors and terminals. Nothing is shown or sent from them.
- Block-listed apps (`[apps] block`) get no suggestions at all.
- Nothing is learned from pasted text.
- Metrics are counts and timings only, never typed text.
- Text goes only to the provider you configured.
- The caret comes from TSF only. There is no AI and no vision for caret or screen tracking.

## 8. Glossary

| Term | Meaning |
|---|---|
| TSF | Text Services Framework: the Windows API IMEs use to read and edit text in any app |
| Text service, TIP | The COM component (`CompletionistTip.dll`) that plugs into TSF. TIP means text input processor |
| IME | Input method editor. Completionist is registered the way an IME is, but it never changes what you type unless you accept |
| Input scope | A hint an app gives Windows about a field (password, URL, email, number). Completionist stays silent in those |
| Edit session | The TSF mechanism for reading or changing text. Chromium apps sometimes fail it during focus changes, so Completionist retries |
| Engine | The Python process behind the DLL |
| Pipe | The named pipe `\\.\pipe\completionist-engine` between the DLL and the engine |
| Word completion | The dropdown of likely words at the caret |
| Phrase | A greyed continuation of the sentence, shown in the top row of the popup |
| Phrase row | The top row of the popup, where the phrase streams in |
| FIM | Fill-in-the-middle: a completion mode where the model also sees the text after the caret |
| Anchored context | Context cut at fixed boundaries so the provider's prefix cache hits and cost stays low |
| Allow-list, block-list | `[apps] allow` lists apps where phrases appear on their own. `[apps] block` lists apps with no suggestions |
| Silent field | A field where Completionist shows and sends nothing, and learns nothing |
| Personal store | `personal.sqlite`: your word and word-pair counts |
| Promotion | Adding a word to your vocabulary after 3 uses |
| N-gram table | Counts of word pairs and triples (`ngrams.sqlite`) used to rank by context |
| Budget | The daily spend cap for phrases ($0.50) |
| Metrics | Counts and timings in `metrics.sqlite`, shown by `completionist-stats` |
| Harness | The scripts under `tip/tests` that drive the real DLL through real TSF. `tsf_e2e.ps1` steals focus |
| Spike | The throwaway M0 experiment in `tip/spike/` that proved the TSF approach |
| Milestone | A group of tasks (M0, M1, ...). Each minor release is one milestone |
| Release tiers | Patch (a sentence), minor (a paragraph, one milestone), major (an essay) |
| Highway | On the roadmap map, one part of the app: Engine, Text service, Tooling, Browser extension |
| Interchange | A major release: every open highway meets here |
| Station | A minor release, drawn across the highways its milestone touched |
| Stop | A patch release, on the highway it touched |
| Cul-de-sac | A bug-fix patch, drawn as a short spur |

## 9. Releases and the roadmap

- **Semver.** `MAJOR.MINOR.PATCH`. The release version lives only in `docs/roadmap/roadmap.js`, not in package versions.
- **Writing tiers.** A patch gets exactly one sentence (under 50 words). A minor release gets one paragraph (50 to 220 words) and is one milestone. A major release gets an essay of at least 4 paragraphs and 450 words. `scripts/check_roadmap.py` enforces the lengths.
- **`roadmap.js` is the source of truth**, and **every commit updates it**: a new line at the top of `log`, and `python scripts/check_roadmap.py` must print `roadmap ok`.
- **Two pages read it**, opened from disk and never published: `index.html` (the highway map) and `tasks.html` (the task board).

The highway map rules:

| Thing | Drawn as |
|---|---|
| **Highway** (a part of the app) | A thick horizontal road in its highway color, stacked: Engine on top, then Text service, Tooling, Browser extension |
| **Major release** (X.0.0) | An **interchange**: every open highway curves into one bundle and back out, with a shield and an overhead gantry sign |
| **Project start** | An interchange at the far left where every highway without `opens` begins |
| **Minor release** (X.Y.0) | A **station**: a tall capsule across the highways it touched, with an exit sign above it |
| **Patch** (X.Y.Z) | A **stop**: a circle on the one highway it touched, or a small capsule across several |
| **Bug-fix patch** (`"kind": "fix"`) | A **cul-de-sac**: a short spur ending in a round turnaround |
| **Released** | Filled solid with a check mark |
| **Up next** | Magenta ring with a slowly pulsing halo and a "NEXT" tag |
| **Planned** | Surface-colored fill with a dashed outline |
| **Road built** (the milestone's tasks on that highway are done) | Solid highway color with a white dashed lane marking |
| **Under construction** (doing, next or blocked) | Diagonal stripes with a barricade where construction starts |
| **Not built** (todo) | The highway color at 35% opacity, dashed |
| **Open road, no work planned** | A thin dotted line at 35% opacity |
| **Now** | A magenta line and a "YOU ARE HERE" pin just after the latest released version |

- A task's `area` decides its highway, and every area belongs to exactly one highway (`highways` in `roadmap.js`).
- Patch releases name the highways they touched. Minor releases get theirs from their milestone's tasks. Major releases take every open highway.
- A highway with `"opens": "X.0.0"` starts at that major release, and nothing before it may name it.
- Road between two anchors (each milestone's release, each interchange and the end of the road) belongs to the milestone at the right-hand anchor, and its length is split by that milestone's tasks on the highway. Built road can therefore run past "now": the work is done but not released yet.

## 10. Evergreen: the official color scheme

Evergreen is the color scheme of the assistant and its pages: pine-green guide signs and highways, with magenta for now and next. There is no orange-and-white and no blue-and-black. The tokens live in `docs/roadmap/theme.css`; pages use them and add no hex values of their own.

| Token | Light | Dark | Role |
|---|---|---|---|
| `--paper` | `#EEF2EC` | `#0E1512` | page background |
| `--paper-2` | `#E3EAE1` | `#121B17` | map panel, bands |
| `--surface` | `#FBFCF8` | `#141C18` | cards, road casing |
| `--ink` | `#16221C` | `#E4EBE6` | text |
| `--muted` | `#55635B` | `#97A69D` | secondary text |
| `--ghost` | `#626E67` | `#7F8C84` | planned outlines, faint text |
| `--line` / `--line-strong` | `#D5DDD6` / `#B7C3BA` | `#25302A` / `#34423A` | borders |
| `--sign` | `#0A5A3D` | `#0F6B48` | guide-sign green: signs, masthead, current page tab |
| `--sign-deep` | `#06402B` | `#094A31` | sign edge |
| `--sign-ink` / `--sign-dim` | `#FFFFFF` / `#CFE6DA` | `#FFFFFF` / `#BFE3CF` | text on signs |
| `--hw-engine` | `#0B6E4B` | `#079461` | Engine highway (pine) |
| `--hw-tsf` | `#5E3BA3` | `#7553BD` | Text service highway (violet) |
| `--hw-tooling` | `#0CA1A7` | `#11A3A9` | Tooling highway (teal) |
| `--hw-browser` | `#787804` | `#6C6C01` | Browser extension highway (olive) |
| `--accent` | `#AB3F84` | `#C0188D` | magenta: now, up next, construction, focus ring |
| `--accent-ink` | `#9A3576` | `#EE8FC6` | magenta text and links |
| `--accent-soft` / `--on-accent` | `#F4E1EC` / `#FFFFFF` | `#3A1830` / `#FFFFFF` | pill background, text on magenta |
| `--ok` / `--ok-soft` | `#1F6B45` / `#DCEDE2` | `#5FCB8E` / `#15301F` | status done |
| `--warn` / `--warn-soft` | `#7A5C00` / `#F3E9C8` | `#E3C052` / `#332A10` | status next |
| `--bad` / `--bad-soft` | `#B3261E` / `#F8DEDA` | `#FF8C7E` / `#3A1916` | status blocked, open risk |
| `--todo` / `--todo-soft` | `#56625B` / `#E1E7E2` | `#A5B0A9` / `#1E2823` | status todo |
| `--vms` / `--vms-ink` | `#101814` / `#FF9ED2` | `#060A08` / `#FF8FCB` | the "now" electronic sign |

**Validation** was done with the dataviz skill's `validate_palette.js` (Machado 2009 color-blindness simulation, OKLab ΔE×100):

- Adjacent highways passed every check, with worst color-blind ΔE 17.3 in light mode and 16.6 in dark.
- The accent against every highway passed every check, with worst ΔE 8.5 in light mode and 8.4 in dark.
- Every text pair is at least 4.5:1. For example, `--accent-ink` on `--paper` is 5.9, and white on `--sign` is 8.3.

To re-validate the highway colors:

```bash
node <dataviz skill>/scripts/validate_palette.js "#0B6E4B,#5E3BA3,#0CA1A7,#787804" --mode light --surface "#FBFCF8"
node <dataviz skill>/scripts/validate_palette.js "#079461,#7553BD,#11A3A9,#6C6C01" --mode dark --surface "#141C18"
```

**Rules:**

- **Highway colors only ever mean highways:** roads, lane markers, dots and task stripes. Never status.
- **Magenta only means now, up next, in progress, selection or focus, and links.** It's never a highway.
- **Status is never shown by color alone.** There's always a word or a shape too (check, ring, dashes, barricade).
- **Sign green is the brand surface.** Text on it is white.
- **No orange and no blue brand color, anywhere.**
- **Dark mode is its own set of values.** Only the texture is inverted. Change both modes together.
- **A new highway color** must pass `validate_palette.js` in both modes, against the highways next to it (the default adjacent check) and with the accent alternating against every highway. Put the results in this file.

## 11. Typography

- **Overpass** (Google Fonts, in the style of Highway Gothic), weights 400 to 900 and italics 800 and 900: UI, signs and headings.
- **Fraunces**, optical sizes 9 to 144, weights 400, 600 and 900, and italic: release writing, essays and drop caps.
- **Overpass Mono**, weights 400, 600 and 700: versions, ids, dates and numbers.

The Google Fonts link goes in the `<head>` of every roadmap page, before `theme.css`. It is in `index.html` and `tasks.html`.

## 12. Key decisions and constraints

From `roadmap.js` `decisions`:

- **Build Completionist as a TSF text service.** The spike confirmed the caret rectangle, real context, key capture and insertion in Notepad and in Chromium and Electron apps.
- **A thin C++ DLL, with all intelligence in one Python engine process**, joined by a named pipe with length-prefixed JSON.
- **Phrases come from cheap completion models, never premium chat models.** The budget is under $0.50 a day, through anchored context and a daily cap.
- **No AI or vision for caret or screen tracking.** TSF gives the caret. UI Automation reads context after v1.
- **Google Docs and the Google search box are out of scope**: canvas rendering, and Google's own suggestion dropdown.
- **The n-gram corpus is WikiText-103.** A chat-style corpus could be added later for casual text.
- **Releases follow semantic versioning**, with writing that matches each release's size.
- **The roadmap pages are plain HTML files opened from disk.** They are not published anywhere.
- **The roadmap is a highway map** in the Evergreen color scheme.

From the working agreements in `CLAUDE.md`:

- **Cost ceiling:** heavy use must stay under $0.50 a day.
- **Keys live in user environment variables** that the user sets. Never ask for API keys in chat. Config only names the variable.
- **The user doesn't write C++.** Claude owns the C++ code and the toolchain, which stays CLI-only and small on disk.
- **The user does hands-on app testing.** Give a short checklist, then read the logs. Don't drive the screen unless asked.
- **The TSF harness steals focus.** Run `tip/tests/tsf_e2e.ps1` only when the user is away.
- **Don't change Windows system settings.** Give the user the command instead (the keyboard list, the registry). `regsvr32` raises a UAC prompt the user approves.
- **Don't over-engineer.** Completionist is a typing tool. Build what was asked, the simplest way, and pick sensible defaults.
- **Branch per chunk of work.** Ask before pushing to `main`.

## 13. Where things live

| Path | What |
|---|---|
| `engine/` | Python engine, a standalone uv project. `cd engine && uv sync && uv run pytest` |
| `tip/` | The C++ TSF DLL. `tip/spike/` is the M0 spike |
| `docs/roadmap/roadmap.js` | The roadmap data: tasks, releases, decisions, risks and the log |
| `docs/roadmap/index.html` | The highway map (with `map-layout.js`, `map.js` and `page.js`) |
| `docs/roadmap/tasks.html` | The task board |
| `docs/roadmap/theme.css`, `common.js` | The Evergreen tokens and components, and the shared helpers (`window.RM`) |
| `docs/superpowers/` | The design spec and plan for the highway map |
| `scripts/` | `check_roadmap.py`, `map_layout.test.mjs`, `install.ps1`, `uninstall.ps1` |
| `README.md` | Install and use |
| `CLAUDE.md` | How agents work on this repo |
| `CONTEXT.md` | This file |
| `%LOCALAPPDATA%\Completionist\` | Engine and DLL data and logs |
| `%APPDATA%\Completionist\config.toml` | User config |
