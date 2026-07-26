rem @echo off

set QT_MAJOR_VERSION=6

set BUILD_FOLDER=build
if [%QT_MAJOR_VERSION%]==[5] set BUILD_FOLDER=build_Qt5

if [%BUILD_TYPE%]==[] set BUILD_TYPE=Debug
set BUILD_DIR=%~dp0%BUILD_FOLDER%\%BUILD_TYPE%
