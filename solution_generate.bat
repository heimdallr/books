rem @echo off

set start_time=%DATE% %TIME%
set PRODUCT_VERSION=2.1.0

call src\ext\scripts\batch\check_executable.bat cmake
if NOT [%ERRORLEVEL%]==[0] goto end

set tee_name=tee.exe
call src\ext\scripts\batch\check_executable.bat %tee_name%
if NOT [%ERRORLEVEL%]==[0] goto end

set originalDir=%CD%

if [%BUILD_TYPE%]==[] set BUILD_TYPE=Debug

set BUILD_DIR=build\%BUILD_TYPE%
mkdir %~dp0%BUILD_DIR%
cd %~dp0%BUILD_DIR%
del *.sln

cmake ^
--no-warn-unused-cli ^
-DCMAKE_GENERATOR_PLATFORM=x64 ^
-DCMAKE_CONFIGURATION_TYPES=Debug;Release ^
-DCMAKE_BUILD_TYPE=%BUILD_TYPE% ^
-DPRODUCT_VERSION=%PRODUCT_VERSION% ^
-G "Visual Studio 17 2022" %~dp0 2>&1 | %tee_name% generate_solution.log

echo -- Start: %start_time%
echo -- Stop:  %DATE% %TIME%
cd %originalDir%

:end
