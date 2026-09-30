# Full check of the real DLL through real TSF, with the real engine and real n-gram data, no install:
#   .\tsf_e2e.ps1            # add -Unaware to run as a DPI-unaware app
# Runs the harness twice against a throwaway engine (a copy of your n-gram file, empty personal store,
# a local fake phrase provider, no API key needed):
#   1. this app NOT allow-listed: word scenarios plus the Ctrl+Space phrase scenarios
#   2. this app allow-listed: phrases arrive on their own after a pause
# Saves popup screenshots as PNGs in out\shots. Refuses to run if a completionist-engine is already running.
# The phrase scenarios press Ctrl for real (SendInput) for a few milliseconds at a time.
param([switch]$Unaware, [switch]$ResilienceOnly)  # -ResilienceOnly: just the engine kill/restart run
$ErrorActionPreference = "Stop"
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$tip = Split-Path -Parent $here
$repo = Split-Path -Parent $tip
$out = Join-Path $tip "out"
$dll = Join-Path $out "CompletionistTip.dll"
$exe = Join-Path $out "tsf_harness.exe"
$shots = Join-Path $out "shots"
if (-not (Test-Path $dll)) { throw "Build first: $dll not found" }
if (Get-CimInstance Win32_Process | Where-Object { $_.CommandLine -like "*completionist-engine*" -and $_.Name -match "python|uv" }) { throw "A completionist-engine is already running; stop it first." }

