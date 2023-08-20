@echo off
if "%*"=="" goto read
set SOLUTION_PATH=%~dp0%1
goto find

:read
set /p PLATFORM=< %TEMP%\platform.ini
set SOLUTION_PATH=%~dp0build\x%PLATFORM%

:find
FOR %%F IN (%SOLUTION_PATH%\*.sln) DO (
	set SOLUTION_FILE=%%F
	goto run
)

echo Cannot found solution file
goto end

:run
echo %SOLUTION_FILE% starting
set PATH=%PATH%;%ICU_DIR%\bin
start "" "%SOLUTION_FILE%"

:end
