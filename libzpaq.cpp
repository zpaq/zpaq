/* libzpaq.cpp

LIBZPAQ Version 0.02
Written by Matt Mahoney, Sept. 28, 2010

LIBZPAQ is a C++ library for compression and decompression of data
conforming to the ZPAQ level 1 standard described in
http://mattmahoney.net/dc/zpaq1.pdf
See accompanying libzpaq.txt for documentation.

The LIBZPAQ software is placed in the public domain. It may be used
without restriction. LIBZPAQ is provided "as is" with no warranty.

*/

#include "libzpaq.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

namespace libzpaq {

// Standard library redirections
void* calloc(int a, int b) {return ::calloc(a, b);}
void free(void* p) {::free(p);}
int memcmp(const void* d, const void* s, int n) {
  return ::memcmp(d, s, n);}
void* memset(void* d, int c, int n) {return ::memset(d, c, n);}
double log(double x) {return ::log(x);}
double exp(double x) {return ::exp(x);}
double pow(double x, double y) {return ::pow(x, y);}

// Memory reader and writer (no bounds check)
int get(const char** r) {
  return *(*r)++&255;
}

void put(int c, char** w) {
  *(*w)++ = c;
}

//////////////////////////// SHA1 ////////////////////////////

// SHA1 code, see http://en.wikipedia.org/wiki/SHA-1

// Start a new hash
void SHA1::init() {
  len0=len1=0;
  h[0]=0x67452301;
  h[1]=0xEFCDAB89;
  h[2]=0x98BADCFE;
  h[3]=0x10325476;
  h[4]=0xC3D2E1F0;
}

// Return old result and start a new hash
const char* SHA1::result() {

  // pad and append length
  const U32 s1=len1, s0=len0;
  put(0x80);
  while ((len0&511)!=448)
    put(0);
  put(s1>>24);
  put(s1>>16);
  put(s1>>8);
  put(s1);
  put(s0>>24);
  put(s0>>16);
  put(s0>>8);
  put(s0);

  // copy h to hbuf
  for (int i=0; i<5; ++i) {
    hbuf[4*i]=h[i]>>24;
    hbuf[4*i+1]=h[i]>>16;
    hbuf[4*i+2]=h[i]>>8;
    hbuf[4*i+3]=h[i];
  }

  // return hash prior to clearing state
  init();
  return hbuf;
}

// Hash 1 block of 64 bytes
void SHA1::process() {
  for (int i=16; i<80; ++i) {
    w[i]=w[i-3]^w[i-8]^w[i-14]^w[i-16];
    w[i]=w[i]<<1|w[i]>>31;
  }
  U32 a=h[0];
  U32 b=h[1];
  U32 c=h[2];
  U32 d=h[3];
  U32 e=h[4];
  for (int i=0; i<20; ++i) {
    const U32 f=b&c|~b&d, k=0x5A827999;
    const U32 t=(a<<5|a>>27)+f+e+k+w[i];
    e=d;
    d=c;
    c=b<<30|b>>2;
    b=a;
    a=t;
  }
  for (int i=20; i<40; ++i) {
    const U32 f=b^c^d, k=0x6ED9EBA1;
    const U32 t=(a<<5|a>>27)+f+e+k+w[i];
    e=d;
    d=c;
    c=b<<30|b>>2;
    b=a;
    a=t;
  }
  for (int i=40; i<60; ++i) {
    const U32 f=b&c|b&d|c&d, k=0x8F1BBCDC;
    const U32 t=(a<<5|a>>27)+f+e+k+w[i];
    e=d;
    d=c;
    c=b<<30|b>>2;
    b=a;
    a=t;
  }
  for (int i=60; i<80; ++i) {
    const U32 f=b^c^d, k=0xCA62C1D6;
    const U32 t=(a<<5|a>>27)+f+e+k+w[i];
    e=d;
    d=c;
    c=b<<30|b>>2;
    b=a;
    a=t;
  }
  h[0]+=a;
  h[1]+=b;
  h[2]+=c;
  h[3]+=d;
  h[4]+=e;
}

//////////////////////////// Component ///////////////////////

// A Component is a context model, indirect context model, match model,
// fixed weight mixer, adaptive 2 input mixer without or with current
// partial byte as context, adaptive m input mixer (without or with),
// or SSE (without or with).

const int compsize[256]={0,2,3,2,3,4,6,6,3,5};

void Component::init() {
  limit=cxt=a=b=c=0;
  cm.resize(0);
  ht.resize(0);
  a16.resize(0);
}

////////////////////////// StateTable //////////////////////////

// How many states with count of n0 zeros, n1 ones (0...2)
int StateTable::num_states(int n0, int n1) {
  const int B=6;
  const int bound[B]={20,48,15,8,6,5}; // n0 -> max n1, n1 -> max n0
  if (n0<n1) return num_states(n1, n0);
  if (n0<0 || n1<0 || n1>=B || n0>bound[n1]) return 0;
  return 1+(n1>0 && n0+n1<=17);
}

// New value of count n0 if 1 is observed (and vice versa)
void StateTable::discount(int& n0) {
  n0=(n0>=1)+(n0>=2)+(n0>=3)+(n0>=4)+(n0>=5)+(n0>=7)+(n0>=8);
}

// compute next n0,n1 (0 to N) given input y (0 or 1)
void StateTable::next_state(int& n0, int& n1, int y) {
  if (n0<n1)
    next_state(n1, n0, 1-y);
  else {
    if (y) {
      ++n1;
      discount(n0);
    }
    else {
      ++n0;
      discount(n1);
    }
    // 20,0,0 -> 20,0
    // 48,1,0 -> 48,1
    // 15,2,0 -> 8,1
    //  8,3,0 -> 6,2
    //  8,3,1 -> 5,3
    //  6,4,0 -> 5,3
    //  5,5,0 -> 5,4
    //  5,5,1 -> 4,5
    while (!num_states(n0, n1)) {
      if (n1<2) --n0;
      else {
        n0=(n0*(n1-1)+(n1/2))/n1;
        --n1;
      }
    }
  }
}

// Initialize next state table ns[state*4] -> next if 0, next if 1, n0, n1
StateTable::StateTable() {

  // Assign states by increasing priority
  const int N=50;
  U8 t[N][N][2]={{{0}}}; // (n0,n1,y) -> state number
  int state=0;
  for (int i=0; i<N; ++i) {
    for (int n1=0; n1<=i; ++n1) {
      int n0=i-n1;
      int n=num_states(n0, n1);
      assert(n>=0 && n<=2);
      if (n) {
        t[n0][n1][0]=state;
        t[n0][n1][1]=state+n-1;
        state+=n;
      }
    }
  }
       
  // Generate next state table
  memset(ns, 0, sizeof(ns));
  for (int n0=0; n0<N; ++n0) {
    for (int n1=0; n1<N; ++n1) {
      for (int y=0; y<num_states(n0, n1); ++y) {
        int s=t[n0][n1][y];
        assert(s>=0 && s<256);
        int s0=n0, s1=n1;
        next_state(s0, s1, 0);
        assert(s0>=0 && s0<N && s1>=0 && s1<N);
        ns[s*4+0]=t[s0][s1][0];
        s0=n0, s1=n1;
        next_state(s0, s1, 1);
        assert(s0>=0 && s0<N && s1>=0 && s1<N);
        ns[s*4+1]=t[s0][s1][1];
        ns[s*4+2]=n0;
        ns[s*4+3]=n1;
      }
    }
  }
}

//////////////////////// optimizations ////////////////////

// Optimization code can be generated by "zpaq oc" with various
// config files. Case labels and goto labels must be edited to remove
// duplicates.

// Read 16 bit little-endian number
int toU16(const char* p) {
  return (p[0]&255)+256*(p[1]&255);
}

// A list of headers for which optimizations are available
const char models[]={

  // fast.cfg
  26,0,1,2,0,0,2,3,16,8,19,0,0,
  // HCOMP
  96,4,28,
  59,10,59,112,25,10,59,10,59,112,56,0,

  // mid.cfg
  69,0,3,3,0,0,8,3,5,8,13,0,8,17,1,8, 
  18,2,8,18,3,8,19,4,4,22,24,7,16,0,7,24,
  255,0,
  // HCOMP
  17,104,74,4,95,1,59,112,10,25,59,112,10,25,59,112,
  10,25,59,112,10,25,59,112,10,25,59,10,59,112,25,69,
  207,8,112,56,0,

  // max.cfg
  196,0,5,9,0,0,22,1,160,3,5,8,13,1,8,16,
  2,8,18,3,8,19,4,8,19,5,8,20,6,4,22,24,
  3,17,8,19,9,3,13,3,13,3,13,3,14,7,16,0,
  15,24,255,7,8,0,16,10,255,6,0,15,16,24,0,9,
  8,17,32,255,6,8,17,18,16,255,9,16,19,32,255,6,
  0,19,20,16,0,0,
  // HCOMP
  17,104,74,4,95,2,59,112,10,25,
  59,112,10,25,59,112,10,25,59,112,10,25,59,112,10,25,
  59,10,59,112,10,25,59,112,10,25,69,183,32,239,64,47,
  14,231,91,47,10,25,60,26,48,134,151,20,112,63,9,70,
  223,0,39,3,25,112,26,52,25,25,74,10,4,59,112,25,
  10,4,59,112,25,10,4,59,112,25,65,143,212,72,4,59,
  112,8,143,216,8,68,175,60,60,25,69,207,9,112,25,25,
  25,25,25,112,56,0,

  // end of list
  0,0};

}  // end namespace libzpaq
