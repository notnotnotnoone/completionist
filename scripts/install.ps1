# Installs Completionist for your Windows user, from this checkout. Run it yourself in PowerShell:
#   .\scripts\install.ps1
# It:
#   1. installs the engine's Python dependencies (uv sync)
#   2. builds the text service DLL (VS 2022 Build Tools)
#   3. registers the DLL with Windows (one UAC prompt)
#   4. adds the Completionist keyboard to your English language(s)
#   5. adds a logon task that starts the engine (tray icon, no console window) and starts it now
# Safe to run again. Undo with .\scripts\uninstall.ps1.
# Options: -SkipBuild (reuse tip\out\CompletionistTip.dll), -NoKeyboard (don't touch language settings), -NoStartup (no logon task).
param([switch]$SkipBuild, [switch]$NoKeyboard, [switch]$NoStartup)
$ErrorActionPreference = "Stop"

$repo = Split-Path -Parent $PSScriptRoot
$engine = Join-Path $repo "engine"
$tip = Join-Path $repo "tip"
$out = Join-Path $tip "out"
$rendererBuild = Join-Path $tip "renderer\out\CompletionistRenderer.exe"
$rendererInstall = Join-Path $out "CompletionistRenderer.exe"
$rollback = Join-Path $out "rollback"
$taskName = "Completionist engine"

function Step($text) { Write-Host "`n==> $text" -ForegroundColor Cyan }

Step "Engine dependencies"
if (-not (Get-Command uv -ErrorAction SilentlyContinue)) { throw "uv is not installed. Get it from https://docs.astral.sh/uv/ and run this again." }
Push-Location $engine
try {
    uv sync
    if ($LASTEXITCODE -ne 0) { throw "uv sync failed" }
} finally { Pop-Location }
$pythonw = Join-Path $engine ".venv\Scripts\pythonw.exe"
if (-not (Test-Path $pythonw)) { throw "$pythonw not found after uv sync" }

function Assert-OwnedPath($path) {
    $root = [IO.Path]::GetFullPath($repo).TrimEnd('\') + '\'
    $full = [IO.Path]::GetFullPath($path)
    if (-not $full.StartsWith($root, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Refusing to modify a path outside this checkout: $full"
    }
    return $full
}

# Save the current artifact pair before either build can replace one. The fixed
# rollback directory is inside this checkout and is deliberately retained.
$null = Assert-OwnedPath $rendererInstall
$null = Assert-OwnedPath $rollback
New-Item -ItemType Directory -Force -Path $rollback | Out-Null
$dllPath = Join-Path $out "CompletionistTip.dll"
$oldDll = Join-Path $rollback "CompletionistTip.dll"
$oldRenderer = Join-Path $rollback "CompletionistRenderer.exe"
Remove-Item -LiteralPath (Assert-OwnedPath $oldDll), (Assert-OwnedPath $oldRenderer) -Force -ErrorAction SilentlyContinue
if (Test-Path $dllPath) { Copy-Item -LiteralPath (Assert-OwnedPath $dllPath) -Destination (Assert-OwnedPath $oldDll) -Force }
if (Test-Path $rendererInstall) { Copy-Item -LiteralPath (Assert-OwnedPath $rendererInstall) -Destination (Assert-OwnedPath $oldRenderer) -Force }
$hadDll = Test-Path $oldDll
$hadRenderer = Test-Path $oldRenderer

try {
    if ($SkipBuild) {
        if (-not (Test-Path $dllPath)) { throw "-SkipBuild given but $dllPath doesn't exist" }
        if (-not (Test-Path $rendererInstall)) { throw "-SkipBuild requires the paired renderer at $rendererInstall" }
        $rendererBuild = $rendererInstall
        "Reusing the existing DLL/renderer pair."
    } else {
        Step "Building renderer artifact"
        & (Join-Path $tip "renderer\build.cmd")
        if ($LASTEXITCODE -ne 0) { throw "Renderer build failed (needs the VS 2022 Build Tools and Windows SDK)" }
        if (-not (Test-Path $rendererBuild)) { throw "Renderer build did not produce $rendererBuild" }

        Step "Text service DLL"
        & (Join-Path $tip "build.cmd")
        if ($LASTEXITCODE -ne 0) { throw "DLL build failed (needs the VS 2022 Build Tools with the C++ workload)" }
    }

    # Install the renderer before registration activates the new DLL. The engine
    # still keeps external rendering disabled unless explicitly opted in.
    if ($rendererBuild -ne $rendererInstall) {
        Copy-Item -LiteralPath $rendererBuild -Destination (Assert-OwnedPath $rendererInstall) -Force
    }
    Step "Registering with Windows (approve the UAC prompt)"
    & (Join-Path $tip "register.ps1")
    if ($LASTEXITCODE -ne 0) { throw "DLL registration failed" }
} catch {
    # Restore the prior DLL/renderer pair only at these verified checkout paths.
    if ($hadDll) { Copy-Item -LiteralPath (Assert-OwnedPath $oldDll) -Destination (Assert-OwnedPath $dllPath) -Force }
    elseif (Test-Path $dllPath) { Remove-Item -LiteralPath (Assert-OwnedPath $dllPath) -Force }
    if ($hadRenderer) { Copy-Item -LiteralPath (Assert-OwnedPath $oldRenderer) -Destination (Assert-OwnedPath $rendererInstall) -Force }
    elseif (Test-Path $rendererInstall) { Remove-Item -LiteralPath (Assert-OwnedPath $rendererInstall) -Force }
    throw
}

if (-not $NoKeyboard) {
    Step "Adding the Completionist keyboard"
    & (Join-Path $tip "enable-keyboard.ps1")
}

# Leftovers from before the rename (the app was called Typer): remove its logon task.
if (Get-ScheduledTask -TaskName "Typer engine" -ErrorAction SilentlyContinue) { Unregister-ScheduledTask -TaskName "Typer engine" -Confirm:$false }

if (-not $NoStartup) {
    Step "Logon task: '$taskName'"
    $action = New-ScheduledTaskAction -Execute $pythonw -Argument "-m completionist_engine" -WorkingDirectory $engine
    $trigger = New-ScheduledTaskTrigger -AtLogOn -User "$env:USERDOMAIN\$env:USERNAME"
    $settings = New-ScheduledTaskSettingsSet -AllowStartIfOnBatteries -DontStopIfGoingOnBatteries -ExecutionTimeLimit ([TimeSpan]::Zero) -MultipleInstances IgnoreNew
    Register-ScheduledTask -TaskName $taskName -Action $action -Trigger $trigger -Settings $settings -Force | Out-Null
    "Engine will start when you log on."
}

Step "Starting the engine"
Start-Process -FilePath $pythonw -ArgumentList "-m", "completionist_engine" -WorkingDirectory $engine
Write-Host @"

Completionist is installed.
  - The tray icon (bottom right, maybe under ^) pauses/resumes it, shows stats and opens the settings file.
  - Switch to the Completionist keyboard with Win+Space, then type in any app. Restart apps that were open.
  - Phrases need an API key: tray menu > Open settings file, then add  api_key = "your key"  under [phrase]  (applies within a couple of seconds)
  - Word ranking gets better with the n-gram file:  cd engine; uv run completionist-build-ngrams --help
  - Logs and data live in $env:LOCALAPPDATA\Completionist
"@
