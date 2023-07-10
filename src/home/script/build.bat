@echo off
set /p string=<%~dp0..\flibrary_old\src\version\build.h

for /f "tokens=5 delims= " %%a in ("%string%") do (
  set BUILD_NUMBER=%%a
  )
set /A BUILD_NUMBER=BUILD_NUMBER+1
echo BUILD_NUMBER = %BUILD_NUMBER%
echo constexpr int BUILD_NUMBER = %BUILD_NUMBER% ;>%~dp0..\flibrary_old\src\version\build.h
