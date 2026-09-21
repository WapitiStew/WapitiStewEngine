@echo off
echo Starting Python scripts...

REM convert_charactor_newline.py �����s
echo ConvertCode_CharactorNewLine...
py convert_character_newline.py
if %ERRORLEVEL% neq 0 (
  echo Failed convert_charactor_newline.py
  pause
  exit /b 1
)

echo UpdateFormats...
REM change_doxycomment_file.py �����s
py change_doxycomment_file.py
if %ERRORLEVEL% neq 0 (
  echo Failed change_doxycomment_file.py
  pause
  exit /b 1
)

REM insert_doxycomment_file.py �����s
echo InseertFormat...
py insert_doxycomment_file.py
if %ERRORLEVEL% neq 0 (
  echo Failed insert_doxycomment_file.py
  pause
  exit /b 1
)

REM rewrite_doxycomment_copyright.py �����s
echo Copyright...
py rewrite_doxycomment_copyright.py
if %ERRORLEVEL% neq 0 (
  echo Failed rewrite_doxycomment_copyright.py
  pause
  exit /b 1
)

REM convert_charactor_newline.py �����s
echo ConvertCode_CharactorNewLine...
py convert_character_newline.py
if %ERRORLEVEL% neq 0 (
  echo Failed convert_charactor_newline.py
  pause
  exit /b 1
)

echo Succeed all python scripts
pause