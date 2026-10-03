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

function Assert-CheckoutOwnedPath($path) {
    $root = [IO.Path]::GetFullPath($repo).TrimEnd('\') + '\'
    $full = [IO.Path]::GetFullPath($path)
    if (-not $full.StartsWith($root, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Refusing to inspect a path outside this checkout: $full"
    }
    return $full
}

function Test-RendererRemovable($path) {
    if (-not (Test-Path -LiteralPath $path)) { return $true }
    try {
        $stream = [IO.File]::Open($path, [IO.FileMode]::Open, [IO.FileAccess]::Read, [IO.FileShare]::None)
        $stream.Dispose()
        return $true
    } catch [IO.IOException] {
        return $false
    } catch [UnauthorizedAccessException] {
        return $false
    }
}

function Wait-RendererRemovable($path, [int]$timeoutSeconds = 5) {
    $deadline = [DateTime]::UtcNow.AddSeconds($timeoutSeconds)
    do {
        if (Test-RendererRemovable $path) { return $true }
        Start-Sleep -Milliseconds 200
    } while ([DateTime]::UtcNow -lt $deadline)
    return $false
}

# Check the owned renderer before unregistering the keyboard/DLL or removing the
# scheduled task. A manually started opted-in engine is not ours to kill.
$renderer = Assert-CheckoutOwnedPath $renderer
if (-not (Test-RendererRemovable $renderer)) {
    if (-not (Wait-RendererRemovable $renderer)) {
        throw "The Completionist renderer is still in use. Close the Completionist engine normally and retry uninstall. The keyboard and DLL registration were left unchanged."
    }
}

Step "Stopping the engine"
if (Get-ScheduledTask -TaskName $taskName -ErrorAction SilentlyContinue) {
    Stop-ScheduledTask -TaskName $taskName -ErrorAction SilentlyContinue
}
if (-not (Wait-RendererRemovable $renderer)) {
    throw "The Completionist renderer is still in use. Close the Completionist engine normally and retry uninstall. The scheduled task remains registered; keyboard and DLL registration were left unchanged."
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
    if (-not (Test-RendererRemovable $renderer)) {
        throw "The Completionist renderer became locked during uninstall. Close the Completionist engine normally and retry."
    }
    Remove-Item -LiteralPath $renderer -Force
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
