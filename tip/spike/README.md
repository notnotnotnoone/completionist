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

*(filled in during the spike)*
