@echo off
rem Builds and runs the native tests (pure logic: key router, protocol codec). No TSF or Windows APIs needed.
setlocal
set "HERE=%~dp0"
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1
if not exist "%HERE%out" mkdir "%HERE%out"
cl /nologo /std:c++20 /W4 /WX /EHsc /Zi /utf-8 /Fo"%HERE%out\\" /Fd"%HERE%out\\" /Fe"%HERE%out\tests.exe" ^
   "%HERE%tests\test_main.cpp" "%HERE%tests\test_popup_model.cpp" "%HERE%tests\test_protocol.cpp" "%HERE%src\protocol.cpp" || exit /b 1
"%HERE%out\tests.exe"
