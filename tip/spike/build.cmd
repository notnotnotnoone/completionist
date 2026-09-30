@echo off
rem Builds out\TyperSpike.dll (x64, static CRT) with the VS 2022 Build Tools.
setlocal
set "HERE=%~dp0"
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1
if not exist "%HERE%out" mkdir "%HERE%out"
rem Apps keep the old DLL loaded (and locked). A loaded DLL can still be renamed, so move it aside;
rem the registered path then points at the fresh build, which apps pick up when restarted.
if exist "%HERE%out\TyperSpike.dll" move /y "%HERE%out\TyperSpike.dll" "%HERE%out\TyperSpike.%RANDOM%.old" >nul
del /q "%HERE%out\TyperSpike.*.old" 2>nul
cl /nologo /std:c++20 /W4 /EHsc /O2 /MT /Zi /DUNICODE /D_UNICODE /LD "%HERE%TyperSpike.cpp" ^
   /Fo"%HERE%out\\" /Fd"%HERE%out\\" /Fe"%HERE%out\TyperSpike.dll" ^
   /link /DEBUG /INCREMENTAL:NO /DEF:"%HERE%TyperSpike.def" user32.lib gdi32.lib ole32.lib oleaut32.lib advapi32.lib uuid.lib
