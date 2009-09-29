/* lzppre.cpp LZP preprocessor

(C) 2009, Ocarina Networks, Inc.
    Written by Matt Mahoney, matmahoney@yahoo.com, Sept. 28, 2009.

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

Usage: lzppre ph pm esc minlen hmul input output

lzppre preprocesses its input for compression. The inverse operation is
provided by the ZPAQL code in the comments below. Encoding is as follows.
The sequence (esc 0) codes for esc. The sequence (esc n) where n > 0 codes
for a match of length n+minlen from an output buffer of the last 2^pm
bytes after the last place that had the same rolling context hash.
The hash is computed from input byte c by hash=(hash*hmul+c)&(1<<ph)-1;
(i.e. the low ph bits).

This code is extracted from the zpaq 1.04 preprocessor. It is no longer
part of zpaq 1.05 or higher. zpaq 1.05 only uses external preprocessors.
To use, be sure lzppre.exe is in the current directory and run:

  zpaq cmin.cfg archive files...

to compress using this program to preprocess. The configuration file
min.cfg should contain the line:

  pcomp lzppre 18 20 5 3 40

if using the code below. Other values are allowed with corresponding
adjustments to the ZPAQL code copied to min.cfg.

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <math.h>
#include <assert.h>

// 1, 2, 4 byte unsigned integers
typedef unsigned char U8;
typedef unsigned short U16;
typedef unsigned int U32;

// Print an error message and exit
void error(const char* msg="") {
  fprintf(stderr, "\nError: %s\n", msg);
  exit(1);
}

// An Array of T is cleared and aligned on a 64 byte address
//   with no constructors called. No copy or assignment.
// Array<T> a(n, ex=0);  - creates n<<ex elements of type T
// a[i] - index
// a(i) - index mod n, n must be a power of 2
// a.size() - gets n
template <class T>
class Array {
private:
  T *data; // user location of [0] on a 64 byte boundary
  int n;   // user size-1
  int offset;  // distance back in bytes to start of actual allocation
  void operator=(const Array&);  // no assignment
  Array(const Array&);  // no copy
public:
  Array(int sz=0, int ex=0): data(0), n(-1), offset(0) {
    resize(sz, ex);} // [0..sz-1] = 0
  void resize(int sz, int ex=0); // change size, erase content to zeros
  ~Array() {resize(0);}  // free memory
  int size() {return n+1;}  // get size
  T& operator[](int i) {assert(n>=0 && i>=0 && U32(i)<=U32(n)); return data[i];}
  T& operator()(int i) {assert(n>=0 && (n&(n+1))==0); return data[i&n];}
};

template<class T>
void Array<T>::resize(int sz, int ex) {
  while (ex>0) {
    if (sz<0 || sz>=(1<<30)) fprintf(stderr, "Array too big\n"), exit(1);
    sz*=2, --ex;
  }
  if (sz<0) fprintf(stderr, "Array too big\n"), exit(1);
  if (n>-1) {
    assert(offset>0 && offset<=64);
    assert((char*)data-offset);
    free((char*)data-offset);
  }
  n=-1;
  if (sz<=0) return;
  n=sz-1;
  data=(T*)calloc(64+(n+1)*sizeof(T), 1);
  if (!data) fprintf(stderr, "Out of memory\n"), exit(1);
  offset=64-int((long)data&63);
  assert(offset>0 && offset<=64);
  data=(T*)((char*)data+offset);
}

//////////////////////////// PreProcessor ////////////////////////

const U32 EOS=U32(-1);

class PreProcessor {
  FILE *out;  // output
  int ph, pm; // memory sizes for H, M from config file
  int esc, minlen, hmul;  // lzp parameters
  int state;  // 0 = init, 1 = after
  U32 b, c, d;  // general purpose state for transforms
  Array<U8> m;
  Array<U32> h;
  void lzp(U32 a);  // (p) LZP transform
  void lzp_flush(); // used by lzp()
public:
  PreProcessor(FILE *f, int e, int m_, int h_, int ph_, int pm_);
  void compress(U32 a);
};

PreProcessor::PreProcessor(FILE *f, int e, int m_, int h_, int ph_, int pm_):
    out(f), ph(ph_), pm(pm_), esc(e), minlen(m_), hmul(h_) {
  state=0;
  b=c=d=0;
  m.resize(1, pm);
  h.resize(1, ph);
}

// LZP preprocessor. Strings of length (minlen+len) that match the
// last occurrence occuring in the same context hash within 2^pm
// are replaced with the 2 byte sequence (esc len) where len=(1...255).
// The byte esc is replaced with (esc 0). The context hash is updated
// by byte A by hash = hash*hmul+A mod 2^ph.
void PreProcessor::lzp(U32 a) {
  // State is as follows:
  // F = is last byte ESC?
  // D = context hash
  // B = number of bytes output
  // M = output buffer[0...B-1], size 2^pm
  // C = pointer to match in M, C < B
  // H = index mapping D to last match in M, size 2^ph */

