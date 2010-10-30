                       LIBZPAQ Distribution 2.00
                        Matt Mahoney, Dell Inc.
                             Oct. 30, 2010

ZPAQ is a proposed standard for highly compressed data. This package
contains the specification, a library application programming interface
for reading and writing ZPAQ formatted compressed data, and 3 applications
that use the library: a file compressor (zpipe), an archiver (zp) and
a self extracting stub (zpsfx).
Contents:

   File         Ver.  Date                Description         License
------------   -----  -----------  -----------------------  ------------
readme.txt            Oct 30 2010  This file                Free to copy
zpaq.pdf       Rev 1  Sep 29 2009  Format specification     Free to copy
unzpaq.cpp     1.08   Oct 14 2009  Reference decoder        GPL

libzpaq.txt    2.00   Oct 30 2010  API documentation        Public domain
libzpaq.cpp    2.00   Oct 30 2010  API source code          Public domain
libzpaqo.cpp   2.00   Oct 30 2010  API source code          Publid domain
libzpaq.h      2.00   Oct 30 2010  API header file          Public domain

zpaq.cpp       2.00   Oct 30 2010  Archiver source code     GPL
zpaq.exe       2.00   Oct 30 2010  Windows executable       GPL

zpipe.cpp      2.01   Oct 14 2010  File compressor source   GPL
zpipe.exe      2.01   Oct 30 2010  Windows executable       GPL

zpsfx.cpp      1.00   Oct 20 2010  Self extracting stub     Public domain
zpsfx.exe      1.00   Oct 30 2010  Self extracting stub     Public domain
zpsfx.tag             Oct 20 2010  Tag for zpsfx            Public domain

compile.bat           Oct 30 2010  File used to compile all Public domain

fast.cfg              Apr 26 2010  zpaq source for          GPL
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
the 3 default models (fast, mid, max).

zpipe compresses or decompresses from standard input to standard
output. It supports 3 compression levels: 1=fast, 2=mid, 3=max
(slowest but best compression). For example:

  zpipe -3 < input > archive.zpaq    (use best compression)
  zpipe -d < archive.zpaq > output   (decompress)

zpaq creates, lists, and extracts archives containing many files.
For example:

  zpaq c3 archive file1 file2   (compress (3=max) 2 files to archive.zpaq)
  zpaq a1 archive file3         (append (1=fast) another file)
  zpaq l archive                (list contents: file1 file2 file3)
  zpaq x archive                (extract all 3 files)

zpaq also supports development of new compression algorithms which
are specified in configuration (.cfg) files and external preprocessors.
See zpaq.cpp for how to install and to use these features. Briefly,
for Windows, put all of these files in c:\zpaq and install MinGW g++.

zpsfx is used to create self extracting archives:

  copy/b zpsfx.exe+zpsfx.tag+archive.zpaq archive.exe

Then running archive.exe with no arguments will extract the compressed files.

Running zpaq or zpipe with no arguments prints a brief help message.
More detailed usage instructions are documented in the source code.

All of these programs can read each other's output. When zpipe
decompresses an archive, all of the output is concatenated together.
When zpaq or unzpaq decompresses a file compressed with zpipe, the user must
specify the output file name. zpsfx requires stored filenames, so appending
to zpsfx with zpipe will not work. zpsfx is public domain to allow distributing
self extracting archives without source code, which GPL might not allow.

The executables were compiled with g++ 4.5.0 and compressed with upx 3.06w
as shown in compile.bat.

Libzpaq and the applications that use it (zpipe and zpaq, but not unzpaq)
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
and used in zpipe and zpaq.

History:

Sep 27 2010: libzpaq 0.02, zp 2.00, zpipe 2.00

Oct 14 2010: libzpaq 0.03, zp 2.01, zpipe 2.01. libzpaq interface changed.
Reader and Writer were changed from template paramters to virtual
base classes to speed up compilation. Corresponding changes made to
zp and zpipe.

Oct 20 2010: libzpaqo.cpp separates the optimized code from libzpaq.cpp.
Added zpsfx 1.00.

Oct 30 2010: replaced zp with zpaq 2.00. The library implementation
(not interface) was changed to support it.
