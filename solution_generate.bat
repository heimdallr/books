rem @echo off

set start_time=%DATE% %TIME%
set PRODUCT_VERSION=2.5.4
set PRODUCT_GUID=80B196FB-9529-459D-960E-0E0F00CE0981

call src\ext\scripts\batch\check_executable.bat cmake
if NOT [%ERRORLEVEL%]==[0] goto end

set tee_name=tee.exe
call src\ext\scripts\batch\check_executable.bat %tee_name%
if NOT [%ERRORLEVEL%]==[0] goto end

set originalDir=%CD%

if [%BUILD_TYPE%]==[] set BUILD_TYPE=Debug

set BUILD_DIR=%~dp0build\%BUILD_TYPE%
mkdir %BUILD_DIR%
cd %BUILD_DIR%
del *.sln

set CONAN_PROFILE=%~dp0src\ext\scripts\conan\profiles\msvc2022_x86_64_%BUILD_TYPE%
conan install ^
%~dp0src\home\script\conan ^
--output-folder %BUILD_DIR% ^
-pr:b %CONAN_PROFILE% ^
-pr:h %CONAN_PROFILE% ^
--build=missing 2>&1 | %tee_name% conan.log
if %errorlevel% NEQ 0 goto end

cmake ^
--no-warn-unused-cli ^
-DCMAKE_GENERATOR_PLATFORM=x64 ^
-DCMAKE_CONFIGURATION_TYPES=Debug;Release ^
-DCMAKE_BUILD_TYPE=%BUILD_TYPE% ^
-DPRODUCT_VERSION=%PRODUCT_VERSION% ^
-DCMAKE_TOOLCHAIN_FILE="%BUILD_DIR%/conan_toolchain.cmake" ^
-DCMAKE_POLICY_DEFAULT_CMP0091=NEW ^
-DPRODUCT_UID=%PRODUCT_GUID% ^
-DQt6_DIR=D:/sdk/Qt/Qt6/6.10.0/msvc2022_64/lib/cmake/Qt6 ^
%* ^
-G "Visual Studio 17 2022" %~dp0 2>&1 | %tee_name% generate_solution.log

echo -- Start: %start_time%
echo -- Stop:  %DATE% %TIME%
cd %originalDir%

:end
