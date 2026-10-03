@echo off
setlocal
set "HERE=%~dp0"
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1
if not exist "%HERE%out" mkdir "%HERE%out"
set "FXC=%WindowsSdkDir%bin\%WindowsSDKVersion%x64\fxc.exe"
if not exist "%FXC%" (echo Missing Windows SDK fxc: "%FXC%" & exit /b 2)
"%FXC%" /nologo /T vs_4_0 /E VS /Fh "%HERE%out\shaders.h" /Vn shaders_fullscreenVs "%HERE%blur.hlsl" || exit /b 3
"%FXC%" /nologo /T ps_4_0 /E PS /Fh "%HERE%out\blur_ps.h" /Vn shaders_blurPs "%HERE%blur.hlsl" || exit /b 3
powershell -NoProfile -Command "$v=(Get-Content -Raw '%HERE%out\shaders.h') -replace '#pragma once',''; $p=(Get-Content -Raw '%HERE%out\blur_ps.h') -replace '#pragma once',''; '#pragma once' + [Environment]::NewLine + $v + [Environment]::NewLine + $p + [Environment]::NewLine + 'namespace shaders {' + [Environment]::NewLine + 'inline constexpr unsigned fullscreenVsSize=sizeof(shaders_fullscreenVs);' + [Environment]::NewLine + 'inline constexpr unsigned blurPsSize=sizeof(shaders_blurPs);' + [Environment]::NewLine + '}' | Set-Content -Encoding ascii '%HERE%out\shaders.h'"
if errorlevel 1 exit /b 4
cl /nologo /std:c++20 /W4 /WX /EHsc /O2 /MT /DUNICODE /D_UNICODE /I"%HERE%out" "%HERE%main.cpp" "%HERE%fixture.cpp" "%HERE%surfaces.cpp" "%HERE%capture.cpp" "%HERE%material.cpp" "%HERE%text.cpp" "%HERE%..\src\popup_layout.cpp" /Fo"%HERE%out\\" /Fe"%HERE%out\CompletionistRenderer.exe" /link /SUBSYSTEM:WINDOWS user32.lib shell32.lib d3d11.lib dxgi.lib dcomp.lib d2d1.lib dwrite.lib windowscodecs.lib ole32.lib || exit /b 5
