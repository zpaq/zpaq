/* libzpaq.h 
LIBZPAQ Version 0.03
Written by Matt Mahoney, Oct. 14, 2010

LIBZPAQ is a C++ library for compression and decompression of data
conforming to the ZPAQ level 1 standard described in
http://mattmahoney.net/dc/zpaq1.pdf

The LIBZPAQ software is placed in the public domain. It may be used
without restriction. LIBZPAQ is provided "as is" with no warranty.

See accompanying libzpaq.txt for documentation.
*/

#ifndef LIBZPAQ_H
#define LIBZPAQ_H

#include <assert.h>
#include <stddef.h>

namespace libzpaq {

// 1, 2, 4 byte unsigned integers
typedef unsigned char U8;
typedef unsigned short U16;
typedef unsigned int U32;

// Standard library prototypes redirected to libzpaq.cpp
void* calloc(int, int);
void free(void*);

// Callback for error handling
extern void error(const char* msg);

// Virtual base classes for input and output
class Reader {
public:
  virtual int get() = 0;  // should return 0..255, or -1 at EOF
};

class Writer {
public:
  virtual void put(int c) = 0;  // should output low 8 bits of c
};

// Read 16 bit little-endian number
int toU16(const char* p);

// A list of headers for which optimizations are available
extern const char models[];

// An Array of T is cleared and aligned on a 64 byte address
//   with no constructors called. No copy or assignment.
// Array<T> a(n, ex=0);  - creates n<<ex elements of type T
// a[i] - index
// a(i) - index mod n, n must be a power of 2
// a.size() - gets n
template <typename T>
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
template<typename T>
void Array<T>::resize(int sz, int ex) {
  while (ex>0) {
    if (sz<0 || sz>=(1<<30)) error("Array too big");
    sz*=2, --ex;
  }
  if (sz<0) error("Array too big");
  if (n>-1) {
    assert(offset>0 && offset<=64);
    assert((char*)data-offset);
    free((char*)data-offset);
  }
  n=-1;
  if (sz<=0) return;
  n=sz-1;
  data=(T*)calloc(64+(n+1)*sizeof(T), 1);
  if (!data) error("Out of memory");
  offset=64-int((ptrdiff_t)data&63);
  assert(offset>0 && offset<=64);
  data=(T*)((char*)data+offset);
}

////////////////////// SHA1 ////////////////////

// For computing SHA-1 checksums
class SHA1 {
public:
  void put(int c) {  // hash 1 byte
    U32& r=w[len0>>5&15];
    r=r<<8|c&255;
    if (!(len0+=8)) ++len1;
    if ((len0&511)==0) process();
  }
  double size() const {return len0/8+len1*536870912.0;} // size in bytes
  const char* result();  // get hash and reset
  SHA1() {init();}
private:
  void init();      // reset, but don't clear hbuf
  U32 len0, len1;   // length in bits (low, high)
  U32 h[5];         // hash state
  U32 w[80];        // input buffer
  char hbuf[20];    // result
  void process();   // hash 1 block
};

//////////////////////////// ZPAQL //////////////////////////////

// Symbolic constants, instruction size, and names
typedef enum {NONE,CONS,CM,ICM,MATCH,AVG,MIX2,MIX,ISSE,SSE} CompType;
extern const int compsize[256];

// A ZPAQL machine COMP+HCOMP or PCOMP.
class ZPAQL {
public:
  ZPAQL();
  void clear();           // Free memory, erase program, reset machine state
  void inith();           // Initialize as HCOMP to run
  void initp();           // Initialize as PCOMP to run
  double memory();        // Return memory requirement in bytes
  void run(U32 input);    // Execute with input
  int read(Reader* in2);  // Read header
  bool write(Writer* out2);  // Write header, true unless empty PCOMP

  Writer* output;         // Destination for OUT instruction, or 0 to suppress
  SHA1* sha1;             // Points to checksum computer
  U32 H(int i) {return h(i);}  // get element of h

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
  void selectModel(); // Find optimized code
  void init(int hbits, int mbits);  // initialize H and M sizes
  int execute();  // execute 1 instruction, return 0 after HALT, else 1
  void run0(U32 input);  // default run() when select==0
  void div(U32 x) {if (x) a/=x; else a=0;}
  void mod(U32 x) {if (x) a%=x; else a=0;}
  void swap(U32& x) {a^=x; x^=a; a^=x;}
  void swap(U8& x)  {a^=x; x^=a; a^=x;}
  void err();  // exit with run time error
};

//////////////////////////// Component ////////////////////////////

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
  void init();    // initialize to all 0
  Component() {init();}
};

