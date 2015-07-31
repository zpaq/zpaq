@echo off

REM This will compile zpaq using the latest available Visual Studio version

IF /i "%~1" EQU "" (
    REM no platform specified. Self-calling twice with x86 and x64 as argument
    start /min /wait "x86" cmd /c %0 x86
    start /min /wait "x64" cmd /c %0 x64
    goto:eof
)

call:detect_visual_studio
IF NOT DEFINED vsinstalldir  (
  echo.Cannot find Visual Studio Version
  pause
  goto:eof
)

call "%vsinstalldir%\vc\vcvarsall.bat" %~1
cl /Ox /EHsc zpaq.cpp libzpaq.cpp /link advapi32.lib /out:zpaq_%~1.exe

goto:eof

::::::::::::::::::::::::::::::::::::::::::::::

:detect_visual_studio
    IF DEFINED VS140COMNTOOLS (
        echo Using Visual 2015
        call "%VS140COMNTOOLS%\vsvars32.bat"
    ) ELSE IF DEFINED VS120COMNTOOLS (
        echo Using Visual 2013
        call "%VS120COMNTOOLS%\vsvars32.bat"
    ) ELSE IF DEFINED VS110COMNTOOLS (
        echo Using Visual 2012
        call "%VS110COMNTOOLS%\vsvars32.bat"
    ) ELSE IF DEFINED VS100COMNTOOLS (
        echo Using Visual 2010
        call "%VS100COMNTOOLS%\vsvars32.bat"
    ) ELSE IF DEFINED VS90COMNTOOLS (
        echo Using Visual 2008
        call "%VS90COMNTOOLS%\vsvars32.bat"
    ) ELSE IF DEFINED VS80COMNTOOLS (
        echo Using Visual 2005
        call "%VS80COMNTOOLS%\vsvars32.bat"
    ) ELSE IF DEFINED VS71COMNTOOLS (
        echo Using Visual 2003 .NET
        call "%VS71COMNTOOLS%\vsvars32.bat"
    ) ELSE IF DEFINED VS70COMNTOOLS (
        echo Using Visual 2002 .NET
        call "%VS70COMNTOOLS%\vsvars32.bat"
    )

    goto:eof
