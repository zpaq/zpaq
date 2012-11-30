/* unzpaq200.cpp - ZPAQ level 2 reference decoder version 2.00.

  Copyright (C) 2012, Dell Inc. Written by Matt Mahoney.

  Permission is hereby granted, free of charge, to any person obtaining a
  copy of this software and associated documentation files (the "Software"),
  to deal in the Software without restriction, including without limitation
  the rights to use, copy, modify, merge, publish, distribute, sublicense,
  and/or sell copies of the Software, and to permit persons to whom the
  Software is furnished to do so without restriction.
  This Software is provided "as is" without warranty.

This program is the reference decoder for the ZPAQ level 2 standard.
It does not require any special options to compile other than usual
optimizations. It is assumed that 'int' is a 32 bit type.
On older compilers that don't support <stdint.h>,
compile with -DNO_STDINT. For example:

  g++ -O3 unzpaq200.cpp -o unzpaq200        (GNU g++)
  cl /O2 unzpaq200.cpp                      (Microsoft C++)
  dmc -o -6 unzpaq200.cpp                   (Digital Mars)
  bcc32 -O -6 -DNO_STDINT unzpaq200.cpp     (Borland v5.5)

To run:

  unzpaq200 [input [output]]

If input and output file names are omitted, then they default to
standard input and standard output. This will only work in operating
systems that support binary I/O (e.g. Linux but not Windows).

The input file may be any ZPAQ level 1 or level 2 compliant archive.
Any stored filenames and comments are printed to stderr in the format:

  filename comment -> size

but are otherwise ignored. "size" is the number of bytes written.
Normally the filename field would suggest the name of the output file
(or to append to the previous file if empty), but the ZPAQ standard does
not require this.

Any non-ZPAQ input is ignored and will result in an empty output file.
However, if any ZPAQ input is present, then it must be correctly
formatted or else the program will exit with an error message.
If checksums are stored, then they must match.

The ZPAQ standard does not define a compression algorithm. Thus,
this program only decompresses.

ZPAQ defines a standard for arbitrary streams of bytes, not just files.
Thus, it is valid to override the followng methods to read or write
other data structures such as strings or arrays:

  int Reader::get();  // return a byte in the range 0..255, or -1 at EOF.
  void Writer::put(int c);  // write the low 8 bits of c.

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>

// 1, 2, 4, 8 byte unsigned integers

#ifdef NO_STDINT
typedef unsigned char U8;
typedef unsigned short U16;
typedef unsigned int U32;
typedef unsigned __int64 U64;
#else
#include <stdint.h>
typedef uint8_t U8;
typedef uint16_t U16;
typedef uint32_t U32;
typedef uint64_t U64;
#endif

// Callback for user defined ZPAQ error handling.
// It will be called on input error with an English language error message.
// This function should not return.
extern void error(const char* msg);

// Virtual base classes for input and output
// get() and put() must be overridden to read or write 1 byte.
class Reader {
public:
  virtual int get() = 0;  // should return 0..255, or -1 at EOF
  virtual ~Reader() {}
};

class Writer {
public:
  virtual void put(int c) = 0;  // should output low 8 bits of c
  virtual ~Writer() {}
};

// An Array of T is cleared and aligned on a 64 byte address
//   with no constructors called. No copy or assignment.
// Array<T> a(n, ex=0);  - creates n<<ex elements of type T
// a[i] - index
// a(i) - index mod n, n must be a power of 2
// a.size() - gets n
template <typename T>
class Array {
  T *data;     // user location of [0] on a 64 byte boundary
  size_t n;    // user size
  int offset;  // distance back in bytes to start of actual allocation
  void operator=(const Array&);  // no assignment
  Array(const Array&);  // no copy
public:
  Array(size_t sz=0, int ex=0): data(0), n(0), offset(0) {
    resize(sz, ex);} // [0..sz-1] = 0
  void resize(size_t sz, int ex=0); // change size, erase content to zeros
  ~Array() {resize(0);}  // free memory
  size_t size() const {return n;}  // get size
  int isize() const {return int(n);}  // get size as an int
  T& operator[](size_t i) {assert(n>0 && i<n); return data[i];}
  T& operator()(size_t i) {assert(n>0 && (n&(n-1))==0); return data[i&(n-1)];}
};

// Change size to sz<<ex elements of 0
template<typename T>
void Array<T>::resize(size_t sz, int ex) {
  assert(size_t(-1)>0);  // unsigned type?
  while (ex>0) {
    if (sz>sz*2) error("Array too big");
    sz*=2, --ex;
  }
  if (n>0) {
    assert(offset>=0 && offset<=64);
    assert((char*)data-offset);
    free((char*)data-offset);
  }
  n=0;
  if (sz==0) return;
  n=sz;
  const size_t nb=128+n*sizeof(T);  // test for overflow
  if (nb<=128 || (nb-128)/sizeof(T)!=n) error("Array too big");
  data=(T*)calloc(nb, 1);
  if (!data) error("Out of memory");

  // Align array on a 64 byte address.
  // This optimization is NOT required by the ZPAQ standard.
  offset=64-(((char*)data-(char*)0)&63);
  assert(offset>0 && offset<=64);
  data=(T*)((char*)data+offset);
}

//////////////////////////// SHA1 ////////////////////////////

// For computing SHA-1 checksums
class SHA1 {
public:
  void put(int c) {  // hash 1 byte
    U32& r=w[len0>>5&15];
    r=(r<<8)|(c&255);
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
  const U32 k1=0x5A827999, k2=0x6ED9EBA1, k3=0x8F1BBCDC, k4=0xCA62C1D6;
#define f1(a,b,c,d,e,i) e+=(a<<5|a>>27)+((b&c)|(~b&d))+k1+w[i]; b=b<<30|b>>2;
#define f5(i) f1(a,b,c,d,e,i) f1(e,a,b,c,d,i+1) f1(d,e,a,b,c,i+2) \
              f1(c,d,e,a,b,i+3) f1(b,c,d,e,a,i+4)
  f5(0) f5(5) f5(10) f5(15)
#undef f1
#define f1(a,b,c,d,e,i) e+=(a<<5|a>>27)+(b^c^d)+k2+w[i]; b=b<<30|b>>2;
  f5(20) f5(25) f5(30) f5(35)
#undef f1
#define f1(a,b,c,d,e,i) e+=(a<<5|a>>27)+((b&c)|(b&d)|(c&d))+k3+w[i]; \
        b=b<<30|b>>2;
  f5(40) f5(45) f5(50) f5(55)
#undef f1
#define f1(a,b,c,d,e,i) e+=(a<<5|a>>27)+(b^c^d)+k4+w[i]; b=b<<30|b>>2;
  f5(60) f5(65) f5(70) f5(75)
#undef f1
#undef f5
  h[0]+=a;
  h[1]+=b;
  h[2]+=c;
  h[3]+=d;
  h[4]+=e;
}

//////////////////////////// ZPAQL ///////////////////////////

// Symbolic instructions and their sizes
typedef enum {NONE,CONS,CM,ICM,MATCH,AVG,MIX2,MIX,ISSE,SSE} CompType;
const int compsize[256]={0,2,3,2,3,4,6,6,3,5};

// A ZPAQL virtual machine COMP+HCOMP or PCOMP.
class ZPAQL {
public:
  ZPAQL();
  void clear();           // Free memory, erase program, reset machine state
  void inith();           // Initialize as HCOMP to run
  void initp();           // Initialize as PCOMP to run
  void run(U32 input);    // Execute with input
  int read(Reader* in2);  // Read header
  void outc(int c);       // output a byte

  Writer* output;         // Destination for OUT instruction, or 0 to suppress
  SHA1* sha1;             // Points to checksum computer
  U32 H(int i) {return h(i);}  // get element of h

  // ZPAQL block header
  Array<U8> header;   // hsize[2] hh hm ph pm n COMP (guard) HCOMP (guard)
  int cend;           // COMP in header[7...cend-1]
  int hbegin, hend;   // HCOMP/PCOMP in header[hbegin...hend-1]

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
  void div(U32 x) {if (x) a/=x; else a=0;}
  void mod(U32 x) {if (x) a%=x; else a=0;}
  void swap(U32& x) {a^=x; x^=a; a^=x;}
  void swap(U8& x)  {a^=x; x^=a; a^=x;}
  void err();  // exit with run time error
};

// Read header from in2
int ZPAQL::read(Reader* in2) {

  // Get header size and allocate
  int hsize=in2->get();
  hsize+=in2->get()*256;
  header.resize(hsize+300);
  cend=hbegin=hend=0;
  header[cend++]=hsize&255;
  header[cend++]=hsize>>8;
  while (cend<7) header[cend++]=in2->get(); // hh hm ph pm n

  // Read COMP
  int n=header[cend-1];
  for (int i=0; i<n; ++i) {
    int type=in2->get();  // component type
    if (type==-1) error("unexpected end of file");
    header[cend++]=type;  // component type
    int size=compsize[type];
    if (size<1) error("Invalid component type");
    if (cend+size>header.isize()-8) error("COMP list too big");
    for (int j=1; j<size; ++j)
      header[cend++]=in2->get();
  }
  if ((header[cend++]=in2->get())!=0) error("missing COMP END");

  // Insert a guard gap and read HCOMP
  hbegin=hend=cend+128;
  while (hend<hsize+129) {
    assert(hend<header.isize()-8);
    int op=in2->get();
    if (op==-1) error("unexpected end of file");
    header[hend++]=op;
  }
  if ((header[hend++]=in2->get())!=0) error("missing HCOMP END");
  assert(cend>=7 && cend<header.isize());
  assert(hbegin==cend+128 && hbegin<header.isize());
  assert(hend>hbegin && hend<header.isize());
  assert(hsize==header[0]+256*header[1]);
  assert(hsize==cend-2+hend-hbegin);
  return cend+hend-hbegin;
}

// Free memory, but preserve output, sha1 pointers
void ZPAQL::clear() {
  cend=hbegin=hend=0;  // COMP and HCOMP locations
  a=b=c=d=f=pc=0;      // machine state
  header.resize(0);
  h.resize(0);
  m.resize(0);
  r.resize(0);
}

// Constructor
ZPAQL::ZPAQL() {
  output=0;
  sha1=0;
  clear();
}

// Initialize machine state as HCOMP
void ZPAQL::inith() {
  assert(header.isize()>6);
  assert(output==0);
  assert(sha1==0);
  init(header[2], header[3]); // hh, hm
}

// Initialize machine state as PCOMP
void ZPAQL::initp() {
  assert(header.isize()>6);
  init(header[4], header[5]); // ph, pm
}

// Initialize machine state to run a program.
void ZPAQL::init(int hbits, int mbits) {
  assert(header.isize()>0);
  assert(cend>=7);
  assert(hbegin>=cend+128);
  assert(hend>=hbegin);
  assert(hend<header.isize()-130);
  assert(header[0]+256*header[1]==cend-2+hend-hbegin);
  h.resize(1, hbits);
  m.resize(1, mbits);
  r.resize(256);
  a=b=c=d=pc=f=0;
}

// Run program on input by interpreting header
void ZPAQL::run(U32 input) {
  assert(cend>6);
  assert(hbegin>=cend+128);
  assert(hend>=hbegin);
  assert(hend<header.isize()-130);
  assert(m.size()>0);
  assert(h.size()>0);
  assert(header[0]+256*header[1]==cend+hend-hbegin-2);
  pc=hbegin;
  a=input;
  while (execute()) ;
}

void ZPAQL::outc(int c) {
  if (output) output->put(c);
  if (sha1) sha1->put(c);
}

// Execute one instruction, return 0 after HALT else 1
int ZPAQL::execute() {
  switch(header[pc++]) {
    case 0: err(); break; // ERROR
    case 1: ++a; break; // A++
    case 2: --a; break; // A--
    case 3: a = ~a; break; // A!
    case 4: a = 0; break; // A=0
    case 7: a = r[header[pc++]]; break; // A=R N
    case 8: swap(b); break; // B<>A
    case 9: ++b; break; // B++
    case 10: --b; break; // B--
    case 11: b = ~b; break; // B!
    case 12: b = 0; break; // B=0
    case 15: b = r[header[pc++]]; break; // B=R N
    case 16: swap(c); break; // C<>A
    case 17: ++c; break; // C++
    case 18: --c; break; // C--
    case 19: c = ~c; break; // C!
    case 20: c = 0; break; // C=0
    case 23: c = r[header[pc++]]; break; // C=R N
    case 24: swap(d); break; // D<>A
    case 25: ++d; break; // D++
    case 26: --d; break; // D--
    case 27: d = ~d; break; // D!
    case 28: d = 0; break; // D=0
    case 31: d = r[header[pc++]]; break; // D=R N
    case 32: swap(m(b)); break; // *B<>A
    case 33: ++m(b); break; // *B++
    case 34: --m(b); break; // *B--
    case 35: m(b) = ~m(b); break; // *B!
    case 36: m(b) = 0; break; // *B=0
    case 39: if (f) pc+=((header[pc]+128)&255)-127; else ++pc; break; // JT N
    case 40: swap(m(c)); break; // *C<>A
    case 41: ++m(c); break; // *C++
    case 42: --m(c); break; // *C--
    case 43: m(c) = ~m(c); break; // *C!
    case 44: m(c) = 0; break; // *C=0
    case 47: if (!f) pc+=((header[pc]+128)&255)-127; else ++pc; break; // JF N
    case 48: swap(h(d)); break; // *D<>A
    case 49: ++h(d); break; // *D++
    case 50: --h(d); break; // *D--
    case 51: h(d) = ~h(d); break; // *D!
    case 52: h(d) = 0; break; // *D=0
    case 55: r[header[pc++]] = a; break; // R=A N
    case 56: return 0  ; // HALT
    case 57: outc(a&255); break; // OUT
    case 59: a = (a+m(b)+512)*773; break; // HASH
    case 60: h(d) = (h(d)+a+512)*773; break; // HASHD
    case 63: pc+=((header[pc]+128)&255)-127; break; // JMP N
    case 64: a = a; break; // A=A
    case 65: a = b; break; // A=B
    case 66: a = c; break; // A=C
    case 67: a = d; break; // A=D
    case 68: a = m(b); break; // A=*B
    case 69: a = m(c); break; // A=*C
    case 70: a = h(d); break; // A=*D
    case 71: a = header[pc++]; break; // A= N
    case 72: b = a; break; // B=A
    case 73: b = b; break; // B=B
    case 74: b = c; break; // B=C
    case 75: b = d; break; // B=D
    case 76: b = m(b); break; // B=*B
    case 77: b = m(c); break; // B=*C
    case 78: b = h(d); break; // B=*D
    case 79: b = header[pc++]; break; // B= N
    case 80: c = a; break; // C=A
    case 81: c = b; break; // C=B
    case 82: c = c; break; // C=C
    case 83: c = d; break; // C=D
    case 84: c = m(b); break; // C=*B
    case 85: c = m(c); break; // C=*C
    case 86: c = h(d); break; // C=*D
    case 87: c = header[pc++]; break; // C= N
    case 88: d = a; break; // D=A
    case 89: d = b; break; // D=B
    case 90: d = c; break; // D=C
    case 91: d = d; break; // D=D
    case 92: d = m(b); break; // D=*B
    case 93: d = m(c); break; // D=*C
    case 94: d = h(d); break; // D=*D
    case 95: d = header[pc++]; break; // D= N
    case 96: m(b) = a; break; // *B=A
    case 97: m(b) = b; break; // *B=B
    case 98: m(b) = c; break; // *B=C
    case 99: m(b) = d; break; // *B=D
    case 100: m(b) = m(b); break; // *B=*B
    case 101: m(b) = m(c); break; // *B=*C
    case 102: m(b) = h(d); break; // *B=*D
    case 103: m(b) = header[pc++]; break; // *B= N
    case 104: m(c) = a; break; // *C=A
    case 105: m(c) = b; break; // *C=B
    case 106: m(c) = c; break; // *C=C
    case 107: m(c) = d; break; // *C=D
    case 108: m(c) = m(b); break; // *C=*B
    case 109: m(c) = m(c); break; // *C=*C
    case 110: m(c) = h(d); break; // *C=*D
    case 111: m(c) = header[pc++]; break; // *C= N
    case 112: h(d) = a; break; // *D=A
    case 113: h(d) = b; break; // *D=B
    case 114: h(d) = c; break; // *D=C
    case 115: h(d) = d; break; // *D=D
    case 116: h(d) = m(b); break; // *D=*B
    case 117: h(d) = m(c); break; // *D=*C
    case 118: h(d) = h(d); break; // *D=*D
    case 119: h(d) = header[pc++]; break; // *D= N
    case 128: a += a; break; // A+=A
    case 129: a += b; break; // A+=B
    case 130: a += c; break; // A+=C
    case 131: a += d; break; // A+=D
    case 132: a += m(b); break; // A+=*B
    case 133: a += m(c); break; // A+=*C
    case 134: a += h(d); break; // A+=*D
    case 135: a += header[pc++]; break; // A+= N
    case 136: a -= a; break; // A-=A
    case 137: a -= b; break; // A-=B
    case 138: a -= c; break; // A-=C
    case 139: a -= d; break; // A-=D
    case 140: a -= m(b); break; // A-=*B
    case 141: a -= m(c); break; // A-=*C
    case 142: a -= h(d); break; // A-=*D
    case 143: a -= header[pc++]; break; // A-= N
    case 144: a *= a; break; // A*=A
    case 145: a *= b; break; // A*=B
    case 146: a *= c; break; // A*=C
    case 147: a *= d; break; // A*=D
    case 148: a *= m(b); break; // A*=*B
    case 149: a *= m(c); break; // A*=*C
    case 150: a *= h(d); break; // A*=*D
    case 151: a *= header[pc++]; break; // A*= N
    case 152: div(a); break; // A/=A
    case 153: div(b); break; // A/=B
    case 154: div(c); break; // A/=C
    case 155: div(d); break; // A/=D
    case 156: div(m(b)); break; // A/=*B
    case 157: div(m(c)); break; // A/=*C
    case 158: div(h(d)); break; // A/=*D
    case 159: div(header[pc++]); break; // A/= N
    case 160: mod(a); break; // A%=A
    case 161: mod(b); break; // A%=B
    case 162: mod(c); break; // A%=C
    case 163: mod(d); break; // A%=D
    case 164: mod(m(b)); break; // A%=*B
    case 165: mod(m(c)); break; // A%=*C
    case 166: mod(h(d)); break; // A%=*D
    case 167: mod(header[pc++]); break; // A%= N
    case 168: a &= a; break; // A&=A
    case 169: a &= b; break; // A&=B
    case 170: a &= c; break; // A&=C
    case 171: a &= d; break; // A&=D
    case 172: a &= m(b); break; // A&=*B
    case 173: a &= m(c); break; // A&=*C
    case 174: a &= h(d); break; // A&=*D
    case 175: a &= header[pc++]; break; // A&= N
    case 176: a &= ~ a; break; // A&~A
    case 177: a &= ~ b; break; // A&~B
    case 178: a &= ~ c; break; // A&~C
    case 179: a &= ~ d; break; // A&~D
    case 180: a &= ~ m(b); break; // A&~*B
    case 181: a &= ~ m(c); break; // A&~*C
    case 182: a &= ~ h(d); break; // A&~*D
    case 183: a &= ~ header[pc++]; break; // A&~ N
    case 184: a |= a; break; // A|=A
    case 185: a |= b; break; // A|=B
    case 186: a |= c; break; // A|=C
    case 187: a |= d; break; // A|=D
    case 188: a |= m(b); break; // A|=*B
    case 189: a |= m(c); break; // A|=*C
    case 190: a |= h(d); break; // A|=*D
    case 191: a |= header[pc++]; break; // A|= N
    case 192: a ^= a; break; // A^=A
    case 193: a ^= b; break; // A^=B
    case 194: a ^= c; break; // A^=C
    case 195: a ^= d; break; // A^=D
    case 196: a ^= m(b); break; // A^=*B
    case 197: a ^= m(c); break; // A^=*C
    case 198: a ^= h(d); break; // A^=*D
    case 199: a ^= header[pc++]; break; // A^= N
    case 200: a <<= (a&31); break; // A<<=A
    case 201: a <<= (b&31); break; // A<<=B
    case 202: a <<= (c&31); break; // A<<=C
    case 203: a <<= (d&31); break; // A<<=D
    case 204: a <<= (m(b)&31); break; // A<<=*B
    case 205: a <<= (m(c)&31); break; // A<<=*C
    case 206: a <<= (h(d)&31); break; // A<<=*D
    case 207: a <<= (header[pc++]&31); break; // A<<= N
    case 208: a >>= (a&31); break; // A>>=A
    case 209: a >>= (b&31); break; // A>>=B
    case 210: a >>= (c&31); break; // A>>=C
    case 211: a >>= (d&31); break; // A>>=D
    case 212: a >>= (m(b)&31); break; // A>>=*B
    case 213: a >>= (m(c)&31); break; // A>>=*C
    case 214: a >>= (h(d)&31); break; // A>>=*D
    case 215: a >>= (header[pc++]&31); break; // A>>= N
    case 216: f = (a == a); break; // A==A
    case 217: f = (a == b); break; // A==B
    case 218: f = (a == c); break; // A==C
    case 219: f = (a == d); break; // A==D
    case 220: f = (a == U32(m(b))); break; // A==*B
    case 221: f = (a == U32(m(c))); break; // A==*C
    case 222: f = (a == h(d)); break; // A==*D
    case 223: f = (a == U32(header[pc++])); break; // A== N
    case 224: f = (a < a); break; // A<A
    case 225: f = (a < b); break; // A<B
    case 226: f = (a < c); break; // A<C
    case 227: f = (a < d); break; // A<D
    case 228: f = (a < U32(m(b))); break; // A<*B
    case 229: f = (a < U32(m(c))); break; // A<*C
    case 230: f = (a < h(d)); break; // A<*D
    case 231: f = (a < U32(header[pc++])); break; // A< N
    case 232: f = (a > a); break; // A>A
    case 233: f = (a > b); break; // A>B
    case 234: f = (a > c); break; // A>C
    case 235: f = (a > d); break; // A>D
    case 236: f = (a > U32(m(b))); break; // A>*B
    case 237: f = (a > U32(m(c))); break; // A>*C
    case 238: f = (a > h(d)); break; // A>*D
    case 239: f = (a > U32(header[pc++])); break; // A> N
    case 255: if((pc=hbegin+header[pc]+256*header[pc+1])>=hend)err();break;//LJ
    default: err();
  }
  return 1;
}

// Print illegal instruction error message and exit
void ZPAQL::err() {
  error("ZPAQL execution error");
}

///////////////////////// Component //////////////////////////

// A Component is a context model, indirect context model, match model,
// fixed weight mixer, adaptive 2 input mixer without or with current
// partial byte as context, adaptive m input mixer (without or with),
// or SSE (without or with).

struct Component {
  U32 limit;      // max count for cm
  U32 cxt;        // saved context
  U32 a, b, c;    // multi-purpose variables
  Array<U32> cm;  // cm[cxt] -> p in bits 31..10, n in 9..0; MATCH index
  Array<U8> ht;   // ICM/ISSE hash table[0..size1][0..15] and MATCH buf
  Array<U16> a16; // MIX weights
  void init();    // initialize to all 0
  Component() {init();}
};

void Component::init() {
  limit=cxt=a=b=c=0;
  cm.resize(0);
  ht.resize(0);
  a16.resize(0);
}

////////////////////////// StateTable ////////////////////////

// Next state table generator
class StateTable {
  enum {N=64}; // sizes of b, t
  int num_states(int n0, int n1);  // compute t[n0][n1][1]
  void discount(int& n0);  // set new value of n0 after 1 or n1 after 0
  void next_state(int& n0, int& n1, int y);  // new (n0,n1) after bit y
public:
  U8 ns[1024]; // state*4 -> next state if 0, if 1, n0, n1
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

///////////////////////// Predictor //////////////////////////

// A predictor guesses the next bit
class Predictor {
public:
  Predictor(ZPAQL&);
  void init();          // build model
  int predict();        // probability that next bit is a 1 (0..32767)
  void update(int y);   // train on bit y (0..1)
  bool isModeled() {    // n>0 components?
    assert(z.header.isize()>6);
    return z.header[6]!=0;
  }

private:

  // Predictor state
  int c8;               // last 0...7 bits.
  int hmap4;            // c8 split into nibbles
  int p[256];           // predictions
  U32 h[256];           // unrolled copy of z.h
  ZPAQL& z;             // VM to compute context hashes, includes H, n
  Component comp[256];  // the model, includes P

  // Modeling support functions
  int dt2k[256];        // division table for match: dt2k[i] = 2^12/i
  int dt[1024];         // division table for cm: dt[i] = 2^16/(i+1.5)
  U16 squasht[4096];    // squash() lookup table
  short stretcht[32768];// stretch() lookup table
  StateTable st;        // next, cminit functions

  // reduce prediction error in cr.cm
  void train(Component& cr, int y) {
    assert(y==0 || y==1);
    U32& pn=cr.cm(cr.cxt);
    U32 count=pn&0x3ff;
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
  size_t find(Array<U8>& ht, int sizebits, U32 cxt);
};

// Initailize model-independent tables
Predictor::Predictor(ZPAQL& zr):
    c8(1), hmap4(1), z(zr) {
  assert(sizeof(U8)==1);
  assert(sizeof(U16)==2);
  assert(sizeof(U32)==4);
  assert(sizeof(U64)==8);
  assert(sizeof(short)==2);
  assert(sizeof(int)==4);

  // Initialize tables
  dt2k[0]=0;
  for (int i=1; i<256; ++i)
    dt2k[i]=2048/i;
  for (int i=0; i<1024; ++i)
    dt[i]=(1<<17)/(i*2+3)*2;
  for (int i=0; i<32768; ++i)
    stretcht[i]=int(log((i+0.5)/(32767.5-i))*64+0.5+100000)-100000;
  for (int i=0; i<4096; ++i)
    squasht[i]=int(32768.0/(1+exp((i-2048)*(-1.0/64))));

  // Verify floating point math for squash() and stretch()
  U32 sqsum=0, stsum=0;
  for (int i=32767; i>=0; --i)
    stsum=stsum*3+stretch(i);
  for (int i=4095; i>=0; --i)
    sqsum=sqsum*3+squash(i-2048);
  assert(stsum==3887533746u);
  assert(sqsum==2278286169u);
}

// Initialize the predictor with a new model in z
void Predictor::init() {

  // Initialize context hash function
  z.inith();

  // Initialize predictions
  for (int i=0; i<256; ++i) h[i]=p[i]=0;

  // Initialize components
  for (int i=0; i<256; ++i)  // clear old model
    comp[i].init();
  int n=z.header[6]; // hsize[0..1] hh hm ph pm n (comp)[n] 0 0[128] (hcomp) 0
  const U8* cp=&z.header[7];  // start of component list
  for (int i=0; i<n; ++i) {
    assert(cp<&z.header[z.cend]);
    assert(cp>&z.header[0] && cp<&z.header[z.header.isize()-8]);
    Component& cr=comp[i];
    switch(cp[0]) {
      case CONS:  // c
        p[i]=(cp[1]-128)*4;
        break;
      case CM: // sizebits limit
        if (cp[1]>32) error("max size for CM is 32");
        cr.cm.resize(1, cp[1]);  // packed CM (22 bits) + CMCOUNT (10 bits)
        cr.limit=cp[2]*4;
        for (size_t j=0; j<cr.cm.size(); ++j)
          cr.cm[j]=0x80000000;
        break;
      case ICM: // sizebits
        if (cp[1]>26) error("max size for ICM is 26");
        cr.limit=1023;
        cr.cm.resize(256);
        cr.ht.resize(64, cp[1]);
        for (size_t j=0; j<cr.cm.size(); ++j)
          cr.cm[j]=st.cminit(j);
        break;
      case MATCH:  // sizebits
        if (cp[1]>32 || cp[2]>32) error("max size for MATCH is 32 32");
        cr.cm.resize(1, cp[1]);  // index
        cr.ht.resize(1, cp[2]);  // buf
        cr.ht(0)=1;
        break;
      case AVG: // j k wt
        if (cp[1]>=i) error("AVG j >= i");
        if (cp[2]>=i) error("AVG k >= i");
        break;
      case MIX2:  // sizebits j k rate mask
        if (cp[1]>32) error("max size for MIX2 is 32");
        if (cp[3]>=i) error("MIX2 k >= i");
        if (cp[2]>=i) error("MIX2 j >= i");
        cr.c=(size_t(1)<<cp[1]); // size (number of contexts)
        cr.a16.resize(1, cp[1]);  // wt[size][m]
        for (size_t j=0; j<cr.a16.size(); ++j)
          cr.a16[j]=32768;
        break;
      case MIX: {  // sizebits j m rate mask
        if (cp[1]>32) error("max size for MIX is 32");
        if (cp[2]>=i) error("MIX j >= i");
        if (cp[3]<1 || cp[3]>i-cp[2]) error("MIX m not in 1..i-j");
        int m=cp[3];  // number of inputs
        assert(m>=1);
        cr.c=(size_t(1)<<cp[1]); // size (number of contexts)
        cr.cm.resize(m, cp[1]);  // wt[size][m]
        for (size_t j=0; j<cr.cm.size(); ++j)
          cr.cm[j]=65536/m;
        break;
      }
      case ISSE:  // sizebits j
        if (cp[1]>32) error("max size for ISSE is 32");
        if (cp[2]>=i) error("ISSE j >= i");
        cr.ht.resize(64, cp[1]);
        cr.cm.resize(512);
        for (int j=0; j<256; ++j) {
          cr.cm[j*2]=1<<15;
          cr.cm[j*2+1]=clamp512k(stretch(st.cminit(j)>>8)<<10);
        }
        break;
      case SSE: // sizebits j start limit
        if (cp[1]>32) error("max size for SSE is 32");
        if (cp[2]>=i) error("SSE j >= i");
        if (cp[3]>cp[4]*4) error("SSE start > limit*4");
        cr.cm.resize(32, cp[1]);
        cr.limit=cp[4]*4;
        for (size_t j=0; j<cr.cm.size(); ++j)
          cr.cm[j]=squash((j&31)*64-992)<<17|cp[3];
        break;
      default: error("unknown component type");
    }
    assert(compsize[*cp]>0);
    cp+=compsize[*cp];
    assert(cp>=&z.header[7] && cp<&z.header[z.cend]);
  }
}

// Return next bit prediction using interpreted COMP code
int Predictor::predict() {
  assert(c8>=1 && c8<=255);

  // Predict next bit
  int n=z.header[6];
  assert(n>0 && n<=255);
  const U8* cp=&z.header[7];
  assert(cp[-1]==n);
  for (int i=0; i<n; ++i) {
    assert(cp>&z.header[0] && cp<&z.header[z.header.isize()-8]);
    Component& cr=comp[i];
    switch(cp[0]) {
      case CONS:  // c
        break;
      case CM:  // sizebits limit
        cr.cxt=h[i]^hmap4;
        p[i]=stretch(cr.cm(cr.cxt)>>17);
        break;
      case ICM: // sizebits
        assert((hmap4&15)>0);
        if (c8==1 || (c8&0xf0)==16) cr.c=find(cr.ht, cp[1]+2, h[i]+16*c8);
        cr.cxt=cr.ht[cr.c+(hmap4&15)];
        p[i]=stretch(cr.cm(cr.cxt)>>8);
        break;
      case MATCH: // sizebits bufbits: a=len, b=offset, c=bit, cxt=bitpos,
                  //                   ht=buf, limit=pos
        assert(cr.cm.size()==(size_t(1)<<cp[1]));
        assert(cr.ht.size()==(size_t(1)<<cp[2]));
        assert(cr.a<=255);
        assert(cr.c==0 || cr.c==1);
        assert(cr.cxt<8);
        assert(cr.limit<cr.ht.size());
        if (cr.a==0) p[i]=0;
        else {
          cr.c=(cr.ht(cr.limit-cr.b)>>(7-cr.cxt))&1; // predicted bit
          p[i]=stretch(dt2k[cr.a]*(cr.c*-2+1)&32767);
        }
        break;
      case AVG: // j k wt
        p[i]=(p[cp[1]]*cp[3]+p[cp[2]]*(256-cp[3]))>>8;
        break;
      case MIX2: { // sizebits j k rate mask
                   // c=size cm=wt[size] cxt=input
        cr.cxt=((h[i]+(c8&cp[5]))&(cr.c-1));
        assert(cr.cxt<cr.a16.size());
        int w=cr.a16[cr.cxt];
        assert(w>=0 && w<65536);
        p[i]=(w*p[cp[2]]+(65536-w)*p[cp[3]])>>16;
        assert(p[i]>=-2048 && p[i]<2048);
      }
        break;
      case MIX: {  // sizebits j m rate mask
                   // c=size cm=wt[size][m] cxt=index of wt in cm
        int m=cp[3];
        assert(m>=1 && m<=i);
        cr.cxt=h[i]+(c8&cp[5]);
        cr.cxt=(cr.cxt&(cr.c-1))*m; // pointer to row of weights
        assert(cr.cxt<=cr.cm.size()-m);
        int* wt=(int*)&cr.cm[cr.cxt];
        p[i]=0;
        for (int j=0; j<m; ++j)
          p[i]+=(wt[j]>>8)*p[cp[2]+j];
        p[i]=clamp2k(p[i]>>8);
      }
        break;
      case ISSE: { // sizebits j -- c=hi, cxt=bh
        assert((hmap4&15)>0);
        if (c8==1 || (c8&0xf0)==16)
          cr.c=find(cr.ht, cp[1]+2, h[i]+16*c8);
        cr.cxt=cr.ht[cr.c+(hmap4&15)];  // bit history
        int *wt=(int*)&cr.cm[cr.cxt*2];
        p[i]=clamp2k((wt[0]*p[cp[2]]+wt[1]*64)>>16);
      }
        break;
      case SSE: { // sizebits j start limit
        cr.cxt=(h[i]+c8)*32;
        int pq=p[cp[2]]+992;
        if (pq<0) pq=0;
        if (pq>1983) pq=1983;
        int wt=pq&63;
        pq>>=6;
        assert(pq>=0 && pq<=30);
        cr.cxt+=pq;
        p[i]=stretch(((cr.cm(cr.cxt)>>10)*(64-wt)+(cr.cm(cr.cxt+1)>>10)*wt)
             >>13);
        cr.cxt+=wt>>5;
      }
        break;
      default:
        error("component predict not implemented");
    }
    cp+=compsize[cp[0]];
    assert(cp<&z.header[z.cend]);
    assert(p[i]>=-2048 && p[i]<2048);
  }
  assert(cp[0]==NONE);
  return squash(p[n-1]);
}

// Update model with decoded bit y (0...1)
void Predictor::update(int y) {
  assert(y==0 || y==1);
  assert(c8>=1 && c8<=255);
  assert(hmap4>=1 && hmap4<=511);

  // Update components
  const U8* cp=&z.header[7];
  int n=z.header[6];
  assert(n>=1 && n<=255);
  assert(cp[-1]==n);
  for (int i=0; i<n; ++i) {
    Component& cr=comp[i];
    switch(cp[0]) {
      case CONS:  // c
        break;
      case CM:  // sizebits limit
        train(cr, y);
        break;
      case ICM: { // sizebits: cxt=ht[b]=bh, ht[c][0..15]=bh row, cxt=bh
        cr.ht[cr.c+(hmap4&15)]=st.next(cr.ht[cr.c+(hmap4&15)], y);
        U32& pn=cr.cm(cr.cxt);
        pn+=int(y*32767-(pn>>8))>>2;
      }
        break;
      case MATCH: // sizebits bufbits:
                  //   a=len, b=offset, c=bit, cm=index, cxt=bitpos
                  //   ht=buf, limit=pos
      {
        assert(cr.a<=255);
        assert(cr.c==0 || cr.c==1);
        assert(cr.cxt<8);
        assert(cr.cm.size()==(size_t(1)<<cp[1]));
        assert(cr.ht.size()==(size_t(1)<<cp[2]));
        assert(cr.limit<cr.ht.size());
        if (int(cr.c)!=y) cr.a=0;  // mismatch?
        cr.ht(cr.limit)+=cr.ht(cr.limit)+y;
        if (++cr.cxt==8) {
          cr.cxt=0;
          ++cr.limit;
          cr.limit&=(1<<cp[2])-1;
          if (cr.a==0) {  // look for a match
            cr.b=cr.limit-cr.cm(h[i]);
            if (cr.b&(cr.ht.size()-1))
              while (cr.a<255
                     && cr.ht(cr.limit-cr.a-1)==cr.ht(cr.limit-cr.a-cr.b-1))
                ++cr.a;
          }
          else cr.a+=cr.a<255;
          cr.cm(h[i])=cr.limit;
        }
      }
        break;
      case AVG:  // j k wt
        break;
      case MIX2: { // sizebits j k rate mask
                   // cm=wt[size], cxt=input
        assert(cr.a16.size()==cr.c);
        assert(cr.cxt<cr.a16.size());
        int err=(y*32767-squash(p[i]))*cp[4]>>5;
        int w=cr.a16[cr.cxt];
        w+=(err*(p[cp[2]]-p[cp[3]])+(1<<12))>>13;
        if (w<0) w=0;
        if (w>65535) w=65535;
        cr.a16[cr.cxt]=w;
      }
        break;
      case MIX: {   // sizebits j m rate mask
                    // cm=wt[size][m], cxt=input
        int m=cp[3];
        assert(m>0 && m<=i);
        assert(cr.cm.size()==m*cr.c);
        assert(cr.cxt+m<=cr.cm.size());
        int err=(y*32767-squash(p[i]))*cp[4]>>4;
        int* wt=(int*)&cr.cm[cr.cxt];
        for (int j=0; j<m; ++j)
          wt[j]=clamp512k(wt[j]+((err*p[cp[2]+j]+(1<<12))>>13));
      }
        break;
      case ISSE: { // sizebits j  -- c=hi, cxt=bh
        assert(cr.cxt==U32(cr.ht[cr.c+(hmap4&15)]));
        int err=y*32767-squash(p[i]);
        int *wt=(int*)&cr.cm[cr.cxt*2];
        wt[0]=clamp512k(wt[0]+((err*p[cp[2]]+(1<<12))>>13));
        wt[1]=clamp512k(wt[1]+((err+16)>>5));
        cr.ht[cr.c+(hmap4&15)]=st.next(cr.cxt, y);
      }
        break;
      case SSE:  // sizebits j start limit
        train(cr, y);
        break;
      default:
        assert(0);
    }
    cp+=compsize[cp[0]];
    assert(cp>=&z.header[7] && cp<&z.header[z.cend] 
           && cp<&z.header[z.header.isize()-8]);
  }
  assert(cp[0]==NONE);

  // Save bit y in c8, hmap4
  c8+=c8+y;
  if (c8>=256) {
    z.run(c8-256);
    hmap4=1;
    c8=1;
    for (int i=0; i<n; ++i) h[i]=z.H(i);
  }
  else if (c8>=16 && c8<32)
    hmap4=(hmap4&0xf)<<5|y<<4|1;
  else
    hmap4=(hmap4&0x1f0)|(((hmap4&0xf)*2+y)&0xf);
}

// Find cxt row in hash table ht. ht has rows of 16 indexed by the
// low sizebits of cxt with element 0 having the next higher 8 bits for
// collision detection. If not found after 3 adjacent tries, replace the
// row with lowest element 1 as priority. Return index of row.
size_t Predictor::find(Array<U8>& ht, int sizebits, U32 cxt) {
  assert(ht.size()==size_t(16)<<sizebits);
  int chk=cxt>>sizebits&255;
  size_t h0=(cxt*16)&(ht.size()-16);
  if (ht[h0]==chk) return h0;
  size_t h1=h0^16;
  if (ht[h1]==chk) return h1;
  size_t h2=h0^32;
  if (ht[h2]==chk) return h2;
  if (ht[h0+1]<=ht[h1+1] && ht[h0+1]<=ht[h2+1])
    return memset(&ht[h0], 0, 16), ht[h0]=chk, h0;
  else if (ht[h1+1]<ht[h2+1])
    return memset(&ht[h1], 0, 16), ht[h1]=chk, h1;
  else
    return memset(&ht[h2], 0, 16), ht[h2]=chk, h2;
}

//////////////////////////// Decoder /////////////////////////

// Decoder decompresses using an arithmetic code
class Decoder {
public:
  Reader* in;        // destination
  Decoder(ZPAQL& z);
  int decompress();  // return a byte or EOF
  void init();       // initialize at start of block
private:
  U32 low, high;     // range
  U32 curr;          // last 4 bytes of archive
  Predictor pr;      // to get p
  int decode(int p); // return decoded bit (0..1) with prob. p (0..65535)
};

Decoder::Decoder(ZPAQL& z):
    in(0), low(1), high(0xFFFFFFFF), curr(0), pr(z) {
}

void Decoder::init() {
  pr.init();
  if (pr.isModeled()) low=1, high=0xFFFFFFFF, curr=0;
  else low=high=curr=0;
}

// Return next bit of decoded input, which has 16 bit probability p of being 1
int Decoder::decode(int p) {
  assert(p>=0 && p<65536);
  assert(high>low && low>0);
  if (curr<low || curr>high) error("archive corrupted");
  assert(curr>=low && curr<=high);
  U32 mid=low+U32(((high-low)*U64(U32(p)))>>16);  // split range
  assert(high>mid && mid>=low);
  int y=curr<=mid;
  if (y) high=mid; else low=mid+1; // pick half
  while ((high^low)<0x1000000) { // shift out identical leading bytes
    high=high<<8|255;
    low=low<<8;
    low+=(low==0);
    int c=in->get();
    if (c<0) error("unexpected end of file");
    curr=curr<<8|c;
  }
  return y;
}

// Decompress 1 byte or -1 at end of input
int Decoder::decompress() {
  if (pr.isModeled()) {  // n>0 components?
    if (curr==0) {  // segment initialization
      for (int i=0; i<4; ++i)
        curr=curr<<8|in->get();
    }
    if (decode(0)) {
      if (curr!=0) error("decoding end of input");
      return -1;
    }
    else {
      int c=1;
      while (c<256) {  // get 8 bits
        int p=pr.predict()*2+1;
        c+=c+decode(p);
        pr.update(c&1);
      }
      return c-256;
    }
  }
  else {
    if (curr==0) {  // segment initialization
      for (int i=0; i<4; ++i)
        curr=curr<<8|in->get();
      if (curr==0) return -1;
    }
    assert(curr>0);
    --curr;
    return in->get();
  }
}

/////////////////////////// PostProcessor ////////////////////

class PostProcessor {
  int state;   // input parse state: 0=INIT, 1=PASS, 2..4=loading, 5=POST
  int hsize;   // header size
  int ph, pm;  // sizes of H and M in z
public:
  ZPAQL z;     // holds PCOMP
  PostProcessor(): state(0), hsize(0), ph(0), pm(0) {}
  void init(int h, int m);  // ph, pm sizes of H and M
  int write(int c);  // Input a byte, return state
  void setOutput(Writer* out) {z.output=out;}
  void setSHA1(SHA1* sha1ptr) {z.sha1=sha1ptr;}
  int getState() const {return state;}
};

// Copy ph, pm from block header
void PostProcessor::init(int h, int m) {
  state=hsize=0;
  ph=h;
  pm=m;
  z.clear();
}

// (PASS=0 | PROG=1 psize[0..1] pcomp[0..psize-1]) data... EOB=-1
// Return state: 1=PASS, 2..4=loading PROG, 5=PROG loaded
int PostProcessor::write(int c) {
  assert(c>=-1 && c<=255);
  switch (state) {
    case 0:  // initial state
      if (c<0) error("Unexpected EOS");
      state=c+1;  // 1=PASS, 2=PROG
      if (state>2) error("unknown post processing type");
      if (state==1) z.clear();
      break;
    case 1:  // PASS
      if (c>=0) z.outc(c);
      break;
    case 2: // PROG
      if (c<0) error("Unexpected EOS");
      hsize=c;  // low byte of size
      state=3;
      break;
    case 3:  // PROG psize[0]
      if (c<0) error("Unexpected EOS");
      hsize+=c*256;  // high byte of psize
      z.header.resize(hsize+300);
      z.cend=8;
      z.hbegin=z.hend=z.cend+128;
      z.header[4]=ph;
      z.header[5]=pm;
      state=4;
      break;
    case 4:  // PROG psize[0..1] pcomp[0...]
      if (c<0) error("Unexpected EOS");
      assert(z.hend<z.header.isize());
      z.header[z.hend++]=c;  // one byte of pcomp
      if (z.hend-z.hbegin==hsize) {  // last byte of pcomp?
        hsize=z.cend-2+z.hend-z.hbegin;
        z.header[0]=hsize&255;  // header size with empty COMP
        z.header[1]=hsize>>8;
        z.initp();
        state=5;
      }
      break;
    case 5:  // PROG ... data
      z.run(c);
      break;
  }
  return state;
}

//////////////////////// Decompresser ////////////////////////

// For decompression and listing archive contents
class Decompresser {
public:
  Decompresser(): z(), dec(z), pp(), state(BLOCK), decode_state(FIRSTSEG) {}
  void setInput(Reader* in) {dec.in=in;}
  bool findBlock();
  bool findFilename(Writer* = 0);
  void readComment(Writer* = 0);
  void setOutput(Writer* out) {pp.setOutput(out);}
  void setSHA1(SHA1* sha1ptr) {pp.setSHA1(sha1ptr);}
  void decompress();  // decompress segment
  void readSegmentEnd(char* sha1string = 0);
private:
  ZPAQL z;
  Decoder dec;
  PostProcessor pp;
  enum {BLOCK, FILENAME, COMMENT, DATA, SEGEND} state;  // expected next
  enum {FIRSTSEG, SEG} decode_state;  // which segment in block?
};

// Find the start of a block and return true if found. Set memptr
// to memory used.
bool Decompresser::findBlock() {
  assert(state==BLOCK);

  // Find start of block
  U32 h1=0x3D49B113, h2=0x29EB7F93, h3=0x2614BE13, h4=0x3828EB13;
  // Rolling hashes initialized to hash of first 13 bytes
  int c;
  while ((c=dec.in->get())!=-1) {
    h1=h1*12+c;
    h2=h2*20+c;
    h3=h3*28+c;
    h4=h4*44+c;
    if (h1==0xB16B88F1 && h2==0xFF5376F1 && h3==0x72AC5BF1 && h4==0x2F909AF1)
      break;  // hash of 16 byte string
  }
  if (c==-1) return false;

  // Read header
  if ((c=dec.in->get())!=1 && c!=2) error("unsupported ZPAQ level");
  if (dec.in->get()!=1) error("unsupported ZPAQL type");
  z.read(dec.in);
  if (c==1 && z.header.isize()>6 && z.header[6]==0)
    error("ZPAQ level 1 requires at least 1 component");
  state=FILENAME;
  decode_state=FIRSTSEG;
  return true;
}

// Read the start of a segment (1) or end of block code (255).
// If a segment is found, write the filename and return true, else false.
bool Decompresser::findFilename(Writer* filename) {
  assert(state==FILENAME);
  int c=dec.in->get();
  if (c==1) {  // segment found
    while (true) {
      c=dec.in->get();
      if (c==-1) error("unexpected EOF");
      if (c==0) {
        state=COMMENT;
        return true;
      }
      if (filename) filename->put(c);
    }
  }
  else if (c==255) {  // end of block found
    state=BLOCK;
    return false;
  }
  else
    error("missing segment or end of block");
  return false;
}

// Read the comment from the segment header
void Decompresser::readComment(Writer* comment) {
  assert(state==COMMENT);
  state=DATA;
  while (true) {
    int c=dec.in->get();
    if (c==-1) error("unexpected EOF");
    if (c==0) break;
    if (comment) comment->put(c);
  }
  if (dec.in->get()!=0) error("missing reserved byte");
}

// Decompress n bytes, or all if n < 0. Return false if done
void Decompresser::decompress() {
  assert(state==DATA);

  // Initialize models to start decompressing block
  if (decode_state==FIRSTSEG) {
    dec.init();
    assert(z.header.size()>5);
    pp.init(z.header[4], z.header[5]);
    decode_state=SEG;
  }

  // Decompress and load PCOMP into postprocessor
  while ((pp.getState()&3)!=1)
    pp.write(dec.decompress());

  // Decompress n bytes, or all if n < 0
  while (true) {
    int c=dec.decompress();
    pp.write(c);
    if (c==-1) {
      state=SEGEND;
      return;
    }
  }
}

// Read end of block. If a SHA1 checksum is present, write 1 and the
// 20 byte checksum into sha1string, else write 0 in first byte.
// If sha1string is 0 then discard it.
void Decompresser::readSegmentEnd(char* sha1string) {
  assert(state==SEGEND);

  // Read checksum
  int c=dec.in->get();
  if (c==254) {
    if (sha1string) sha1string[0]=0;  // no checksum
  }
  else if (c==253) {
    if (sha1string) sha1string[0]=1;
    for (int i=1; i<=20; ++i) {
      c=dec.in->get();
      if (sha1string) sha1string[i]=c;
    }
  }
  else
    error("missing end of segment marker");
  state=FILENAME;
}

///////////////////////// Driver program ////////////////////

void error(const char* msg) {
  fprintf(stderr, "unzpaq200: %s\n", msg);
  exit(1);
}

struct FileReader: public Reader {
  FILE* f;
  int get() {return getc(f);}
};

struct FileWriter: public Writer {
  FILE* f;
  void put(int c) {putc(c, f);}
};

struct StderrWriter: public Writer {
  void put(int c) {putc(c, stderr);}
};

int main(int argc, char** argv) {

  // Open input and output files
  FileReader in;
  in.f=stdin;
  if (argc>1) in.f=fopen(argv[1], "rb");
  if (!in.f) return perror(argv[1]), 1;

  FileWriter out;
  out.f=stdout;
  if (argc>2) out.f=fopen(argv[2], "wb");
  if (!out.f) return perror(argv[2]), 1;

  // Initialize decompresser
  StderrWriter sout;
  SHA1 sha1;
  Decompresser d;
  d.setInput(&in);
  d.setOutput(&out);
  d.setSHA1(&sha1);

  // Decompress blocks
  while (d.findBlock()) {

    // Decompress segments, printing stored filenames and comments
    while (d.findFilename(&sout)) {
      fprintf(stderr, " ");
      d.readComment(&sout);
      fprintf(stderr, " -> ");
      d.decompress();

      // Print output size and verify checksum
      char checksum[21];
      d.readSegmentEnd(checksum);
      fprintf(stderr, "%1.0f\n", sha1.size());
      if (checksum[0] && memcmp(checksum+1, sha1.result(), 20))
        error("SHA1 checksum mismatch");
    }
  }
  return 0;
}
