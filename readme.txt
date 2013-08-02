zpaq641.zip, Aug. 2, 2013. Contents:

zpaq.exe      6.41   Archiver, 32 bit Windows command line executable.
zpaq64.exe    6.41   For 64 bit Windows.
zpaq.cpp      6.41   zpaq user's guide and source code.
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
backup of directory trees. It supports 7 multi-threaded compression
levels and file fragment level deduplication. It adds only files whose
date has changed, and keeps both old and new versions. You can roll
back the archive date to restore from old versions of the archive.
The default compression level is faster than zip usually with better
compression. Archives produced with the current version are readable
by all versions back to 6.00 and by all future versions.

zpaq.exe can run under either 32 or 64 bit Windows. zpaq64.exe runs only
under 64 bit Windows. The difference is that only the 64 bit version can
use more than 3 GB of memory, even if you have more. This difference is
only important for the higher (slower) compression levels. The default
level uses about 100 MB memory per thread.

To install in Linux, compile it with make or see instructions below.


EXAMPLES

  zpaq a e:backup c:\ -not c:\windows

Compress the C: drive except for the windows directory to archive
e:backup.zpaq. When the archive is created, it may take 2-3 hours
to compress 100 GB. Subsequent daily backups will only add files whose date
has changed, taking perhaps 2-3 minutes. Files that have been moved, renamed,
or copied will update the archive index but not take additional space,
due to deduplication. If a file is modified or deleted, the old version
still remains in the archive.

  zpaq x e:backup c:\users\bob -to tmp -until 20130701

Restore a copy of a directory tree with a new name
(e.g. c:\users\bob\pictures is extracted to tmp\pictures) as it existed
as of the last backup on or before July 1, 2013.

  zpaq l e:backup -quiet 1000000 -all

List the archive contents, showing all versions (not just the latest)
of files at least 1 MB in size.

  zpaq c e:backup -force

Compares the archive contents with the original files and reports
any differences. -force tells it to compare the file contents rather than
the dates, attributes, and sizes (which is slower but more reliable).

  zpaq

Show a brief help message.

Complete documentation can be found in zpaq.cpp. You do not have to
read any source code. I just found it convenient to put both in the
same file.


TO COMPILE

zpaq.exe was compiled with MinGW g++ 4.8.0 and compressed
with upx 3.08w as follows:

  g++ -O3 -s -m64 -static -DNDEBUG zpaq.cpp libzpaq.cpp divsufsort.c -o zpaq64
  g++ -O3 -s -m32 -static -DNDEBUG zpaq.cpp libzpaq.cpp divsufsort.c -Wl,--large-address-aware -o zpaq
  upx zpaq.exe

Option -Wl,--large-address-aware makes 3 GB memory available to the 32 bit
Windows version.

To compile zpaq for Linux, include the options: -Dunix -fopenmp
-fopenmp is for divsufsort. It implies -pthread which is required for zpaq.

To compile for non-x86 or an old processor not supporting SSE2, use -DNOJIT

-DNDEBUG turns off run time checks in divsufsort. They are off by default
in zpaq and libzpaq. To turn them on use -DDEBUG

-static is only needed if you plan to run the program on a different
computer than you compiled it on. It makes the program larger.

upx compresses 32 bit Windows executables. It is not required. It will
not work on 64 bit executables.

You can compile using Visual Studio as follows:

  cl /O2 /EHsc zpaq.cpp libzpaq.cpp divsufsort.c

The following are needed to compile:

libzpaq is written by Matt Mahoney and released to the public domain.
It is an API providing compression and decompression services
for developers. See libzpaq.h for documentation.

libdivsufsort-lite v2.00 is (C) 2003-2008, Yuta Mori under the MIT open
source license (see source code). It is mirrored from 
http://code.google.com/p/libdivsufsort/ for your convenience.
