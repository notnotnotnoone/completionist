@echo off
setlocal
set "HERE=%~dp0"
call "%HERE%build.cmd" || exit /b 1
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 2
cl /nologo /std:c++20 /W4 /WX /EHsc /O2 /MT /I"%HERE%out" "%HERE%tests\test_blur.cpp" "%HERE%material.cpp" /Fo"%HERE%out\\" /Fe"%HERE%out\test_blur.exe" /link d3d11.lib dxgi.lib || exit /b 3
if /I "%~1"=="--compile-only" exit /b 0
"%HERE%out\test_blur.exe"