////////////////////////// StateTable //////////////////////////

// Next state table generator
class StateTable {
  enum {N=64}; // sizes of b, t
  U8 ns[1024]; // state*4 -> next state if 0, if 1, n0, n1
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

//////////////////////////// Predictor ////////////////////////////

// A predictor guesses the next bit
class Predictor {
public:
  Predictor(ZPAQL&);
  void init();          // build model
  int predict();        // probability that next bit is a 1 (0..4095)
  void update(int y);   // train on bit y (0..1)
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

////////////////////////////// Decoder ////////////////////////////

// Decoder decompresses using an arithmetic code
class Decoder {
public:
  Reader* in;  // destination
  Decoder(ZPAQL& z);
  int decompress();  // return a byte or EOF
  int skip();  // skip to the end of the segment, return next byte
  void init() {pr.init(); low=1; high=0xFFFFFFFF; curr=0;}
private:
  U32 low, high; // range
  U32 curr;  // last 4 bytes of archive
  Predictor pr;  // to get p
  int decode(int p); // return decoded bit (0..1) with prob. p (0..65535)
};

/////////////////////////// PostProcessor ////////////////////

class PostProcessor {
  int state;   // input parse state: 0=INIT, 1=PASS, 2..4=loading, 5=POST
  int hsize;   // header size
  int ph, pm;  // sizes of H and M in z
public:
  ZPAQL z;     // holds PCOMP
  PostProcessor(): state(0), hsize(0), ph(0), pm(0) {}
  void init(ZPAQL& hz);
  int write(int c);  // Input a byte, return state
  int getState() const {return state;}
};

//////////////////////////// Encoder ///////////////////////////////

// Encoder compresses using an arithmetic code
class Encoder {
public:
  Encoder(ZPAQL& z):
    out(0), low(1), high(0xFFFFFFFF), pr(z) {}
  void init();
  void compress(int c);  // c is 0..255 or EOF
  Writer* out;  // destination
private:
  U32 low, high; // range
  Predictor pr;  // to get p
  void encode(int y, int p); // encode bit y (0..1) with probability p (0..8191)
};

//////////////////////// Compressor /////////////////////////

class Compressor {
public:
  Compressor(): enc(z), in(0), state(INIT) {}
  void setOutput(Writer* out) {enc.out=out;}
  void writeTag();
  void startBlock(int level);  // level=1,2,3
  void startBlock(const char* hcomp);
  void startSegment(const char* filename = 0, const char* comment = 0);
  void setInput(Reader* i) {in=i;}
  void postProcess(const char* pcomp = 0);
  bool compress(int n = -1);  // n bytes, -1=all, return true until done
  void endSegment(const char* sha1string = 0);
  void endBlock();
private:
  ZPAQL z;
  Encoder enc;
  Reader* in;
  enum {INIT, BLOCK1, SEG1, BLOCK2, SEG2} state;
};

/////////////////////////// Decompresser //////////////////////////

// For decompression and listing archive contents
class Decompresser {
public:
  Decompresser(): z(), dec(z), pp(), state(INIT) {}
  void setInput(Reader* in) {dec.in=in;}
  bool findBlock(double* memptr = 0);
  void hcomp(Writer* out2) {z.write(out2);}
  bool findFilename(Writer* = 0);
  void readComment(Writer* = 0);
  void setOutput(Writer* out) {pp.z.output=out;}
  void setSHA1(SHA1* sha1ptr) {pp.z.sha1=sha1ptr;}
  bool decompress(int n = -1);  // n bytes, -1=all, return true until done
  bool pcomp(Writer* out2) {return pp.z.write(out2);}
  void readSegmentEnd(char* sha1string = 0);
private:
  ZPAQL z;
  Decoder dec;
  PostProcessor pp;
  enum {INIT, BLOCK, SEG1, SEG2, SEGEND, BLOCKSKIP, SEG1SKIP, SEG2SKIP} state;
};

/////////////////////////// compress() ///////////////////////

void compress(Reader* in, Writer* out, int level);

/////////////////////////// decompress() /////////////////////

void decompress(Reader* in, Writer* out);

}  // namespace libzpaq

#endif  // LIBZPAQ_H
