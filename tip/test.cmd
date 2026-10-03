@echo off
rem Builds the native pure-logic and layout tests. --compile-only never launches them.
setlocal
set "HERE=%~dp0"
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1
if not exist "%HERE%out" mkdir "%HERE%out"
cl /nologo /std:c++20 /W4 /WX /EHsc /O2 /MT /utf-8 /Fo"%HERE%out\\" /Fd"%HERE%out\\" /Fe"%HERE%out\tests.exe" ^
   "%HERE%tests\test_main.cpp" "%HERE%tests\test_popup_model.cpp" "%HERE%tests\test_protocol.cpp" ^
   "%HERE%tests\test_render_protocol.cpp" "%HERE%src\protocol.cpp" "%HERE%src\render_protocol.cpp" || exit /b 2
cl /nologo /std:c++20 /W4 /WX /EHsc /O2 /MT /utf-8 /DUNICODE /D_UNICODE /Fo"%HERE%out\\" /Fd"%HERE%out\\" /Fe"%HERE%out\test_popup_layout.exe" ^
   "%HERE%tests\test_main.cpp" "%HERE%tests\test_popup_layout.cpp" ^
   "%HERE%src\popup_layout.cpp" "%HERE%renderer\text.cpp" ^
   /link d2d1.lib dwrite.lib user32.lib gdi32.lib ole32.lib || exit /b 3
if /I "%~1"=="--compile-only" exit /b 0
"%HERE%out\tests.exe" || exit /b 4
"%HERE%out\test_popup_layout.exe"
