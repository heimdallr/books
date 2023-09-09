@echo off

call %~dp0solution_generate.bat Release
if %errorlevel% NEQ 0 goto Error

set start_time=%DATE% %TIME%

echo building
cmake --build %BUILD_DIR% --config Release
if %errorlevel% NEQ 0 goto Error

rem echo testing
rem ctest --test-dir %BUILD_DIR% -C Release
rem if %errorlevel% NEQ 0 goto Error

goto End

:Error
echo building failed
exit /B 1

:End
echo building time
echo -- Start: %start_time%
echo -- Stop:  %DATE% %TIME%
