@echo off
set format_tool="D:\programs\llvm\23.1.1\bin\clang-format.exe"
set originalDir=%CD%
cd %~dp0src\home
for /r %%t in (*.cpp *.h *.hpp) do %format_tool% --verbose -i -style=file --assume-filename=%~dp0.clang-format "%%t"
cd %~dp0src\ext\foundation
for /r %%t in (*.cpp *.h *.hpp) do %format_tool% --verbose -i -style=file --assume-filename=%~dp0.clang-format "%%t"
cd %originalDir%
