@echo off
if exist %1 goto ok

where %1 > nul
if %ERRORLEVEL% == 0 goto ok

cls
echo.
echo                       m
echo    $m                mm            m
echo     "$mmmmm        m$"    mmmmmmm$"
echo           """$m   m$    m$""""""
echo         mmmmmmm$$$$$$$$$"mmmm
echo   mmm$$$$$$$$$$$$$$$$$$ m$$$$m  "    m  "
echo $$$$$$$$$$$$$$$$$$$$$$  $$$$$$"$$$
echo  mmmmmmmmmmmmmmmmmmmmm  $$$$$$$$$$
echo  $$$$$$$$$$$$$$$$$$$$$  $$$$$$$"""  m
echo  "$$$$$$$$$$$$$$$$$$$$$ $$$$$$  "      "
echo      """""""$$$$$$$$$$$m """"
echo        mmmmmmmm"  m$   "$mmmmm
echo      $$""""""      "$     """"""$$
echo    m$"               "m           "
echo                        "
echo.
echo Check your %1 stupid asshole
pause

set ERRORLEVEL=1
goto end

:ok
echo %1 found

:end