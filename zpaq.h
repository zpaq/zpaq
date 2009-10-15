/*  header file for zpaq v1.08 archiver and file compressor.

(C) 2009, Ocarina Networks, Inc.
    Written by Matt Mahoney, matmahoney@yahoo.com, Oct. 13, 2009.

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

See zpaq.cpp source code for documentation.

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

const int LEVEL=1;  // ZPAQ level 0=experimental 1=final

// 1, 2, 4 byte unsigned integers
typedef unsigned char U8;
typedef unsigned short U16;
typedef unsigned int U32;

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
  int size() const {return n+1;}  // get size
  T& operator[](int i) {assert(n>=0 && i>=0 && U32(i)<=U32(n)); return data[i];}
  T& operator()(int i) {assert(n>=0 && (n&(n+1))==0); return data[i&n];}
};

// Change size to sz<<ex elements of 0
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

// The SHA1 class is used to compute segment checksums.
// SHA-1 code modified from RFC 3174.
// http://www.faqs.org/rfcs/rfc3174.html

enum
{
    shaSuccess = 0,
    shaNull,            /* Null pointer parameter */
    shaInputTooLong,    /* input data too long */
    shaStateError       /* called Input after Result */
};
const int SHA1HashSize=20;

class SHA1 {
  U32 Intermediate_Hash[SHA1HashSize/4]; /* Message Digest  */
  U32 Length_Low;            /* Message length in bits      */
  U32 Length_High;           /* Message length in bits      */
  int Message_Block_Index;   /* Index into message block array */
  U8 Message_Block[64];      /* 512-bit message blocks      */
  int Computed;              /* Is the digest computed?         */
  int Corrupted;             /* Is the message digest corrupted? */
  U8 result_buf[20];         // Place to put result
  void SHA1PadMessage();
  void SHA1ProcessMessageBlock();
  U32 SHA1CircularShift(int bits, U32 word) {
     return (((word) << (bits)) | ((word) >> (32-(bits))));
  }
  int SHA1Reset();   // Initalize
  int SHA1Input(const U8 *, unsigned int n);  // Hash n bytes
  int SHA1Result(U8 Message_Digest[SHA1HashSize]);  // Store result
public:
  SHA1() {SHA1Reset();}  // Begin hash
  void put(int c) {  // Hash 1 byte
    U8 ch=c;
    SHA1Input(&ch, 1);
  }
  int result(int i);  // Finish and return byte i (0..19) of SHA1 hash
};


// A Reader reads from a file or an array U8 p[n]
class Reader {
  FILE *in;
  const U8 *ptr;
  int len;
public:
  Reader(FILE *f): in(f), ptr(0), len(0) {}  // Read from file
  Reader(const U8 *p, int n): in(0), ptr(p), len(n) {}  // Read from p[n]
  int get() {  // return 1 byte or EOF
    if (in) return getc(in);
    else if (ptr && len) return --len, *ptr++;
    else return EOF;
  }
};

// A ZPAQL machine COMP+HCOMP or PCOMP.
class ZPAQL {
public:
  ZPAQL();
  void read(Reader r);    // Read header from archive or array
  void write(FILE* out);  // Write header to archive
  void inith();           // Initialize as HCOMP to run
  void initp();           // Initialize as PCOMP to run
  U32 H(int i) {return h(i);}  // get element of h
  void run(U32 input);    // Execute with input
  FILE* output;           // Destination for OUT instruction, or 0 to suppress
  SHA1* sha1;             // Points to checksum computer
  void step(U32 input, bool ishex); // Execute while displaying progress
  double memory();        // Return memory requirement in bytes

  // ZPAQ1 block header
  Array<U8> header;   // hsize[2] hh hm ph pm n COMP (guard) HCOMP (guard)
  int cend;           // COMP in header[7...cend-1]
  int hbegin, hend;   // HCOMP/PCOMP in header[hbegin...hend-1]
  int select;         // Which optimized version of run()? (default 0)

private:
  // Machine state for executing HCOMP
  Array<U8> m;        // memory array M for HCOMP
  Array<U32> h;       // hash array H for HCOMP
  Array<U32> r;       // 256 element register array
  U32 a, b, c, d;     // machine registers
  int f;              // condition flag
  int pc;             // program counter

