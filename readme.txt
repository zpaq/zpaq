README for ZPIPE - Jan. 18, 2010.

zpipe is a ZPAQ compatible data compressor and decompresser
that reads from standard input and writes to standard output.

To compress:   zpipe c < input > output
To decompress: zpipe d < input > output

Version 1.00 Contents:

zpipe.exe - Executable program for Windows, Sept. 30, 2009.

zpipe.cpp - Source code for Windows/g++, Sept. 29, 2009.

zpipe-fix-build-on-linux.diff - Patch for Linux (by Hanno Böck)
  Jan. 18, 2010.

To compile in Windows (g++ 4.4.0)

  g++ -O2 -march=pentiumpro -fomit-frame-pointer -s zpipe.cpp -o zpipe

To turn off assertion checking (faster), compile with -DNDEBUG

For Linux, apply the patch or remove the two lines from main()
at the end of the source code:

  setmode(0, O_BINARY);  // stdin in binary mode
  setmode(1, O_BINARY);  // stdout in binary mode

This code is non-standard but necessary in Windows because by default
standard input and output are open in text mode. Not all Windows
compilers will accept it. The code is not needed in UNIX or Linux.

The ZPAQ standard is published at http://mattmahoney.net/dc/

zpipe is compatible with all ZPAQ compressors and decompressers
that follow the standard. In particular compression and decompression
are equivalent to the zpaq 1.10 commands:

  zpaq nicmid.cfg output input
  zpaq x input output

In other words, the compressed output is stored as a single segment
in a single block with no filename or comment but with a SHA1 checksum.
The compression model is mid.cfg. It requires 111 MB memory to compress
or decompress.

The code is Copyright 2009, Ocarina Networks and is licensed
under the GNU General Public License (GPL) version 3.
http://www.gnu.org/copyleft/gpl.html
It is written by Matt Mahoney.
