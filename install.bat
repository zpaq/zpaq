@echo off
: Script to install ZPAQ. For example:
:   install c:\bin g++.exe
: will install c:\bin\zpaq.exe with support files for g++ in c:\bin\zpaq\*
: Allowed compilers are g++.exe cl.exe bcc32.exe dmc.exe

: Environment variables set here are local to the script
setlocal EnableDelayedExpansion
set CWD=%~dp0

if "%2" == "" (
  echo To install ZPAQ in DIR: install DIR COMPILER
  echo For example: install c:\bin g++.exe
  echo will install c:\bin\zpaq.exe and support files c:\bin\zpaq\* using g++
  echo COMPILER must be one of g++.exe cl.exe bcc32.exe dmc.exe
  goto :EOF
)

: Make sure needed files exist
for %%i in (zpaq.cpp libzpaq.h libzpaq.cpp) do (
  if not exist %%i (
    echo File %%i not found
    echo Download zpaq and libzpaq from http://mattmahoney.net/dc/zpaq.html
    goto :EOF
  )
)
goto skip_check_path
: Make sure compiler is in PATH
if "%~$PATH:2" == "" (
  echo %2 not found in PATH
  echo PATH=%PATH%
  goto :EOF
)  else (
  echo Installing for %~$PATH:2
)
:skip_check_path
: Remove old installation
del zpaq.exe libzpaq.o* zpaq.o* %1\zpaq.exe %1\zpaq\zpaq.o* %1\zpaq\libzpaq.o* %1\zpaq\libzpaq.h >nul: 2>&1

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
echo %Z%

: Compile with g++
if %2 == g++.exe (
  g++ -O3 -DNDEBUG -march=native -s -fomit-frame-pointer -Wall -pedantic zpaq.cpp libzpaq.cpp libzpaqo.cpp -DOPT="\"g++ -O3 -march=native -s -fomit-frame-pointer -I%Z%\\zpaq %Z%\\zpaq\\zpaq.o %Z%\\zpaq\\libzpaq.o zpaqopt.cpp -o zpaqopt.exe\"" -o zpaq.exe
  if not exist zpaq.exe goto :EOF
  g++ -O3 -DNDEBUG -march=native -s -fomit-frame-pointer -c zpaq.cpp libzpaq.cpp
)

: Compile with Visual C++
if %2 == cl.exe (
  cl /Ox /GL /DNDEBUG /c divsufsort.c
  cl /Ox /GL /DNDEBUG /I%CWD% zpaq.cpp libzpaq.cpp divsufsort.obj
  del divsufsort.obj
)

: Compile with Borland
if %2 == bcc32.exe (
  bcc32 -O -6 -w- -DNDEBUG -DOPT="\"bcc32 -O -6 -w- -I%Z%\\zpaq zpaqopt.cpp %Z%\\zpaq\\zpaq.obj %Z%\\zpaq\\libzpaq.obj\"" zpaq.cpp libzpaq.cpp libzpaqo.cpp
  bcc32 -O -6 -w- -DNDEBUG -c zpaq.cpp libzpaq.cpp
  del libzpaqo.obj zpaq.tds
)

: Compile with Digital Mars
if %2 == dmc.exe (
  dmc -o -6 -DOPT="\"dmc -o -6 -I%Z%\\zpaq zpaqopt.cpp %Z%\\zpaq\\zpaq.obj %Z%\\zpaq\\libzpaq.obj\"" zpaq.cpp libzpaq.cpp libzpaqo.cpp
  del zpaq.obj libzpaq.obj libzpaqo.obj
  dmc -o -6 -c zpaq.cpp libzpaq.cpp
  del zpaq.tds zpaq.map
)

: Check if compile was successful
if not exist zpaq.exe (
  echo Compile failed
  goto :EOF
)

: Compress zpaq.exe with upx if available
for %%I in (upx.exe) do (
  if not "%%~$PATH:I" == "" (
    echo %%~$PATH:I zpaq.exe
    upx -qq -9 zpaq.exe
  )
)

: install object files
move/Y zpaq.exe %1 >nul:
for %%i in (libzpaq.o* zpaq.o*) do (
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

: Check if zpaq.exe is installed
if exist %1\zpaq.exe (
  echo Install of %1\zpaq.exe successful
) else (
  echo Install of %1\zpaq.exe failed
  goto :EOF
)

goto skip_check_path2

: Check if zpaq.exe is in PATH
for %%I in (zpaq.exe) do (
  if "%%~$PATH:I" == "" (
    echo Be sure to add %1 to your PATH
    echo PATH=%PATH%
  ) else (
    echo %%~$PATH:I is in your PATH
  )
)
:skip_check_path2
