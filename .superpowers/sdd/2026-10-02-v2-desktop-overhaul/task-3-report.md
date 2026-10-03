# Task 3 source implementation report

## Scope and status

The version-1 codecs, pure session logic, Windows boundary helper source and compile-only test targets described below are implemented. **M10.16 remains doing.** Native runtime RED/GREEN, Win32 endpoint/hook behavior and the prerequisite Stage 1 live gate remain unrun or unverified; this report records source and compile evidence and does not claim the roadmap task is complete. The renderer host remains inert: no client launch, native window creation, capture enablement, installation or host switch was added.

## Interfaces and behavior

- `tip/src/render_protocol.h/.cpp` defines `completionist::render::Identity`, `Rect`, `Command`, `AiState`, `Candidate`, `Snapshot` and `Ack` once. `PopupSettings` comes from `tip/src/popup_settings.h`. `EncodeShow` emits a complete four-byte little-endian length-prefixed UTF-8 JSON frame; `ParseShow` and `ParseAck` consume bounded JSON bodies for schema 1. `IsCurrentAck` accepts only a presented acknowledgement whose full owner identity and revision match the current snapshot.
- `tip/src/json_internal.h` is the extracted parser used by both existing engine protocol parsing and the renderer codec. It keeps each number's original token for exact integer conversion, rejects duplicate members and invalid JSON number grammar, and bounds object/array sizes and nesting. Renderer uint64 values use `from_chars`, preserving `UINT64_MAX` for host HWND, generation and revision and rejecting overflow, fractional and boolean values.
- Codec validation bounds frames at 1 MiB, candidate count at 64, marks at 256 per candidate, strings and session IDs, settings and enum values. UTF-8 decoding/encoding uses the existing replacement-character conversion. Physical negative rectangle coordinates survive unchanged. Unknown optional fields are ignored; malformed or missing required fields fail parsing. Selection follows `PopupModel` sentinels: `-1` is the phrase, `-2` is no highlighted row, and nonnegative values must identify a real word. A pending status snapshot can keep the `-2` shelf with no fabricated word.
- `tip/renderer/session.h/.cpp` accepts only snapshots matching the actual foreground PID supplied by the OS boundary, with valid content/selection and newer owner generation/revision. It renews only the active identity, expires after 1500 ms without renewal, retires revoked generations, clears cached snapshot/content on revoke and rejects stale acknowledgements. Foreground and time predicates remain injectable as plain values for deterministic test source.
- `tip/renderer/windows.h/.cpp` creates a named-pipe instance with a DACL limited to the current logon SID and `PIPE_REJECT_REMOTE_CLIENTS`. Peer validation queries actual pipe client/server PIDs and process tokens and compares user SID, logon SID, token AuthenticationId and token session ID. The renderer-side client validator range-checks the opaque HWND before conversion, requires an actual foreground window owned by the declared peer process, and never dereferences client memory. WinEvent foreground/desktop-switch hooks and WTS lock/disconnect/logoff notifications revoke the session; `RevocationHooks` unregisters them during teardown. These OS helpers are source-compiled but are not wired into the inert renderer process pending later integration.
- `tip/tests/test_render_protocol.cpp`, `tip/renderer/tests/test_session.cpp` and `tip/renderer/tests/test_windows.cpp` add native assertion cases for framing, round trips, UTF-8 replacement, malformed/truncated/schema/required-field rejection, optional fields, 1 MiB and bounded-array limits, negative coordinates, exact uint64 boundaries, ack staleness, generation/revision arbitration, foreground checks, lease expiry, revocation, takeover and OS-identity predicates.
- `tip/test.cmd` and `tip/renderer/test.cmd` accept `--compile-only`; the host target now compiles as C++20 x64 with `/W4 /WX /MT`. The renderer target also compile-builds its security/session assertion executable and the existing blur fixture without launching either executable.

## Verification evidence

The two explicitly compile-only commands completed with exit code 0:

```text
& .\tip\test.cmd --compile-only; $code=$LASTEXITCODE; Write-Output "exit=$code"; exit $code
test_main.cpp
test_popup_model.cpp
test_protocol.cpp
test_render_protocol.cpp
protocol.cpp
render_protocol.cpp
Generating Code...
exit=0
```

```text
& .\tip\renderer\test.cmd --compile-only; $code=$LASTEXITCODE; Write-Output "exit=$code"; exit $code
compilation header save succeeded; see C:\projects\Experiments\completionist\tip\renderer\out\shaders.h
compilation header save succeeded; see C:\projects\Experiments\completionist\tip\renderer\out\blur_ps.h
main.cpp
fixture.cpp
surfaces.cpp
capture.cpp
material.cpp
Generating Code...
test_blur.cpp
material.cpp
Generating Code...
test_main.cpp
test_session.cpp
test_windows.cpp
session.cpp
windows.cpp
render_protocol.cpp
protocol.cpp
Generating Code...
exit=0
```

`python scripts/check_roadmap.py` printed `roadmap ok: 96/108 tasks done, latest release 1.1.14, target 2.0.0, updated 2026-10-03`. The compile-only scripts built test executables but did not launch them. `M10.16` remains `doing` with these compile-only results and runtime limits recorded in the roadmap log.

## Unverified limits

Native assertion runtime RED/GREEN status is **UNRUN** by owner instruction. No native executable, fixture, test harness, debugger, window or live-capture path was launched. Win32 named-pipe ACL enforcement, actual token queries, HWND/foreground validation and hook delivery/cleanup are compile-reviewed but not runtime-verified. The prerequisite Stage 1 live gate is also unverified. The OS helpers remain inert until renderer integration. The current source does not add the asynchronous `RenderClient` queue; renderer publication, acknowledgement dispatch and end-to-end pipe lifecycle remain later integration work.

## Review round 1 correction

The shared JSON parser now materializes at most 4096 elements per container, marks a container truncated at that boundary, and continues syntax validation while skipping further values. It also caps a document at 1 MiB. The existing engine parser ignores a truncated optional `origins` array or truncated optional `popup` object, and unknown oversized metadata is skipped without discarding valid words. Truncated required engine `words`/`kinds`/`marks` arrays and required renderer `words`/`marks` arrays remain rejected.

Authored regression source includes `oversized_or_malformed_optional_metadata_does_not_discard_engine_words` in `tip/tests/test_protocol.cpp` for an oversized origins array, malformed origins member and oversized unknown array/object. `render_parser_enforces_frame_and_bounded_array_limits` in `tip/tests/test_render_protocol.cpp` retains candidate-limit coverage and now also supplies more than the shared parser's 4096-entry materialization limit, which `ParseShow` must reject because `words` is required.

The host protocol source and test executable were compile-checked only; the assertions remain **UNRUN**:

```text
& .\tip\test.cmd --compile-only; $code=$LASTEXITCODE; Write-Output "exit=$code"; exit $code
test_main.cpp
test_popup_model.cpp
test_protocol.cpp
test_render_protocol.cpp
protocol.cpp
render_protocol.cpp
Generating Code...
exit=0
```

No renderer compile-only rerun or native runtime run was performed for this correction. M10.16 remains `doing`; its runtime, Win32 and Stage 1 gates remain unverified.
