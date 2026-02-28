rem @echo off

set start_time=%DATE% %TIME%

call src\ext\scripts\batch\check_executable.bat cmake
if NOT [%ERRORLEVEL%]==[0] goto end

set tee_name=tee.exe
call src\ext\scripts\batch\check_executable.bat %tee_name%
if NOT [%ERRORLEVEL%]==[0] goto end

if [%BUILD_TYPE%]==[] set BUILD_TYPE=Debug
set BUILD_DIR=%~dp0build\%BUILD_TYPE%
mkdir %BUILD_DIR%
del %BUILD_DIR%\*.sln

cmake -B %BUILD_DIR% ^
--no-warn-unused-cli ^
-DCMAKE_BUILD_TYPE=%BUILD_TYPE% ^
-DQt6_DIR=D:/sdk/Qt/Qt6/6.10.0/msvc2022_64/lib/cmake/Qt6 ^
%* ^
-G "Visual Studio 17 2022" %~dp0 2>&1 | %tee_name% %BUILD_DIR%\configure.log

echo -- Start: %start_time%
echo -- Stop:  %DATE% %TIME%

:end
