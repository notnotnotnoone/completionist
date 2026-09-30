# Builds and runs load_test.exe against out\CompletionistTip.dll (build the DLL first with ..\build.cmd).
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$out = Join-Path $here "..\out"
$dll = Join-Path $out "CompletionistTip.dll"
if (-not (Test-Path $dll)) { throw "Build first: $dll not found" }
$build = @"
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
cl /nologo /std:c++20 /W4 /WX /EHsc /Zi /utf-8 /DUNICODE /D_UNICODE /Fo"$out\\" /Fd"$out\\" /Fe"$out\load_test.exe" "$here\load_test.cpp" ole32.lib uuid.lib user32.lib
"@
$buildCmd = Join-Path $out "build_load_test.cmd"
Set-Content -Path $buildCmd -Value $build -Encoding ascii
cmd /c "`"$buildCmd`" >nul 2>nul"
if (-not (Test-Path "$out\load_test.exe")) { throw "load_test failed to build" }
& "$out\load_test.exe" $dll 40
exit $LASTEXITCODE
