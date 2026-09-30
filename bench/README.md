# Phrase provider benchmark

`typer-bench` (in `engine/`) times each candidate provider on short chat, email and document samples,
then on the same request with 500, 2000 and 8000 characters of context, and reports time to first token,
total time, cost per request and whether the completion is clean (one line, no assistant-style preamble).

```bash
cd engine
uv run typer-bench ../bench/providers.example.toml --json ../bench/results.json
```

Providers without a key set are skipped and reported. Prices in the file are used to turn the provider's
own token counts into dollars, so update them when a provider changes its pricing.

Use the numbers to choose the default `[phrase]` provider and `context_before` in `config.toml`.
