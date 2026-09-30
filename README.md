# Typer

System-wide, VS Code–style English autocomplete for Windows. A word dropdown at the caret in every app, plus a phrase continuation you accept with Tab.

It's built as a **Text Services Framework (TSF) text service**, the same mechanism IMEs use. That way the caret position, surrounding text, key handling and text insertion all come from Windows itself instead of keyboard hooks and screen guessing.

The full spec is in [issue #1 (PRD)](https://github.com/notnotnotnoone/typer/issues/1).

**Roadmap:** open [`docs/roadmap/index.html`](docs/roadmap/index.html) (tasks) or [`docs/roadmap/releases.html`](docs/roadmap/releases.html) (release timeline) in a browser. Both read [`docs/roadmap/roadmap.js`](docs/roadmap/roadmap.js), which agents keep current (see [`CLAUDE.md`](CLAUDE.md)).

## Install

Needs [uv](https://docs.astral.sh/uv/) and Visual Studio 2022 Build Tools with the **Desktop development with C++** workload and the Windows 11 SDK. In PowerShell:

```powershell
.\scripts\install.ps1
```

It installs the engine's dependencies, builds and registers the DLL (one UAC prompt), adds the Typer keyboard to your English language(s), creates a logon task and starts the engine. Then switch to the Typer keyboard with Win+Space and type. Apps that were already open need a restart.

```powershell
.\scripts\uninstall.ps1              # add -DeleteData to remove %LOCALAPPDATA%\Typer too
```

Phrases (the greyed continuation) need a cheap completion-model key, set once as a user environment variable. The key is never stored in Typer's config:

```powershell
setx DEEPSEEK_API_KEY "your key"
```

Then restart the engine from the tray icon. Phrases stop for the day once they've cost the daily budget (default $0.50).

## Using it

| Key | What |
|---|---|
| Tab | Accept the highlighted row (the phrase, if one is showing, else the top word) |
| Up / Down | Move the highlight |
| Ctrl+Right | Accept the next word of the phrase |
| Ctrl+Space | Ask for a phrase now (in apps where phrases aren't automatic) |
| Esc | Dismiss until the next word |
| Ctrl+Alt+P | Pause / resume Typer everywhere |

Enter is never touched. Nothing is shown or sent from password fields.

The tray icon pauses and resumes, shows what Typer has saved you, and opens the settings file and logs. `typer-stats` (in `engine/`) prints the same numbers in the terminal.

## Layout

| Path | What |
|---|---|
| `engine/` | Python engine (uv project): pipe server, word completion, phrase provider, learning, metrics, tray |
| `tip/` | C++ TSF text service DLL (adapted from Microsoft's SampleIME). The M0 spike is in `tip/spike/` |
| `bench/` | Provider comparison (`typer-bench`): candidates file and how to run it |
| `docs/roadmap/` | Roadmap data, task board and release timeline (open the HTML files from disk) |
| `scripts/` | Roadmap checker, `install.ps1`, `uninstall.ps1` |

## Development

```bash
cd engine
uv sync
uv run pytest
```

C++ builds and tests are in [`tip/README.md`](tip/README.md).
