@echo off
setlocal EnableExtensions

set "WSE_ROOT=%~dp0"
set "WSE_BOOTSTRAP=%WSE_ROOT%bootstrap.py"
set "PYTHON_LAUNCHER=%WSE_ROOT%tools\bootstrap\invoke-local-python.ps1"

powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%PYTHON_LAUNCHER%" -ScriptPath "%WSE_BOOTSTRAP%" %*
exit /b %ERRORLEVEL%
