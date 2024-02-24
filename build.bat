@echo off

set BUILD_TYPE=Release
call %~dp0solution_generate.bat
if %errorlevel% NEQ 0 goto Error

set start_time=%DATE% %TIME%

echo building
cmake --build %BUILD_DIR% --config Release
if %errorlevel% NEQ 0 goto Error

rem echo testing
rem ctest --test-dir %BUILD_DIR% -C Release
rem if %errorlevel% NEQ 0 goto Error

echo installer creating
ISCC.exe /DRootDir=%~dp0 /DMyAppVersion=%PRODUCT_VERSION% %~dp0src\home\flibrary\app\resources\installer\flibrary.iss

echo portable creating
7z a %~dp0build\bin\installer\flibrary_portable_%PRODUCT_VERSION%.7z %~dp0build\bin\%BUILD_TYPE%\*

goto End

:Error
echo someting went wrong :(
exit /B 1

:End
echo working time
echo -- Start: %start_time%
echo -- Stop:  %DATE% %TIME%
