README for pzpaq 0.05 - Feb. 10, 2011.

pzpaq is a parallel ZPAQ compatible file compressor and
decompressor for Windows and Linux.

pzpaq is (C) 2011, Dell Inc. It is written by Matt Mahoney.
pzpaq is licensed under the GNU General Public License v3.
See http://www.gnu.org/licenses/gpl.html

For quick help, type pzpaq -h
Don't run with no arguments because it will try to compress
from standard input to standard output and just beep at you.

For complete user documentation, see http://mattmahoney.net/dc/pzpaq

The latest version of pzpaq is available from
http://mattmahoney.net/dc/zpaq.html#pzpaq

Contents:
  pzpaq.exe - Windows executable
  pzpaq.cpp - Source code. You also need libzpaq from
              http://mattmahoney.net/dc/zpaq.html#libzpaq
  install_pzpaq.bat - Installation script for Windows
  install_pzpaq.sh - Installation script for Linux
  pzpaq.1.pod - pod2man documentation source.

Only use the installation script if you have g++ installed
(MinGW in Windows). In Windows if you don't have g++ then
you can just put pzpaq.exe somewhere in your PATH and use it
right away.

If you have g++ installed then pzpaq can be installed with
an option for faster decompression of files created with other
ZPAQ compatible programs. In Windows:

  install_pzpaq c:\bin g++.exe

where c:\bin is the place you want to put pzpaq.exe (preferably
in your PATH) and g++.exe is your compiler. The script
experimentally supports cl.exe (Visual C++) if you have a bunch
of environment variables set to compile from the command line.
It doesn't support other compilers.

If you don't have all the files you need then the script will
tell you to get libzpaq. You will need:

  libzpaq.cpp
  libzpaqo.cpp
  libzpaq.h

from http://mattmahoney.net/dc/zpaq.html#libzpaq
which the script will compile to .o files and put in c:\bin\zpaq

In Linux:

  chmod +x install_pzpaq.sh
  sudo ./install_pzpaq.sh

There are no arguments. You will need g++ and bash. You will also
need pod2man to install the man page.
It will install the following files:

  /usr/bin/pzpaq
  /usr/lib/zpaq/libzpaq.o
  /usr/lib/zpaq/pzpaq.o
  /usr/include/libzpaq.h
  /usr/share/man/man1/pzpaq.1.gz

If you want them elsewhere, you can change the variables BIN, LIB,
INC, and MAN, in the script.

In either Windows or Linux, the files libzpaq.o and libzpaq.h are
shared with zpaq, so you should probably put both in the same place.


