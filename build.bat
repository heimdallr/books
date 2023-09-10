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

echo installer creating
"C:\Program Files (x86)\Inno Setup 6\ISCC.exe" /DRootDir=%~dp0 %~dp0src\home\flibrary\app\resources\flibrary.iss

goto End

:Error
echo someting went wrong :(
exit /B 1

:End
echo working time
echo -- Start: %start_time%
echo -- Stop:  %DATE% %TIME%
