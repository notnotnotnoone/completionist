# PRD: Completionist 2.1.0, Tense-aware suggestions

Status: draft for owner review · 2026-10-05 · roadmap destination `2.1.0` (task `M11.1`)

## 1. Problem

Word ranking today looks at three things: how common a word is, which words came just before it (n-grams), and what the writer personally uses. None of them notice **tense**.

Typing "Yesterday we walk" should lean toward "walked"; typing "Every day we walk" should lean toward "walks" or "walking". Right now both get the same list, and the n-gram data (built from WikiText, which is encyclopedic and mostly past tense) quietly tilts everything toward past-tense forms even in a present-tense sentence.

V2.0 already reserved a place for this: the information dock shows `Tense —` and the V2 spec says real classification is "V2.1". 2.1.0 fills that in.

## 2. Goal

Use whether the **current sentence** is past or present as one more signal when ranking word suggestions, and show the detected tense in the dock.

**Done when** (from the release plan): past and present sentence context can influence ranking in representative examples, while ambiguous or mixed-tense context leaves the existing ranking unchanged.

## 3. Non-goals

- No AI, model or cloud call for tense. It is a small local rule check.
- No change to phrase (cloud) suggestions. See open question 3.
- No future tense, no grammar checking, no other languages.
- No new settings page, no tuning knobs. One on/off switch at most (open question 4).
- No change to what text is stored or sent anywhere. Tense is computed from the text already read for ranking, per request, and never saved.
- No DLL work beyond showing a value the engine already sends.

## 4. How it should behave

### 4.1 Detecting the sentence's tense

Input: the words of the current sentence before the one being typed (the engine already has this as `sentence_words`).

Output: `past`, `present` or `none`.

Cues, deliberately small and explainable:

| Past cue | Present cue |
|---|---|
| Past-tense verb form: `walked`, `went`, `was`, `were`, `had`, `did` | Present verb form: `walk` after a subject, `walks`, `is`, `are`, `am`, `has`, `does` |
| Time words: `yesterday`, `ago`, `last` (week/year), `earlier` | Time words: `today`, `now`, `currently`, `every`, `always`, `usually` |

Rules:

1. Cues of **one** tense only → that tense. One strong cue is enough.
2. Cues of **both** tenses, or no cues → `none`. This is the "mixed or ambiguous leaves ranking unchanged" requirement.
3. A new sentence (`. ! ?` or newline) resets to `none`.
4. Past participles after `have/has/had` (`has walked`) and modals (`will walk`, `can walk`) are **not** tense cues. Only the plain forms above count. Anything we are unsure of is `none`.

Wrong detection is worse than no detection, so the detector errs toward `none`.

### 4.2 Ranking effect

When tense is `past` or `present`, candidates that are **verb forms** get a modest multiplicative nudge in `_score` (`words.py`): up for forms that fit the tense, slightly down for forms that clearly don't. Non-verbs and ambiguous words are untouched.

- The nudge is small enough that a clearly better typed-prefix match or n-gram match still wins. Tense breaks ties and re-orders near-ties, it never overrides the prefix.
- Typo-corrected (fuzzy) candidates are not nudged.
- `none` means **byte-for-byte the current ranking** (guarded by a test).

Verb-form knowledge comes from a small table: a hand-checked list of common irregular verbs (`go/went`, `be/was/were`, `have/had`…, a few hundred) plus regular `-ed` / `-s` forms accepted only when the stem is also a dictionary word (so `red`, `need`, `bed` are not read as past tense).

### 4.3 The dock

The dock's tense field shows `Past`, `Present`, or `—` for `none`. This replaces the V2 dash placeholder. The engine adds a `tense` field to the word reply; the renderer shows it. Old DLLs ignore unknown fields.

### 4.4 Examples (these become the pytest cases)

| Typed | Expect |
|---|---|
| `Yesterday I walk` | `walked` ranks above `walks` |
| `Every day I walk` | `walks`/`walking` rank above `walked` |
| `Yesterday I walk every` (both cues) | unchanged from today's ranking |
| `walk` with no sentence before it | unchanged |
| `The red` | `red`-ending words not treated as past tense |
| Fresh sentence after `.` | tense resets |

## 5. Success measures

- The examples above pass in the engine pytest suite through the public completion interface.
- In a one-evening real-app test the owner finds the suggestions fit the sentence more often than not, and never noticeably worse.
- Lookup stays fast: no visible added delay (the existing lookup budget of p95 under 10 ms holds).
- Dock shows `Past` / `Present` / `—` correctly in Notepad and Chrome.

## 6. Risks

| Risk | Mitigation |
|---|---|
| Wrong tense guess reorders good suggestions | Detector errs to `none`; nudge is small and tie-breaking only |
| `-ed`/`-s` false positives (`red`, `bus`, `news`) | Stem must be a real dictionary word; short words excluded |
| Dock flickers between values while typing | Value only changes when a cue arrives; `none` is a normal resting state |
| n-gram data is past-heavy so the nudge fights it | Nudge applied after n-gram blending and tested on present-tense examples |

## 7. Work breakdown (roadmap tasks, all target 2.1.0)

1. **Tense detector**: `engine`: pure function over sentence words, with the examples as tests.
2. **Verb-form table**: `engine`: irregular list plus guarded regular rule.
3. **Tense nudge in ranking**: `engine`: `_score` change, `none` is a no-op.
4. **Report tense to the dock**: `engine` plus `dll`: `tense` reply field, renderer shows it.
5. **IRL check**: owner types the example sentences in Notepad and Chrome; agent reads `engine.log`. Closes on the owner's report.

Per project policy there are no new native tests or harnesses: one small pytest file for the engine, IRL checklist for the dock.

## 8. Decisions (owner, 2026-10-05)

1. **Verb-form source:** a public table, not hand-made. We use **UniMorph English** (public, CC BY-SA, derived from Wiktionary), turned into `engine/src/completionist_engine/data/tense_forms.tsv` by `engine/tools/build_tense_forms.py`. Where UniMorph lacks a regular verb's forms (it has no past for "want"), the script spells them out by the usual rules, but only if the result is a real common word. This replaces the hand-made list and the "stem must be a dictionary word" rule in section 4.2.
2. **Strength:** clashing tense is pushed **down** (x0.6), matching tense is lifted **slightly** (x1.15). A plural-noun lookalike (`walks`, `works`) is only ever lifted, never pushed down. Both numbers are named constants in `words.py` to tune after real use.
3. **Phrases:** unchanged. The AI model works out tense itself, so nothing new is sent to OpenRouter.
4. **Switch:** `[words] tense_aware = true`, also in the viewer's settings.

Detector detail: time words used are `yesterday`, `ago`, `earlier`, `previously`, `formerly` (past) and `today`, `now`, `currently`, `nowadays` (present). `every`, `always` and `usually` were dropped as too ambiguous.
