@echo off
rem Builds out\TyperSpike.dll (x64, static CRT) with the VS 2022 Build Tools.
setlocal
set "HERE=%~dp0"
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1
if not exist "%HERE%out" mkdir "%HERE%out"
cl /nologo /std:c++20 /W4 /EHsc /O2 /MT /Zi /DUNICODE /D_UNICODE /LD "%HERE%TyperSpike.cpp" ^
   /Fo"%HERE%out\\" /Fd"%HERE%out\\" /Fe"%HERE%out\TyperSpike.dll" ^
   /link /DEBUG /DEF:"%HERE%TyperSpike.def" user32.lib gdi32.lib ole32.lib oleaut32.lib advapi32.lib uuid.lib
