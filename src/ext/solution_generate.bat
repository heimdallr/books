@echo on

set start_time=%DATE% %TIME%
set cmake_path=D:\programs\cmake\3.26.0\bin\cmake.exe

set tee_name=tee.exe
call scripts\batch\check_executable.bat %tee_name%
if NOT [%ERRORLEVEL%]==[0] goto end
set GENERATOR="Visual Studio 17 2022"

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

%cmake_path% ^
--no-warn-unused-cli ^
-DCMAKE_GENERATOR_PLATFORM=%GENERATOR_PLATFORM% ^
-DCMAKE_CONFIGURATION_TYPES=Debug;Release ^
-DCMAKE_INSTALL_PREFIX=%BUILD_DIR%\..\.. ^
%* ^
-G %GENERATOR% %~dp0\zlib 2>&1 | %tee_name% ..\generate_solution.log

%cmake_path% --build . --config Release --target install
%cmake_path% --build . --config Debug --target install

cd %originalDir%

set BUILD_DIR=%~dp0..\..\build%PLATFORM%-thirdparty\build\quazip
mkdir %BUILD_DIR%
cd %BUILD_DIR%
del *.sln

@echo on

%cmake_path% ^
--no-warn-unused-cli ^
-DCMAKE_GENERATOR_PLATFORM=%GENERATOR_PLATFORM% ^
-DCMAKE_CONFIGURATION_TYPES=Debug;Release ^
-DCMAKE_INSTALL_PREFIX=%BUILD_DIR%\..\.. ^
-DQT_DIR=%QT_DIR%/lib/cmake/Qt5 ^
-DQt5_DIR=%QT_DIR%/lib/cmake/Qt5 ^
-DZLIB_ROOT=%BUILD_DIR%\..\.. ^
-DZLIB_INCLUDE_DIR=%BUILD_DIR%\..\..\include\zlib ^
%* ^
-G %GENERATOR% %~dp0\quazip 2>&1 | %tee_name% ..\generate_solution.log

%cmake_path% --build . --config Release --target install
%cmake_path% --build . --config Debug --target install

cd %originalDir%

echo -- Start: %start_time%
echo -- Stop:  %DATE% %TIME%

:end
