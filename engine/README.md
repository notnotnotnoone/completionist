# typer-engine

The background process behind Typer. It serves word completions (and later phrase continuations) to the TSF text service over a named pipe. See the [PRD](https://github.com/notnotnotnoone/typer/issues/1).

```bash
uv sync
uv run pytest
uv run typer-engine            # serve on \\.\pipe	yper-engine
uv run typer-probe "I'd like to recomm"   # ask a running engine, without the DLL
```

## How words are ranked

1. **Frequency.** `wordfreq` gives the base vocabulary (~146k words), ranked Zipf-style.
2. **Context.** With `ngrams.sqlite` in the data folder, bigram and trigram counts re-rank by the previous one or two words ("I'd like to" -> "know" over "knowledge"). The same corpus stats drop misspellings and surnames from the rare end of the vocabulary.
3. **Habits.** The personal store boosts words and word pairs you use, and adds words outside the dictionary (names, slang) after 3 uses. It holds counts of single words and word pairs only, never text. Only real typing counts (one character at a time); pasted text, caret jumps and backspacing do not. Nothing is learned in password/URL/email/number fields or block-listed apps.

## Data files

Both live in `%LOCALAPPDATA%\Typer` (change with `[data] dir` in the config).

| File | Made by | What |
|---|---|---|
| `ngrams.sqlite` | `typer-build-ngrams` | Word counts and pruned bigram/trigram tables from a text corpus. Optional. |
| `personal.sqlite` | the engine | Your word and word-pair counts. Saved every 10 s and on exit. |

Build the n-gram file from any plain text (`.txt`, `.gz`, or folders of them):

```bash
uv run typer-build-ngrams corpus1.txt corpus2.txt
```

The current file was built from WikiText-103 (81M words): 100k-word vocabulary, 0.8M bigrams, 1.6M trigrams, 66 MB, ~6.5 minutes. It is encyclopedic English, so it helps most on formal text.

## Config

`%APPDATA%\Typer\config.toml`, all optional:

```toml
[apps]
block = ["code.exe"]        # no suggestions at all
allow = ["obsidian.exe"]    # automatic phrase suggestions (Milestone 3)

[words]
limit = 5

[learning]
enabled = true
promote_after = 3

[data]
dir = "C:/somewhere"
```
