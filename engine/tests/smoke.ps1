# Starts the real engine app (tray included) on a throwaway pipe and data folder, asks it for words, and
# checks the log file. Not part of pytest because it shows a tray icon for a few seconds.
$ErrorActionPreference = "Stop"
$engine = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$data = Join-Path $env:TEMP "completionist-smoke-$PID"
New-Item -ItemType Directory -Force $data | Out-Null
$pipe = "\\.\pipe\completionist-smoke-$PID"
$config = Join-Path $data "config.toml"
Set-Content -Path $config -Encoding ascii -Value ("[data]`ndir = '" + ($data -replace '\\', '/') + "'`n")
$out = Join-Path $data "out.txt"
$proc = Start-Process -FilePath "uv" -ArgumentList @("run", "--project", $engine, "completionist-engine", "--pipe", $pipe, "--config", $config) -PassThru -WindowStyle Hidden -RedirectStandardError $out
try {
    for ($i = 0; $i -lt 120 -and -not ((Test-Path $out) -and (Select-String -Path $out -Pattern "listening on" -Quiet)); $i++) { Start-Sleep -Milliseconds 500 }
    Push-Location $engine
    $words = uv run completionist-probe --pipe $pipe "I would recomm"
    Pop-Location
    Write-Host "probe: $($words -join ', ')"
    Start-Sleep -Seconds 1
    Write-Host "--- engine output"
    Get-Content $out -Tail 8
} finally {
    Get-CimInstance Win32_Process | Where-Object { $_.CommandLine -like "*completionist-smoke-$PID*" } | ForEach-Object { Stop-Process -Id $_.ProcessId -Force -ErrorAction SilentlyContinue }
    Start-Sleep -Milliseconds 500
}
Write-Host "--- files: $((Get-ChildItem $data | Select-Object -ExpandProperty Name) -join ', ')"
if ((Get-Content $out -Raw) -match "Traceback") { Write-Host "FAIL: traceback in output"; exit 1 }
Remove-Item $data -Recurse -Force -ErrorAction SilentlyContinue
Write-Host "smoke ok"
