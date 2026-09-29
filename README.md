# Typer

System-wide, VS Code–style English autocomplete for Windows. A word dropdown at the caret in every app, plus a phrase continuation you accept with Tab.

It's built as a **Text Services Framework (TSF) text service**, the same mechanism IMEs use. That way the caret position, surrounding text, key handling and text insertion all come from Windows itself instead of keyboard hooks and screen guessing.

The full spec is in [issue #1 (PRD)](https://github.com/notnotnotnoone/typer/issues/1).

## Layout

| Path | What |
|---|---|
| `engine/` | Python engine (uv project): pipe server, word completion, phrase provider, learning, metrics |
| `tip/` | C++ TSF text service DLL (adapted from Microsoft's SampleIME) *(not started)* |
| `bench/` | Phrase-provider benchmark *(not started)* |
| `data/` | N-gram table build scripts *(not started)* |
| `scripts/` | Install / uninstall scripts *(not started)* |

## Engine

```bash
cd engine
uv sync
uv run pytest
```

## Prerequisites for the DLL

Visual Studio 2022 with the **Desktop development with C++** workload and the **Windows 11 SDK**.
