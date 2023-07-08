@echo off

set start_time=%DATE% %TIME%

call scripts\batch\check_executable.bat cmake
if NOT [%ERRORLEVEL%]==[0] goto end

set tee_name=tee.exe
call scripts\batch\check_executable.bat %tee_name%
if NOT [%ERRORLEVEL%]==[0] goto end

set originalDir=%CD%

if [%WIN64BUILD%]==[true] (
	set GENERATOR_PLATFORM=x64
	set PLATFORM=64
	set QT_DIR=%QT_DIR64%
) else (
	set GENERATOR_PLATFORM=Win32
	set PLATFORM=
	set QT_DIR=%QT_DIR%
)

echo | set /p=%PLATFORM%>%TEMP%\platform.ini

set BUILD_DIR=%~dp0..\..\build%PLATFORM%-thirdparty\build\zlib
mkdir %BUILD_DIR%
cd %BUILD_DIR%
del *.sln

cmake ^
--no-warn-unused-cli ^
-DCMAKE_GENERATOR_PLATFORM=%GENERATOR_PLATFORM% ^
-DCMAKE_CONFIGURATION_TYPES=Debug;Release ^
-DCMAKE_INSTALL_PREFIX=%BUILD_DIR%\..\.. ^
%* ^
-G %CMAKE_GENERATOR% %~dp0\zlib 2>&1 | %tee_name% ..\generate_solution.log

cmake --build . --config Release --target install
cmake --build . --config Debug --target install

cd %originalDir%

set BUILD_DIR=%~dp0..\..\build%PLATFORM%-thirdparty\build\quazip
mkdir %BUILD_DIR%
cd %BUILD_DIR%
del *.sln

cmake ^
--no-warn-unused-cli ^
-DCMAKE_GENERATOR_PLATFORM=%GENERATOR_PLATFORM% ^
-DCMAKE_CONFIGURATION_TYPES=Debug;Release ^
-DCMAKE_INSTALL_PREFIX=%BUILD_DIR%\..\.. ^
-DQT_DIR=%QT_DIR%/lib/cmake/Qt6 ^
-DQt6_DIR=%QT_DIR%/lib/cmake/Qt6 ^
-DZLIB_ROOT=%BUILD_DIR%\..\.. ^
-DZLIB_INCLUDE_DIR=%BUILD_DIR%\..\..\include\zlib ^
%* ^
-G %CMAKE_GENERATOR% %~dp0\quazip 2>&1 | %tee_name% ..\generate_solution.log

cmake --build . --config Release --target install
cmake --build . --config Debug --target install

cd %originalDir%

echo -- Start: %start_time%
echo -- Stop:  %DATE% %TIME%

:end
