# Typer

System-wide, VS Code–style English autocomplete for Windows. A word dropdown at the caret in every app, plus a phrase continuation you accept with Tab.

It's built as a **Text Services Framework (TSF) text service**, the same mechanism IMEs use. That way the caret position, surrounding text, key handling and text insertion all come from Windows itself instead of keyboard hooks and screen guessing.

The full spec is in [issue #1 (PRD)](https://github.com/notnotnotnoone/typer/issues/1).

**Roadmap:** open [`docs/roadmap/index.html`](docs/roadmap/index.html) (tasks) or [`docs/roadmap/releases.html`](docs/roadmap/releases.html) (release timeline) in a browser. Both read [`docs/roadmap/roadmap.js`](docs/roadmap/roadmap.js), which agents keep current (see [`CLAUDE.md`](CLAUDE.md)).

## Layout

| Path | What |
|---|---|
| `engine/` | Python engine (uv project): pipe server, word completion, phrase provider, learning, metrics |
| `tip/` | C++ TSF text service DLL (adapted from Microsoft's SampleIME). The M0 spike is in `tip/spike/`; the real DLL is M1 |
| `bench/` | Provider comparison (`typer-bench`): candidates file and how to run it |
| `data/` | N-gram table build scripts *(not started)* |
| `docs/roadmap/` | Roadmap data, task board and release timeline (open the HTML files from disk) |
| `scripts/` | Roadmap checker; install / uninstall scripts later |

## Engine

```bash
cd engine
uv sync
uv run pytest
```

## Prerequisites for the DLL

Visual Studio 2022 with the **Desktop development with C++** workload and the **Windows 11 SDK**.
