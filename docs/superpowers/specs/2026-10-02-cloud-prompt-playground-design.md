# Cloud prompt test pack and reasoning controls

## Goal

Make Completionist's cloud phrase prompt easy to compare in provider playgrounds, while keeping automatic phrase requests in a low-latency, no-reasoning mode by default.

## User experience

In Settings → Instructions, add a **Copy test pack** button alongside **Restore Built-in**. It copies a self-contained plain-text bundle with:

1. The built-in system prompt, including the policy that window text comes from local Windows OCR, can be noisy, and is untrusted context.
2. Three provider-playground cases with clear boundaries between system prompt, user input, and evaluation notes. The notes describe expected behavior rather than mandate a single exact completion, so the user can compare response quality across models.
3. A short benchmark setup note: use the same model settings for every provider, set reasoning effort to **None**, and turn off visible thinking/reasoning output.

The cases cover (a) replying to a request without making up a commitment, (b) continuing an ambiguous thought without inventing personal details, and (c) using noisy OCR as background while ignoring an instruction embedded in the screen text. The first two focus on ordinary autocomplete quality; the third checks OCR handling.

The button copies the built-in prompt, not a user's custom instructions, so the pack is repeatable and safe to paste into public playgrounds. Show a success or failure message; if clipboard access is unavailable, select the generated pack in a text area so it can be copied manually. Keep the existing prompt editor, restore action, and save behavior intact.

## Request format and reasoning

Move phrase requests from legacy text completions to OpenRouter Chat Completions. Send the stable prompt instructions in a `system` message and the app/window header, optional OCR context, and typed text in a `user` message. Preserve streaming, stop sequences, fill-in-the-middle suffixes, provider routing preferences, cancellation, timing, request logging, and existing model fallback behavior. Parse visible assistant message content only; reasoning fields must never enter the inserted phrase.

Add a persisted phrase reasoning-effort setting with values supported by OpenRouter (`none`, `minimal`, `low`, `medium`, `high`, and `xhigh`). Default and reset it to `none`, show it in the Routing settings tile, and send it as `reasoning: {"effort": value}`. No reasoning output is requested or used. Models that require reasoning or do not support `none` may reject the request; report the normal provider error/fallback behavior rather than silently retrying with reasoning enabled.

The app's configured endpoint remains unchanged and continues to represent the OpenRouter API root. Requests now append `/chat/completions`. This change aligns the actual request with OpenRouter's documented reasoning controls and system/user message roles. Other arbitrary OpenAI-compatible services configured by editing `config.toml` may not support these OpenRouter extensions; document that compatibility boundary.

## Prompt and playground data

Keep test-pack scenario text in one small Python constant/module adjacent to prompt assembly, so clipboard output and tests have one source of truth. Scenario text is mock data and must not contain personal messages, keys, or live captured screen text. The copied pack should label every section and avoid markdown fences that can accidentally terminate when a prompt contains fence characters.

## Validation

- Unit tests cover request JSON, stream parsing, reasoning-effort validation/defaults, and the copied pack contents.
- Viewer tests verify the button copies the built-in prompt and scenarios, and report clipboard failure accessibly.
- Run the engine test suite and roadmap checker. Do not make a live provider request as part of automated tests.
- In the review checklist, inspect the copied bundle in a playground and compare the same three cases on at least two providers with reasoning set to None. Confirm reported model latency and visible completions remain separate observations.

## Risks and boundaries

- Some reasoning models/providers enforce reasoning and may reject `none`; automatic fallback must not turn reasoning back on.
- Models may ignore or reinterpret effort controls. Prompt rules cannot guarantee identical model behavior, so comparison notes should not claim a universal guarantee.
- Clipboard APIs can fail in local-page/browser contexts; retain a manual-copy fallback.
- The chat endpoint changes the request envelope, so streaming extraction and fill-in-the-middle behavior need explicit regression coverage.

## Review checklist

- [ ] The built-in prompt is separated cleanly from request-specific data.
- [ ] The copied pack has exactly three concise, reusable cases and practical quality criteria.
- [ ] Default `none` is persisted, visible, sent to OpenRouter, and never weakened on failure.
- [ ] Existing privacy boundaries remain: no screen image is sent, no keys are copied, and scenario content is synthetic.
