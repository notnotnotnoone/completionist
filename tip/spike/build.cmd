@echo off
rem Builds out\CompletionistSpike.dll (x64, static CRT) with the VS 2022 Build Tools.
setlocal
set "HERE=%~dp0"
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1
if not exist "%HERE%out" mkdir "%HERE%out"
rem Apps keep the old DLL loaded (and locked). A loaded DLL can still be renamed, so move it aside;
rem the registered path then points at the fresh build, which apps pick up when restarted.
if exist "%HERE%out\CompletionistSpike.dll" move /y "%HERE%out\CompletionistSpike.dll" "%HERE%out\CompletionistSpike.%RANDOM%.old" >nul
del /q "%HERE%out\CompletionistSpike.*.old" 2>nul
cl /nologo /std:c++20 /W4 /EHsc /O2 /MT /Zi /DUNICODE /D_UNICODE /LD "%HERE%CompletionistSpike.cpp" ^
   /Fo"%HERE%out\\" /Fd"%HERE%out\\" /Fe"%HERE%out\CompletionistSpike.dll" ^
   /link /DEBUG /INCREMENTAL:NO /DEF:"%HERE%CompletionistSpike.def" user32.lib gdi32.lib ole32.lib oleaut32.lib advapi32.lib uuid.lib
