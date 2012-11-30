/* bwtrle.cpp v1.0 - Computes Burrows-Wheeler transform with optional
   run length encoding, and inverse.

(C) 2011, Dell Inc. Written by Matt Mahoney.

    LICENSE

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 3 of
    the License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful, but
    WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    General Public License for more details at
    Visit <http://www.gnu.org/copyleft/gpl.html>.

Usage: bwtrle [c|d|e|f] input output

  c = forward BWT without RLE
  d = inverse BWT without RLE
  e = forward BWT with RLE
  f = inverse BWT with RLE

The forward transform creates a single block. It
requires 5 times the file size in memory and fails if the
file is too big. The output format is the BWT with the
virtual EOF replaced with 255. The 4 byte index pointing to
its location is appended to the end in little-endian
(LSB first) format. The inverse transform assumes the
same format.

If RLE encoding is selected, then runs of 2 to 257 bytes
are encoded as the first 2 bytes and a count, for example,
x,x,x,x,x -> x,x,3.

To compile: g++ -O3 bwt.cpp divsufsort.c -o bwt

You need divsufsort.c and divsufsort.h from
libdivsufsort-lite from http://code.google.com/p/libdivsufsort/
See source for license.

*/

#include "divsufsort.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Read n bytes of BWT and invert to out. buf should be allocated
// to 5*n bytes with the BWT string in the first n bytes.
// idx is original index of NOT removed end of block symbol.
void ibwt(unsigned char* buf, FILE* out, size_t n, size_t idx) {
  unsigned int count[256]={0};
  unsigned int* list=(unsigned int*)(buf+n);

  // Count bytes
  for (size_t i=0; i<n; ++i)
    ++count[(buf[i]+1)&255];

  // Cumulative counts including EOF
  count[0]=1;
  for (size_t i=1; i<256; ++i)
    count[i]+=count[i-1];

  // Build linked list including EOF at idx
  for (size_t i=0; i<idx; ++i)
    list[count[buf[i]]++]=i;
  for (size_t i=idx+1; i<n; ++i)
    list[count[buf[i]]++]=i;

  // Scan linked list and output
  for (size_t p=idx; p;) {
    p=list[p];
    putc(buf[p], out);
  }
}

// Encode n copies of c to out
void putn(int c, int n, FILE* out) {
  const int MIN=2;
  while (n>0) {
    for (int i=0; i<n && i<MIN; ++i) putc(c, out);
    if (n<MIN) return;
    n-=MIN;
    if (n>255) putc(255, out), n-=255;
    else putc(n, out), n=0;
  }
}

// RLE decode from in to buf and return output size.
// If buf is 0, then return size only.
// RLE: Decode X X n as n+2 copies of X.
long rledecode(FILE* in, unsigned char* buf) {
  int state=0, c=0;
  long n=0;
  while ((c=getc(in))!=EOF) {
    if (state==0) {
      if (buf) buf[n]=c;
      ++n;
      state=c+1;
    }
    else if (state<=256) {
      if (buf) buf[n]=c;
      ++n;
      state=c+1+256*(c==state-1);
    }
    else {
      while (c-->0) {
        if (buf) buf[n]=state-257;
        ++n;
      }
      state=0;
    }
  }
  return n;
}

int main(int argc, char** argv) {

  // check args
  if (argc<2) {
    fprintf(stderr, "BWT + RLE transform and inverse\n"
      "Usage: bwtrle c|d|e|f input [output]\n"
      "  c,d = forward, inverse BWT without RLE\n"
      "  e,f = forward, inverse BWT with RLE\n");
    return 1;
  }

  // Open files
  FILE* in=stdin;
  FILE* out=stdout;
  if (argc>2 && (in=fopen(argv[2], "rb"))==0)
    return perror(argv[2]), 1;
  if (argc>3 && (out=fopen(argv[3], "wb"))==0)
    return perror(argv[3]), 1;
  const int cmd=argv[1][0];

  // Read input into buf and get block size n
  long n=0;
  if (cmd=='f')
    n=rledecode(in, 0);
  else {
    fseek(in, 0, SEEK_END);
    n=ftell(in);  // input size
  }
  rewind(in);
  unsigned char* buf=(unsigned char*)malloc(n*5+5);
  if (!buf) return fprintf(stderr, "Out of memory\n"), 1;
  if (cmd=='f')
    rledecode(in, buf);
  else if ((long)fread(buf, 1, n, in)!=n)
    return fprintf(stderr, "Read error: size %ld\n", n), 1;

  // Forward BWT
  if (cmd=='c' || cmd=='e') {
    int idx=divbwt(buf, buf, (int*)(buf+n), n);
    if (n>idx) memmove(buf+idx+1, buf+idx, n-idx);
    buf[idx]=255;
    for (int i=0; i<4; ++i) buf[n+i+1]=idx>>(i*8);
    if (cmd=='c')
      fwrite(buf, 1, n+5, out);
    else {  // RLE encode
      int c, c1=0;  // this, last char
      int ct=0;  // pending output count of c1
      for (int i=0; i<n+5; ++i) {
        c=buf[i];
        if (ct>0 && c==c1)
          ++ct;
        else {
          putn(c1, ct, out);
          c1=c;
          ct=1;
        }
      }
      putn(c1, ct, out);
    }
  }

  // Inverse BWT
  else if (cmd=='d' || cmd=='f') {
    n-=4;
    if (n<0)
      return fprintf(stderr, "Input too small n=%ld\n", n), 1;
    int idx=buf[n]|buf[n+1]<<8|buf[n+2]<<16|buf[n+3]<<24;
    ibwt(buf, out, n, idx);
  }

  // Clean up
  free(buf);
  if (out!=stdout) fclose(out);
  if (in!=stdin) fclose(in);
  return 0;
}

