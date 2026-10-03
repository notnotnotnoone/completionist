# Removes Completionist from your Windows user: stops the engine, deletes the logon task, removes the keyboard and
# unregisters the DLL (one UAC prompt). Run it yourself:  .\scripts\uninstall.ps1
# Your data (personal word counts, stats, n-gram file, logs) stays in %LOCALAPPDATA%\Completionist;
# add -DeleteData to delete that too.
param([switch]$DeleteData)
$ErrorActionPreference = "Stop"

$repo = Split-Path -Parent $PSScriptRoot
$tip = Join-Path $repo "tip"
$renderer = Join-Path $tip "out\CompletionistRenderer.exe"
$taskName = "Completionist engine"

function Step($text) { Write-Host "`n==> $text" -ForegroundColor Cyan }

Step "Stopping the engine"
if (Get-ScheduledTask -TaskName $taskName -ErrorAction SilentlyContinue) {
    Stop-ScheduledTask -TaskName $taskName -ErrorAction SilentlyContinue
}

Step "Logon task"
if (Get-ScheduledTask -TaskName $taskName -ErrorAction SilentlyContinue) {
    Unregister-ScheduledTask -TaskName $taskName -Confirm:$false
    "Removed '$taskName'."
} else { "No logon task found." }

Step "Removing the keyboard"
& (Join-Path $tip "enable-keyboard.ps1") -Remove

Step "Unregistering the DLL (approve the UAC prompt)"
if (Test-Path (Join-Path $tip "out\CompletionistTip.dll")) { & (Join-Path $tip "register.ps1") -Unregister } else { "No DLL build found; nothing to unregister." }

# Remove only the renderer artifact installed at this checkout's known path.
if (Test-Path -LiteralPath $renderer) {
    $root = [IO.Path]::GetFullPath($repo).TrimEnd('\') + '\'
    $resolved = [IO.Path]::GetFullPath($renderer)
    if (-not $resolved.StartsWith($root, [StringComparison]::OrdinalIgnoreCase)) { throw "Refusing to remove a path outside this checkout: $resolved" }
    Remove-Item -LiteralPath $resolved -Force
}

if ($DeleteData) {
    Step "Deleting data"
    Remove-Item (Join-Path $env:LOCALAPPDATA "Completionist") -Recurse -Force -ErrorAction SilentlyContinue
    "Deleted $env:LOCALAPPDATA\Completionist"
    # The settings file (with the OpenRouter key in plain text) lives in the roaming profile.
    Remove-Item (Join-Path $env:APPDATA "Completionist") -Recurse -Force -ErrorAction SilentlyContinue
    "Deleted $env:APPDATA\Completionist"
}
"`nCompletionist is uninstalled. Restart open apps so they let go of the DLL."
