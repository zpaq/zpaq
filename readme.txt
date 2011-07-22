README for zpaq v3.01 and libzpaq v3.00 - July 21, 2011.
Matt Mahoney, matmahoney@yahoo.com

This package contains the zpaq archiver and compression development
tool and the libzpaq API for developing applications that read
or write in the ZPAQ level 1 standard format for compressed data.
Contents:

  zpaq.exe              zpaq 32 bit Windows executable (GPL).
  zpaqopt.bat           zpaq JIT configuration for Windows.
  zpaqopt               zpaq JIT configuration for Linux.
  zpaq.cpp              zpaq source code (GPL).
  Makefile.win          For compiling zpaq in Windows/MinGW g++.
  Makefile              For compiling zpaq in Linux/g++.
  libzpaq.h             libzpaq API header file (modified MIT).
  libzpaq.cpp           libzpaq source code (modified MIT).
  libzpaqo.cpp          libzpaq default models (modified MIT).
  divsufsort.c          Used by zpaq (MIT, Yuta Mori).
  divsufsort.h          Used by zpaq (MIT, Yuta Mori).
  zpaq.1.pod            man, HTML source for zpaq documentation.
  libzpaq.3.pod         man, HTML source for libzpaq API documentation.
  readme.txt            This file.

Additional files may be found at http://mattmahoney.net/dc/zpaq.html

  zpaq and libzpaq documentation in HTML format.
  ZPAQ level 1 specification (zpaq1.pdf) describing the archive format.
  Reference decoder supporting the specification.
  Sample zpaq configuration files and preprocessors for custom compression.
  zpipe compressor (uses libzpaq).
  zpaqsfx self extracting stub (uses libzpaq).
  The latest version of this package.

ZPAQ is designed for high compression ratios. The following table
compares compressed size and compression and decompression times
of the 14 file Calgary corpus on a 2.0 GHz T3200 under 32 bit Windows
using zpaq's 4 built in levels.

  Compressor     Size      C/D time (sec)
  ----------   ---------  ---------
  Uncompressed 3,141,622
  zip          1,028,059   0.4  0.1
  bzip2          828,347   0.7  0.4
  7zip           824,296   1.7  0.3
  zpaq -m1       843,572   0.8  0.8
  zpaq -m2       784,373   1.2  1.3
  zpaq -m3       722,197   5.3  5.5
  zpaq -m4       666,624  14.5 15.0

The ZPAQ format is self-describing so that older versions of the
program or other ZPAQ compliant programs like zpipe can decompress
files that use improved algorithms that have not yet been discovered.
zpaq will optionally read algorithm descriptions from configuration
files and user supplied preprocessors, and has tools for testing
debugging, and optimizing them. The extra files are not needed to
decompress. zpaq supports multi-threaded compression and extraction.


INSTALLATION IN WINDOWS

Put zpaq.exe somewhere in your PATH. zpaq is a command line program.
You need to open a command window to run it. Run zpaq with no arguments
for a brief help message. See the documentation for more information.

If you have the MinGW g++ compiler installed and you plan to compress or
decompress using configuration files, then you can benefit from
better speed using JIT optimization. zpaq will translate configuration
files or archives compressed with them into source code to create,
compile, run, and delete a temporary optimized version of itself.
To enable:

  g++ -O3 -c -DOPT zpaq.cpp libzpaq.cpp
  mkdir c:\zpaq
  move zpaq.o c:\zpaq
  move libzpaq.o c:\zpaq
  move libzpaq.h c:\zpaq

and put zpaqopt.bat in your PATH. It should contain on one line:

  g++ -O3 -DNDEBUG %1.cpp c:\zpaq\zpaq.o c:\zpaq\libzpaq.o -Ic:\zpaq\libzpaq.h -o %1.exe

It should compile its argument %1.cpp (a temporary file) to %1.exe,
include libzpaq.h and link to zpaq.o and libzpaq.o. If you use a
different compiler or put the files somewhere else than c:\zpaq,
then make appropriate changes to zpaqopt.bat. You may also use other
optimization options as appropriate. Try testing with zpaq -v if
something doesn't work.

The enclosed zpaq.exe was compiled with MinGW 4.5.0 and compressed
with upx 3.06w as follows:

  g++ -O3 -s -fomit-frame-pointer -DNDEBUG zpaq.cpp libzpaq.cpp divsufsort.c -o zpaq.exe

INSTALLATION IN LINUX

No executable is provided. To compile:

  make
or
  g++ -O3 -DNDEBUG zpaq.cpp libzpaq.cpp divsufsort.c -fopenmp -o zpaq
  g++ -O3 -DNDEBUG -DOPT -c libzpaq.cpp zpaq.cpp

The supplied zpaqopt assumes a local (non root) installation:

  ~/bin/zpaq
  ~/bin/zpaqopt
  ~/zpaq/zpaq.o
  ~/zpaq/libzpaq.o
  ~/zpaq/libzpaq.h

zpaqopt contains on one line:

  g++ -O3 -s -march=native -lpthread -DNDEBUG $1.cpp ~/zpaq/libzpaq.o ~/zpaq/zpaq.o -I~/zpaq -o $1.exe

An installation as root might look like this:

  /usr/lib/zpaq/zpaq.o
  /usr/lib/zpaq/libzpaq.o
  /usr/include/libzpaq.h
  /usr/bin/zpaq
  /usr/bin/zpaqopt

where zpaqopt might contain on one line:

  g++ -O3 -DNDEBUG $1.cpp /usr/lib/zpaq/zpaq.o /usr/lib/zpaq/libzpaq.o -o $1.exe -lpthread

In either case, put zpaq and zpaqopt in your PATH. In zpaqopt, compile
$1.cpp to $1.exe. Unlike Windows you will need the -lpthread option.
-DNDEBUG turns off run time assertions in zpaq and libzpaq.
-DOPT configures the JIT optimized version of zpaq, which will be
linked to source produced by the normal version. -fopenmp is
used by divsufsort (Linux only) and implies -lpthread which
zpaq requires. -s strips debugging symbols. -march=native
is probably appropriate if you are not distributing the executable.


LICENSE

The source code has 3 parts that are licensed differently:

- zpaq - main program (GPL)
- libzpaq - library API (modified MIT)
- libdivsufsort-lite - BWT compression for -m1 and -m2 (MIT)

zpaq and libzpaq are (C) 2011, Dell Inc.
Both are written by Matt Mahoney. zpaq is licensed under GPL v3.
(see http://www.gnu.org/copyleft/gpl.html). It may be used freely but
any distribution must include source code, including modifications,
under the same license. libzpaq is licensed under a modified MIT license
allowing unrestricted use as described in the source code.

libdivsufsort-lite v2.01 is (C) 2003-2008 by Yuta Mori, available
at http://code.google.com/p/libdivsufsort/
It is distributed under a standard MIT license allowing unrestricted
use except that the copyright notice must be included, as described
in the source code.

This readme file and any files not explicitly licensed may be
used without restrictions.


HISTORY

zpaq v3.00 merges the features of zpaq 2.05 (archiving and config
file development) with zp 1.03 (multithreaded compression and
decompression and BWT mode compression). It also includes a block
editing command for reordering archive contents. It is Windows only.

zpaq v3.01 and libzpaq v3.00 includes fixes for 64 bit Linux.
libzpaq now supports components larger than 1 GB.

See above website for complete history.

