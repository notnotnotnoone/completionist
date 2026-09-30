# End-to-end check of the DLL's engine client against the real Python engine (no TSF, no admin).
#   .\e2e.ps1
# Covers: engine not running (silent, no crash), engine starting late (connects), engine killed
# (client notices), engine restarted (reconnects with new replies).
$ErrorActionPreference = "Stop"
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$repo = Split-Path -Parent (Split-Path -Parent $here)
$out = Join-Path $here "..\out"
$pipe = "\\.\pipe\typer-e2e-$PID"
$probeExe = Join-Path $out "pipe_probe.exe"

# Build the probe.
$build = @"
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
cl /nologo /std:c++20 /W4 /WX /EHsc /Zi /utf-8 /DUNICODE /D_UNICODE /Fo"$out\\" /Fd"$out\\" /Fe"$probeExe" "$here\pipe_probe.cpp" "$here\..\src\engine_client.cpp" "$here\..\src\protocol.cpp" "$here\..\src\log.cpp" user32.lib advapi32.lib
"@
$buildCmd = Join-Path $out "build_probe.cmd"
New-Item -ItemType Directory -Force $out | Out-Null
Set-Content -Path $buildCmd -Value $build -Encoding ascii
cmd /c "`"$buildCmd`" >nul 2>nul"
if (-not (Test-Path $probeExe)) { throw "probe failed to build" }

function Start-Engine {
    Start-Process -FilePath "uv" -ArgumentList @("run", "--project", "$repo\engine", "typer-engine", "--pipe", $pipe, "--config", "$out\no-such-config.toml") `
        -PassThru -WindowStyle Hidden
}
function Stop-Engine($process) {
    Get-CimInstance Win32_Process | Where-Object { $_.CommandLine -like "*typer-engine*" -and $_.CommandLine -like "*$pipe*" } | ForEach-Object { Stop-Process -Id $_.ProcessId -Force -ErrorAction SilentlyContinue }
}

# One probe run for the whole scenario (~30 s), engine started and stopped underneath it.
$log = Join-Path $out "e2e-probe.log"
$probe = Start-Process -FilePath $probeExe -ArgumentList @($pipe, "34") -RedirectStandardOutput $log -NoNewWindow -PassThru
$null = $probe.Handle   # keeps the process handle so ExitCode is readable

Start-Sleep -Seconds 3                       # phase 1: no engine
$engine = Start-Engine
Start-Sleep -Seconds 12                      # phase 2: engine loads (a few seconds) and serves
$phase2 = Get-Content $log | Measure-Object | Select-Object -ExpandProperty Count
Stop-Engine $engine
Start-Sleep -Seconds 4                       # phase 3: engine gone
$phase3Mark = (Get-Content $log | Measure-Object).Count
$engine = Start-Engine
Start-Sleep -Seconds 12                      # phase 4: engine back
$probe.WaitForExit(20000) | Out-Null
if (-not $probe.HasExited) { $probe.Kill(); $failures0 = "probe did not exit on its own" }
Stop-Engine $engine

$lines = Get-Content $log
$replies = @($lines | Where-Object { $_ -match "words=recommend" })
$repliesAfterRestart = @($lines | Select-Object -Skip $phase3Mark | Where-Object { $_ -match "words=recommend" })
$sentWhileDown = @($lines | Select-Object -First 25 | Where-Object { $_ -match "connected=0" })

$failures = @()
if ($failures0) { $failures += $failures0 }
if ($sentWhileDown.Count -lt 10) { $failures += "expected the client to report connected=0 before the engine started" }
if ($replies.Count -lt 20) { $failures += "expected replies once the engine was up (got $($replies.Count))" }
if ($repliesAfterRestart.Count -lt 20) { $failures += "expected replies after the engine restarted (got $($repliesAfterRestart.Count))" }
if ($probe.ExitCode -ne 0) { $failures += "probe exited with $($probe.ExitCode)" }

Write-Host ("replies total={0}, after restart={1}, sent while down={2}" -f $replies.Count, $repliesAfterRestart.Count, $sentWhileDown.Count)
Write-Host ($lines | Where-Object { $_ -match "reply" } | Select-Object -First 2)
if ($failures) { $failures | ForEach-Object { Write-Host "FAIL: $_" }; exit 1 }
Write-Host "e2e ok"
