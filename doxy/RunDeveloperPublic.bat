@echo off
call "%~dp0RunDoxygen.bat" --preset developer-public %*
exit /b %ERRORLEVEL%
