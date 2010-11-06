                       LIBZPAQ Distribution 2.01
                        Matt Mahoney, Dell Inc.
                             Nov. 5, 2010

ZPAQ is a proposed standard for highly compressed data. This package
contains the specification, a library application programming interface
for reading and writing ZPAQ formatted compressed data, and 2 applications
that use the library: a file compressor (zpipe), and an archiver (zpaq).
Contents:

   File         Ver.  Date                Description         License
------------   -----  -----------  -----------------------  ------------
readme.txt            Nov 05 2010  This file                Free to copy
zpaq.pdf       Rev 1  Sep 29 2009  Format specification     Free to copy
unzpaq.cpp     1.08   Oct 14 2009  Reference decoder        GPL

libzpaq.txt    2.00   Oct 30 2010  API documentation        Public domain
libzpaq.cpp    2.00   Oct 30 2010  API source code          Public domain
libzpaqo.cpp   2.00   Oct 30 2010  API source code          Public domain
libzpaq.h      2.00   Oct 30 2010  API header file          Public domain

zpipe.cpp      2.01   Oct 14 2010  File compressor source   GPL
zpipe.exe      2.01   Nov 05 2010  Windows executable       GPL

zpaq.cpp       2.01   Nov 05 2010  Archiver source code     GPL
zpaq.exe       2.01   Nov 05 2010  Windows executable       GPL
makezpaq.bat          Nov 05 2010  Script for "zpaq o"      Public domain
makezpsfx.bat         Nov 05 2010  Script for "zpaq e"      Public domain

zpsfx.cpp      1.00   Oct 20 2010  Self extracting stub     Public domain
zpsfx.exe      1.00   Nov 05 2010  Self extracting stub     Public domain
zpsfx.tag             Oct 20 2010  Tag for zpsfx            Public domain

fast.cfg              Apr 26 2010  zpaq source for          GPL
mid.cfg               Oct 09 2009    compression levels     GPL
max.cfg               Oct 09 2009    1, 2, and 3            GPL
min.cfg               Oct 09 2009  fast LZP model           GPL
lzppre.cpp            Sep 28 2009  preprocessor for min.cfg GPL
lzppre.exe            Nov 05 2010  Windows executable       GPL

compile.bat           Nov 05 2010  Script to compile all    Public domain

gpl.txt        3      Jun 29 2007  GPL license              Free to copy

All of the files except gpl.txt were written by Matt Mahoney at Dell Inc.
Files licensed as free to copy may be copied and distributed by anyone,
but changes are not allowed. Files marked as public domain have no
restrictions on their use. Files marked GPL are licensed under the GNU
general public license, which allows copying and changes but requires that
any redistribution of derived code also be licensed under GPL and include
source code. See gpl.txt or http://www.gnu.org/copyleft/gpl.html

The latest version of this package can be found at http://mattmahoney.net/dc/

The ZPAQ specification has two parts. zpaq.pdf describes the format.
The program unzpaq is the reference decoder. unzpaq.cpp does not use the
library API because it was written earlier in conjunction with the
document. The specification only defines the decompression procedure.
The implementation of the compressor is up to the user.

Libzpaq is an API in C++ that allows applications to compress or
decompress ZPAQ formatted data to or from files or objects in memory.
It consists of a header file (libzpaq.h) that should be #included in
the application, and 2 source code files (libzpaq.cpp and libzpaqo.cpp)
that should be linked to it. libzpaqo.cpp is source code for optimizing
the 3 default models (fast, mid, max). See libzpaq.txt for documentation.

zpipe is a simple command line file compressor that compresses or
decompresses from standard input to standard output. It supports 3
compression levels: 1=fast, 2=mid, 3=max. For example:

  zpipe -3 < input > archive.zpaq    (use best compression)
  zpipe -d < archive.zpaq > output   (decompress)

zpaq is an archiver. It will create, append to, list, and extract archives
and create self extracting archives for Windows. It is compatible with zpipe
and all other ZPAQ compliant programs (such as unzpaq). zpaq supports
arbitrary compression algorithms written in the ZPAQL language and external
preprocessors. See zpaq.cpp for documentation.

zpaq creates, lists, and extracts archives containing many files.
For example:

  zpaq c3 archive file1 file2   (compress (3=max) 2 files to archive.zpaq)
  zpaq a1 archive file3         (append (1=fast) another file)
  zpaq l archive                (list contents: file1 file2 file3)
  zpaq x archive                (extract all 3 files)

