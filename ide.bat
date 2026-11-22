@echo off

if "%*"=="" goto read
set BUILD_DIR=%~dp0%1
goto find

:read
call init.bat

:find
FOR %%F IN (%BUILD_DIR%\*.slnx) DO (
	set SOLUTION_FILE=%%F
	goto run
)

echo Cannot found solution file
goto end

:run
echo %SOLUTION_FILE% starting
start "" "%SOLUTION_FILE%"

:end
