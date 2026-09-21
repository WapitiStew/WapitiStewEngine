@echo off
setlocal EnableExtensions
set "SCRIPT_DIR=%~dp0"
set "PYTHON_LAUNCHER=%SCRIPT_DIR%..\tools\bootstrap\invoke-local-python.ps1"
rem An existing Graphviz installation may also be provided explicitly.
if defined WSE_GRAPHVIZ_BIN set "PATH=%WSE_GRAPHVIZ_BIN%;%PATH%"
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%PYTHON_LAUNCHER%" -ScriptPath "%SCRIPT_DIR%RunDoxygen.py" %*
exit /b %ERRORLEVEL%
