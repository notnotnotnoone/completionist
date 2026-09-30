# Completionist text service (the DLL)

A C++ TSF text service. Windows loads it into every app that has text input; it reads the text around the caret, asks the [engine](../engine) for word completions over the named pipe, and draws them in a popup at the caret. Tab, Up, Down and Esc drive the popup; Ctrl+Right takes the next word of a phrase and Ctrl+Space asks for one. Enter is never touched.

`spike/` holds the throwaway Milestone 0 spike this grew out of.

## Layout

| File | What |
|---|---|
| `src/popup_model.h` | Pure popup state and key rules: phrase row, a 150 ms window before a fresh phrase can take Tab, Esc quieting. Tested natively. |
| `src/protocol.{h,cpp}` | Frame codec, JSON, UTF-8/UTF-16. Pure. Tested natively. |
| `src/engine_client.{h,cpp}` | Worker-thread pipe client: reconnect with backoff, drops stale replies, checks the engine runs as the same user. |
| `src/popup.{h,cpp}` | The popup window: DPI-aware, never takes focus, click-through, stays on the caret's monitor. |
| `src/tsf_service.cpp` | The TSF shell: activation, edit sessions, key sink, COM registration. |
| `src/log.{h,cpp}` | `%LOCALAPPDATA%\Completionist\tip.log`. Create an empty file `%LOCALAPPDATA%\Completionist\verbose` for debug lines. |
| `tests/` | Native tests, an engine end-to-end check, a load/lifecycle smoke test and a full TSF harness. |
| `assets/make_icon.py` | Generates `completionist.ico`, which lets Settings list the keyboard. |

## Build and test

```powershell
.\build.cmd          # out\CompletionistTip.dll (x64, static CRT)
.\test.cmd           # native tests: key router and protocol codec
.\tests\e2e.ps1      # client against the real engine: no engine, late start, kill, restart
.\tests\load_test.ps1  # loads the unregistered DLL, activates and tears it down 40 times on a real TSF thread manager
.\tests\tsf_e2e.ps1     # the real DLL through real TSF against a simulated text field, the real engine and a fake phrase provider: words, both phrase modes, and the engine being killed and restarted (screenshots in out\shots). -Unaware runs it DPI-unaware; -ResilienceOnly runs just the kill/restart part
```

**The TSF harness takes over the screen.** It opens a real window, needs the foreground for TSF to give it focus, and taps real Ctrl. Run it while you're away from the PC, not while working; a busy desktop makes it fail.

## Install

Most people should run `..\scripts\install.ps1`, which does all of this and also sets up the engine. By hand: apps keep the DLL loaded (and locked). `build.cmd` renames the old DLL out of the way, so rebuilding always works, but an app only picks up the new build after it restarts. Chrome keeps running in the background: use `chrome://restart`.

```powershell
.\build.cmd
.\register.ps1            # UAC prompt: registers the DLL
.\enable-keyboard.ps1     # adds the Completionist keyboard to your English language(s)
```

Start the engine (`cd ..\engine; uv run completionist-engine`), then switch to the Completionist keyboard with Win+Space in the app you're typing in.

To remove it: `.\enable-keyboard.ps1 -Remove`, then `.\register.ps1 -Unregister`.

## Rules the DLL follows

* It never blocks the app's UI thread: pipe I/O is on a worker thread, replies arrive as window messages.
* No exception crosses a COM boundary.
* If the engine isn't running, nothing happens, and it reconnects on its own.
* Nothing typed into a password field (or where the app has turned keyboards off) is sent anywhere.
* The popup consumes a key only while it's showing words for the text that is currently there.
