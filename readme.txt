ZPIPE v2.00

zpipe compresses or decompresses from standard input to standard output.

To compress fastest:  zpipe -1 < input > output
To compress average:  zpipe -2 < input > output
To compress smallest: zpipe -3 < input > output
To decompress:        zpipe -d < input > output

The compressed format is compatible with the ZPAQ level 1 standard
described in zpaq1.pdf at http://mattmahoney.net/dc/#zpaq
The format is compatible with zp, zpaq, and unzpaq, and with applications
that use libzpaq including demo1 and demo2.

Compression levels 1 through 3 are equivalent to c1 through c3 in zp
and with fast.cfg, mid.cfg, and max.cfg respectively in zpaq.

Compression produces a single block containing a single segment
with no stored filename, comment, or checksum. When decompressed
with an archiver like zp, zpaq, or unzpaq, the user must specify the
output file name.

When decompressing, all of the output data is concatenated. Stored
filenames and comments are ignored. Checksums are not verified.

zpipe (zpipe.cpp and zpipe.exe) is (C) 2010 Dell Inc. It is licensed
under GPLv3. See gpl.txt.

libzpaq is public domain. There are no restrictions on the use of this
code. See zpaqlib.txt.

zpipe and libzpaq are written by Matt Mahoney.
For the latest versions of zpipe and libzpaq, see
http://mattmahoney.net/dc/#zpaq

Contents of zpipe200.zip:

  readme.txt  - This file
  zpipe.cpp   - Source code (GPL)
  zpipe.exe   - Windows executable (GPL)
  libzpaq.txt - libzpaq 0.02 documentation (public domain)
  libzpaq.h   - libzpaq include file (public domain)
  libzpaq.cpp - libzpaq source code (public domain)
  demo1.cpp   - libzpaq usage example (public domain)
  demo2.cpp   - libzpaq usage example (public domain)
  gpl.txt     - GPLv3 license

zpipe.exe was compiled with g++ 4.5.0 and compressed with upx 3.06w:

  g++ -O2 -march=pentiumpro -fomit-frame-pointer -s -DNDEBUG zpipe.cpp libzpaq.h -o zpipe
  upx zpipe.exe
