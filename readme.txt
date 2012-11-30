                       LIBZPAQ Distribution 1.00
                        Matt Mahoney, Dell Inc.
                           Sept. 29, 2010

ZPAQ is a proposed standard for highly compressed data. This package
contains the specification, a library application programming interface
for reading and writing ZPAQ formatted compressed data, and two applications
that use the library: a file compressor, zpipe, and an archiver, zp.
Contents:

   File         Ver.  Date                Description         License
------------   -----  -----------  -----------------------  ------------
readme.txt            Sep 29 2010  This file                Free to copy
zpaq.pdf       Rev 1  Sep 29 2009  Format specification     Free to copy
unzpaq108.cpp  1.08   Oct 14 2009  Reference decoder        GPL

libzpaq.txt    0.02   Sep 27 2010  API documentation        Public domain
libzpaq.cpp    0.02   Sep 28 2010  API source code          Public domain
libzpaq.h      0.02   Sep 28 2010  API header file          Public domain

zpipe.cpp      2.00   Sep 28 2010  File compressor source   GPL
zpipe.exe      2.00   Sep 28 2010  Windows executable       GPL

zp.cpp         2.00   Sep 29 2010  Archiver source          GPL
zp.exe         2.00   Sep 29 2010  Windows executable       GPL

fast.cfg              Apr 26 2010  ZPAQL source for         GPL
mid.cfg               Oct 09 2009    compression levels     GPL
max.cfg               Oct 09 2009    1, 2, and 3            GPL

gpl.txt        3      Jun 29 2007  GPL license              Free to copy

All of the files except gpl.txt were written by Matt Mahoney at Dell Inc.
Files licensed as free to copy may be copied and distributed by anyone,
but changes are not allowed. Files marked as public domain have no
restrictions on their use. Files marked GPL are licensed under the GNU
general public license, which allows copying and changes but requires that
any redistribution of derived code also be licensed under GPL and include
source code. See gpl.txt or http://www.gnu.org/copyleft/gpl.html

The latest version of this package can be found at http://mattmahoney.net/dc/

The ZPAQ specification has two parts. zpaq1.pdf describes the format.
The program unzpaq is the reference decoder. unzpaq does not use the
library API because it was written earlier in conjunction with the
document. The specification only defines the decompression procedure.
The implementation of the compressor is up to the user.

Libzpaq is an API in C++ that allows applications to compress or
decompress ZPAQ formatted data to or from files or objects in memory.
It consists of a header file (libzpaq.h) that should be #included in
the application, and source code (libzpaq.cpp) that should be linked
to it.

zpipe compresses or decompresses from standard input to standard
output. It supports 3 compression levels: 1=fast, 2=mid, 3=max
(slowest but best compression). For example:

  zpipe -3 < input > archive.zpaq    (use best compression)
  zpipe -d < archive.zpaq > output   (decompress)

zp creates, lists, and extracts archives containing many files.
For example:

  zp c3 archive file1 file2   (compress (3=max) 2 files to archive.zpaq)
  zp a1 archive file3         (append (1=fast) another file)
  zp l archive                (list contents: file1 file2 file3)
  zp v archive                (verbose listing)
  zp x archive                (extract all 3 files)
  zp x archive out1 out2      (extract first 2 files and rename)
  zp e archive                (extract all files to current directory)

Running either program with no arguments prints a brief help message.
More detailed usage instructions are documented in zpipe.cpp and zp.cpp.

All of these programs can read each other's output. When zpipe
decompresses an archive, all of the output is concatenated together.
When zp or unzpaq decompresses a file compressed with zpipe, the user must
specify the output file name.

The executables were compiled with g++ 4.5.0 and compressed with upx 3.06w
as follows:

  g++ -O2 -march=pentiumpro -fomit-frame-pointer -s -DNDEBUG zpipe.cpp libzpaq.cpp -o zpipe
  upx zpipe.exe

  g++ -O2 -march=pentiumpro -fomit-frame-pointer -s -DNDEBUG zp.cpp libzpaq.cpp -o zp
  upx zp.exe

Libzpaq and the applications that use it (zpipe and zp, but not unzpaq)
support arbitrary compression algorithms, but are optimized for 3 levels
as shown. Speed is measured on a 2.0 GHz T3200 under 32 bit Windows.
The input data is the 14 file Calgary corpus compressed into a single
block (solid archive). zip is shown for comparison.

              Memory     Speed     Calgary corpus
              ------  -----------  ---------------
  1 (fast)     38 MB  0.7  sec/MB    807,214 bytes
  2 (mid)     111 MB  2.3  sec/MB    699,586 bytes
  3 (max)     246 MB  6.4  sec/MB    644,545 bytes
  zip -9       <1 MB  0.13 sec/MB  1,020,719 bytes

The source code for these models is given in fast.cfg, mid.cfg, and
max.cfg. The zpaq program will compile these configuration files
into C++ code and header strings which were inserted into libzpaq
and used in zpipe and zp. zpaq is not included in this distribution
but is available at http://mattmahoney/dc/ as GPL source and a
Windows executable. It also runs under Linux. zpaq.cpp and zpaq1.pdf
tell how to interpret the ZPAQL code in these configuration files.
You do not need these files to compile or run any of the applications
in this package or to use libzpaq.
