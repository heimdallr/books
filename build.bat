@echo off

set BUILD_TYPE=Release
call %~dp0solution_generate.bat %*
if %errorlevel% NEQ 0 goto Error

set start_time=%DATE% %TIME%

echo building
cmake --build %BUILD_DIR% --config Release
if %errorlevel% NEQ 0 goto Error

rem echo testing
rem ctest --test-dir %BUILD_DIR% -C Release
rem if %errorlevel% NEQ 0 goto Error

echo installer creating
cd %BUILD_DIR%
cpack -G WIX -C Release
if %errorlevel% NEQ 0 goto Error
cd %originalDir%
mkdir %~dp0build\installer
move  %~dp0build\%BUILD_TYPE%\*.msi %~dp0build\installer\

ISCC.exe /DRootDir=%~dp0 /DMyAppVersion=%PRODUCT_VERSION% /DMyAppUid=%PRODUCT_GUID% %~dp0src\home\script\innosetup\flibrary.iss
if %errorlevel% NEQ 0 goto Error

echo portable creating
echo portable > %~dp0build/%BUILD_TYPE%/config/installer_mode
set /p SEVEN_ZIP_PATH=<%BUILD_DIR%\path\7z

%SEVEN_ZIP_PATH%7z a %~dp0build\installer\FLibrary_portable_%PRODUCT_VERSION%.7z %~dp0build\%BUILD_TYPE%\bin\*
%SEVEN_ZIP_PATH%7z a %~dp0build\installer\FLibrary_portable_%PRODUCT_VERSION%.7z %~dp0build\%BUILD_TYPE%\config\installer_mode
%SEVEN_ZIP_PATH%7z d %~dp0build\installer\FLibrary_portable_%PRODUCT_VERSION%.7z flistat.exe

goto End

:Error
echo someting went wrong :(
exit /B 1

:End
echo working time
echo -- Start: %start_time%
echo -- Stop:  %DATE% %TIME%
