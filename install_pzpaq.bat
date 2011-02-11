@echo off
: Script to install PZPAQ. For example:
:   install c:\bin g++.exe
: will install c:\bin\pzpaq.exe with support files for g++ in c:\bin\zpaq\*
: Allowed compilers are g++.exe cl.exe

: Environment variables set here are local to the script
setlocal

if "%2" == "" (
  echo To install PZPAQ in DIR: install DIR COMPILER
  echo For example: install c:\bin g++.exe
  echo will install c:\bin\pzpaq.exe and support files c:\bin\zpaq\* using g++
  echo COMPILER must be g++.exe or cl.exe
  goto :EOF
)

: Make sure needed files exist
for %%i in (pzpaq.cpp libzpaq.h libzpaq.cpp) do (
  if not exist %%i (
    echo File %%i not found
    echo Download pzpaq and libzpaq from http://mattmahoney.net/dc/zpaq.html
    goto :EOF
  )
)

: Make sure compiler is in PATH
if "%~$PATH:2" == "" (
  echo %2 not found in PATH
  echo PATH=%PATH%
  goto :EOF
)  else (
  echo Installing for %~$PATH:2
)

: Make install directory
if not exist %1\zpaq (
  echo Creating install directory %1\zpaq
  mkdir %1\zpaq
  if not exist %1\zpaq (
    echo Could not create directory %1\zpaq
    goto :EOF
  )
)

set ZPATH=%1
set Z=%ZPATH:\=\\%
set ZPATH=

copy/Y libzpaq.h %1\zpaq\libzpaq.h >nul:
if not exist %1\zpaq\libzpaq.h (
  echo Could not install %1\zpaq\libzpaq.h
  goto :EOF
)

: Compile with g++
if %2 == g++.exe (
  g++ -O3 -DNDEBUG -march=native -s -fomit-frame-pointer -Wall -pedantic pzpaq.cpp libzpaq.cpp libzpaqo.cpp -DOPT="\"g++ -O3 -march=native -s -fomit-frame-pointer -I%Z%\\zpaq %Z%\\zpaq\\pzpaq.o %Z%\\zpaq\\libzpaq.o $1.cpp -o $1.exe\"" -o pzpaq.exe
  if not exist pzpaq.exe goto :EOF
  g++ -O3 -DNDEBUG -march=native -s -fomit-frame-pointer -c pzpaq.cpp libzpaq.cpp
)

: Compile with Visual C++
if %2 == cl.exe (
  cl /Ox /GL /DNDEBUG /EHsc /DOPT="\"cl /Ox /GL /EHsc $1.cpp /Fe$1.exe /Fo$1.obj /I%Z%\\zpaq %Z%\\zpaq\\pzpaq.obj %Z%\\zpaq\\libzpaq.obj\"" pzpaq.cpp libzpaq.cpp libzpaqo.cpp
  cl /Ox /GL /DNDEBUG /EHsc /c pzpaq.cpp libzpaq.cpp
  del libzpaqo.obj
)

: Check if compile was successful
if not exist pzpaq.exe (
  echo Compile failed
  goto :EOF
)

: Compress pzpaq.exe with upx if available
for %%I in (upx.exe) do (
  if not "%%~$PATH:I" == "" (
    echo %%~$PATH:I pzpaq.exe
    upx -qq -9 pzpaq.exe
  )
)

: install object files
move/Y pzpaq.exe %1 >nul:
for %%i in (libzpaq.o* pzpaq.o*) do (
  if not exist %%i (
    echo Compile of %%i failed
    goto :EOF
  ) else (
    move/Y %%i %1\zpaq\ >nul:
    if exist %1\zpaq\%%i (
      echo Installed %1\zpaq\%%i
    ) else (
      echo Install of %1\zpaq\%%i failed
      goto :EOF
    )
  )
)

: Check if pzpaq.exe is installed
if exist %1\pzpaq.exe (
  echo Install of %1\pzpaq.exe successful
) else (
  echo Install of %1\pzpaq.exe failed
  goto :EOF
)

: Check if pzpaq.exe is in PATH
for %%I in (pzpaq.exe) do (
  if "%%~$PATH:I" == "" (
    echo Be sure to add %1 to your PATH
    echo PATH=%PATH%
  ) else (
    echo %%~$PATH:I is in your PATH
  )
)
