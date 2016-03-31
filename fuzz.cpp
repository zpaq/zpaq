#include "libzpaq.h"
#include <stdio.h>
#include <stdlib.h>

void libzpaq::error(const char* msg) {  // print message and exit
  fprintf(stderr, "Oops: %s\n", msg);
  exit (EXIT_SUCCESS);
}

class In: public libzpaq::Reader {
public:
  int get() {return getchar();}  // returns byte 0..255 or -1 at EOF
} in;

class Out: public libzpaq::Writer {
public:
  void put(int c) {putchar(c);}  // writes 1 byte 0..255
} out;

int main(int argc, char** argv) {
  bool compress = true;

  if (argc > 1) {
    if (strcmp (argv[1], "d") == 0)
      compress = false;
    else if (strcmp (argv[1], "c") == 0)
      compress = true;
    else {
      fprintf (stderr, "Invalid argument, must be 'c' or 'd' (default 'c')\n");
      exit (EXIT_FAILURE);
    }
  }

  if (compress)
    libzpaq::compress(&in, &out, "1");  // "0".."5" = faster..better
  else
    libzpaq::decompress(&in, &out);

  return EXIT_SUCCESS;
}