$build = @"
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
cl /nologo /std:c++20 /W4 /WX /EHsc /Zi /utf-8 /DUNICODE /D_UNICODE /Fo"$out\\" /Fd"$out\\" /Fe"$exe" "$here\tsf_harness.cpp" ole32.lib oleaut32.lib uuid.lib user32.lib gdi32.lib
"@
$buildCmd = Join-Path $out "build_harness.cmd"
Set-Content -Path $buildCmd -Value $build -Encoding ascii
Remove-Item $exe -ErrorAction SilentlyContinue
cmd /c "`"$buildCmd`" >nul 2>nul"
if (-not (Test-Path $exe)) { throw "harness failed to build (run $buildCmd to see why)" }

$data = Join-Path $env:TEMP "completionist-e2e-data-$PID"
New-Item -ItemType Directory -Force $data, $shots | Out-Null
$ngrams = Join-Path $env:LOCALAPPDATA "Completionist\ngrams.sqlite"
if (Test-Path $ngrams) { Copy-Item $ngrams $data } else { Write-Host "note: no n-gram file, ranking by frequency only" }
Remove-Item "$shots\*" -ErrorAction SilentlyContinue

# The fake provider.
$providerLog = Join-Path $out "provider-e2e.log"
Remove-Item $providerLog -ErrorAction SilentlyContinue
$provider = Start-Process -FilePath "uv" -ArgumentList @("run", "--project", "$repo\engine", "python", "$here\fake_provider_server.py") -PassThru -WindowStyle Hidden -RedirectStandardOutput $providerLog -RedirectStandardError (Join-Path $out "provider-e2e.err")
for ($i = 0; $i -lt 120 -and -not ((Test-Path $providerLog) -and (Get-Content $providerLog -ErrorAction SilentlyContinue | Select-String "http://")); $i++) { Start-Sleep -Milliseconds 500 }
$providerUrl = (Get-Content $providerLog | Select-String "http://" | Select-Object -First 1).ToString().Trim()
if (-not $providerUrl) { throw "fake provider did not start" }
$env:COMPLETIONIST_E2E_KEY = "test-key"

function Invoke-Run($label, $allow, $extraArgs) {
    $config = Join-Path $data "config.toml"
    $dataDir = $data -replace '\\', '/'
    Set-Content -Path $config -Encoding ascii -Value @"
[apps]
allow = [$allow]

[data]
dir = '$dataDir'

[phrase]
base_url = "$providerUrl"
api_key_env = "COMPLETIONIST_E2E_KEY"
debounce_ms = 100
timeout = 3.0
"@
    Remove-Item (Join-Path $data "personal.sqlite*"), (Join-Path $data "spend.json") -ErrorAction SilentlyContinue
    $log = Join-Path $out "engine-e2e.log"
    Remove-Item $log -ErrorAction SilentlyContinue
    $engine = Start-Process -FilePath "uv" -ArgumentList @("run", "--project", "$repo\engine", "completionist-engine", "--config", $config) -PassThru -WindowStyle Hidden -RedirectStandardError $log
    try {
        for ($i = 0; $i -lt 180 -and -not ((Test-Path $log) -and (Select-String -Path $log -Pattern "listening on" -Quiet)); $i++) { Start-Sleep -Milliseconds 500 }
        if (-not (Select-String -Path $log -Pattern "listening on" -Quiet)) { throw ("engine did not start: " + (Get-Content $log -Tail 5)) }
        Start-Sleep -Milliseconds 300
        Write-Host "=== $label ==="
        & $exe $dll $shots @extraArgs | Out-Host
        $script:harnessExit = $LASTEXITCODE
    } finally {
        Get-CimInstance Win32_Process | Where-Object { $_.CommandLine -like "*completionist-engine*" -and $_.CommandLine -like "*$data*" } |
            ForEach-Object { Stop-Process -Id $_.ProcessId -Force -ErrorAction SilentlyContinue }
        Start-Sleep -Milliseconds 500
    }
}

# Third run: the engine is killed and restarted while the app is in use. The harness raises flag files and
# this script plays the engine's part (see resilience_scenarios.h).
function Invoke-Resilience {
    $config = Join-Path $data "config.toml"
    $dataDir = $data -replace '\\', '/'
    Set-Content -Path $config -Encoding ascii -Value "[data]`ndir = '$dataDir'`n"
    $flags = Join-Path $data "flags"
    New-Item -ItemType Directory -Force $flags | Out-Null
    $stdout = Join-Path $out "resilience.out"
    $script:starts = 0
    function Start-Engine {
        $script:starts++
        $log = Join-Path $out "engine-e2e-$script:starts.log"
        Remove-Item $log -ErrorAction SilentlyContinue
        $null = Start-Process -FilePath "uv" -ArgumentList @("run", "--project", "$repo\engine", "completionist-engine", "--config", $config, "--no-tray", "--log-level", "DEBUG") -WindowStyle Hidden -RedirectStandardError $log
        for ($i = 0; $i -lt 180 -and -not ((Test-Path $log) -and (Select-String -Path $log -Pattern "listening on" -Quiet)); $i++) { Start-Sleep -Milliseconds 500 }
        if (-not (Select-String -Path $log -Pattern "listening on" -Quiet)) { throw "engine did not start" }
    }
    function Stop-Engine {
        Get-CimInstance Win32_Process | Where-Object { $_.CommandLine -like "*completionist-engine*" -and $_.CommandLine -like "*$data*" } |
            ForEach-Object { Stop-Process -Id $_.ProcessId -Force -ErrorAction SilentlyContinue }
        Start-Sleep -Milliseconds 500
    }
    Start-Engine
    Write-Host "=== engine killed and restarted mid-use ==="
    $harness = Start-Process -FilePath $exe -ArgumentList (@("`"$dll`"", "`"$shots`"") + $unawareArg + @("resilience", "`"$flags`"")) -PassThru -NoNewWindow
    try {
        $deadline = (Get-Date).AddMinutes(3)
        while (-not $harness.HasExited -and (Get-Date) -lt $deadline) {
            if ((Test-Path "$flags\kill") -and -not (Test-Path "$flags\killed")) { Stop-Engine; New-Item "$flags\killed" -ItemType File | Out-Null }
            if ((Test-Path "$flags\restart") -and -not (Test-Path "$flags\restarted")) { Start-Engine; New-Item "$flags\restarted" -ItemType File | Out-Null }
            Start-Sleep -Milliseconds 100
        }
        if (-not $harness.HasExited) { Stop-Process -Id $harness.Id -Force; Write-Host "  FAIL resilience harness timed out"; $script:harnessExit = 1 } else { $script:harnessExit = $harness.ExitCode }
    } finally {
        # (the harness prints straight to the console)
        Stop-Engine
    }
}

$unawareArg = if ($Unaware) { @("unaware") } else { @() }
$codes = @()
try {
    if (-not $ResilienceOnly) {
    Invoke-Run "words and Ctrl+Space (app not allow-listed)" "" $unawareArg
    $codes += $script:harnessExit
    Invoke-Run "automatic phrases (app allow-listed)" '"tsf_harness.exe"' (@("auto") + $unawareArg)
    $codes += $script:harnessExit
    }
    Invoke-Resilience
    $codes += $script:harnessExit
} finally {
    Stop-Process -Id $provider.Id -Force -ErrorAction SilentlyContinue
    Get-CimInstance Win32_Process | Where-Object { $_.CommandLine -like "*fake_provider_server*" } | ForEach-Object { Stop-Process -Id $_.ProcessId -Force -ErrorAction SilentlyContinue }
    Start-Sleep -Milliseconds 300
    Remove-Item $data -Recurse -Force -ErrorAction SilentlyContinue
}

python "$here\raw_to_png.py" $shots
exit ([int](($codes | Measure-Object -Maximum).Maximum))
