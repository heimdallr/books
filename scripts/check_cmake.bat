set CMAKE_PACKAGE=cmake_installer/3.16.0@conan/stable

call %~dp0check_executable.bat conan
if NOT [%ERRORLEVEL%]==[0] goto end

conan install -r movavi -b never %CMAKE_PACKAGE%

call %~dp0check_executable.bat grep
if NOT [%ERRORLEVEL%]==[0] goto end

FOR /F "tokens=2,3 USEBACKQ delims=: " %%a IN (`conan info %CMAKE_PACKAGE% --paths ^| grep package_folder`) DO (
SET cmake_path=%%a:%%b\bin\cmake.exe
)
call %~dp0check_executable.bat %cmake_path%
if NOT [%ERRORLEVEL%]==[0] goto end

:end
