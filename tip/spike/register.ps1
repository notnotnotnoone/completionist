# Registers (or with -Unregister, removes) the spike text service. Needs admin; shows a UAC prompt.
param([switch]$Unregister)

$dll = Join-Path $PSScriptRoot 'out\TyperSpike.dll'
if (-not (Test-Path $dll)) { throw "Build first: $dll not found" }

$arguments = @('/s')
if ($Unregister) { $arguments += '/u' }
$arguments += "`"$dll`""
$process = Start-Process -FilePath "$env:SystemRoot\System32\regsvr32.exe" -ArgumentList $arguments -Verb RunAs -Wait -PassThru
if ($process.ExitCode -ne 0) { throw "regsvr32 failed with exit code $($process.ExitCode)" }
if ($Unregister) { 'Typer Spike unregistered.' } else { 'Typer Spike registered.' }
