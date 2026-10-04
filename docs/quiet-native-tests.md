# Quiet native test runs

`scripts/quiet_native_tests.py` is a guarded entry point for five staged pure-test binaries only:

- `tests.exe` and `test_popup_layout.exe` in `tip/out/task9-review/host-tests/`
- `test_session.exe`, `test_capture_policy.exe` and `test_blur.exe` in `tip/out/task9-review/renderer-tests/`

It rejects other program names and accepts no child arguments. The renderer application, the TSF harness, live capture tools and install scripts are outside this launcher.

Build the small C++ runner with `scripts/build_quiet_native_runner.cmd`. Then run one test at a time, for example:

```powershell
python scripts/quiet_native_tests.py test_popup_layout.exe
python scripts/quiet_native_tests.py test_session.exe --timeout-ms 120000
```

Each run has a two-minute default timeout and a ten-minute maximum. Output is drained continuously and saved under `tip/out/task9-review/native-test-logs/`; each stream is capped at 1 MiB. A timeout closes the private Windows Job Object, which terminates that run's assigned process tree.

Before starting the C++ runner, the Python entry point sets and verifies inherited `SEM_FAILCRITICALERRORS`, `SEM_NOGPFAULTERRORBOX` and `SEM_NOOPENFILEERRORBOX` process error-mode flags. The C++ runner repeats the check, calls `WerSetFlags(WER_FAULT_REPORTING_NO_UI)`, starts the test suspended with no console, assigns it to a private Job Object configured to kill its members on close and terminate on an unhandled exception, then resumes it. If any required setup fails, it does not resume the test.

Run `python scripts/quiet_native_tests.py --self-check` only after the runner has been compiled and independently reviewed. Its protected child fixtures report inherited flags, raise a controlled unhandled exception and time out. They run through the same suspended-startup, Job Object and bounded-output path. The fixtures do not exercise custom application UI. Windows error-mode flags and WER's no-UI flag suppress the operating-system fault dialogs covered by those APIs; this launcher cannot prevent arbitrary custom dialogs created by a test.

This is a local, process-scoped guard. It changes no registry values or machine-wide WER settings. Its scope and source review are recorded in `.superpowers/sdd/2026-10-02-v2-desktop-overhaul/quiet-runner-report.md`.
