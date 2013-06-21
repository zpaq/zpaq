zpaq633.zip, June 21, 2013. Contents:

zpaq.exe      6.33   Archiver, 32 bit Windows command line executable.
zpaq64.exe    6.33   For 64 bit Windows.
zpaq.cpp      6.33   zpaq user's guide and source code.
libzpaq.h     6.25   libzpaq API documentation and header.
libzpaq.cpp   6.33   libzpaq API source code.
divsufsort.h  2.00   libdivsufsoft-lite header.
divsufsort.c  2.00   libdivsufsort-lite source code.
Makefile             To compile in Linux: make

All versions of this software can be found at
http://mattmahoney.net/dc/zpaq.html
Please report bugs to Matt Mahoney at mattmahoneyfl@gmail.com

zpaq is (C) 2011-2013, Dell Inc., written by Matt Mahoney.
Licensed under GPL v3. http://www.gnu.org/copyleft/gpl.html
zpaq is a journaling archiver optimized for user-level incremental
backup of directory trees. It supports 10 multi-threaded compression
levels and file fragment level deduplication. It adds only files whose
date has changed, and keeps both old and new versions. You can roll
back the archive date to restore from old versions of the archive.
The default compression level is faster than zip usually with better
compression.

The 32 bit version can run under either 32 or 64 bit Windows. The
64 bit version runs only under 64 bit Windows. The difference is
that the 64 bit version can use more than 2 GB of memory. The difference
is only important for some experimental compression modes.

libzpaq is written by Matt Mahoney and released to the public domain.
It is an API providing compression and decompression services
for developers. See libzpaq.h for documentation. It is needed to
compile zpaq and zpaqd.

libdivsufsort-lite v2.00 is (C) 2003-2008, Yuta Mori under the MIT open
source license (see source code). It is mirrored from 
http://code.google.com/p/libdivsufsort/ for your convenience.
It is needed to compile zpaq.

zpaq.exe was compiled with MinGW g++ 4.8.0 and compressed
with upx 3.08w as follows:

  g++ -O3 -s -m64 -static -DNDEBUG zpaq.cpp libzpaq.cpp divsufsort.c -o zpaq64
  g++ -O3 -s -m32 -static -DNDEBUG zpaq.cpp libzpaq.cpp divsufsort.c -o zpaq
  upx zpaq.exe

To compile zpaq for Linux, include the options: -Dunix -fopenmp
-fopenmp is for divsufsort. It implies -pthread which is required for zpaq.

To compile for non-x86 or an old processor not supporting SSE2, use -DNOJIT

-DNDEBUG turns off run time checks in divsufsort. They are off by default
in zpaq and libzpaq. To turn them on use -DDEBUG

-static is only needed if you plan to run the program on a different
computer than you compiled it on. It makes the program larger.

upx compresses 32 bit Windows executables. It is not required. It will
not work on 64 bit executables.
