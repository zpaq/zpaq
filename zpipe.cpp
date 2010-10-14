/*  zpipe streaming file compressor v2.01

(C) 2010, Dell Inc.
    Written by Matt Mahoney, matmahoney@yahoo.com, Oct. 14, 2010.

    LICENSE

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 3 of
    the License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful, but
    WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    General Public License for more details.
    Visit <http://www.gnu.org/copyleft/gpl.html>.

To compress or decompress:   zpipe -option < input > output

Options are:
  -1 compress fastest (min.cfg)
  -2 compress average (mid.cfg)
  -3 compress smallest (max.cfg)
  -d decompress

Compressed output is in ZPAQ format as one segment within one
block. The segment has no filename, commment, or checksum. It is readable
by other ZPAQ compatible decompressors. -2 is equivalent to:

  zpaq nicmid.cfg output input

Decompression will accept ZPAQ compressed files from any source,
including embedded in other data, such as self extracting archives.
If the input archive contains more than one file, then all of the
output is concatenated. It does not verify checksums.

To compile:

g++ -O2 -march=pentiumpro -fomit-frame-pointer -s zpipe.cpp libzpaq.cpp -o zpipe
To turn off assertion checking (faster), compile with -DNDEBUG

*/

#include <stdio.h>
#include <stdlib.h>
#ifndef unix
#include <fcntl.h> // for setmode(), requires g++
#endif

// libzpaq requires error() and concrete derivations of Reader and Writer
namespace libzpaq {
  void error(const char* msg) {
    fprintf(stderr, "zpipe error: %s\n", msg);
    exit(1);
  }
}
#include "libzpaq.h"

struct File: public libzpaq::Reader, public libzpaq::Writer {
  FILE* f;
  File(FILE* f_): f(f_) {}
  int get() {return getc(f);}
  void put(int c) {putc(c, f);}
};

// Print help message and exit
void usage() {
  fprintf(stderr, 
    "zpipe 2.01 file compressor %s\n"
    "(C) 2010, Dell Inc.\n"
    "Licensed under GPL v3. See http://www.gnu.org/copyleft/gpl.html\n"
    "\n"
    "Usage: zpipe -option < input > output\n"
    "Options are:\n"
    "  -1   compress fastest\n"
    "  -2   compress average\n"
    "  -3   compress smallest\n"
    "  -d   decompress\n", __DATE__);
  exit(1);
}

int main(int argc, char** argv) {

  // In Windows, put stdin and stdout in binary mode
#ifndef unix
  setmode(0, O_BINARY);  // stdin in binary mode
  setmode(1, O_BINARY);  // stdout in binary mode
#endif
  File in(stdin);
  File out(stdout);

  // Read command line
  char option=0;
  if (argc==2 && argv[1][0]=='-')
    option=argv[1][1];
  else
    usage();

  // Compress
  if (option=='d')
    libzpaq::decompress(&in, &out);
  else if (option>='1' && option<='3')
    libzpaq::compress(&in, &out, option-'0');
  else
    usage();
  return 0;
}
