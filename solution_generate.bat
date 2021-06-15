rem @echo off

set start_time=%DATE% %TIME%

call src\ext\scripts\batch\check_cmake.bat
if NOT [%ERRORLEVEL%]==[0] goto end

set tee_name=tee.exe
call src\ext\scripts\batch\check_executable.bat %tee_name%
if NOT [%ERRORLEVEL%]==[0] goto end

set originalDir=%CD%
if [%WIN64BUILD%]==[true] (
	set GENERATOR_PLATFORM=x64
	set PLATFORM="64"
	set GENERATOR="Visual Studio 16 2019"
) else (
	set GENERATOR_PLATFORM=Win32
	set PLATFORM=
	set GENERATOR="Visual Studio 16 2019"
)

echo | set /p=%PLATFORM%>%TEMP%\platform.ini

set BUILD_DIR=build%PLATFORM%
mkdir %~dp0%BUILD_DIR%
cd %~dp0%BUILD_DIR%
del *.sln

%cmake_path% ^
--no-warn-unused-cli ^
-DCMAKE_GENERATOR_PLATFORM=%GENERATOR_PLATFORM% ^
-DCMAKE_CONFIGURATION_TYPES=Debug;Release ^
%* ^
-G %GENERATOR% %~dp0 2>&1 | %tee_name% generate_solution.log

echo -- Start: %start_time%
echo -- Stop:  %DATE% %TIME%
cd %originalDir%

:end
