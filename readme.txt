zpaq v6.21, Feb. 6, 2013. Contents:

zpaq.exe       6.21   Archiver, 32 bit Windows command line executable.
zpaq621-64.exe 6.21   64 bit Windows executable (added Feb. 8, 2013).
zpaq.cpp       6.21   zpaq user's guide and source code.
zpaqd.exe      6.19   Development tool, 32 bit Windows command line executable.
zpaqd.cpp      6.19   zpaqd user's guide and source code.
libzpaq.h      6.19   libzpaq API documentation and header.
libzpaq.cpp    6.19   libzpaq API source code.
divsufsort.h   2.00   libdivsufsoft-lite header.
divsofsort.c   2.00   libdivsufsort-lite source code.

All versions of this software can be found at
http://mattmahoney.net/dc/zpaq.html
Please report bugs to Matt Mahoney at mattmahoneyfl@gmail.com

zpaq is (C) 2012, Dell Inc., written by Matt Mahoney.
Licensed under GPL v3. http://www.gnu.org/copyleft/gpl.html
zpaq is a journaling archiver optimized for user-level incremental
backup of directory trees. It supports 10 multi-threaded compression
levels and file-fragment level deduplication. It adds only files whose
date has changed, and keeps both old and new versions. You can roll
back the archive date to restore from old versions of the archive.
The default compression level is faster than zip usually with better
compression.

zpaqd is written by Matt Mahoney and released to the public domain.
It is a tool for developing, testing, and debugging new compression
algorithms in the ZPAQ format. A ZPAQ archive is self-describing so that
older decompressers can read archives produced by newer versions of
the compressor when they use improved algorithms. Compression algorithms
are described in configuration files written in the ZPAQL language
described in libzpaq.h. The ZPAQ archive format is described in the
specification at http://mattmahoney.net/dc/zpaq201.pdf

libzpaq is written by Matt Mahoney and released to the public domain.
It is an API providing compression and decompression services
for developers. See libzpaq.h for documentation. It is needed to
compile zpaq and zpaqd.

libdivsufsort-lite v2.00 is (C) 2003-2008, Yuta Mori under the MIT open
source license (see source code). It is mirrored from 
http://code.google.com/p/libdivsufsort/ for your convenience.
It is needed to compile zpaq.

zpaq.exe and zpaqd.exe were compiled with MinGW g++ 4.7.0 and compressed
with upx 3.06w as follows:

  g++ -O3 -s -static -Wall zpaq.cpp libzpaq.cpp divsufsort.c -DNDEBUG -o zpaq
  g++ -O3 -s -static -Wall zpaqd.cpp libzpaq.cpp -o zpaqd
  upx zpaq.exe zpaqd.exe

To compile zpaq for Linux, include the options: -Dunix -fopenmp
To compile zpaqd for Linux, use -Dunix
To compile either program for non-x86, use -DNOJIT
