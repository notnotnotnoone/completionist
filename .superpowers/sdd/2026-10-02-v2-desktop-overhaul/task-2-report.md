# Task 2 report: truthful AI lifecycle and candidate origins

M10.15 remains **doing**. Engine status and origin metadata are implemented; native protocol runtime assertions remain unverified pending the owner's clearance to launch native test executables.

## Behavior

`PhraseScheduler.display_status(now)` reads the current due time, provider-origin identity, provider start time, first yielded chunk, and terminal result. It does not change scheduler state or run a timer. `PhraseSession` records provider start at the provider's `request_sent` event and publishes `working` after the response starts, before any text chunk. Provider updates continue to use the latest keystroke id while lifecycle transitions must match the original provider request id. Canceled or stale starts, chunks, and terminal events cannot bring old state back.

Candidate origins follow the final case-adjusted and deduplicated word list, plus chunk/next-word assembly and truncation. A boosted base-vocabulary word remains `local`; an out-of-base candidate is `learned` only when its personal-store count meets the configured promotion threshold. A corpus-only out-of-base next word is filtered out. Legacy words/phrase messages retain their old shape when optional metadata is absent.

The native parser independently ignores malformed optional status/origin data while preserving valid required words. It maps unknown status values to `unavailable`; durations require finite whole numbers from 0 through 600000, and origins are retained only as a valid, aligned array.

## Python checks

The required RED command was:

```text
.\engine\.venv\Scripts\python.exe -m pytest engine/tests/test_phrase_scheduler.py engine/tests/test_phrases_pipe.py engine/tests/test_engine.py --basetemp=.superpowers/pytest-task2 -p no:cacheprovider
```

It reported **37 passed, 5 failed**. Four failures identified the missing status snapshot and provider lifecycle methods. The fifth was an overly specific order in the new origin assertion; the existing engine ranked `worse` before `worry`, confirming the test needed to avoid prescribing a ranking change.

The focused GREEN command, including protocol serialization tests, was:

```text
.\engine\.venv\Scripts\python.exe -m pytest engine/tests/test_phrase_scheduler.py engine/tests/test_phrases_pipe.py engine/tests/test_engine.py engine/tests/test_protocol.py --basetemp=.superpowers/pytest-task2 -p no:cacheprovider
```

Result: **69 passed** at the initial implementation checkpoint. After adding an explicit personal-store promotion check for out-of-base origins and confirming ranking regressions, the final focused command was:

```text
.\engine\.venv\Scripts\python.exe -m pytest engine/tests/test_phrase_scheduler.py engine/tests/test_phrases_pipe.py engine/tests/test_engine.py engine/tests/test_protocol.py engine/tests/test_ranking.py --basetemp=.superpowers/pytest-task2 -p no:cacheprovider
```

Result: **131 passed**.

An initial full-engine invocation did not capture its final output or exit code. After updating the legacy assertions for status-only pushes and origins, the full engine command was rerun with explicit session polling:

```text
.\engine\.venv\Scripts\python.exe -m pytest engine/tests --basetemp=.superpowers/pytest-task2 -p no:cacheprovider
```

Result: **633 passed, 1 skipped in 18.46s**, exit code 0. This run also passed the timing-sensitive p95 test.

After updating those old assertions, the changed Python areas were checked with:

```text
.\engine\.venv\Scripts\python.exe -m pytest engine/tests/test_phrases.py engine/tests/test_server.py --basetemp=.superpowers/pytest-task2 -p no:cacheprovider -q
```

Result: **38 passed** on that run. A later combined regression command covering 107 tests reported **106 passed, 1 failed**; the remaining failure was `test_round_trip_with_the_fuzzy_index_on_is_under_10ms_at_p95` (measured p95 21.47 ms). An immediate isolated rerun also failed (p95 24.53 ms), while another isolated run after the focused suite passed in 2.06 s. The final full engine run passed this timing check. No ranking or performance algorithm was changed.

## Native and static checks

The native sources and tests compiled successfully with VS 2022 Build Tools using `/std:c++20 /W4 /WX /EHsc /Zi /utf-8`. The compiled inputs were `tip/tests/test_main.cpp`, `tip/tests/test_popup_model.cpp`, `tip/tests/test_protocol.cpp`, and `tip/src/protocol.cpp`; the output was `.superpowers/native-task2/tests.exe`. Compilation exited 0 and printed the four source names followed by `Generating Code...`.

The produced executable was not launched, and `tip/test.cmd` was not run. Native parser assertions for absent/valid/invalid metadata therefore remain **compile-only and runtime-unverified**. No native app, fixture, debugger, live capture, or harness was launched.

`python scripts/check_roadmap.py` printed `roadmap ok`; `git diff --check` found no whitespace errors.

## Changed files

- Engine lifecycle and wire metadata: `engine/src/completionist_engine/phrase_scheduler.py`, `phrases.py`, `protocol.py`, `words.py`, and `engine.py`.
- Native optional metadata parsing: `tip/src/protocol.h`, `tip/src/protocol.cpp`.
- Tests: new `engine/tests/test_phrase_scheduler.py`; updates to `test_engine.py`, `test_phrases.py`, `test_phrases_pipe.py`, `test_protocol.py`, `test_server.py`, and `tip/tests/test_protocol.cpp`.
- Roadmap: M10.15 is marked doing and the implementation/pending-runtime note is logged.

The named native runtime checks and the final full engine result still need verification. The timing-sensitive p95 test failed on the latest two runs and should be assessed under a quiet machine.
