@echo off

set BUILD_TYPE=Release

call init.bat

del /s /q %BUILD_DIR%\bin\*
del /s /q %BUILD_DIR%\FLibrary\*
del /s /q %BUILD_DIR%\*.msi
del /s /q %~dp0build\installer\*

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

mkdir %~dp0build\installer

echo installer creating
cd %BUILD_DIR%
cpack -G WIX -C Release
if %errorlevel% NEQ 0 goto Error
cd %originalDir%
move  %BUILD_DIR%\*.msi %~dp0build\installer\

set MIN_WIN_WERSION=10.0.17763
if [%OS%]==[Win7] set MIN_WIN_WERSION=6.1
ISCC.exe /DRootDir=%~dp0 /DMyAppVersion=%PRODUCT_VERSION% /DMyAppUid=%PRODUCT_GUID% /DMyBuildFolder=%BUILD_DIR%\bin /DMyOS=%OS% /DMyPlatform=%PLATFORM% /DMyMinVersion=%MIN_WIN_WERSION% %~dp0src\home\script\install\flibrary.iss
if %errorlevel% NEQ 0 goto Error

echo portable creating
echo portable > %BUILD_DIR%/FLibrary/installer_mode

copy /Y %~dp0src\home\script\install\ProtonStart.sh %BUILD_DIR%\FLibrary\ProtonStart.sh
%SEVEN_ZIP_PATH%7z a %~dp0build\installer\FLibrary-%PRODUCT_VERSION%-portable-%OS%-%PLATFORM%.7z %BUILD_DIR%\FLibrary

goto End

:Error
echo someting went wrong :(
exit /B 1

:End
echo working time
echo -- Start: %start_time%
echo -- Stop:  %DATE% %TIME%
