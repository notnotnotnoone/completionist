# Installs Typer for your Windows user, from this checkout. Run it yourself in PowerShell:
#   .\scripts\install.ps1
# It:
#   1. installs the engine's Python dependencies (uv sync)
#   2. builds the text service DLL (VS 2022 Build Tools)
#   3. registers the DLL with Windows (one UAC prompt)
#   4. adds the Typer keyboard to your English language(s)
#   5. adds a logon task that starts the engine (tray icon, no console window) and starts it now
# Safe to run again. Undo with .\scripts\uninstall.ps1.
# Options: -SkipBuild (reuse tip\out\TyperTip.dll), -NoKeyboard (don't touch language settings), -NoStartup (no logon task).
param([switch]$SkipBuild, [switch]$NoKeyboard, [switch]$NoStartup)
$ErrorActionPreference = "Stop"

$repo = Split-Path -Parent $PSScriptRoot
$engine = Join-Path $repo "engine"
$tip = Join-Path $repo "tip"
$taskName = "Typer engine"

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

Step "Text service DLL"
if ($SkipBuild) {
    if (-not (Test-Path (Join-Path $tip "out\TyperTip.dll"))) { throw "-SkipBuild given but tip\out\TyperTip.dll doesn't exist" }
    "Reusing the existing build."
} else {
    & (Join-Path $tip "build.cmd")
    if ($LASTEXITCODE -ne 0) { throw "DLL build failed (needs the VS 2022 Build Tools with the C++ workload)" }
}

Step "Registering with Windows (approve the UAC prompt)"
& (Join-Path $tip "register.ps1")

if (-not $NoKeyboard) {
    Step "Adding the Typer keyboard"
    & (Join-Path $tip "enable-keyboard.ps1")
}

# A running engine holds the pipe; stop any so the new one takes over.
Get-CimInstance Win32_Process | Where-Object { $_.Name -match "^pythonw?\.exe$" -and $_.CommandLine -like "*typer_engine*" } |
    ForEach-Object { Stop-Process -Id $_.ProcessId -Force -ErrorAction SilentlyContinue }

if (-not $NoStartup) {
    Step "Logon task: '$taskName'"
    $action = New-ScheduledTaskAction -Execute $pythonw -Argument "-m typer_engine" -WorkingDirectory $engine
    $trigger = New-ScheduledTaskTrigger -AtLogOn -User "$env:USERDOMAIN\$env:USERNAME"
    $settings = New-ScheduledTaskSettingsSet -AllowStartIfOnBatteries -DontStopIfGoingOnBatteries -ExecutionTimeLimit ([TimeSpan]::Zero) -MultipleInstances IgnoreNew
    Register-ScheduledTask -TaskName $taskName -Action $action -Trigger $trigger -Settings $settings -Force | Out-Null
    "Engine will start when you log on."
}

Step "Starting the engine"
Start-Process -FilePath $pythonw -ArgumentList "-m", "typer_engine" -WorkingDirectory $engine
Write-Host @"

Typer is installed.
  - The tray icon (bottom right, maybe under ^) pauses/resumes it, shows stats and opens the settings file.
  - Switch to the Typer keyboard with Win+Space, then type in any app. Restart apps that were open.
  - Phrases need an API key:  setx OPENROUTER_API_KEY "your key"   (then restart the engine from the tray: Quit, run install again)
  - Word ranking gets better with the n-gram file:  cd engine; uv run typer-build-ngrams --help
  - Logs and data live in $env:LOCALAPPDATA\Typer
"@
