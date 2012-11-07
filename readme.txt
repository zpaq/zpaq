zpaq v6.16 archiver, Nov. 5, 2012. Contents:

zpaq.exe      32 bit Windows executable, run from a command window.
zpaq64.exe    64 bit Windows executable (added Nov. 6, 2012).
zpaq.cpp      User's guide and source code.
libzpaq.h     libzpaq API documentation and header v6.00a.
libzpaq.cpp   libzpaq API source code v6.01.
divsufsort.h  libdivsufsoft-lite header.
divsofsort.c  libdivsufsort-lite source code.

zpaq is (C) 2012, Dell Inc., written by Matt Mahoney.
Licensed under GPL v3. http://www.gnu.org/copyleft/gpl.html

libzpaq is an API providing compression and decompression services
for developers. See libzpaq.h for documentation. It is public domain.

libdivsufsort-lite v2.00 is (C) 2003-2008, Yuta Mori under the MIT open
source license (see source code). It is mirrored from 
http://code.google.com/p/libdivsufsort/ for your convenience.
It and libzpaq are needed to compile zpaq.

zpaq is journaling: when you add files and directories, it keeps both
the old and new versions. You can roll it back to an earlier version.
It is incremental: only files whose dates have changed are added. It is
deduplicating: identical files and fragments are saved only once.
Speed is similar to zip but with better compression. For example, a disk
backup:

  zpaq -add e:backup.zpaq c:\* -not c:\windows

will take a couple hours to compress 100 GB the first time, then a couple
minutes for subsequent backups each night. To list version dates:

  zpaq -list e:backup.zpaq -summary

To recover a copy of an old version of a directory:

  zpaq -extract e:backup.zpaq c:\users\bob -to tmp\bob -version 5

Command line documentation is in zpaq.cpp.
If you find a bug, please let me know at mattmahoneyfl@gmail.com.
All zpaq versions can be found at http://mattmahoney.net/zpaq

zpaq.exe was compiled with MinGW g++ 4.7.0 and compressed with
upx 3.06w as follows:

  g++ -O3 -s -static -Wall zpaq.cpp libzpaq.cpp divsufsort.c -DNDEBUG -o zpaq
  upx zpaq.exe

To compile for Linux, include the options: -Dunix -fopenmp
