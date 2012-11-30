/* libzpaq.cpp

LIBZPAQ Version 0.03
Written by Matt Mahoney, Oct. 14, 2010

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

/////////////////////////// ZPAQL //////////////////////////

// Write header to out2, return true if HCOMP/PCOMP section is present
bool ZPAQL::write(Writer* out2) {
  if (header.size()<=6) return false;
  assert(header[0]+256*header[1]==cend-2+hend-hbegin);
  assert(cend>=7);
  assert(hbegin>=cend);
  assert(hend>=hbegin);
  assert(out2);
  if (header[6]>0) {  // if any components then write COMP
    for (int i=0; i<cend; ++i)
      out2->put(header[i]);
  }
  else {  // write PCOMP size only
    out2->put(hend-hbegin&255);
    out2->put(hend-hbegin>>8);
  }
  for (int i=hbegin; i<hend; ++i)
    out2->put(header[i]);
  return true;
}

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
    if (cend+size>header.size()-8) error("COMP list too big");
    for (int j=1; j<size; ++j)
      header[cend++]=in2->get();
  }
  if ((header[cend++]=in2->get())!=0) error("missing COMP END");

  // Insert a guard gap and read HCOMP
  hbegin=hend=cend+128;
  while (hend<hsize+129) {
    assert(hend<header.size()-8);
    int op=in2->get();
    if (op==-1) error("unexpected end of file");
    header[hend++]=op;
  }
  if ((header[hend++]=in2->get())!=0) error("missing HCOMP END");
  assert(cend>=7 && cend<header.size());
  assert(hbegin==cend+128 && hbegin<header.size());
  assert(hend>hbegin && hend<header.size());
  assert(hsize==header[0]+256*header[1]);
  assert(hsize==cend-2+hend-hbegin);
  selectModel();  // set select if an optimization is available
  return cend+hend-hbegin;
}

// Free memory, but preserve output, sha1 pointers
void ZPAQL::clear() {
  cend=hbegin=hend=0;  // COMP and HCOMP locations
  a=b=c=d=f=pc=0;      // machine state
  select=0;
  header.resize(0);
  h.resize(0);
  m.resize(0);
  r.resize(0);
}

// Constructor
ZPAQL::ZPAQL() {
  clear();
  output=0;
  sha1=0;
}

// Initialize machine state as HCOMP
void ZPAQL::inith() {
  assert(header.size()>6);
  assert(output==0);
  assert(sha1==0);
  init(header[2], header[3]); // hh, hm
}

// Initialize machine state as PCOMP
void ZPAQL::initp() {
  assert(header.size()>6);
  init(header[4], header[5]); // ph, pm
}

// Return memory requirement in bytes
double ZPAQL::memory() {
  double mem=pow(2.0,header[2]+2)+pow(2.0,header[3])  // hh hm
            +pow(2.0,header[4]+2)+pow(2.0,header[5])  // ph pm
            +header.size();
  int cp=7;  // start of comp list
  for (int i=0; i<header[6]; ++i) {  // n
    assert(cp<cend);
    double size=pow(2.0, header[cp+1]); // sizebits
    switch(header[cp]) {
      case CM: mem+=4*size; break;
      case ICM: mem+=64*size+1024; break;
      case MATCH: mem+=4*size+pow(2.0, header[cp+2]); break; // bufbits
      case MIX2: mem+=2*size; break;
      case MIX: mem+=4*size*header[cp+3]; break; // m
      case ISSE: mem+=64*size+2048; break;
      case SSE: mem+=128*size; break;
    }
    cp+=compsize[header[cp]];
  }
  return mem;
}

// Initialize machine state to run a program.
// Set select to nonzero if header matches anything in the cache
// or else add it.
void ZPAQL::init(int hbits, int mbits) {
  assert(header.size()>0);
  assert(cend>=7);
  assert(hbegin>=cend+128);
  assert(hend>=hbegin);
  assert(hend<header.size()-130);
  assert(header[0]+256*header[1]==cend-2+hend-hbegin);
  h.resize(1, hbits);
  m.resize(1, mbits);
  r.resize(256);
  a=b=c=d=pc=f=0;
}

// Run program on input by interpreting header
void ZPAQL::run0(U32 input) {
  assert(cend>6);
  assert(hbegin>=cend+128);
  assert(hend>=hbegin);
  assert(hend<header.size()-130);
  assert(m.size()>0);
  assert(h.size()>0);
  assert(header[0]+256*header[1]==cend+hend-hbegin-2);
  pc=hbegin;
  a=input;
  while (execute()) ;
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
    case 57: if (output) output->put(a); if (sha1) sha1->put(a); break; // OUT
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

// Search header for an optimization and set select>0 if found.
void ZPAQL::selectModel() {
  int p=0, len, count=0;
  while (true) {
    ++count;
    len=toU16(models+p);
    if (len<1) break;
    if (cend+hend-hbegin==len+2 && memcmp(&header[0], models+p, cend)==0
        && memcmp(&header[hbegin], models+p+cend, hend-hbegin)==0)
      select=count;
    p+=len+2;
  }
}

///////////////////////// Predictor /////////////////////////

// Initailize model-independent tables
Predictor::Predictor(ZPAQL& zr):
    c8(1), hmap4(1), z(zr) {
  assert(sizeof(U8)==1);
  assert(sizeof(U16)==2);
  assert(sizeof(U32)==4);
  assert(sizeof(short)==2);
  assert(sizeof(int)==4);
  assert(sizeof(ptrdiff_t)==sizeof(char*));

  // Initialize tables
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
  for (int i=0; i<256; ++i) p[i]=0;

  // Initialize components
  for (int i=0; i<256; ++i)  // clear old model
    comp[i].init();
  int n=z.header[6]; // hsize[0..1] hh hm ph pm n (comp)[n] END 0[128] (hcomp) END
  if (n<1 || n>255) error("n must be 1..255 components");
  const U8* cp=&z.header[7];  // start of component list
  for (int i=0; i<n; ++i) {
    assert(cp<&z.header[z.cend]);
    assert(cp>&z.header[0] && cp<&z.header[z.header.size()-8]);
    Component& cr=comp[i];
    switch(cp[0]) {
      case CONS:  // c
        p[i]=(cp[1]-128)*4;
        break;
      case CM: // sizebits limit
        cr.cm.resize(1, cp[1]);  // packed CM (22 bits) + CMCOUNT (10 bits)
        cr.limit=cp[2]*4;
        for (int j=0; j<cr.cm.size(); ++j)
          cr.cm[j]=0x80000000;
        break;
      case ICM: // sizebits
        cr.limit=1023;
        cr.cm.resize(256);
        cr.ht.resize(64, cp[1]);
        for (int j=0; j<cr.cm.size(); ++j)
          cr.cm[j]=st.cminit(j);
        break;
      case MATCH:  // sizebits
        cr.cm.resize(1, cp[1]);  // index
        cr.ht.resize(1, cp[2]);  // buf
        cr.ht(0)=1;
        break;
      case AVG: // j k wt
        break;
      case MIX2:  // sizebits j k rate mask
        if (cp[3]>=i) error("MIX2 k >= i");
        if (cp[2]>=i) error("MIX2 j >= i");
        cr.c=(1<<cp[1]); // size (number of contexts)
        cr.a16.resize(1, cp[1]);  // wt[size][m]
        for (int j=0; j<cr.a16.size(); ++j)
          cr.a16[j]=32768;
        break;
      case MIX: {  // sizebits j m rate mask
        if (cp[2]>=i) error("MIX j >= i");
        if (cp[3]<1 || cp[3]>i-cp[2])
          error("MIX m not in 1..i-j");
        int m=cp[3];  // number of inputs
        assert(m>=1);
        cr.c=(1<<cp[1]); // size (number of contexts)
        cr.cm.resize(m, cp[1]);  // wt[size][m]
        for (int j=0; j<cr.cm.size(); ++j)
          cr.cm[j]=65536/m;
        break;
      }
      case ISSE:  // sizebits j
        if (cp[2]>=i) error("ISSE j >= i");
        cr.ht.resize(64, cp[1]);
        cr.cm.resize(512);
        for (int j=0; j<256; ++j) {
          cr.cm[j*2]=1<<15;
          cr.cm[j*2+1]=clamp512k(stretch(st.cminit(j)>>8)<<10);
        }
        break;
      case SSE: // sizebits j start limit
        if (cp[2]>=i) error("SSE j >= i");
        if (cp[3]>cp[4]*4) error("SSE start > limit*4");
        cr.cm.resize(32, cp[1]);
        cr.limit=cp[4]*4;
        for (int j=0; j<cr.cm.size(); ++j)
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
int Predictor::predict0() {
  assert(c8>=1 && c8<=255);

  // Predict next bit
  int n=z.header[6];
  assert(n>0 && n<=255);
  const U8* cp=&z.header[7];
  assert(cp[-1]==n);
  for (int i=0; i<n; ++i) {
    assert(cp>&z.header[0] && cp<&z.header[z.header.size()-8]);
    Component& cr=comp[i];
    switch(cp[0]) {
      case CONS:  // c
        break;
      case CM:  // sizebits limit
        cr.cxt=z.H(i)^hmap4;
        p[i]=stretch(cr.cm(cr.cxt)>>17);
        break;
      case ICM: // sizebits
        assert((hmap4&15)>0);
        if (c8==1 || (c8&0xf0)==16) cr.c=find(cr.ht, cp[1]+2, z.H(i)+16*c8);
        cr.cxt=cr.ht[cr.c+(hmap4&15)];
        p[i]=stretch(cr.cm(cr.cxt)>>8);
        break;
      case MATCH: // sizebits bufbits: a=len, b=offset, c=bit, cxt=256/len,
                  //                   ht=buf, limit=8*pos+bp
        assert(cr.a>=0 && cr.a<=255);
        if (cr.a==0) p[i]=0;
        else {
          cr.c=cr.ht((cr.limit>>3)-cr.b)>>(7-(cr.limit&7))&1; // predicted bit
          p[i]=stretch(cr.cxt*(cr.c*-2+1)&32767);
        }
        break;
      case AVG: // j k wt
        p[i]=(p[cp[1]]*cp[3]+p[cp[2]]*(256-cp[3]))>>8;
        break;
      case MIX2: { // sizebits j k rate mask
                   // c=size cm=wt[size][m] cxt=input
        cr.cxt=((z.H(i)+(c8&cp[5]))&(cr.c-1));
        assert(int(cr.cxt)>=0 && int(cr.cxt)<cr.a16.size());
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
        cr.cxt=z.H(i)+(c8&cp[5]);
        cr.cxt=(cr.cxt&(cr.c-1))*m; // pointer to row of weights
        assert(int(cr.cxt)>=0 && int(cr.cxt)<=cr.cm.size()-m);
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
          cr.c=find(cr.ht, cp[1]+2, z.H(i)+16*c8);
        cr.cxt=cr.ht[cr.c+(hmap4&15)];  // bit history
        int *wt=(int*)&cr.cm[cr.cxt*2];
        p[i]=clamp2k((wt[0]*p[cp[2]]+wt[1]*64)>>16);
      }
        break;
      case SSE: { // sizebits j start limit
        cr.cxt=(z.H(i)+c8)*32;
        int pq=p[cp[2]]+992;
        if (pq<0) pq=0;
        if (pq>1983) pq=1983;
        int wt=pq&63;
        pq>>=6;
        assert(pq>=0 && pq<=30);
        cr.cxt+=pq;
        p[i]=stretch(((cr.cm(cr.cxt)>>10)*(64-wt)+(cr.cm(cr.cxt+1)>>10)*wt)>>13);
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
void Predictor::update0(int y) {
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
                  //   a=len, b=offset, c=bit, cm=index, cxt=256/len
                  //   ht=buf, limit=8*pos+bp
      {
        assert(cr.a>=0 && cr.a<=255);
        assert(cr.c==0 || cr.c==1);
        if (cr.c!=y) cr.a=0;  // mismatch?
        cr.ht(cr.limit>>3)+=cr.ht(cr.limit>>3)+y;
        if ((++cr.limit&7)==0) {
          int pos=cr.limit>>3;
          if (cr.a==0) {  // look for a match
            cr.b=pos-cr.cm(z.H(i));
            if (cr.b&(cr.ht.size()-1))
              while (cr.a<255 && cr.ht(pos-cr.a-1)==cr.ht(pos-cr.a-cr.b-1))
                ++cr.a;
          }
          else cr.a+=cr.a<255;
          cr.cm(z.H(i))=pos;
          if (cr.a>0) cr.cxt=2048/cr.a;
        }
      }
        break;
      case AVG:  // j k wt
        break;
      case MIX2: { // sizebits j k rate mask
                   // cm=input[2],wt[size][2], cxt=weight row
        assert(cr.a16.size()==cr.c);
        assert(int(cr.cxt)>=0 && int(cr.cxt)<cr.a16.size());
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
        assert(int(cr.cxt)>=0 && int(cr.cxt)<=cr.cm.size()-m);
        int err=(y*32767-squash(p[i]))*cp[4]>>4;
        int* wt=(int*)&cr.cm[cr.cxt];
        for (int j=0; j<m; ++j)
          wt[j]=clamp512k(wt[j]+((err*p[cp[2]+j]+(1<<12))>>13));
      }
        break;
      case ISSE: { // sizebits j  -- c=hi, cxt=bh
        assert(int(cr.cxt)==cr.ht[cr.c+(hmap4&15)]);
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
           && cp<&z.header[z.header.size()-8]);
  }
  assert(cp[0]==NONE);

  // Save bit y in c8, hmap4
  c8+=c8+y;
  if (c8>=256) {
    z.run(c8-256);
    hmap4=1;
    c8=1;
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
int Predictor::find(Array<U8>& ht, int sizebits, U32 cxt) {
  assert(ht.size()==16<<sizebits);
  int chk=cxt>>sizebits&255;
  int h0=(cxt*16)&(ht.size()-16);
  if (ht[h0]==chk) return h0;
  int h1=h0^16;
  if (ht[h1]==chk) return h1;
  int h2=h0^32;
  if (ht[h2]==chk) return h2;
  if (ht[h0+1]<=ht[h1+1] && ht[h0+1]<=ht[h2+1])
    return memset(&ht[h0], 0, 16), ht[h0]=chk, h0;
  else if (ht[h1+1]<ht[h2+1])
    return memset(&ht[h1], 0, 16), ht[h1]=chk, h1;
  else
    return memset(&ht[h2], 0, 16), ht[h2]=chk, h2;
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

// Optimized predict
int Predictor::predict() {
  switch(z.select) {

    // fast.cfg
    case 1: {
      // 2 components

      // 0 ICM 16
      if (c8==1 || (c8&0xf0)==16)
        comp[0].c=find(comp[0].ht, 16+2, z.H(0)+16*c8);
      comp[0].cxt=comp[0].ht[comp[0].c+(hmap4&15)];
      p[0]=stretch(comp[0].cm(comp[0].cxt)>>8);

      // 1 ISSE 19 0
      {
        if (c8==1 || (c8&0xf0)==16)
          comp[1].c=find(comp[1].ht, 21, z.H(1)+16*c8);
        comp[1].cxt=comp[1].ht[comp[1].c+(hmap4&15)];
        int *wt=(int*)&comp[1].cm[comp[1].cxt*2];
        p[1]=clamp2k((wt[0]*p[0]+wt[1]*64)>>16);
      }
      return squash(p[1]);
    }

    // mid.cfg
    case 2: {
      // 8 components

      // 0 ICM 5
      if (c8==1 || (c8&0xf0)==16)
        comp[0].c=find(comp[0].ht, 5+2, z.H(0)+16*c8);
      comp[0].cxt=comp[0].ht[comp[0].c+(hmap4&15)];
      p[0]=stretch(comp[0].cm(comp[0].cxt)>>8);

      // 1 ISSE 13 0
      {
        if (c8==1 || (c8&0xf0)==16)
          comp[1].c=find(comp[1].ht, 15, z.H(1)+16*c8);
        comp[1].cxt=comp[1].ht[comp[1].c+(hmap4&15)];
        int *wt=(int*)&comp[1].cm[comp[1].cxt*2];
        p[1]=clamp2k((wt[0]*p[0]+wt[1]*64)>>16);
      }

      // 2 ISSE 17 1
      {
        if (c8==1 || (c8&0xf0)==16)
          comp[2].c=find(comp[2].ht, 19, z.H(2)+16*c8);
        comp[2].cxt=comp[2].ht[comp[2].c+(hmap4&15)];
        int *wt=(int*)&comp[2].cm[comp[2].cxt*2];
        p[2]=clamp2k((wt[0]*p[1]+wt[1]*64)>>16);
      }

      // 3 ISSE 18 2
      {
        if (c8==1 || (c8&0xf0)==16)
          comp[3].c=find(comp[3].ht, 20, z.H(3)+16*c8);
        comp[3].cxt=comp[3].ht[comp[3].c+(hmap4&15)];
        int *wt=(int*)&comp[3].cm[comp[3].cxt*2];
        p[3]=clamp2k((wt[0]*p[2]+wt[1]*64)>>16);
      }

      // 4 ISSE 18 3
      {
        if (c8==1 || (c8&0xf0)==16)
          comp[4].c=find(comp[4].ht, 20, z.H(4)+16*c8);
        comp[4].cxt=comp[4].ht[comp[4].c+(hmap4&15)];
        int *wt=(int*)&comp[4].cm[comp[4].cxt*2];
        p[4]=clamp2k((wt[0]*p[3]+wt[1]*64)>>16);
      }

      // 5 ISSE 19 4
      {
        if (c8==1 || (c8&0xf0)==16)
          comp[5].c=find(comp[5].ht, 21, z.H(5)+16*c8);
        comp[5].cxt=comp[5].ht[comp[5].c+(hmap4&15)];
        int *wt=(int*)&comp[5].cm[comp[5].cxt*2];
        p[5]=clamp2k((wt[0]*p[4]+wt[1]*64)>>16);
      }

      // 6 MATCH 22 24
      if (comp[6].a==0) p[6]=0;
      else {
        comp[6].c=comp[6].ht((comp[6].limit>>3)
           -comp[6].b)>>(7-(comp[6].limit&7))&1;
        p[6]=stretch(comp[6].cxt*(comp[6].c*-2+1)&32767);
      }

      // 7 MIX 16 0 7 24 255
      {
        comp[7].cxt=z.H(7)+(c8&255);
        comp[7].cxt=(comp[7].cxt&(comp[7].c-1))*7;
        int* wt=(int*)&comp[7].cm[comp[7].cxt];
        p[7]=(wt[0]>>8)*p[0];
        p[7]+=(wt[1]>>8)*p[1];
        p[7]+=(wt[2]>>8)*p[2];
        p[7]+=(wt[3]>>8)*p[3];
        p[7]+=(wt[4]>>8)*p[4];
        p[7]+=(wt[5]>>8)*p[5];
        p[7]+=(wt[6]>>8)*p[6];
        p[7]=clamp2k(p[7]>>8);
      }
      return squash(p[7]);
    }

    // max.cfg
    case 3: {
      // 22 components

      // 0 CONST 160

      // 1 ICM 5
      if (c8==1 || (c8&0xf0)==16)
        comp[1].c=find(comp[1].ht, 5+2, z.H(1)+16*c8);
      comp[1].cxt=comp[1].ht[comp[1].c+(hmap4&15)];
      p[1]=stretch(comp[1].cm(comp[1].cxt)>>8);

      // 2 ISSE 13 1
      {
        if (c8==1 || (c8&0xf0)==16)
          comp[2].c=find(comp[2].ht, 15, z.H(2)+16*c8);
        comp[2].cxt=comp[2].ht[comp[2].c+(hmap4&15)];
        int *wt=(int*)&comp[2].cm[comp[2].cxt*2];
        p[2]=clamp2k((wt[0]*p[1]+wt[1]*64)>>16);
      }

      // 3 ISSE 16 2
      {
        if (c8==1 || (c8&0xf0)==16)
          comp[3].c=find(comp[3].ht, 18, z.H(3)+16*c8);
        comp[3].cxt=comp[3].ht[comp[3].c+(hmap4&15)];
        int *wt=(int*)&comp[3].cm[comp[3].cxt*2];
        p[3]=clamp2k((wt[0]*p[2]+wt[1]*64)>>16);
      }

      // 4 ISSE 18 3
      {
        if (c8==1 || (c8&0xf0)==16)
          comp[4].c=find(comp[4].ht, 20, z.H(4)+16*c8);
        comp[4].cxt=comp[4].ht[comp[4].c+(hmap4&15)];
        int *wt=(int*)&comp[4].cm[comp[4].cxt*2];
        p[4]=clamp2k((wt[0]*p[3]+wt[1]*64)>>16);
      }

      // 5 ISSE 19 4
      {
        if (c8==1 || (c8&0xf0)==16)
          comp[5].c=find(comp[5].ht, 21, z.H(5)+16*c8);
        comp[5].cxt=comp[5].ht[comp[5].c+(hmap4&15)];
        int *wt=(int*)&comp[5].cm[comp[5].cxt*2];
        p[5]=clamp2k((wt[0]*p[4]+wt[1]*64)>>16);
      }

      // 6 ISSE 19 5
      {
        if (c8==1 || (c8&0xf0)==16)
          comp[6].c=find(comp[6].ht, 21, z.H(6)+16*c8);
        comp[6].cxt=comp[6].ht[comp[6].c+(hmap4&15)];
        int *wt=(int*)&comp[6].cm[comp[6].cxt*2];
        p[6]=clamp2k((wt[0]*p[5]+wt[1]*64)>>16);
      }

      // 7 ISSE 20 6
      {
        if (c8==1 || (c8&0xf0)==16)
          comp[7].c=find(comp[7].ht, 22, z.H(7)+16*c8);
        comp[7].cxt=comp[7].ht[comp[7].c+(hmap4&15)];
        int *wt=(int*)&comp[7].cm[comp[7].cxt*2];
        p[7]=clamp2k((wt[0]*p[6]+wt[1]*64)>>16);
      }

      // 8 MATCH 22 24
      if (comp[8].a==0) p[8]=0;
      else {
        comp[8].c=comp[8].ht((comp[8].limit>>3)
           -comp[8].b)>>(7-(comp[8].limit&7))&1;
        p[8]=stretch(comp[8].cxt*(comp[8].c*-2+1)&32767);
      }

      // 9 ICM 17
      if (c8==1 || (c8&0xf0)==16)
        comp[9].c=find(comp[9].ht, 17+2, z.H(9)+16*c8);
      comp[9].cxt=comp[9].ht[comp[9].c+(hmap4&15)];
      p[9]=stretch(comp[9].cm(comp[9].cxt)>>8);

      // 10 ISSE 19 9
      {
        if (c8==1 || (c8&0xf0)==16)
          comp[10].c=find(comp[10].ht, 21, z.H(10)+16*c8);
        comp[10].cxt=comp[10].ht[comp[10].c+(hmap4&15)];
        int *wt=(int*)&comp[10].cm[comp[10].cxt*2];
        p[10]=clamp2k((wt[0]*p[9]+wt[1]*64)>>16);
      }

      // 11 ICM 13
      if (c8==1 || (c8&0xf0)==16)
        comp[11].c=find(comp[11].ht, 13+2, z.H(11)+16*c8);
      comp[11].cxt=comp[11].ht[comp[11].c+(hmap4&15)];
      p[11]=stretch(comp[11].cm(comp[11].cxt)>>8);

      // 12 ICM 13
      if (c8==1 || (c8&0xf0)==16)
        comp[12].c=find(comp[12].ht, 13+2, z.H(12)+16*c8);
      comp[12].cxt=comp[12].ht[comp[12].c+(hmap4&15)];
      p[12]=stretch(comp[12].cm(comp[12].cxt)>>8);

      // 13 ICM 13
      if (c8==1 || (c8&0xf0)==16)
        comp[13].c=find(comp[13].ht, 13+2, z.H(13)+16*c8);
      comp[13].cxt=comp[13].ht[comp[13].c+(hmap4&15)];
      p[13]=stretch(comp[13].cm(comp[13].cxt)>>8);

      // 14 ICM 14
      if (c8==1 || (c8&0xf0)==16)
        comp[14].c=find(comp[14].ht, 14+2, z.H(14)+16*c8);
      comp[14].cxt=comp[14].ht[comp[14].c+(hmap4&15)];
      p[14]=stretch(comp[14].cm(comp[14].cxt)>>8);

      // 15 MIX 16 0 15 24 255
      {
        comp[15].cxt=z.H(15)+(c8&255);
        comp[15].cxt=(comp[15].cxt&(comp[15].c-1))*15;
        int* wt=(int*)&comp[15].cm[comp[15].cxt];
        p[15]=(wt[0]>>8)*p[0];
        p[15]+=(wt[1]>>8)*p[1];
        p[15]+=(wt[2]>>8)*p[2];
        p[15]+=(wt[3]>>8)*p[3];
        p[15]+=(wt[4]>>8)*p[4];
        p[15]+=(wt[5]>>8)*p[5];
        p[15]+=(wt[6]>>8)*p[6];
        p[15]+=(wt[7]>>8)*p[7];
        p[15]+=(wt[8]>>8)*p[8];
        p[15]+=(wt[9]>>8)*p[9];
        p[15]+=(wt[10]>>8)*p[10];
        p[15]+=(wt[11]>>8)*p[11];
        p[15]+=(wt[12]>>8)*p[12];
        p[15]+=(wt[13]>>8)*p[13];
        p[15]+=(wt[14]>>8)*p[14];
        p[15]=clamp2k(p[15]>>8);
      }

      // 16 MIX 8 0 16 10 255
      {
        comp[16].cxt=z.H(16)+(c8&255);
        comp[16].cxt=(comp[16].cxt&(comp[16].c-1))*16;
        int* wt=(int*)&comp[16].cm[comp[16].cxt];
        p[16]=(wt[0]>>8)*p[0];
        p[16]+=(wt[1]>>8)*p[1];
        p[16]+=(wt[2]>>8)*p[2];
        p[16]+=(wt[3]>>8)*p[3];
        p[16]+=(wt[4]>>8)*p[4];
        p[16]+=(wt[5]>>8)*p[5];
        p[16]+=(wt[6]>>8)*p[6];
        p[16]+=(wt[7]>>8)*p[7];
        p[16]+=(wt[8]>>8)*p[8];
        p[16]+=(wt[9]>>8)*p[9];
        p[16]+=(wt[10]>>8)*p[10];
        p[16]+=(wt[11]>>8)*p[11];
        p[16]+=(wt[12]>>8)*p[12];
        p[16]+=(wt[13]>>8)*p[13];
        p[16]+=(wt[14]>>8)*p[14];
        p[16]+=(wt[15]>>8)*p[15];
        p[16]=clamp2k(p[16]>>8);
      }

      // 17 MIX2 0 15 16 24 0
      {
        comp[17].cxt=((z.H(17)+(c8&0))&(comp[17].c-1));
        int w=comp[17].a16[comp[17].cxt];
        p[17]=(w*p[15]+(65536-w)*p[16])>>16;
      }

      // 18 SSE 8 17 32 255
      {
        comp[18].cxt=(z.H(18)+c8)*32;
        int pq=p[17]+992;
        if (pq<0) pq=0;
        if (pq>1983) pq=1983;
        int wt=pq&63;
        pq>>=6;
        comp[18].cxt+=pq;
        p[18]=stretch(((comp[18].cm(comp[18].cxt)>>10)*(64-wt)
           +(comp[18].cm(comp[18].cxt+1)>>10)*wt)>>13);
        comp[18].cxt+=wt>>5;
      }

      // 19 MIX2 8 17 18 16 255
      {
        comp[19].cxt=((z.H(19)+(c8&255))&(comp[19].c-1));
        int w=comp[19].a16[comp[19].cxt];
        p[19]=(w*p[17]+(65536-w)*p[18])>>16;
      }

      // 20 SSE 16 19 32 255
      {
        comp[20].cxt=(z.H(20)+c8)*32;
        int pq=p[19]+992;
        if (pq<0) pq=0;
        if (pq>1983) pq=1983;
        int wt=pq&63;
        pq>>=6;
        comp[20].cxt+=pq;
        p[20]=stretch(((comp[20].cm(comp[20].cxt)>>10)*(64-wt)
           +(comp[20].cm(comp[20].cxt+1)>>10)*wt)>>13);
        comp[20].cxt+=wt>>5;
      }

      // 21 MIX2 0 19 20 16 0
      {
        comp[21].cxt=((z.H(21)+(c8&0))&(comp[21].c-1));
        int w=comp[21].a16[comp[21].cxt];
        p[21]=(w*p[19]+(65536-w)*p[20])>>16;
      }
      return squash(p[21]);
    }

    // Not optimized
    default: return predict0();
  }
}

void Predictor::update(int y) {
  switch(z.select) {

    // fast.cfg
    case 1: {
      // 2 components

      // 0 ICM 16
      {
        comp[0].ht[comp[0].c+(hmap4&15)]=
            st.next(comp[0].ht[comp[0].c+(hmap4&15)], y);
        U32& pn=comp[0].cm(comp[0].cxt);
        pn+=int(y*32767-(pn>>8))>>2;
      }

      // 1 ISSE 19 0
      {
        int err=y*32767-squash(p[1]);
        int *wt=(int*)&comp[1].cm[comp[1].cxt*2];
        wt[0]=clamp512k(wt[0]+((err*p[0]+(1<<12))>>13));
        wt[1]=clamp512k(wt[1]+((err+16)>>5));
        comp[1].ht[comp[1].c+(hmap4&15)]=st.next(comp[1].cxt, y);
      }
      break;
    }

    // mid.cfg
    case 2: {
      // 8 components

      // 0 ICM 5
      {
        comp[0].ht[comp[0].c+(hmap4&15)]=
            st.next(comp[0].ht[comp[0].c+(hmap4&15)], y);
        U32& pn=comp[0].cm(comp[0].cxt);
        pn+=int(y*32767-(pn>>8))>>2;
      }

      // 1 ISSE 13 0
      {
        int err=y*32767-squash(p[1]);
        int *wt=(int*)&comp[1].cm[comp[1].cxt*2];
        wt[0]=clamp512k(wt[0]+((err*p[0]+(1<<12))>>13));
        wt[1]=clamp512k(wt[1]+((err+16)>>5));
        comp[1].ht[comp[1].c+(hmap4&15)]=st.next(comp[1].cxt, y);
      }

      // 2 ISSE 17 1
      {
        int err=y*32767-squash(p[2]);
        int *wt=(int*)&comp[2].cm[comp[2].cxt*2];
        wt[0]=clamp512k(wt[0]+((err*p[1]+(1<<12))>>13));
        wt[1]=clamp512k(wt[1]+((err+16)>>5));
        comp[2].ht[comp[2].c+(hmap4&15)]=st.next(comp[2].cxt, y);
      }

      // 3 ISSE 18 2
      {
        int err=y*32767-squash(p[3]);
        int *wt=(int*)&comp[3].cm[comp[3].cxt*2];
        wt[0]=clamp512k(wt[0]+((err*p[2]+(1<<12))>>13));
        wt[1]=clamp512k(wt[1]+((err+16)>>5));
        comp[3].ht[comp[3].c+(hmap4&15)]=st.next(comp[3].cxt, y);
      }

      // 4 ISSE 18 3
      {
        int err=y*32767-squash(p[4]);
        int *wt=(int*)&comp[4].cm[comp[4].cxt*2];
        wt[0]=clamp512k(wt[0]+((err*p[3]+(1<<12))>>13));
        wt[1]=clamp512k(wt[1]+((err+16)>>5));
        comp[4].ht[comp[4].c+(hmap4&15)]=st.next(comp[4].cxt, y);
      }

      // 5 ISSE 19 4
      {
        int err=y*32767-squash(p[5]);
        int *wt=(int*)&comp[5].cm[comp[5].cxt*2];
        wt[0]=clamp512k(wt[0]+((err*p[4]+(1<<12))>>13));
        wt[1]=clamp512k(wt[1]+((err+16)>>5));
        comp[5].ht[comp[5].c+(hmap4&15)]=st.next(comp[5].cxt, y);
      }

      // 6 MATCH 22 24
      {
        if (comp[6].c!=y) comp[6].a=0;
        comp[6].ht(comp[6].limit>>3)+=comp[6].ht(comp[6].limit>>3)+y;
        if ((++comp[6].limit&7)==0) {
          int pos=comp[6].limit>>3;
          if (comp[6].a==0) {
            comp[6].b=pos-comp[6].cm(z.H(6));
            if (comp[6].b&(comp[6].ht.size()-1))
              while (comp[6].a<255 && comp[6].ht(pos-comp[6].a-1)
                     ==comp[6].ht(pos-comp[6].a-comp[6].b-1))
                ++comp[6].a;
          }
          else comp[6].a+=comp[6].a<255;
          comp[6].cm(z.H(6))=pos;
          if (comp[6].a>0) comp[6].cxt=2048/comp[6].a;
        }
      }

      // 7 MIX 16 0 7 24 255
      {
        int err=(y*32767-squash(p[7]))*24>>4;
        int* wt=(int*)&comp[7].cm[comp[7].cxt];
          wt[0]=clamp512k(wt[0]+((err*p[0]+(1<<12))>>13));
          wt[1]=clamp512k(wt[1]+((err*p[1]+(1<<12))>>13));
          wt[2]=clamp512k(wt[2]+((err*p[2]+(1<<12))>>13));
          wt[3]=clamp512k(wt[3]+((err*p[3]+(1<<12))>>13));
          wt[4]=clamp512k(wt[4]+((err*p[4]+(1<<12))>>13));
          wt[5]=clamp512k(wt[5]+((err*p[5]+(1<<12))>>13));
          wt[6]=clamp512k(wt[6]+((err*p[6]+(1<<12))>>13));
      }
      break;
    }

    // max.cfg
    case 3: {
      // 22 components

      // 0 CONST 160

      // 1 ICM 5
      {
        comp[1].ht[comp[1].c+(hmap4&15)]=
            st.next(comp[1].ht[comp[1].c+(hmap4&15)], y);
        U32& pn=comp[1].cm(comp[1].cxt);
        pn+=int(y*32767-(pn>>8))>>2;
      }

      // 2 ISSE 13 1
      {
        int err=y*32767-squash(p[2]);
        int *wt=(int*)&comp[2].cm[comp[2].cxt*2];
        wt[0]=clamp512k(wt[0]+((err*p[1]+(1<<12))>>13));
        wt[1]=clamp512k(wt[1]+((err+16)>>5));
        comp[2].ht[comp[2].c+(hmap4&15)]=st.next(comp[2].cxt, y);
      }

      // 3 ISSE 16 2
      {
        int err=y*32767-squash(p[3]);
        int *wt=(int*)&comp[3].cm[comp[3].cxt*2];
        wt[0]=clamp512k(wt[0]+((err*p[2]+(1<<12))>>13));
        wt[1]=clamp512k(wt[1]+((err+16)>>5));
        comp[3].ht[comp[3].c+(hmap4&15)]=st.next(comp[3].cxt, y);
      }

      // 4 ISSE 18 3
      {
        int err=y*32767-squash(p[4]);
        int *wt=(int*)&comp[4].cm[comp[4].cxt*2];
        wt[0]=clamp512k(wt[0]+((err*p[3]+(1<<12))>>13));
        wt[1]=clamp512k(wt[1]+((err+16)>>5));
        comp[4].ht[comp[4].c+(hmap4&15)]=st.next(comp[4].cxt, y);
      }

      // 5 ISSE 19 4
      {
        int err=y*32767-squash(p[5]);
        int *wt=(int*)&comp[5].cm[comp[5].cxt*2];
        wt[0]=clamp512k(wt[0]+((err*p[4]+(1<<12))>>13));
        wt[1]=clamp512k(wt[1]+((err+16)>>5));
        comp[5].ht[comp[5].c+(hmap4&15)]=st.next(comp[5].cxt, y);
      }

      // 6 ISSE 19 5
      {
        int err=y*32767-squash(p[6]);
        int *wt=(int*)&comp[6].cm[comp[6].cxt*2];
        wt[0]=clamp512k(wt[0]+((err*p[5]+(1<<12))>>13));
        wt[1]=clamp512k(wt[1]+((err+16)>>5));
        comp[6].ht[comp[6].c+(hmap4&15)]=st.next(comp[6].cxt, y);
      }

      // 7 ISSE 20 6
      {
        int err=y*32767-squash(p[7]);
        int *wt=(int*)&comp[7].cm[comp[7].cxt*2];
        wt[0]=clamp512k(wt[0]+((err*p[6]+(1<<12))>>13));
        wt[1]=clamp512k(wt[1]+((err+16)>>5));
        comp[7].ht[comp[7].c+(hmap4&15)]=st.next(comp[7].cxt, y);
      }

      // 8 MATCH 22 24
      {
        if (comp[8].c!=y) comp[8].a=0;
        comp[8].ht(comp[8].limit>>3)+=comp[8].ht(comp[8].limit>>3)+y;
        if ((++comp[8].limit&7)==0) {
          int pos=comp[8].limit>>3;
          if (comp[8].a==0) {
            comp[8].b=pos-comp[8].cm(z.H(8));
            if (comp[8].b&(comp[8].ht.size()-1))
              while (comp[8].a<255 && comp[8].ht(pos-comp[8].a-1)
                     ==comp[8].ht(pos-comp[8].a-comp[8].b-1))
                ++comp[8].a;
          }
          else comp[8].a+=comp[8].a<255;
          comp[8].cm(z.H(8))=pos;
          if (comp[8].a>0) comp[8].cxt=2048/comp[8].a;
        }
      }

      // 9 ICM 17
      {
        comp[9].ht[comp[9].c+(hmap4&15)]=
            st.next(comp[9].ht[comp[9].c+(hmap4&15)], y);
        U32& pn=comp[9].cm(comp[9].cxt);
        pn+=int(y*32767-(pn>>8))>>2;
      }

      // 10 ISSE 19 9
      {
        int err=y*32767-squash(p[10]);
        int *wt=(int*)&comp[10].cm[comp[10].cxt*2];
        wt[0]=clamp512k(wt[0]+((err*p[9]+(1<<12))>>13));
        wt[1]=clamp512k(wt[1]+((err+16)>>5));
        comp[10].ht[comp[10].c+(hmap4&15)]=st.next(comp[10].cxt, y);
      }

      // 11 ICM 13
      {
        comp[11].ht[comp[11].c+(hmap4&15)]=
            st.next(comp[11].ht[comp[11].c+(hmap4&15)], y);
        U32& pn=comp[11].cm(comp[11].cxt);
        pn+=int(y*32767-(pn>>8))>>2;
      }

      // 12 ICM 13
      {
        comp[12].ht[comp[12].c+(hmap4&15)]=
            st.next(comp[12].ht[comp[12].c+(hmap4&15)], y);
        U32& pn=comp[12].cm(comp[12].cxt);
        pn+=int(y*32767-(pn>>8))>>2;
      }

      // 13 ICM 13
      {
        comp[13].ht[comp[13].c+(hmap4&15)]=
            st.next(comp[13].ht[comp[13].c+(hmap4&15)], y);
        U32& pn=comp[13].cm(comp[13].cxt);
        pn+=int(y*32767-(pn>>8))>>2;
      }

      // 14 ICM 14
      {
        comp[14].ht[comp[14].c+(hmap4&15)]=
            st.next(comp[14].ht[comp[14].c+(hmap4&15)], y);
        U32& pn=comp[14].cm(comp[14].cxt);
        pn+=int(y*32767-(pn>>8))>>2;
      }

      // 15 MIX 16 0 15 24 255
      {
        int err=(y*32767-squash(p[15]))*24>>4;
        int* wt=(int*)&comp[15].cm[comp[15].cxt];
          wt[0]=clamp512k(wt[0]+((err*p[0]+(1<<12))>>13));
          wt[1]=clamp512k(wt[1]+((err*p[1]+(1<<12))>>13));
          wt[2]=clamp512k(wt[2]+((err*p[2]+(1<<12))>>13));
          wt[3]=clamp512k(wt[3]+((err*p[3]+(1<<12))>>13));
          wt[4]=clamp512k(wt[4]+((err*p[4]+(1<<12))>>13));
          wt[5]=clamp512k(wt[5]+((err*p[5]+(1<<12))>>13));
          wt[6]=clamp512k(wt[6]+((err*p[6]+(1<<12))>>13));
          wt[7]=clamp512k(wt[7]+((err*p[7]+(1<<12))>>13));
          wt[8]=clamp512k(wt[8]+((err*p[8]+(1<<12))>>13));
          wt[9]=clamp512k(wt[9]+((err*p[9]+(1<<12))>>13));
          wt[10]=clamp512k(wt[10]+((err*p[10]+(1<<12))>>13));
          wt[11]=clamp512k(wt[11]+((err*p[11]+(1<<12))>>13));
          wt[12]=clamp512k(wt[12]+((err*p[12]+(1<<12))>>13));
          wt[13]=clamp512k(wt[13]+((err*p[13]+(1<<12))>>13));
          wt[14]=clamp512k(wt[14]+((err*p[14]+(1<<12))>>13));
      }

      // 16 MIX 8 0 16 10 255
      {
        int err=(y*32767-squash(p[16]))*10>>4;
        int* wt=(int*)&comp[16].cm[comp[16].cxt];
          wt[0]=clamp512k(wt[0]+((err*p[0]+(1<<12))>>13));
          wt[1]=clamp512k(wt[1]+((err*p[1]+(1<<12))>>13));
          wt[2]=clamp512k(wt[2]+((err*p[2]+(1<<12))>>13));
          wt[3]=clamp512k(wt[3]+((err*p[3]+(1<<12))>>13));
          wt[4]=clamp512k(wt[4]+((err*p[4]+(1<<12))>>13));
          wt[5]=clamp512k(wt[5]+((err*p[5]+(1<<12))>>13));
          wt[6]=clamp512k(wt[6]+((err*p[6]+(1<<12))>>13));
          wt[7]=clamp512k(wt[7]+((err*p[7]+(1<<12))>>13));
          wt[8]=clamp512k(wt[8]+((err*p[8]+(1<<12))>>13));
          wt[9]=clamp512k(wt[9]+((err*p[9]+(1<<12))>>13));
          wt[10]=clamp512k(wt[10]+((err*p[10]+(1<<12))>>13));
          wt[11]=clamp512k(wt[11]+((err*p[11]+(1<<12))>>13));
          wt[12]=clamp512k(wt[12]+((err*p[12]+(1<<12))>>13));
          wt[13]=clamp512k(wt[13]+((err*p[13]+(1<<12))>>13));
          wt[14]=clamp512k(wt[14]+((err*p[14]+(1<<12))>>13));
          wt[15]=clamp512k(wt[15]+((err*p[15]+(1<<12))>>13));
      }

      // 17 MIX2 0 15 16 24 0
      {
        int err=(y*32767-squash(p[17]))*24>>5;
        int w=comp[17].a16[comp[17].cxt];
        w+=(err*(p[15]-p[16])+(1<<12))>>13;
        if (w<0) w=0;
        if (w>65535) w=65535;
        comp[17].a16[comp[17].cxt]=w;
      }

      // 18 SSE 8 17 32 255
      train(comp[18], y);

      // 19 MIX2 8 17 18 16 255
      {
        int err=(y*32767-squash(p[19]))*16>>5;
        int w=comp[19].a16[comp[19].cxt];
        w+=(err*(p[17]-p[18])+(1<<12))>>13;
        if (w<0) w=0;
        if (w>65535) w=65535;
        comp[19].a16[comp[19].cxt]=w;
      }

      // 20 SSE 16 19 32 255
      train(comp[20], y);

      // 21 MIX2 0 19 20 16 0
      {
        int err=(y*32767-squash(p[21]))*16>>5;
        int w=comp[21].a16[comp[21].cxt];
        w+=(err*(p[19]-p[20])+(1<<12))>>13;
        if (w<0) w=0;
        if (w>65535) w=65535;
        comp[21].a16[comp[21].cxt]=w;
      }
      break;
    }

    // Not optimized
    default: return update0(y);
  }
  c8+=c8+y;
  if (c8>=256) {
    z.run(c8-256);
    hmap4=1;
    c8=1;
  }
  else if (c8>=16 && c8<32)
    hmap4=(hmap4&0xf)<<5|y<<4|1;
  else
    hmap4=(hmap4&0x1f0)|(((hmap4&0xf)*2+y)&0xf);
}

void ZPAQL::run(U32 input) {
  switch(select) {

    // fast.cfg
    case 1: {
      a = input;
      m(b) = a;
      a = 0;
      d = 0;
      a = (a+m(b)+512)*773;
      --b;
      a = (a+m(b)+512)*773;
      h(d) = a;
      ++d;
      --b;
      a = (a+m(b)+512)*773;
      --b;
      a = (a+m(b)+512)*773;
      h(d) = a;
      return;
    }

    // mid.cfg
    case 2: {
      a = input;
      ++c;
      m(c) = a;
      b = c;
      a = 0;
      d = 1;
      a = (a+m(b)+512)*773;
      h(d) = a;
      --b;
      ++d;
      a = (a+m(b)+512)*773;
      h(d) = a;
      --b;
      ++d;
      a = (a+m(b)+512)*773;
      h(d) = a;
      --b;
      ++d;
      a = (a+m(b)+512)*773;
      h(d) = a;
      --b;
      ++d;
      a = (a+m(b)+512)*773;
      h(d) = a;
      --b;
      ++d;
      a = (a+m(b)+512)*773;
      --b;
      a = (a+m(b)+512)*773;
      h(d) = a;
      ++d;
      a = m(c);
      a <<= (8&31);
      h(d) = a;
      return;
    }

    // max.cfg
    case 3: {
      a = input;
      ++c;
      m(c) = a;
      b = c;
      a = 0;
      d = 2;
      a = (a+m(b)+512)*773;
      h(d) = a;
      --b;
      ++d;
      a = (a+m(b)+512)*773;
      h(d) = a;
      --b;
      ++d;
      a = (a+m(b)+512)*773;
      h(d) = a;
      --b;
      ++d;
      a = (a+m(b)+512)*773;
      h(d) = a;
      --b;
      ++d;
      a = (a+m(b)+512)*773;
      h(d) = a;
      --b;
      ++d;
      a = (a+m(b)+512)*773;
      --b;
      a = (a+m(b)+512)*773;
      h(d) = a;
      --b;
      ++d;
      a = (a+m(b)+512)*773;
      h(d) = a;
      --b;
      ++d;
      a = m(c);
      a &= ~ 32;
      f = (a > U32(64));
      if (!f) goto L300057;
      f = (a < U32(91));
      if (!f) goto L300057;
      ++d;
      h(d) = (h(d)+a+512)*773;
      --d;
      swap(h(d));
      a += h(d);
      a *= 20;
      h(d) = a;
      goto L300066;
L300057:
      a = h(d);
      f = (a == U32(0));
      if (f) goto L300065;
      ++d;
      h(d) = a;
      --d;
L300065:
      h(d) = 0;
L300066:
      ++d;
      ++d;
      b = c;
      --b;
      a = 0;
      a = (a+m(b)+512)*773;
      h(d) = a;
      ++d;
      --b;
      a = 0;
      a = (a+m(b)+512)*773;
      h(d) = a;
      ++d;
      --b;
      a = 0;
      a = (a+m(b)+512)*773;
      h(d) = a;
      ++d;
      a = b;
      a -= 212;
      b = a;
      a = 0;
      a = (a+m(b)+512)*773;
      h(d) = a;
      swap(b);
      a -= 216;
      swap(b);
      a = m(b);
      a &= 60;
      h(d) = (h(d)+a+512)*773;
      ++d;
      a = m(c);
      a <<= (9&31);
      h(d) = a;
      ++d;
      ++d;
      ++d;
      ++d;
      ++d;
      h(d) = a;
      return;
    }

    // Not optimized
    default: run0(input);
  }
}

/////////////////////// Decoder ///////////////////////

Decoder::Decoder(ZPAQL& z):
  in(0), low(1), high(0xFFFFFFFF), curr(0), pr(z) {}

// Return next bit of decoded input, which has 16 bit probability p of being 1
int Decoder::decode(int p) {
  assert(p>=0 && p<65536);
  assert(high>low && low>0);
  if (curr<low || curr>high) error("archive corrupted");
  assert(curr>=low && curr<=high);
  U32 mid=low+((high-low)>>16)*p+((((high-low)&0xffff)*p)>>16); // split range
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
  if (curr==0) {  // segment initialization
    for (int i=0; i<4; ++i)
      curr=curr<<8|in->get();
  }
  if (decode(0)) {
    if (curr!=0) error("decoding end of stream");
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

// Find end of compressed data and return next byte
int Decoder::skip() {
  int c;
  while (curr==0)  // at start?
    curr=in->get();
  while (curr && (c=in->get())>=0)  // find 4 zeros
    curr=curr<<8|c;
  while ((c=in->get())==0) ;  // might be more than 4
  return c;
}

////////////////////// PostProcessor //////////////////////

// Copy ph, pm from block header
void PostProcessor::init(ZPAQL& hz) {
  state=hsize=0;
  ph=hz.header[4];
  pm=hz.header[5];
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
      if (z.output && c>=0) z.output->put(c);  // data
      if (z.sha1 && c>=0) z.sha1->put(c);
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
      assert(z.hend<z.header.size());
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

////////////////////// Encoder ////////////////////

// Initialize for start of block
void Encoder::init() {
  low=1;
  high=0xFFFFFFFF;
  pr.init();
}

// compress bit y having probability p/64K
void Encoder::encode(int y, int p) {
  assert(out);
  assert(p>=0 && p<65536);
  assert(y==0 || y==1);
  assert(high>low && low>0);
  U32 mid=low+((high-low)>>16)*p+((((high-low)&0xffff)*p)>>16); // split range
  assert(high>mid && mid>=low);
  if (y) high=mid; else low=mid+1; // pick half
  while ((high^low)<0x1000000) { // write identical leading bytes
    out->put(high>>24);  // same as low>>24
    high=high<<8|255;
    low=low<<8;
    low+=(low==0); // so we don't code 4 0 bytes in a row
  }
}

// compress byte c (0..255 or -1=EOS)
void Encoder::compress(int c) {
  assert(out);
  if (c==-1)
    encode(1, 0);
  else {
    assert(c>=0 && c<=255);
    encode(0, 0);
    for (int i=7; i>=0; --i) {
      int p=pr.predict()*2+1;
      assert(p>0 && p<65536);
      int y=c>>i&1;
      encode(y, p);
      pr.update(y);
    }
  }
}

///////////////////// Compressor //////////////////////

// Write 13 byte start tag
// "\x37\x6B\x53\x74\xA0\x31\x83\xD3\x8C\xB2\x28\xB0\xD3"
void Compressor::writeTag() {
  assert(state==INIT);
  enc.out->put(0x37);
  enc.out->put(0x6b);
  enc.out->put(0x53);
  enc.out->put(0x74);
  enc.out->put(0xa0);
  enc.out->put(0x31);
  enc.out->put(0x83);
  enc.out->put(0xd3);
  enc.out->put(0x8c);
  enc.out->put(0xb2);
  enc.out->put(0x28);
  enc.out->put(0xb0);
  enc.out->put(0xd3);
}

void Compressor::startBlock(int level) {
  if (level<1 || level>3) error("compression level must be 1, 2, or 3");
  const char* p=models;
  for (; level>1 && toU16(p); --level)
    p+=toU16(p)+2;
  startBlock(p);
}

// Memory reader
class MemoryReader: public Reader {
  const char* p;
public:
  MemoryReader(const char* p_): p(p_) {}
  int get() {return *p++&255;}
};

// Write a block header
void Compressor::startBlock(const char* hcomp) {
  assert(state==INIT);
  assert(hcomp);
  int len=toU16(hcomp)+2;
  enc.out->put('z');
  enc.out->put('P');
  enc.out->put('Q');
  enc.out->put(1);  // level
  enc.out->put(1);
  for (int i=0; i<len; ++i)  // write compression model hcomp
    enc.out->put(hcomp[i]);
  MemoryReader m(hcomp);
  z.read(&m);
  state=BLOCK1;
}

// Write a segment header
void Compressor::startSegment(const char* filename, const char* comment) {
  assert(state==BLOCK1 || state==BLOCK2);
  enc.out->put(1);
  while (filename && *filename)
    enc.out->put(*filename++);
  enc.out->put(0);
  while (comment && *comment)
    enc.out->put(*comment++);
  enc.out->put(0);
  enc.out->put(0);
  if (state==BLOCK1) state=SEG1;
  if (state==BLOCK2) state=SEG2;
}

// Initialize encoding and write pcomp to first segment
void Compressor::postProcess(const char* pcomp) {
  assert(state==SEG1);
  enc.init();
  if (pcomp) {
    enc.compress(1);
    int len=toU16(pcomp)+2;
    for (int i=0; i<len; ++i)
      enc.compress(pcomp[i]);
  }
  else
    enc.compress(0);
  state=SEG2;
}

// Compress n bytes, or to EOF if n <= 0
bool Compressor::compress(int n) {
  assert(state==SEG2);
  int ch=0;
  while (n && (ch=in->get())>=0) {
    enc.compress(ch);
    if (n>0) --n;
  }
  return ch>=0;
}

// End segment, write sha1string if present
void Compressor::endSegment(const char* sha1string) {
  assert(state==SEG2);
  enc.compress(-1);
  enc.out->put(0);
  enc.out->put(0);
  enc.out->put(0);
  enc.out->put(0);
  if (sha1string) {
    enc.out->put(253);
    for (int i=0; i<20; ++i)
      enc.out->put(sha1string[i]);
  }
  else
    enc.out->put(254);
  state=BLOCK2;
}

// End block
void Compressor::endBlock() {
  assert(state==BLOCK2);
  enc.out->put(255);
  state=INIT;
}

/////////////////////// Decompresser /////////////////////

// Find the start of a block and return true if found. Set memptr
// to memory used.
bool Decompresser::findBlock(double* memptr) {
  assert(state==INIT);

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
  if (dec.in->get()!=1) error("unsupported ZPAQ level");
  if (dec.in->get()!=1) error("unsupported ZPAQL type");
  z.read(dec.in);
  dec.init();
  pp.init(z);
  if (memptr) *memptr=z.memory();
  state=BLOCK;
  return true;
}

// Read the start of a segment (1) or end of block code (255).
// If a segment is found, write the filename and return true, else false.
bool Decompresser::findFilename(Writer* filename) {
  assert(state==BLOCK || state==BLOCKSKIP);
  int c=dec.in->get();
  if (c==1) {  // segment found
    while (true) {
      c=dec.in->get();
      if (c==-1) error("unexpected EOF");
      if (c==0) {
        if (state==BLOCK) state=SEG1;
        if (state==BLOCKSKIP) state=SEG1SKIP;
        return true;
      }
      if (filename) filename->put(c);
    }
  }
  else if (c==255) {  // end of block found
    state=INIT;
    return false;
  }
  else
    error("missing segment or end of block");
  return false;
}

// Read the comment from the segment header
void Decompresser::readComment(Writer* comment) {
  assert(state==SEG1 || state==SEG1SKIP);
  if (state==SEG1) state=SEG2;
  if (state==SEG1SKIP) state=SEG2SKIP;
  while (true) {
    int c=dec.in->get();
    if (c==-1) error("unexpected EOF");
    if (c==0) break;
    if (comment) comment->put(c);
  }
  if (dec.in->get()!=0) error("missing reserved byte");
}

// Decompress n bytes, or all if n < 0. Return false if done
bool Decompresser::decompress(int n) {
  assert(state==SEG2);

  // Decompress and load PCOMP into postprocessor
  while ((pp.getState()&3)!=1)
    pp.write(dec.decompress());

  // Decompress n bytes, or all if n < 0
  while (n) {
    int c=dec.decompress();
    pp.write(c);
    if (c==-1) {
      state=SEGEND;
      return false;
    }
    if (n>0) --n;
  }
  return true;
}

// Read end of block. If a SHA1 checksum is present, write 1 and the
// 20 byte checksum into sha1string, else write 0 in first byte.
// If sha1string is 0 then discard it.
void Decompresser::readSegmentEnd(char* sha1string) {
  assert(state==SEGEND || state==SEG2 || state==SEG2SKIP);

  // Skip remaining data if any and get next byte
  int c=0;
  if (state==SEG2 || state==SEG2SKIP) {
    c=dec.skip();
    state=BLOCKSKIP;
  }
  else if (state==SEGEND) {
    c=dec.in->get();
    state=BLOCK;
  }

  // Read checksum
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
}

/////////////////////////// compress() ///////////////////////

void compress(Reader* in, Writer* out, int level) {
  assert(level>=1 && level<=3);
  Compressor c;
  c.setInput(in);
  c.setOutput(out);
  c.startBlock(level);
  c.startSegment();
  c.postProcess();
  c.compress();
  c.endSegment();
  c.endBlock();
}

/////////////////////////// decompress() /////////////////////

void decompress(Reader* in, Writer* out) {
  Decompresser d;
  d.setInput(in);
  d.setOutput(out);
  while (d.findBlock()) {       // don't calculate memory
    while (d.findFilename()) {  // discard filename
      d.readComment();          // discard comment
      d.decompress();           // to end of segment
      d.readSegmentEnd();       // discard sha1string
    }
  }
}

}  // end namespace libzpaq
