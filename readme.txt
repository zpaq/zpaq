zpaq v6.04 readme. Oct. 1, 2012.

zpaq v6.04 is a journaling incremental deduplicating archiving compressor.
All zpaq versions can be found at see http://mattmahoney.net/zpaq

Journaling means that the archive is append-only. When you update it,
the old versions of files are kept and you can recover them by specifying
the version of the archive you want to extract from.

Incremental means that when you tell it to add files, it compares the
date with the stored date and only adds the files that have changed.
You can override this.

Deduplicating means that identical files and file fragments are stored
only once. Files are fragmented along content-dependent boundaries
(average size 64K) and the SHA-1 hashes of the fragments are computed
and compared with the hashes stored in the archive. If there is a match,
then only a pointer is saved.

Files and directory trees are compressed in ZPAQ format, which is
open source, precisely specified, and requires no license to use. The
specification is supported by a public domain reference decoder
and a public domain library API providing streaming compression and
decompression services to or from files and/or memory. The format
is self-describing, which means that when new compression algorithms
are developed, old decompressers will still be able to read their output.
ZPAQ is based on the PAQ context-mixing algorithm, which achieves good
compression ratios, but also supports fast algorithms like LZ77 and BWT.

The zpaq archiver has 4 built-in compression levels and also accepts
custom compression algorithms described by configuration files written
in the ZPAQL language and by external preprocessors (which are only needed
to compress and not to extract). zpaq has tools for developers
to test and debug configuration files. Examples can be found at the
above website.

zpaq is licensed under GPL v3 from Dell Inc. It uses libzpaq (public
domain) and libdivsufsort-lite (MIT license by Yuta Mori). zpaq and libzpaq
are written by Matt Mahoney. Contents:

zpaq.exe      32 bit Windows executable, run from a command window.
zpaq.cpp      zpaq command line documentation and source code.
libzpaq.h     libzpaq API documentation (including ZPAQL) and header.
libzpaq.cpp   libzpaq API source code.
divsufsort.h  libdivsufsoft-lite header.
divsofsort.c  libdivsufsort-lite source code.

The Windows executable was compiled with MinGW g++ 4.6.1
and compressed with upx 3.06w as follows:

  g++ -O3 -msse2 -s -static -Wall zpaq.cpp libzpaq.cpp divsufsort.c -DNDEBUG -o zpaq
  upx zpaq.exe

To back up your hard drive to archive e:backup.zpaq on an external drive:

  zpaq -add e:backup c:\

The first backup may take a few hours per 100 GB. Subsequent backups will
take only a few minutes to compare dates and add any changes. To list
contents:

  zpaq -list e:backup

To list just a directory tree and compare:

  zpaq -list e:backup c:\Users\Joe

Each internal file is marked with a character to indicate whether the
corresponding external file has the same or different date or does not exist.
Files might be listed multiple times with different versions if they were
changed and added more than once. The version number is incremented by
each -add. -list also shows the dates corresponding to each version number.
To extract a directory from the archive as it existed after the 10'th -add
and put it in a temporary directory:

  zpaq -extract e:backup c:\Users\Joe -to tmp -version 10

will extract e.g. c:\Users\Joe\Desktop\letter.doc to tmp\Desktop\letter.doc
and so on. The default is to restore all files to their original locations
but not overwrite any files unless you specify -force.

Older versions of zpaq (before v6.00) and some other archives like PeaZip
do not support journaling, incremental update, or deduplication. To
create archives without these features that they can read, use the
-streaming option.

Complete command line documentation can be found in zpaq.cpp. Configuration
file syntax, if you are so inclined, is found in libzpaq.h.
