/* demo1.cpp - Demo file compressor for libzpaq.

Written by Matt Mahoney, Sept. 27, 2010.
This program is placed in the public domain.
It is provided "as is" with no warranty.

To use: demo1 command input output
Commands:
  1 = compress fast
  2 = compress medium
  3 = compress best
  d = decompress

For example, to compress the file book1:

  demo1 3 book1 book1.zpaq
  demo1 d book1.zpaq book1

To compile with g++, Borland, or Mars:

  g++ -O2 demo1.cpp libzpaq.cpp -o demo1
  bcc32 -O -6 demo1.cpp libzpaq.cpp
  dmc -o -6 demo1.cpp libzpaq.cpp

Use option -DNDEBUG to turn off run time checks for more speed.
Use other optimization options as appropriate for your platform.
These are just examples.
*/

#include <stdio.h>
#include <stdlib.h>

// libzpaq required functions: get(), put(), and error()
namespace libzpaq {

  inline int get(FILE* in) {
    int c=getc(in);
    return c==EOF?-1:c;
  }

  inline void put(int c, FILE* out) {
    putc(c, out);
  }

  void error(const char* msg) {
    fprintf(stderr, "libzpaq error: %s\n", msg);
    exit(1);
  }
}

#include "libzpaq.h"

// File compressor and decompresser
int main(int argc, char** argv) {

  // Print help message
  if (argc<=3) {
    printf("To compress or decompress files: demo1 cmd input output\n"
      "Commands: 1=fast, 2=mid, 3=max, d=decompress\n");
    return 1;
  }

  // Open input and output files
  char cmd=argv[1][0];
  FILE* in=fopen(argv[2], "rb");
  if (!in) return perror(argv[2]), 1;
  FILE* out=fopen(argv[3], "wb");
  if (!out) return perror(argv[3]), 1;

  // Compress
  if (cmd>='1' && cmd<='3')
    libzpaq::compress(in, out, cmd-'0');

  // Decompress
  else if (cmd=='d')
    libzpaq::decompress(in, out);
}