  // Support code
  void init(int hbits, int mbits);  // initialize H and M sizes
  int execute();  // execute 1 instruction, return 0 after HALT, else 1
  void run0(U32 input);  // default run() when select==0
  void div(U32 x) {if (x) a/=x; else a=0;}
  void mod(U32 x) {if (x) a%=x; else a=0;}
  void swap(U32& x) {a^=x; x^=a; a^=x;}
  void swap(U8& x)  {a^=x; x^=a; a^=x;}
  void err();  // exit with run time error
};

// A Component is a context model, indirect context model, match model,
// fixed weight mixer, adaptive 2 input mixer without or with current
// partial byte as context, adaptive m input mixer (without or with),
// or SSE (without or with).

struct Component {
  int limit;      // max count for cm
  U32 cxt;        // saved context
  int a, b, c;    // multi-purpose variables
  Array<U32> cm;  // cm[cxt] -> p in bits 31..10, n in 9..0; MATCH index
  Array<U8> ht;   // ICM hash table[0..size1][0..15] of bit histories; MATCH buf
  Array<U16> a16; // multi-use
  Component();    // initialize to all 0
};

// Next state table generator
class StateTable {
  enum {B=6, N=64}; // sizes of b, t
  static U8 ns[1024]; // state*4 -> next state if 0, if 1, n0, n1
  static const int bound[B];  // n0 -> max n1, n1 -> max n0
  int num_states(int n0, int n1);  // compute t[n0][n1][1]
  void discount(int& n0);  // set new value of n0 after 1 or n1 after 0
  void next_state(int& n0, int& n1, int y);  // new (n0,n1) after bit y
public:
  int next(int state, int y) {  // next state for bit y
    assert(state>=0 && state<256);
    assert(y>=0 && y<4);
    return ns[state*4+y];
  }
  int cminit(int state) {  // initial probability of 1 * 2^23
    assert(state>=0 && state<256);
    return ((ns[state*4+3]*2+1)<<22)/(ns[state*4+2]+ns[state*4+3]+1);
  }
  StateTable();
};

// A predictor guesses the next bit
class Predictor {
public:
  Predictor(ZPAQL&);    // build model
  int predict();        // probability that next bit is a 1 (0..4095)
  void update(int y);   // train on bit y (0..1)
  void stat();          // print statistics
private:

  // Predictor state
  int c8;               // last 0...7 bits.
  int hmap4;            // c8 split into nibbles
  int p[256];           // predictions
  ZPAQL& z;             // VM to compute context hashes, includes H, n
  Component comp[256];  // the model, includes P

  // Modeling support functions
  int predict0();       // default
  int predict1();       // optimized
  void update0(int y);  // default
  void update1(int y);  // optimized
  int dt[1024];         // division table for cm: dt[i] = 2^16/(i+1.5)
  U16 squasht[4096];    // squash() lookup table
  short stretcht[32768];// stretch() lookup table
  StateTable st;        // next, cminit functions

  // reduce prediction error in cr.cm
  void train(Component& cr, int y) {
    assert(y==0 || y==1);
    U32& pn=cr.cm(cr.cxt);
    int count=pn&0x3ff;
    int error=y*32767-(cr.cm(cr.cxt)>>17);
    pn+=(error*dt[count]&-1024)+(count<cr.limit);
  }

  // x -> floor(32768/(1+exp(-x/64)))
  int squash(int x) {
    assert(x>=-2048 && x<=2047);
    return squasht[x+2048];
  }

  // x -> round(64*log((x+0.5)/(32767.5-x))), approx inverse of squash
  int stretch(int x) {
    assert(x>=0 && x<=32767);
    return stretcht[x];
  }

  // bound x to a 12 bit signed int
  int clamp2k(int x) {
    if (x<-2048) return -2048;
    else if (x>2047) return 2047;
    else return x;
  }

  // bound x to a 20 bit signed int
  int clamp512k(int x) {
    if (x<-(1<<19)) return -(1<<19);
    else if (x>=(1<<19)) return (1<<19)-1;
    else return x;
  }

  // Get cxt in ht, creating a new row if needed
  int find(Array<U8>& ht, int sizebits, U32 cxt);
};

// Globals for optimization
extern const char *pre_cmd;    // preprocessor command
extern const U8 *zlist;        // model header for COMP, HCOMP
extern const U8 *pzlist;       // postprocessor code
