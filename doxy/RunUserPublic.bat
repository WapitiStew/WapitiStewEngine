@echo off
call "%~dp0RunDoxygen.bat" --preset user-public %*
exit /b %ERRORLEVEL%