zpaq also supports development of new compression algorithms which
are specified in configuration (.cfg) files and external preprocessors.
The files fast.cfg, mid.cfg, and max.cfg are included. For example:

  zpaq cfast archive files...   (same as c1)

Other configurations are available. For example, min.cfg and its accompanying
preprocessor lzppre.exe (source lzppre.cpp) is faster than fast.cfg
but does not compress as small.

  zpaq cmin archive files...

You can speed up compression and decompression with the "o" prefix if
you have a C++ compiler. This option converts the ZPAQL code in the
config file to C++ and compiles and runs it.

  zpaq ocmin archive files...   (compress faster)
  zpaq ox archive               (extract faster)

To create a self extracting archive:

  zpaq e archive                (creates archive.exe)
  zpaq oe archive               (creates a faster archive.exe)

To extract:

  archive.exe

No compiler is needed to extract even if created with "oe".

zpaq makes a self extracting archive by appending it to zpsfx.exe
(the decompression code, source zpsfx.cpp) and the 13 byte file zpsfx.tag
which identifies the start of the data. These files are public domain
so that archives can be distributed without source code. You can
also modify zpsfx.cpp if you need to write an installer that does other
stuff.


BENCHMARKS

"zpaq oc" speed is measured on a 2.0 GHz T3200 under 32 bit Windows.
The input data is the 14 file Calgary corpus compressed into a single
block (solid archive). Decompression uses about the same time and
memory as compression. zip is shown for comparison.

   Model      Memory     Speed     Calgary corpus
  --------    ------  -----------  ---------------
    (min)       4 MB  0.5  sec/MB  1,030,817 bytes
  1 (fast)     38 MB  0.7  sec/MB    807,214 bytes
  2 (mid)     111 MB  2.3  sec/MB    699,586 bytes
  3 (max)     246 MB  6.4  sec/MB    644,545 bytes
  zip -9       <1 MB  0.13 sec/MB  1,020,719 bytes


TO INSTALL (Windows)

1. Create a directory C:\zpaq and put all the files here.
2. Add C:\zpaq to your PATH.
3. Install MinGW g++ from http://www.mingw.org/
4. Install upx from http://upx.sourceforge.net/
5. Run compile.bat

If you don't plan to use the "o" option then you only need steps 1 and 2.
This does not affect speed for the 3 built in compression levels.
Windows executables (.exe) files created by step 5 are already included.

You can skip step 2 (set PATH) if you don't mind typing the command
"\zpaq\zpaq" instead of "zpaq" every time.

You can install in some other directory besides c:\zpaq but then
you will need to edit the 3 .bat files (compile.bat, makezpaq.bat,
and makezpsfx.bat) to replace "c:\zpaq" with your chosen folder every
place it occurs.

You don't need to install UPX, but then your executables and self
extracting archives will be larger. A self extracting archive will
add about 100 KB instead of 40 KB. You will also need to remove
the "upx" commands from compile.bat and makezpsfx.bat. You can
use other .exe compressors besides upx if you modify these two
files appropriately to call them.

You can use other compilers besides g++ but you will have to update
the "g++" commands in the 3 .bat files appropriately to call your
other compiler with appropriate options.

You don't have to use the options provided. In particular, different
optimization options might be more appropriate for your computer.

-DNDEBUG turns off run time checks. You can try removing this if you
find a bug.

-DOPT is needed to create zpaq.o, but should not be used
to create zpaq.exe. The effect is to disable the "o" option when
zpaq creates and runs an optimized copy of itself. Note that some
compilers produce .obj files instead of .o files. Adjust your .bat
files accordingly.

I have not tested this in Linux but I wrote the code with plans
in mind.

If you write your own compression algorithms, then put the config
files and preprocessors either in the install directory (c:\zpaq)
or the current directory.


HISTORY

Sep 27 2010: libzpaq 0.02, zp 2.00, zpipe 2.00

Oct 14 2010: libzpaq 0.03, zp 2.01, zpipe 2.01. libzpaq interface changed.
Reader and Writer were changed from template paramters to virtual
base classes to speed up compilation. Corresponding changes made to
zp and zpipe.

Oct 20 2010: libzpaqo.cpp separates the optimized code from libzpaq.cpp.
Added zpsfx 1.00.

Oct 30 2010: replaced zp with zpaq 2.00. The library implementation
(not interface) was changed to support it.

Nov 05 2010: zpaq 2.01. Supports self extracting archives. Simplified
installation (everything goes in C:\zpaq).