/*
  (ZPAQL code for LZP inverse transform with ESC=5, MINLEN=3, HMUL=40)
  jf 30 (last byte was esc?)
    a> 0
      jf 37 (goto output esc)
    a+= 3 r=a 0
      c=*d
        *d=b (top of copy loop)
        a=*c *b=a b++ c++
        out
        d<>a a*= 40 a+=d d<>a
        a=r 0 a-- r=a 0
      a> 0 jt -20 (to top of copy loop)
    halt
  a== 5 jf 1
    halt
  a> 255 jf 4
    a<a halt (F=0)

(output esc)
  a= 5 
(output:)
  *d=b
  *b=a b++
  out
  d<>a a*= 40 a+=d d<>a
  halt


  static const U8 prog[59]={  // compiled from above
    1,56,0,47,30,239,0,47,37,135,0,55,0,86,113,69,96,9,
    17,57,24,151,0,131,24,7,0,2,55,0,239,0,39,236,56,223,0,
    47,1,56,239,255,47,4,224,56,71,0,113,96,9,57,24,151,0,131,
    24,56,0};
  if (state==0) {
    for (int i=0; i<59; ++i) {
      if (i==36 || i==47) encp->compress(ESC);
      else if (i==10) encp->compress(MINLEN);
      else if (i==22 || i==54) encp->compress(HMUL);
      else encp->compress(prog[i]);
    }
    state=1;
  }
*/
  // Forward transform uses similar state information:
  // b = number of bytes input
  // c = number of bytes output
  // d = hash of context at c
  // m = buffer with pending output at m(c...b-1)
  // h = index of context hashes h(d) -> m(0...c-1)

  if (a==EOS) {
    while (b!=c)
      lzp_flush();
  }
  else {
    m(b++)=a;
    if (b>256+minlen+c || b==(1<<pm)+c)
      lzp_flush();
  }
}

void PreProcessor::lzp_flush() {
  assert(c!=b);

  // Look for a match
  int len=0;
  U32 p=h(d);
  if (c-p>0 && c-p+258+minlen<U32(1<<pm))
    while (len<255+minlen && m(p+len)==m(c+len) && c+len!=b)
      ++len;
  if (len>minlen) {
    putc(esc, out);
    putc(len-minlen, out);
    while (len-->0) {
      assert(c!=b);
      h(d)=c;
      d=d*hmul+m(c++);
    }
  }

  // Encode a literal
  else {
    putc(m(c), out);
    if (m(c)==esc)
      putc(0, out);
    h(d)=c;
    d=d*hmul+m(c++);
  }
}

// Compress one byte (0...255) or EOS
void PreProcessor::compress(U32 a) {
 lzp(a);
}

int main(int argc, char** argv) {
  if (argc<8)
    printf("Usage: lzppre ph pm esc minlen hmul input output\n"), exit(1);
  FILE *in=fopen(argv[6], "rb");
  if (!in) perror(argv[6]), exit(1);
  FILE *out=fopen(argv[7], "wb");
  if (!out) perror(argv[7]), exit(1);
  int ph=atoi(argv[1]);
  int pm=atoi(argv[2]);
  int esc=atoi(argv[3]);
  int minlen=atoi(argv[4]);
  int hmul=atoi(argv[5]);
  PreProcessor pp(out, esc, minlen, hmul, ph, pm);
  int c;
  while ((c=getc(in))!=EOF)
    pp.compress(c);
  pp.compress(EOS);
  return 0;
}
