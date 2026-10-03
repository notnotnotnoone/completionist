@echo off
rem Builds and runs native pure-logic tests. Pass --compile-only to build without launching them.
setlocal
set "HERE=%~dp0"
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1
if not exist "%HERE%out" mkdir "%HERE%out"
cl /nologo /std:c++20 /W4 /WX /EHsc /O2 /MT /utf-8 /Fo"%HERE%out\\" /Fd"%HERE%out\\" /Fe"%HERE%out\tests.exe" ^
   "%HERE%tests\test_main.cpp" "%HERE%tests\test_popup_model.cpp" "%HERE%tests\test_protocol.cpp" ^
   "%HERE%tests\test_render_protocol.cpp" "%HERE%src\protocol.cpp" "%HERE%src\render_protocol.cpp" || exit /b 1
if /I "%~1"=="--compile-only" exit /b 0
"%HERE%out\tests.exe"
