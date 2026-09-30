@echo off
rem Builds out\TyperTip.dll (x64, static CRT) with the VS 2022 Build Tools.
setlocal
set "HERE=%~dp0"
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1
if not exist "%HERE%out" mkdir "%HERE%out"
rem Apps keep the old DLL loaded (and locked). A loaded DLL can still be renamed, so move it aside;
rem the registered path then points at the fresh build, which apps pick up when restarted.
if exist "%HERE%out\TyperTip.dll" move /y "%HERE%out\TyperTip.dll" "%HERE%out\TyperTip.%RANDOM%.old" >nul
del /q "%HERE%out\TyperTip.*.old" 2>nul
rc /nologo /fo "%HERE%out\typer.res" "%HERE%src\typer.rc" || exit /b 1
cl /nologo /std:c++20 /W4 /WX /EHsc /O2 /MT /Zi /utf-8 /DUNICODE /D_UNICODE /LD ^
   "%HERE%src\tsf_service.cpp" "%HERE%src\engine_client.cpp" "%HERE%src\popup.cpp" "%HERE%src\protocol.cpp" "%HERE%src\log.cpp" ^
   /Fo"%HERE%out\\" /Fd"%HERE%out\\" /Fe"%HERE%out\TyperTip.dll" ^
   /link /DEBUG /INCREMENTAL:NO /DEF:"%HERE%src\typer.def" "%HERE%out\typer.res" ^
   user32.lib gdi32.lib ole32.lib oleaut32.lib advapi32.lib uuid.lib
