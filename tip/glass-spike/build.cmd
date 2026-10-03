@echo off
setlocal
set "HERE=%~dp0"
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1
if not exist "%HERE%out" mkdir "%HERE%out"
cl /nologo /std:c++20 /W4 /EHsc /O2 /MT /DUNICODE /D_UNICODE "%HERE%main.cpp" /Fo"%HERE%out\\" /Fe"%HERE%out\glass-spike.exe" /link /SUBSYSTEM:WINDOWS user32.lib gdi32.lib d3d11.lib dxgi.lib d3dcompiler.lib || exit /b 1
copy /y "%HERE%glass.hlsl" "%HERE%out\glass.hlsl" >nul
