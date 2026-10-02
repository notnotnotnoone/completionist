"""Local accessible window text, with no screenshot or third-party dependency."""

import logging
import subprocess
from collections.abc import Callable
from typing import Any

from completionist_engine.screen_context import Capture, Foreground, ScreenContext, ScreenSource, _tail

logger = logging.getLogger("completionist_engine.screen")


class AccessibleContext(ScreenContext):
    def __init__(self, source: ScreenSource, read_window: Callable[[int], str], *, wanted: Callable[[str], bool], **kwargs: Any) -> None:
        super().__init__(source, None, wanted=wanted, **kwargs)
        self._read_window = read_window

    def _read(self, fg: Foreground) -> None:
        generation = self._generation
        self._tried = fg.key
        try:
            text = _tail(self._read_window(fg.hwnd).strip(), self._cap)
        except Exception as err:
            logger.debug("accessible text unavailable in %s: %s", fg.app, type(err).__name__)
            return
        with self._capture_lock:
            if generation != self._generation or not self._wanted(fg.app):
                return
            self._current = Capture(self._next_id, self._clock(), fg.app, fg.title, fg.key, text, b"")
            self._next_id += 1


def read_accessible_window(hwnd: int) -> str:
    """Use Windows' bundled managed UI Automation API on the background watcher.

    A short-lived hidden process isolates providers that hang and needs no Python COM package.
    Only visible Text/Document controls are read; password and editable controls are skipped.
    """
    script = r"""$ErrorActionPreference = 'Stop'
[Console]::OutputEncoding = [System.Text.UTF8Encoding]::new($false)
Add-Type -AssemblyName UIAutomationClient
Add-Type -AssemblyName UIAutomationTypes
$root = [System.Windows.Automation.AutomationElement]::FromHandle([IntPtr]HWND_VALUE)
$walk = [System.Windows.Automation.TreeWalker]::ControlViewWalker
$queue = [System.Collections.Generic.Queue[System.Windows.Automation.AutomationElement]]::new()
$queue.Enqueue($root)
$lines = [System.Collections.Generic.List[string]]::new()
$count = 0
while ($queue.Count -gt 0 -and $count -lt 200) {
  $node = $queue.Dequeue(); $count++
  try {
    $current = $node.Current
    if ($current.IsPassword -or $current.IsOffscreen) { continue }
    $type = $current.ControlType
    if ($type -eq [System.Windows.Automation.ControlType]::Text) {
      if ($current.Name) { $lines.Add($current.Name) }
    } elseif ($type -eq [System.Windows.Automation.ControlType]::Document) {
      $pattern = $null
      if ($node.TryGetCurrentPattern([System.Windows.Automation.TextPattern]::Pattern, [ref]$pattern)) {
        $lines.Add($pattern.DocumentRange.GetText(2000))
      }
    }
    $child = $walk.GetFirstChild($node)
    while ($null -ne $child -and $queue.Count -lt 200) {
      $queue.Enqueue($child); $child = $walk.GetNextSibling($child)
    }
  } catch { continue }
}
$text = ($lines | Select-Object -Unique) -join "`n"
if ($text.Length -gt 2000) { $text = $text.Substring($text.Length - 2000) }
[Console]::Write($text)
""".replace("HWND_VALUE", str(int(hwnd)))
    try:
        result = subprocess.run(
            ["powershell.exe", "-NoProfile", "-NonInteractive", "-Command", script],
            capture_output=True, timeout=3, creationflags=subprocess.CREATE_NO_WINDOW,
        )
    except (OSError, subprocess.TimeoutExpired):
        return ""
    return result.stdout.decode("utf-8", errors="replace") if result.returncode == 0 else ""
