@echo off
setlocal
set "ROOT=%~dp0.."
set "OUT=%~dp0out"
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1
if not exist "%OUT%" mkdir "%OUT%"
pushd "%OUT%" || exit /b 2
cl /nologo /std:c++20 /W4 /WX /EHsc /O2 /MT /DUNICODE /D_UNICODE /utf-8 /Fo.\ /Fdquiet_native_runner.pdb /Fequiet_native_runner.exe "%~dp0quiet_native_runner.cpp" /link wer.lib
set "BUILD_RESULT=%ERRORLEVEL%"
popd
if not "%BUILD_RESULT%"=="0" exit /b 2
exit /b 0
