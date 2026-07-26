@echo off

set BUILD_TYPE=Release

call init.bat

del /s /q %BUILD_DIR%\bin\*
del /s /q %BUILD_DIR%\FLibrary\*
del /s /q %~dp0%BUILD_FOLDER%\installer\*

call %~dp0configure.bat %*
if %errorlevel% NEQ 0 goto Error

set start_time=%DATE% %TIME%

set /p PRODUCT_VERSION=<%BUILD_DIR%\var\version
set /p PRODUCT_GUID=<%BUILD_DIR%\var\uid
set /p SEVEN_ZIP_PATH=<%BUILD_DIR%\var\7z
set /p OS=<%BUILD_DIR%\var\os

echo building
cmake --build %BUILD_DIR% --config Release
if %errorlevel% NEQ 0 goto Error

cmake --install %BUILD_DIR% --prefix %BUILD_DIR%/FLibrary

rem echo testing
rem ctest --test-dir %BUILD_DIR% -C Release
rem if %errorlevel% NEQ 0 goto Error

mkdir %~dp0%BUILD_FOLDER%\installer

echo installer creating
cd %BUILD_DIR%
cpack -G WIX -C Release
if %errorlevel% NEQ 0 goto Error
cd %originalDir%
move  %~dp0%BUILD_FOLDER%\%BUILD_TYPE%\*.msi %~dp0%BUILD_FOLDER%\installer\

ISCC.exe /DRootDir=%~dp0 /DMyAppVersion=%PRODUCT_VERSION% /DMyAppUid=%PRODUCT_GUID% /DMyOS=%OS% /DMyBuildFolder=%BUILD_FOLDER% %~dp0src\home\script\install\flibrary.iss
if %errorlevel% NEQ 0 goto Error

echo portable creating
echo portable > %BUILD_DIR%/FLibrary/installer_mode

copy /Y %~dp0src\home\script\install\ProtonStart.sh %BUILD_DIR%\FLibrary\ProtonStart.sh
%SEVEN_ZIP_PATH%7z a %~dp0%BUILD_FOLDER%\installer\FLibrary-%PRODUCT_VERSION%-portable-%OS%.7z %BUILD_DIR%\FLibrary

goto End

:Error
echo someting went wrong :(
exit /B 1

:End
echo working time
echo -- Start: %start_time%
echo -- Stop:  %DATE% %TIME%
