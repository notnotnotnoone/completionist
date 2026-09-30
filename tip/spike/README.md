# Milestone 0: TSF spike

Throwaway English text service that answers the spike questions in each app:

1. **Caret position.** A dark popup is drawn under the caret using `ITfContextView::GetTextExt`.
2. **Context exposure.** The popup shows `ctx <before> / <after>`: how many characters the app lets TSF read around the caret (up to 10,000 each way).
3. **Key capture + insertion.** With the popup showing, **Tab** is swallowed and the current word is replaced with its UPPERCASE form through an edit session. Tab is never swallowed when the popup is hidden.

Every inspection is logged to `%LOCALAPPDATA%\Typer\spike.log`, including caret rect, `GetTextExt` HRESULT, input scope, window title and a snippet of the text.

## Use

```powershell
.\build.cmd                # -> out\TyperSpike.dll
.\register.ps1             # UAC prompt
```

Then add the keyboard: **Settings → Time & language → Language & region → your English language (e.g. English (Canada)) → ⋯ → Language options → Add a keyboard → Typer Spike**, and switch to it with **Win + Space**.

Remove it: switch back to your normal keyboard, remove "Typer Spike" in the same Settings page, then run `.\register.ps1 -Unregister`.

The DLL is locked while any app has it loaded. To rebuild, switch keyboards and restart the apps that used it.

## Findings

### Round 1 (2026-09-29)

About 830 inspections across Notepad, Discord, Chrome (Reddit, Google Docs, Google search), the Claude desktop app (Electron) and Explorer.

| App | Popup at caret | Tab swallowed + text replaced | Text readable before / after caret |
|---|---|---|---|
| Notepad | ✅ | ✅ direct `SetText` | whole document (hit the 10,000-character cap) |
| Discord (Electron) | ✅ | ✅ | whole draft typed (23 / 1) |
| Chrome – Reddit | ✅ | ✅ direct `SetText` | whole field typed (15 / 15); long fields still untested |
| Claude app (Electron) | ✅ | – | 213+ characters |
| Chrome – Google Docs | caret rect ✅ | – | **no document text** (a stub of up to 6 characters): canvas rendering, as expected |
| Chrome – Google search box | – | – | Google's own suggestion dropdown gets in the way; out of scope |

- `GetTextExt` **never failed** (0 of ~830). The fallback to measuring the previous character was never needed.
- Replacing text works with a plain `ITfRange::SetText`; the composition fallback wasn't needed anywhere.
- `RequestEditSession` occasionally returns `E_FAIL` in Chromium apps (47 times), apparently during focus changes. The real DLL must treat that as "try again on the next edit".
- The input scope came back as "no property" everywhere. That was a spike bug: input scope is an *app* property (`GetAppProperty`, not `GetProperty`). Fixed for round 2, which also logs `GUID_COMPARTMENT_KEYBOARD_DISABLED`.
- Registration: the keyboard must be registered for the user's English variant (this machine is en-CA). Settings still doesn't list it, even with `IMMERSIVESUPPORT`, probably because there's no icon, so it was enabled with `Set-WinUserLanguageList`. The real DLL needs an icon and an installer step that enables it.
