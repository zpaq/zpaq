zpaq712g.zip, Apr. 26, 2016. Added zpaq-gcc481.exe on Apr. 29, 2016.

zpaq is a journaling archiver optimized for user-level incremental
backup of directory trees in Windows and *nix. It supports AES-256
encryption, 5 multi-threaded compression levels, and content-aware
file fragment level deduplication. For backups it adds only files
whose date has changed, and keeps both old and new versions. You can roll
back the archive date to restore from old versions of the archive.
The default compression level is faster than zip usually with better
compression. zpaq uses a self-describing compressed format to allow
for future improvements without breaking compatibility with older
versions of the program. Contents:

File            Ver.   Description
-----------     ----   -----------
zpaq.exe        7.12   Archiver, 32 bit Windows XP or later.
zpaq64.exe      7.12   Archiver, 64 bit Windows XP or later.
zpaq-gcc481.exe 7.12   32 bit, for older processors not supporting SSSE3.
zpaq.cpp        7.12   zpaq source code.
zpaq.pod        7.12   zpaq man page in pod2man format.
libzpaq.h       7.12   libzpaq API documentation and header.
libzpaq.cpp     7.11   libzpaq API source code.
Makefile               To compile in Linux: make {install|check|clean}
COPYING                Unlicense.

All versions of this software can be found at
http://mattmahoney.net/dc/zpaq.html
Please report bugs to Matt Mahoney at mattmahoneyfl@gmail.com

Code from divsufsort.c is embedded in libzpaq.cpp.
divsufsort.c is (C) 2003-2008 Yuta Mori, MIT license (see source code).
It is also available from libdivsufsort-lite 2.0 from
http://code.google.com/p/libdivsufsort/
All remaining code is public domain. See COPYING.

zpaq.exe can run under either 32 or 64 bit Windows XP or later
(XP, Vista, 7, 8, 8.1, 10). zpaq64.exe runs only under 64 bit Windows.
The 32 bit versions default to using at most 2 cores (you can select more)
and can only use 2 GB memory.

zpaq is a command line program. For a brief description of the commands,
type "zpaq" with no arguments. See zpaq.pod for details.

TO COMPILE

Normally you can use "make" to compile for Unix, Linux, or Mac OS/X
or compile like this:

  g++ -O3 -march=native -Dunix zpaq.cpp libzpaq.cpp -pthread -o zpaq

To compile for non x86 or x86-64 hardware use option -DNOJIT
Some compilers complain about "-march=native" option. If so, take it out.

zpaq for Windows was compiled with MinGW-w64 g++ 6.0.0-20160412 (for zpaq.exe)
and g++ 4.8.1 (for zpaq64.exe and zpaq-gcc481.exe) as follows:

  g++ -O3 -s -m64 -msse2 -static zpaq.cpp libzpaq.cpp -o zpaq64
  g++ -O3 -s -m32 -msse2 -static zpaq.cpp libzpaq.cpp -o zpaq
  g++ -O3 -s -m32 -msse2 -static zpaq.cpp libzpaq.cpp -o zpaq-gcc481

To compile using Visual Studio:
(tested with ver. 10.0 (2010), cl version 16.00.30319.01 for 80x86)

  cl /O2 /EHsc zpaq.cpp libzpaq.cpp advapi32.lib

To generate a man page in Linux or Cygwin:

  pod2man zpaq.pod > zpaq.man

Options have the following meanings:

-Dunix   = select Unix or Linux target in zpaq and libzpaq. The default is
           Windows. Most Linux compilers automatically define unix.
-DDEBUG  = turn on run time checks.
-DNOJIT  = turn off run time optimization of ZPAQL to 32 or 64 bit x86
           in libzpaq. Use this for a non-x86 processor, or old
           processors not supporting SSE2 (mostly before 2001).
-pthread = link to pthread library (required in unix/Linux).

General options:

-O3 or /O2   = optimize for speed.
/EHsc        = enable C++ exception handling (VC++).
-s           = strip debugging symbols. (Some compilers ignore this).
-m32 or -m64 = select a 32 or 64 bit executable.
-static      = use this if you plan to run the program on a different
               machine than you compiled it on. Makes the executable bigger.
