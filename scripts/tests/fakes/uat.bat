@echo off
setlocal
set "INI=%~dp0..\..\..\runtime\UnRealDash\Config\GeneratedEngine.ini"
> "%SMOKE_UAT_RECORD%\arguments.txt" echo %*
if exist "%INI%" (
    copy /b "%INI%" "%SMOKE_UAT_RECORD%\GeneratedEngine.ini" >nul
) else (
    > "%SMOKE_UAT_RECORD%\no-ini.txt" echo absent
)
rem Deleting the override here is how the suite reproduces a cleanup failure without
rem holding a file handle: the script's own Remove-Item then fails on a missing file.
if "%SMOKE_UAT_DELETE_INI%"=="1" if exist "%INI%" del /f /q "%INI%"
exit /b %SMOKE_UAT_EXIT%
