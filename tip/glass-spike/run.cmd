@echo off
call "%~dp0build.cmd" || exit /b 1
start "" "%~dp0out\glass-spike.exe"
