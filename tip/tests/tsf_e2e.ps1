# Full check of the real DLL through real TSF, with the real engine and real n-gram data, no install:
#   .\tsf_e2e.ps1
# Starts the engine on the default pipe with a throwaway data folder (a copy of your n-gram file, empty
# personal store), runs tsf_harness.exe, saves popup screenshots as PNGs in out\shots.
# Refuses to run if an engine is already listening (it would talk to yours).
param([switch]$Unaware)   # run as a DPI-unaware app
$ErrorActionPreference = "Stop"
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$tip = Split-Path -Parent $here
$repo = Split-Path -Parent $tip
$out = Join-Path $tip "out"
$dll = Join-Path $out "TyperTip.dll"
$exe = Join-Path $out "tsf_harness.exe"
$shots = Join-Path $out "shots"
if (-not (Test-Path $dll)) { throw "Build first: $dll not found" }
if (Test-Path "\\.\pipe\typer-engine") { throw "An engine is already running on \\.\pipe\typer-engine; stop it first." }

$build = @"
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
cl /nologo /std:c++20 /W4 /WX /EHsc /Zi /utf-8 /DUNICODE /D_UNICODE /Fo"$out\\" /Fd"$out\\" /Fe"$exe" "$here\tsf_harness.cpp" ole32.lib oleaut32.lib uuid.lib user32.lib gdi32.lib
"@
$buildCmd = Join-Path $out "build_harness.cmd"
Set-Content -Path $buildCmd -Value $build -Encoding ascii
Remove-Item $exe -ErrorAction SilentlyContinue
cmd /c "`"$buildCmd`" >nul 2>nul"
if (-not (Test-Path $exe)) { throw "harness failed to build" }

$data = Join-Path $env:TEMP "typer-e2e-data-$PID"
New-Item -ItemType Directory -Force $data, $shots | Out-Null
$ngrams = Join-Path $env:LOCALAPPDATA "Typer\ngrams.sqlite"
if (Test-Path $ngrams) { Copy-Item $ngrams $data } else { Write-Host "note: no n-gram file, ranking by frequency only" }
$config = Join-Path $data "config.toml"
Set-Content -Path $config -Value ("[data]`ndir = '" + ($data -replace '\\', '/') + "'`n") -Encoding ascii
Remove-Item "$shots\*" -ErrorAction SilentlyContinue

$engine = Start-Process -FilePath "uv" -ArgumentList @("run", "--project", "$repo\engine", "typer-engine", "--config", $config) -PassThru -WindowStyle Hidden -RedirectStandardError (Join-Path $out "engine-e2e.log")
try {
    $log = Join-Path $out "engine-e2e.log"
    for ($i = 0; $i -lt 180 -and -not ((Test-Path $log) -and (Select-String -Path $log -Pattern "listening on" -Quiet)); $i++) { Start-Sleep -Milliseconds 500 }
    if (-not (Select-String -Path $log -Pattern "listening on" -Quiet)) { throw ("engine did not start: " + (Get-Content $log -Tail 5)) }
    Start-Sleep -Milliseconds 300
    if ($Unaware) { & $exe $dll $shots unaware } else { & $exe $dll $shots }
    $code = $LASTEXITCODE
} finally {
    Get-CimInstance Win32_Process | Where-Object { $_.CommandLine -like "*typer-engine*" -and $_.CommandLine -like "*$data*" } |
        ForEach-Object { Stop-Process -Id $_.ProcessId -Force -ErrorAction SilentlyContinue }
    Start-Sleep -Milliseconds 300
    Remove-Item $data -Recurse -Force -ErrorAction SilentlyContinue
}

# Convert the raw captures to PNG so they can be viewed.
python "$here\raw_to_png.py" $shots
exit $code
