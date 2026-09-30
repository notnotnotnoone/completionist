# Registers (or with -Unregister, removes) the Typer text service DLL. Needs admin; shows a UAC prompt.
# Registering makes Windows know about the DLL; enable-keyboard.ps1 then turns the keyboard on.
param([switch]$Unregister)

$dll = Join-Path $PSScriptRoot 'out\TyperTip.dll'
if (-not (Test-Path $dll)) { throw "Build first: $dll not found (run build.cmd)" }

$arguments = @('/s')
if ($Unregister) { $arguments += '/u' }
$arguments += "`"$dll`""
$process = Start-Process -FilePath "$env:SystemRoot\System32\regsvr32.exe" -ArgumentList $arguments -Verb RunAs -Wait -PassThru
if ($process.ExitCode -ne 0) { throw "regsvr32 failed with exit code $($process.ExitCode)" }
if ($Unregister) { 'Typer unregistered.' } else { 'Typer registered. Next: .\enable-keyboard.ps1' }
