README for zpaq v3.00 - July 16, 2011.
Matt Mahoney, matmahoney@yahoo.com

zpaq is an archiver and a tool for developing new compression algorithms.
This package contains source code and a 32 bit Windows executable.
The current version has only been tested in Windows. A Linux
version is planned. The latest version of this software, documenation,
and associated configuration files can be found at
http://mattmahoney.net/dc/zpaq.html

zpaq creates, extracts, and lists archives conforming to the ZPAQ
level 1 standard as described in the document and reference decoder
from the above website. There are 4 built in compression levels
(-m1 through -m4) plus the ability to describe your own compression
algorithms in configuration (.cfg) files. Docmumenation and examples
are available from the above website. zpaq also includes tools
for testing and debugging config files.

zpaq is designed for high performance. The following shows compressed
size of the 14 file Calgary corpus and compression and decompression
times in seconds on a dual core 2.0 GHz T3200 under 32 bit Windows.
zpaq (and 7zip) use both cores.

  Compressor   size      C/D time
  ---------- ---------  ---------
  zip        1,028,059   0.4  0.1
  bzip2        828,347   0.7  0.4
  7zip         824,296   1.7  0.3
  zpaq -m1     843,572   0.8  0.8
  zpaq -m2     784,373   1.2  1.3
  zpaq -m3     722,197   5.3  5.5
  zpaq -m4     666,624  14.5 15.0

See zpaq.cpp for command line usage documentation and installation.

I did not yet include documentation in this package on writing
config files. That documentation is available at the above website
(for an earlier version of zpaq, but the format has not changed).

libzpaq is a library API in C++ that provides services for compressing,
decompressing, and listing ZPAQ archives or byte streams. See libzpaq.txt
or the above website for usage.


LICENSE

The source code has 3 parts:

- zpaq - main program.
- libzpaq - library API providing compression and decompression.
- libdivsufsort-lite - BWT compression for zpaq methods -m1 and -m2.

The main program, zpaq, and library API, libzpaq are (C) 2011, Dell Inc.
and written by Matt Mahoney. zpaq is licensed under GPL v3.
(see http://www.gnu.org/copyleft/gpl.html). It may be used freely but
any distribution must include source code, including modifications,
under the same license. libzpaq is licensed under a modified MIT license
allowing unrestricted use as described in the source code.

libdivsufsort-lite v2.01 is (C) 2003-2008 by Yuta Mori, available
at http://code.google.com/p/libdivsufsort/
It is distributed under a standard MIT license allowing unrestricted
use except that the copyright notice must be included, as described
in the source code.

This readme file may be freely distributed.


HISTORY

zpaq v3.00 merges the features of zpaq 2.05 (archiving and config
file development) with zp 1.03 (multithreaded compression and
decompression and BWT mode compression). It also includes a block
editing command for reordering archive contents.
