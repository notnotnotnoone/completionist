@echo off
setlocal
set "HERE=%~dp0"
call "%HERE%build.cmd" || exit /b 1
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 2
cl /nologo /std:c++20 /W4 /WX /EHsc /O2 /MT /I"%HERE%out" "%HERE%tests\test_blur.cpp" "%HERE%material.cpp" /Fo"%HERE%out\\" /Fe"%HERE%out\test_blur.exe" /link d3d11.lib dxgi.lib || exit /b 3
cl /nologo /std:c++20 /W4 /WX /EHsc /O2 /MT /DUNICODE /D_UNICODE /I"%HERE%out" /I"%HERE%..\src" ^
   "%HERE%..\tests\test_main.cpp" "%HERE%tests\test_session.cpp" "%HERE%tests\test_windows.cpp" ^
   "%HERE%session.cpp" "%HERE%windows.cpp" "%HERE%..\src\render_protocol.cpp" "%HERE%..\src\protocol.cpp" ^
   /Fo"%HERE%out\\" /Fe"%HERE%out\test_session.exe" /link user32.lib advapi32.lib d3d11.lib dxgi.lib wtsapi32.lib || exit /b 4
if /I "%~1"=="--compile-only" exit /b 0
"%HERE%out\test_session.exe" || exit /b 5
"%HERE%out\test_blur.exe"
