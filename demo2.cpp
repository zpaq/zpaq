/* demo2.cpp

This program gives a detailed listing of ZPAQ archives.
Usage: demo2 files...

Written by Matt Mahoney, Sept. 27, 2010.
This program is placed in the public domain.
It is provided "as is" with no warranty.

To compile: g++ demo2.cpp libzpaq.cpp -o demo2

*/

#include <stdio.h>
#include <stdlib.h>
#include <string>

// Some types for writing decimal numbers or discarding output.
class NumberWriter{};
class Null{};

// libzpaq requires definitions of get(), put(), error() in
// namespace libzpaq as shown for each input or output type
// such as FILE*. The function bodies are up to you.
namespace libzpaq {

  // Required error handler. libzpaq will pass it an English language
  // error message. It can do anything you want.
  void error(const char* msg) {
    fprintf(stderr, "Error: %s\n", msg);
    exit(1);
  }

  // Read one byte from a file. The return value should be 0 to 255
  // or -1 at end of input.
  inline int get(FILE* in) {
    return getc(in);
  }

  // Write one byte c to a file. This is how we would do it if we
  // were going to compress or decompress to an output file.
  //   inline void put(int c, FILE* out) {putc(c, out);}

  // Write one byte c as a decimal number.
  void put(int c, NumberWriter*) {
    printf("%d,", c);
  }

  // write one byte c to a string.
  void put(int c, std::string* s) {
    (*s)+=char(c);
  }

  // Write one byte c to the bit bucket.
  void put(int c, Null*) {}

}

// This has to go after the definitions of get(), put(), and error().
// You can put "#define NDEBUG" first to turn off runtime error checking
// (assertions) for more speed.
#include "libzpaq.h"

// Read and list the contents of an archive
void list(const char* archive) {

  // Open archive
  FILE* in=fopen(archive, "rb");
  if (!in) {
    perror(archive);
    return;
  }
  printf("\n%s\n", archive);

  // Some variables to hold values read from the archive. An archive
  // is a sequence of independent blocks. Each block describes the
  // decompression algorithm in two strings called hcomp and pcomp.
  // Each block holds one or more segments that must be decompressed
  // in order from the start of the block. Each segment has an optional
  // filename string, and optional comment string, some compressed data,
  // and an optional 20 byte SHA-1 checksum of the data before compression.
  double memory;
  std::string filename, comment;
  char sha1string[21];

  // Create object d to decompress ZPAQ archives. Input will be read
  // from a FILE*. Decompressed output would go to a Null* which just
  // discards any output. Normally that would also be a FILE.
  libzpaq::Decompresser<FILE, Null> d;

  // Set the input to the archive
  d.setInput(in);

  // Search for the next block and return false when done.
  // If true, calculate memory required for decompression.
  for (int i=1; d.findBlock(&memory); ++i) {
    printf("Block %d needs %1.6f MB memory\n", i, memory/1e6);
    bool firstSegment=true;  // first segment in block?

    // Find the next segment in the block. If found, read the file
    // name from the segment header and write it to filename.
    // The argument can be any pointer type T* that defines put(int, T*).
    // If there are no more segments in the block, return false.
    while (d.findFilename(&filename)) {

      // Read the comment from the segment header (like filename).
      // There is no limit on how long the filename or comment might be.
      d.readComment(&comment);

      // If this is the first segment in the block, then print the
      // hcomp string. This string contains ZPAQL code which describes
      // the decompression algorithm. The argument can be any type T*
      // that defines put(int, T*). Here we just write the code as
      // a list of bytes as decimal numbers. The format is suitable
      // for passing to Compressor::startBlock(hcomp). The first 2
      // bytes is the length of the rest of the string in little-endian
      // (LSB, MSB) format. The maximum possible size is 65537.
      if (firstSegment) {
        printf("hcomp=");
        d.hcomp((NumberWriter*)0);

        // Decompress 0 bytes. We need to do this before getting the
        // pcomp string because it is compressed prior to the data in
        // the beginning of the first segment using the compression
        // algorithm described in hcomp. decompress(0) has the effect
        // of decompressing this string but stopping before decompressing
        // any data. On the other hand, decompress(n) would decompress
        // up to n bytes and return true if there is more data remaining.
        // d.decompress(-1) or d.decompress() would decompress the whole
        // setment and return false. The decompressed output would go
        // to the destination set by d.setOutput().
        d.decompress(0);

        // Print the pcomp string if present. pcomp describes a
        // postprocessing algorithm in ZPAQL code. It is optional.
        // If omitted, then the decompressed data is output directly.
        // pcomp(w) returns true if a string is present and writes
        // it to w, where w is any pointer of type T* where put(int, T*)
        // is defined. If no pcomp string is present, then it returns
        // false without writing anything. If present, the output
        // format is suitable for passing to Compressor::postProcess(pcomp).
        // The first 2 bytes of the string is the length of the rest
        // of the string in little-endian format. The maximum output
        // is 65537 bytes. Here we write the output as a list of
        // decimal numbers.
        printf("\npcomp=");
        if (!d.pcomp((NumberWriter*)0))
          printf("(empty)");
        printf("\n");
        firstSegment=false;
      }

      // Read the SHA-1 checksum at the end of the segment, skipping
      // any remaining compressed data. The checksum is optional.
      // If present, then a 1 will be written to sha1string[0]
      // and the 20 byte checksum will be written to sha1string[1] through
      // sha1string[20]. If there is no checksum, then a 0 will be
      // written to sha1string[0].
      d.readSegmentEnd(sha1string);
      printf("  ");
      if (sha1string[0]) {
        for (int i=1; i<21; ++i)
          printf("%02x", sha1string[i]&255);
      }
      else printf("                    ");

      // Write the filename and comment. We have to clear the
      // strings afterward because we defined put() to append bytes.
      printf(" %10s %s\n", comment.c_str(), filename.c_str());
      filename=comment="";
    }
  }
  fclose(in);
  printf("\n");
}

// List the contents of all archives on the command line.
// If there are no arguments, then print a help message.
int main(int argc, char** argv) {

  // Check args
  if (argc<2) {
    printf("For detailed ZPAQ archive listing: demo2 files...\n");
    exit(1);
  }

  // List archive contents
  for (int i=1; i<argc; ++i)
    list(argv[i]);

  return 0;
}
