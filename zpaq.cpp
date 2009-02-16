/*  zpaq v0.01 archiver and file compressor.

(C) 2009, Ocarina Networks, Inc.
    Written by Matt Mahoney, matmahoney@yahoo.com, Feb. 15, 2009.

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

Usage: zpaq command [archive [files...]]  Commands are:

  a - Compress files and create or append to archive.
      Store file names as given on command line.

  c - Create, overwriting archive.

  aconfig, cconfig - Use compression options in file config

  x - Extract and uncompress files. If no file names are given, then
      extract all files using the stored names. Skips (does not overwrite)
      existing files. If one or more file names are given, then extract
      that number of files and rename them, overwriting existing files,
      and ignoring any remaining files in the archive.

  l - List contents.

Each "a" command appends one block with one segment per file.
See http://cs.fit.edu/~mmahoney/compression/zpaq1.pdf

Versions before 1.00 are in the experimental ZPAQ level 0 format, which
is not compatible with level 1 or other level 0 versions.

Options for development:

  t - Extract like x but without post-processing (for debugging).

  hconfig args... - Compile and then run config once for each numeric
      argument. (If no args, then just compile and check for errors).
      As the program runs, show the instructions being executed
      and the register contents. After each HALT, show the contents of memory.
      The program is initialized as a context hash, using hh and hm as
      the initial sizes of H and M.

  pconfig [input [output]] - Compile and run pconfig as a postprocessor
      (using ph and pm as the initial sizes of H and M). Run the
      program for each byte of input and write it to output. If
      output is omitted, write to stdout. If input is omitted, read
      from stdin.

The configuration file has the following format:

  COMP hh hm ph pm n
    (n component descriptions)
  HCOMP
    (program to generate contexts, size = hh, hm)
  POST
    (preprocessor command, size = ph, pm)
  END

The format is a sequence of tokens separated by arbitrary white space
and comments in parenthesis (which may be (nested)). Tokens are not
case sensitive. Numeric values are mod 256. For example:

  comp 0 0 0 0 1
    0 cm 20 12
  hcomp
    *d<>a a+=*d a*=192 *d=a
    halt
  post
    pass
  end

The meaning is as follows:

- COMP hh hm ph pm n

hh and hm are the log2 sizes of H and M in HCOMP for computing contexts.
ph and pm are the log2 sizes of H and M in PCOMP for post-processing.
There are n components numbered i = 0 to n-1. Possible components are:

- i CONST c
- i CM sizebits limit
- i ICM sizebits
- i MATCH sizebits
- i AVG j k wt
- i MIX2 sizebits j k rate mask
- i MIX sizebits j m rate mask
- i IMIX2 sizebits j k wt rate
- i SSE sizebits j start limit mask

All argments are numbers in 0...255 except sizebits in (0...31),
j, k in (0...i-1), m in (1...i-j).

- HCOMP - describes the program that computes context hashes.
Instructions have the forms:

- L=R
  - where L is A B C D *B *C *D
  - where R is A B C D *B *C *D (0...255)
- AxR
  - where x is += -= *= /= %= &= &~ |= ^= <<= >>= == < >
  - R as before
- Ly
  - L as before
  - where y is <>A ++ -- ! =0
  - except A<>A is not valid.
- J Z
  - where J is JT JF JMP
  - where Z is a number in (-128...127)
- S.=N
  - where S is A B C D *D
  - where N is a number in (0...255)
- ERROR
- HALT
- OUT
- JMP
- HASH
- HASHD

The only POST command is PASS, which does no post-processing.

To compile: g++ -O3 -march=pentiumpro -fomit-frame-pointer -s zpaq.cpp -o zpaq
To turn off assertion checking (faster), compile with -DNDEBUG
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <math.h>
#include <assert.h>

const int LEVEL=0;  // ZPAQ level 0=experimental 1=final

// 1, 2, 4, 8 byte unsigned integers
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
  T& operator[](int i) {
    if (i<0||i>n) fprintf(stderr, "i=%d n=%d offset=%d data=%p\n",i,n,offset,data);
    assert(n>=0 && i>=0 && i<=n); return data[i];}
  T& operator()(int i) {assert(n>=0 && (n&n+1)==0); return data[i&n];}
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

//////////////////////////// ZPAQL //////////////////////////////

// Symbolic constants, instruction size, and names
typedef enum {NONE,CONST,CM,ICM,MATCH,AVG,MIX2,MIX,IMIX2,SSE} CompType;
static const int compsize[256]={0,2,3,2,2,4,6,6,6,6};
static const char* compname[]=
  {"","const","cm","icm","match","avg","mix2","mix","imix2","sse",0};

// Opcodes from ZPAQ spec, table 1, with "X=0" replaced with "X=".
static const char* opcodelist[258]={
"error","a++",  "a--",  "a!",   "a=",   "",     "",     "a.=",
"b<>a", "b++",  "b--",  "b!",   "b=",   "",     "",     "b.=",
"c<>a", "c++",  "c--",  "c!",   "c=",   "",     "",     "c.=",
"d<>a", "d++",  "d--",  "d!",   "d=",   "",     "",     "d.=",
"*b<>a","*b++", "*b--", "*b!",  "*b=",  "",     "",     "jt",
"*c<>a","*c++", "*c--", "*c!",  "*c=",  "",     "",     "jf",
"*d<>a","*d++", "*d--", "*d!",  "*d=",  "",     "",     "*d.=",
"halt", "out",  "",     "hash", "hashd","",     "",     "jmp",
"a=a",  "a=b",  "a=c",  "a=d",  "a=*b", "a=*c", "a=*d", "a=",
"b=a",  "b=b",  "b=c",  "b=d",  "b=*b", "b=*c", "b=*d", "b=",
"c=a",  "c=b",  "c=c",  "c=d",  "c=*b", "c=*c", "c=*d", "c=",
"d=a",  "d=b",  "d=c",  "d=d",  "d=*b", "d=*c", "d=*d", "d=",
"*b=a", "*b=b", "*b=c", "*b=d", "*b=*b","*b=*c","*b=*d","*b=",
"*c=a", "*c=b", "*c=c", "*c=d", "*c=*b","*c=*c","*c=*d","*c=",
"*d=a", "*d=b", "*d=c", "*d=d", "*d=*b","*d=*c","*d=*d","*d=",
"",     "",     "",     "",     "",     "",     "",     "",
"a+=a", "a+=b", "a+=c", "a+=d", "a+=*b","a+=*c","a+=*d","a+=",
"a-=a", "a-=b", "a-=c", "a-=d", "a-=*b","a-=*c","a-=*d","a-=",
"a*=a", "a*=b", "a*=c", "a*=d", "a*=*b","a*=*c","a*=*d","a*=",
"a/=a", "a/=b", "a/=c", "a/=d", "a/=*b","a/=*c","a/=*d","a/=",
"a%=a", "a%=b", "a%=c", "a%=d", "a%=*b","a%=*c","a%=*d","a%=",
"a&=a", "a&=b", "a&=c", "a&=d", "a&=*b","a&=*c","a&=*d","a&=",
"a&~a", "a&~b", "a&~c", "a&~d", "a&~*b","a&~*c","a&~*d","a&~",
"a|=a", "a|=b", "a|=c", "a|=d", "a|=*b","a|=*c","a|=*d","a|=",
"a^=a", "a^=b", "a^=c", "a^=d", "a^=*b","a^=*c","a^=*d","a^=",
"a<<=a","a<<=b","a<<=c","a<<=d","a<<=*b","a<<=*c","a<<=*d","a<<=",
"a>>=a","a>>=b","a>>=c","a>>=d","a>>=*b","a>>=*c","a>>=*d","a>>=",
"a==a", "a==b", "a==c", "a==d", "a==*b","a==*c","a==*d","a==",
"a<a",  "a<b",  "a<c",  "a<d",  "a<*b", "a<*c", "a<*d", "a<",
"a>a",  "a>b",  "a>c",  "a>d",  "a>*b", "a>*c", "a>*d", "a>",
"",     "",     "",     "",     "",     "",     "",     "",
"",     "",     "",     "",     "",     "",     "",     "",
"post", 0};

// A ZPAQL machine HCOMP or PCOMP.
class ZPAQL {
public:
  ZPAQL();
  void read(FILE* in);    // Read header from archive
  void write(FILE* out);  // Write header to archive
  void compile(FILE* in); // Create header from config file
  void list();            // Display header contents
  void inith();           // Initialize as HCOMP
  void initp();           // Initialize as PCOMP
  void run(U32 input);    // Execute with input
  void step(U32 input);   // Execute while displaying progress
  double memory();        // Return memory requirement in bytes
  FILE* output;           // Destination for OUT instruction, or 0 to suppress
  bool verbose;           // Show config file during compile?
  friend class Predictor;
private:

  // ZPAQ1 block header
  int hsize;          // Header size
  Array<U8> header;   // hsize[2] hh hm ph pm n COMP (guard) HCOMP (guard)
  int cend;           // COMP in header[7...cend-1] (empty for PCOMP)
  int hbegin, hend;   // HCOMP in header[hbegin...hend-1]

  // Machine state for executing HCOMP
  Array<U8> m;        // memory array M for HCOMP
  Array<U32> h;       // hash array H for HCOMP
  U32 a, b, c, d;     // machine registers
  int f;              // condition flag
  int pc;             // program counter

  // Support code
  void init(int hbits, int mbits);  // initialize H and M sizes
  const char* token(FILE* in);  // read and print a token or 0 at EOF
  int rtoken(FILE* in, const char* list[]=0);  // read token in list -> index
  void rtoken(FILE* in, const char* s);  // read a token that must be s
  int rtoken(FILE* in, int low, int high);  // read token in low...high
  int execute();  // execute 1 instruction, return 0 after HALT, else 1
  void div(U32 x) {if (x) a/=x; else a=0;}
  void mod(U32 x) {if (x) a%=x; else a=0;}
  void swap(U32& x) {a^=x; x^=a; a^=x;}
  void swap(U8& x)  {a^=x; x^=a; a^=x;}
  void err();  // exit with run time error
};

// Constructor
ZPAQL::ZPAQL() {
  hsize=cend=hbegin=hend=0;
  verbose=true;
  output=0;
}

// Read header
void ZPAQL::read(FILE* in) {
  assert(in);

  // Get header size and allocate
  hsize=getc(in);
  hsize+=getc(in)*256;
  header.resize(hsize+300);
  cend=hbegin=hend=0;
  header[cend++]=hsize&255;
  header[cend++]=hsize>>8;
  while (cend<7) header[cend++]=getc(in); // hh hm ph pm n

  // Read COMP
  int n=header[cend-1];
  for (int i=0; i<n; ++i) {
    int type=getc(in);  // component type
    if (type==EOF) error("unexpected end of file");
    header[cend++]=type;  // component type
    int size=compsize[type];
    if (size<1) error("Invalid component type");
    if (cend+size>header.size()-8) error("COMP list too big");
    for (int j=1; j<size; ++j)
      header[cend++]=getc(in);
  }
  if ((header[cend++]=getc(in))!=0) error("missing COMP END");

  // Insert a guard gap and read HCOMP
  hbegin=hend=cend+128;
  while (hend<hsize+129) {
    assert(hend<header.size()-8);
    int op=getc(in);
    if (op==EOF) error("unexpected end of file");
    header[hend++]=op;
    if ((op&7)==7) header[hend++]=getc(in);
  }
  if ((header[hend++]=getc(in))!=0) error("missing HCOMP END");
  if (hend!=hsize+130) error("opcode straddles end");

  assert(cend>=7 && cend<header.size());
  assert(hbegin==cend+128 && hbegin<header.size());
  assert(hend>hbegin && hend<header.size());
  assert(hsize==header[0]+256*header[1]);
  assert(hsize==cend-2+hend-hbegin);
}

// Write header
void ZPAQL::write(FILE* out) {
  assert(out);
  assert(cend>=7 && cend<header.size());
  assert(hbegin==cend+128 && hbegin<header.size());
  assert(hend>hbegin && hend<header.size());
  assert(hsize==header[0]+256*header[1]);
  assert(hsize==cend-2+hend-hbegin);
  fwrite(&header[0], 1, cend, out);
  fwrite(&header[hbegin], 1, hend-hbegin, out);
}

// Compile a configuration file and store the result in header
void ZPAQL::compile(FILE* in) {

  // Allocate header
  header.resize(0x11000); // includes hsize
 
  // Compile the COMP section of header
  cend=hbegin=hend=2;
  rtoken(in, "comp");
  header[cend++]=rtoken(in, 0, 255); // hh
  header[cend++]=rtoken(in, 0, 255); // hm
  header[cend++]=rtoken(in, 0, 255); // ph
  header[cend++]=rtoken(in, 0, 255); // pm
  int n=header[cend++]=rtoken(in, 0, 255); // n
  if (verbose) printf("\n");
  for (int i=0; i<n; ++i) {
    if (verbose) printf("  ");
    rtoken(in, i, i);
    CompType type=CompType(header[cend++]=rtoken(in, compname));
    int clen=compsize[type];
    assert(clen>0 && clen<10);
    for (int j=1; j<clen; ++j)
      header[cend++]=rtoken(in, 0, 255);
    if (verbose) printf("\n");
  }
  header[cend++]=0; // END

  // Compile HCOMP
  hbegin=hend=cend+128;  // leave a guard gap to catch backwards jumps
  rtoken(in, "hcomp");
  if (verbose) printf("\n");
  while (hend<0x10000) {
    if (verbose) printf("(%4d) ", hend-hbegin);
    int op=rtoken(in, opcodelist);
    if (op==256) break;  // POST
    int operand=-1; // 0...255 if 2 bytes
    if (op<56 && (op&7)==4) { // "L=0" (1 byte) or "L=(1...255)" 2 bytes
      int n=rtoken(in, 0, 255);
      if (n>0) operand=n, op+=67;
    }
    else if ((op&7)==7) { // 2 byte operand, read N
      if (op==39 || op==47 || op==63) { // JT, JF, JMP
        operand=rtoken(in, -128, 127);
        if (verbose) printf("(to %d) ", hend-hbegin+2+operand);
        operand&=255;
      }
      else
        operand=rtoken(in, 0, 255);
    }
    if (verbose) {
      if (operand>=0)
        printf("(%d %d)\n", op, operand);
      else
        printf("(%d)\n", op);
    }
    header[hend++]=op;
    if (operand>=0)
      header[hend++]=operand;
  }
  header[hend++]=0; // END
  if (hend>=0x10000) printf("\nProgram too big\n"), exit(1);

  // Compute header size
  hsize=hend-hbegin+cend-2;
  header[0]=hsize&255;
  header[1]=hsize>>8;
  if (verbose) {
    printf("(cend=%d hbegin=%d hend=%d hsize=%d Memory=%1.3f MB)\n\n", 
      cend, hbegin, hend, hsize, memory()/1000000);
  }
}

// Display header contents. Assume it is constructed correctly.
void ZPAQL::list() {
  assert(cend>=7 && cend<header.size());
  assert(hbegin==cend+128 && hbegin<header.size());
  assert(hend>hbegin && hend<header.size());
  assert(hsize==header[0]+256*header[1]);
  assert(hsize==cend-2+hend-hbegin);

  // Display COMP section
  printf("comp %d %d %d %d %d (hh hm ph pm n, header size=%d)\n",
    header[2], header[3], header[4], header[5], header[6], hsize);
  printf("  (Memory requirement: %1.3f MB)\n", memory()/1000000);
  int h=7;
  for (int i=0; i<header[6]; ++i) {
    int size=compsize[header[h]];
    assert(size>0);
    assert(h+size<header.size());
    printf("  %d %s", i, compname[header[h]]);
    for (int j=1; j<size; ++j)
      printf(" %d", header[h+j]);
    printf("\n");
    h+=size;
  }
  assert(h<header.size() && header[h]==0);
  ++h;
  assert(h==cend);

  // Display HCOMP section
  h+=128;  // skip guard
  assert(h==hbegin);
  printf("hcomp\n");
  while (h<hend-1) {
    assert(h<header.size()-2);
    int op=header[h];
    printf("(%4d) %s", h++-hbegin, opcodelist[op]);
    if (op<56 && (op&7)==4) printf("0");
    if ((op&7)==7) {
      printf(" %d", header[h++]);
      if (op==39 || op==47 || op==63) // JT, JF, JMP
        printf(" (to %d) ", h-hbegin+(int(header[h-1])<<24>>24));
    }
    printf("\n");
  }
  assert(header[h]==0);
  ++h;
  assert(h==hend);
  printf("post\nend\n");
}

// Initialize machine state as HCOMP
void ZPAQL::inith() {
  init(header[2], header[3]); // hh, hm
}

// Initialize machine state as PCOMP
void ZPAQL::initp() {
  init(header[4], header[5]);  // ph, pm
}

// Initialize machine state
void ZPAQL::init(int hbits, int mbits) {
  assert(h.size()==0);
  assert(m.size()==0);
  h.resize(1, hbits);
  m.resize(1, mbits);
  a=b=c=d=pc=f=0;
}

// Run program on input
void ZPAQL::run(U32 input) {
  assert(cend>6);
  assert(hbegin==cend+128);
  assert(hend>hbegin);
  assert(hend<header.size()-130);
  assert(m.size()>0);
  assert(h.size()>0);
  pc=hbegin;
  a=input;
  while (execute());
}

// Execute program input and show progress
void ZPAQL::step(U32 input) {
  assert(cend>6);
  assert(hbegin==cend+128);
  assert(hend>hbegin);
  assert(hend<header.size()-130);
  assert(m.size()>0);
  assert(h.size()>0);
  pc=hbegin;
  a=input;
  printf(
  "  pc   opcode  f      a          b      *b      c      *c      d         *d\n"
  "----- -------- - ---------- ---------- --- ---------- --- ---------- ----------\n");
  printf("               %d %10u %10u %3u %10u %3u %10u %10u\n",
    f, a, b, m(b), c, m(c), d, h(d));
  while (1) {
    assert(pc>=cend && pc<header.size());
    int op=header[pc];
    printf("%5d ", pc-hbegin);
    char inst[16];
    if ((op&7)==7)
      sprintf(inst, "%s %d", opcodelist[op], header[pc+1]);
    else if (op<56 && (op&7)==4)
      sprintf(inst, "%s0", opcodelist[op]);
    else
      sprintf(inst, "%s", opcodelist[op]);
    printf("%-8s", inst);
    if (!execute()) break;
    printf(" %d %10u %10u %3u %10u %3u %10u %10u\n",
      f, a, b, m(b), c, m(c), d, h(d));
  }

  // Dump memory
  printf("\n\nH (size %d) =", h.size());
  for (int i=0; i<h.size(); ++i) {
    if (i%5==0) printf("\n%8d:", i);
    printf(" %10u", h[i]);
  }
  printf("\n\nM (size %d) =", m.size());
  for (int i=0; i<m.size(); ++i) {
    if (i%10==0) printf("\n%8d:", i);
    printf(" %3d", m[i]);
  }
  printf("\n\n");
}

// Return memory requirement in bytes
double ZPAQL::memory() {
  double mem=pow(2,header[2]+2)+pow(2,header[3])  // hh hm
            +pow(2,header[4]+2)+pow(2,header[5])  // ph pm
            +header.size();
  int cp=7;  // start of comp list
  for (int i=0; i<header[6]; ++i) {  // n
    assert(cp<cend);
    double size=pow(2, header[cp+1]); // sizebits
    switch(header[cp]) {
      case CM: mem+=4*size; break;
      case ICM: mem+=64*size+1024; break;
      case MATCH:
      case MIX2: mem+=8*size; break;
      case MIX: mem+=4*size*header[cp+3]; break; // m
      case IMIX2: mem+=64*size+2048; break;
      case SSE: mem+=128*size; break;
    }
    cp+=compsize[header[cp]];
  }
  return mem;
}

// Read a token and return it, or return 0 at EOF. Skip (comments).
// Convert to lower case. Print the token read.
const char* ZPAQL::token(FILE* in) {
  static char s[16];  // result
  int len=0;  // length of s

  // skip to start of token
  int paren=0, c=0;
  while (c<=' ' || paren>0) {
    c=getc(in);
    if (c=='(') ++paren;
    if (c==')') --paren, c=' ';
    if (c==EOF) return 0;
  }

  // read token
  if (isupper(c)) c=tolower(c);
  s[len++]=c;
  while (len<15 && (c=getc(in))!=EOF && c>' ') {
    if (isupper(c)) c=tolower(c);
    if (c>='0' && c<='9' && len>1 && !isalnum(s[len-1]) && s[len-1]!='-') {
      ungetc(c, in);  // A number is a separate token
      break;
    }
    s[len++]=c;
  }
  s[len++]=0;
  if (verbose) printf("%s ", s);
  return s;
}

// Read a token, which must be in the NULL terminated list or else
// exit with an error. If found, return its index.
int ZPAQL::rtoken(FILE* in, const char* list[]) {
  assert(in);
  assert(list);
  const char* tok=token(in);
  if (!tok) fprintf(stderr, "\nUnexpected end of configuration file\n"), exit(1);
  for (int i=0; list[i]; ++i)
    if (!strcmp(list[i], tok))
      return i;
  fprintf(stderr, "\nConfiguration file error at %s\n", tok), exit(1);
  assert(0);
  return -1; // not reached
}

// Read a token which must be the specified value s
void ZPAQL::rtoken(FILE* in, const char* s) {
  assert(s);
  const char* t=token(in);
  if (!t) fprintf(stderr, "\nExpected %s, found EOF\n", s), exit(1);
  if (strcmp(s, t)) fprintf(stderr, "\nExpected %s, found %s\n", s, t), exit(1);
}

// Read a number in (low...high) or exit with an error
int ZPAQL::rtoken(FILE* in, int low, int high) {
  const char* tok=token(in);
  if (!tok) fprintf(stderr, "\nUnexpected end of configuration file\n"), exit(1);
  int n=0;
  const char* p=tok;
  int sign=1;
  if (*p=='-') sign=-1, ++p;
  while (*p) {
    if (isdigit(*p))
      n=n*10+*p-'0';
    else
      fprintf(stderr, "\nConfiguration file error at %s: expected a number\n", tok),
      exit(1);
    ++p;
  }
  n*=sign;
  if (n>=low && n<=high)
    return n;
  fprintf(stderr, "\nConfiguration file error: expected (%d...%d), found %d\n",
    low, high, n);
  exit(1);
  return 0;
}

// Execute one instruction, return 0 after HALT else 1
inline int ZPAQL::execute() {

/* Switch statement below generared by the PERL script shown here.
   The input is a 256 byte text file pasted from table 1 of the ZPAQ spec
   with one opcode per line.

#!/usr/bin/perl
$go="pc+=(header[pc]+128&255)-127";
$code=-1;
print"  switch(header[pc++]) {\n";
while (<>) {
 chomp;
 $code++;
 if ($_ ne "") {
  $comment=$_;
  if   (/^(\*?[ABCD])(\W*)(\*[ABCDN0])$/) {($a,$op,$b)=($1,$2,$3);}
  elsif (/^(\*?[ABCD])(\W*)([ABCDN0])$/) {($a,$op,$b)=($1,$2,$3);}
  elsif (/^(\*?[ABCD])(\W*)$/) {($a,$op,$b)=($1,$2);}
  else {($a,$op,$b)=($_);}
  $a=~tr/A-Z/a-z/;
  $b=~tr/A-Z/a-z/;
  $a=~s/\*([bc])/m($1)/;
  $b=~s/\*([bc])/m($1)/;
  $a=~s/\*d/h(d)/;
  $b=~s/\*d/h(d)/;
  $b=~s/n/header[pc++]/;
  $op=~s/&~/&= ~/;
  $a=~s/error//;
  $a=~s/halt/return 0/;
  print("    case $code: ");
  if ($a eq "jt n") {print"if (f) $go; else ++pc;";}
  elsif ($a eq "jf n") {print"if (!f) $go; else ++pc;";}
  elsif ($a eq "jmp n") {print"$go;";}
  elsif ($a eq "out") {print"if (output) putc(a, output);";}
  elsif ($a eq "hash") {print"a = (a+m(b)+512)*773;"}
  elsif ($a eq "hashd") {print"h(d) = (h(d)+a+512)*773;"}
  elsif ($op eq "<>") {print"swap($a);";}
  elsif ($op eq "==" || $op eq "<" || $op eq ">") {print"f = ($a $op $b);";}
  elsif ($op eq "++" || $op eq "--") {print"$op$a;";}
  elsif ($op eq "!") {print"$a = ~$a;";}
  elsif ($op eq ".=") {print"$a = ($a<<8)+$b;";}
  elsif ($op eq "/=") {print"div($b);";}
  elsif ($op eq "%=") {print"mod($b);";}
  elsif ($a) {print("$a $op $b;");}
  else {print"err();";}
  if ($a ne "return") {print" break;"}
  if ($comment eq "") {$comment="undefined";}
  print" // $comment\n";
 }
}
print"    default: err();\n  }\n";
*/

  switch(header[pc++]) {
    case 0: err(); break; // ERROR
    case 1: ++a; break; // A++
    case 2: --a; break; // A--
    case 3: a = ~a; break; // A!
    case 4: a = 0; break; // A=0
    case 7: a = (a<<8)+header[pc++]; break; // A.=N
    case 8: swap(b); break; // B<>A
    case 9: ++b; break; // B++
    case 10: --b; break; // B--
    case 11: b = ~b; break; // B!
    case 12: b = 0; break; // B=0
    case 15: b = (b<<8)+header[pc++]; break; // B.=N
    case 16: swap(c); break; // C<>A
    case 17: ++c; break; // C++
    case 18: --c; break; // C--
    case 19: c = ~c; break; // C!
    case 20: c = 0; break; // C=0
    case 23: c = (c<<8)+header[pc++]; break; // C.=N
    case 24: swap(d); break; // D<>A
    case 25: ++d; break; // D++
    case 26: --d; break; // D--
    case 27: d = ~d; break; // D!
    case 28: d = 0; break; // D=0
    case 31: d = (d<<8)+header[pc++]; break; // D.=N
    case 32: swap(m(b)); break; // *B<>A
    case 33: ++m(b); break; // *B++
    case 34: --m(b); break; // *B--
    case 35: m(b) = ~m(b); break; // *B!
    case 36: m(b) = 0; break; // *B=0
    case 39: if (f) pc+=(header[pc]+128&255)-127; else ++pc; break; // JT N
    case 40: swap(m(c)); break; // *C<>A
    case 41: ++m(c); break; // *C++
    case 42: --m(c); break; // *C--
    case 43: m(c) = ~m(c); break; // *C!
    case 44: m(c) = 0; break; // *C=0
    case 47: if (!f) pc+=(header[pc]+128&255)-127; else ++pc; break; // JF N
    case 48: swap(h(d)); break; // *D<>A
    case 49: ++h(d); break; // *D++
    case 50: --h(d); break; // *D--
    case 51: h(d) = ~h(d); break; // *D!
    case 52: h(d) = 0; break; // *D=0
    case 55: h(d) = (h(d)<<8)+header[pc++]; break; // *D.=N
    case 56: return 0; // HALT
    case 57: if (output) putc(a, output); break; // OUT
    case 59: a = (a+m(b)+512)*773; break; // HASH
    case 60: h(d) = (h(d)+a+512)*773; break; // HASHD
    case 63: pc+=(header[pc]+128&255)-127; break; // JMP N
    case 64: a = a; break; // A=A
    case 65: a = b; break; // A=B
    case 66: a = c; break; // A=C
    case 67: a = d; break; // A=D
    case 68: a = m(b); break; // A=*B
    case 69: a = m(c); break; // A=*C
    case 70: a = h(d); break; // A=*D
    case 71: a = header[pc++]; break; // A=N
    case 72: b = a; break; // B=A
    case 73: b = b; break; // B=B
    case 74: b = c; break; // B=C
    case 75: b = d; break; // B=D
    case 76: b = m(b); break; // B=*B
    case 77: b = m(c); break; // B=*C
    case 78: b = h(d); break; // B=*D
    case 79: b = header[pc++]; break; // B=N
    case 80: c = a; break; // C=A
    case 81: c = b; break; // C=B
    case 82: c = c; break; // C=C
    case 83: c = d; break; // C=D
    case 84: c = m(b); break; // C=*B
    case 85: c = m(c); break; // C=*C
    case 86: c = h(d); break; // C=*D
    case 87: c = header[pc++]; break; // C=N
    case 88: d = a; break; // D=A
    case 89: d = b; break; // D=B
    case 90: d = c; break; // D=C
    case 91: d = d; break; // D=D
    case 92: d = m(b); break; // D=*B
    case 93: d = m(c); break; // D=*C
    case 94: d = h(d); break; // D=*D
    case 95: d = header[pc++]; break; // D=N
    case 96: m(b) = a; break; // *B=A
    case 97: m(b) = b; break; // *B=B
    case 98: m(b) = c; break; // *B=C
    case 99: m(b) = d; break; // *B=D
    case 100: m(b) = m(b); break; // *B=*B
    case 101: m(b) = m(c); break; // *B=*C
    case 102: m(b) = h(d); break; // *B=*D
    case 103: m(b) = header[pc++]; break; // *B=N
    case 104: m(c) = a; break; // *C=A
    case 105: m(c) = b; break; // *C=B
    case 106: m(c) = c; break; // *C=C
    case 107: m(c) = d; break; // *C=D
    case 108: m(c) = m(b); break; // *C=*B
    case 109: m(c) = m(c); break; // *C=*C
    case 110: m(c) = h(d); break; // *C=*D
    case 111: m(c) = header[pc++]; break; // *C=N
    case 112: h(d) = a; break; // *D=A
    case 113: h(d) = b; break; // *D=B
    case 114: h(d) = c; break; // *D=C
    case 115: h(d) = d; break; // *D=D
    case 116: h(d) = m(b); break; // *D=*B
    case 117: h(d) = m(c); break; // *D=*C
    case 118: h(d) = h(d); break; // *D=*D
    case 119: h(d) = header[pc++]; break; // *D=N
    case 128: a += a; break; // A+=A
    case 129: a += b; break; // A+=B
    case 130: a += c; break; // A+=C
    case 131: a += d; break; // A+=D
    case 132: a += m(b); break; // A+=*B
    case 133: a += m(c); break; // A+=*C
    case 134: a += h(d); break; // A+=*D
    case 135: a += header[pc++]; break; // A+=N
    case 136: a -= a; break; // A-=A
    case 137: a -= b; break; // A-=B
    case 138: a -= c; break; // A-=C
    case 139: a -= d; break; // A-=D
    case 140: a -= m(b); break; // A-=*B
    case 141: a -= m(c); break; // A-=*C
    case 142: a -= h(d); break; // A-=*D
    case 143: a -= header[pc++]; break; // A-=N
    case 144: a *= a; break; // A*=A
    case 145: a *= b; break; // A*=B
    case 146: a *= c; break; // A*=C
    case 147: a *= d; break; // A*=D
    case 148: a *= m(b); break; // A*=*B
    case 149: a *= m(c); break; // A*=*C
    case 150: a *= h(d); break; // A*=*D
    case 151: a *= header[pc++]; break; // A*=N
    case 152: div(a); break; // A/=A
    case 153: div(b); break; // A/=B
    case 154: div(c); break; // A/=C
    case 155: div(d); break; // A/=D
    case 156: div(m(b)); break; // A/=*B
    case 157: div(m(c)); break; // A/=*C
    case 158: div(h(d)); break; // A/=*D
    case 159: div(header[pc++]); break; // A/=N
    case 160: mod(a); break; // A%=A
    case 161: mod(b); break; // A%=B
    case 162: mod(c); break; // A%=C
    case 163: mod(d); break; // A%=D
    case 164: mod(m(b)); break; // A%=*B
    case 165: mod(m(c)); break; // A%=*C
    case 166: mod(h(d)); break; // A%=*D
    case 167: mod(header[pc++]); break; // A%=N
    case 168: a &= a; break; // A&=A
    case 169: a &= b; break; // A&=B
    case 170: a &= c; break; // A&=C
    case 171: a &= d; break; // A&=D
    case 172: a &= m(b); break; // A&=*B
    case 173: a &= m(c); break; // A&=*C
    case 174: a &= h(d); break; // A&=*D
    case 175: a &= header[pc++]; break; // A&=N
    case 176: a &= ~ a; break; // A&~A
    case 177: a &= ~ b; break; // A&~B
    case 178: a &= ~ c; break; // A&~C
    case 179: a &= ~ d; break; // A&~D
    case 180: a &= ~ m(b); break; // A&~*B
    case 181: a &= ~ m(c); break; // A&~*C
    case 182: a &= ~ h(d); break; // A&~*D
    case 183: a &= ~ header[pc++]; break; // A&~N
    case 184: a |= a; break; // A|=A
    case 185: a |= b; break; // A|=B
    case 186: a |= c; break; // A|=C
    case 187: a |= d; break; // A|=D
    case 188: a |= m(b); break; // A|=*B
    case 189: a |= m(c); break; // A|=*C
    case 190: a |= h(d); break; // A|=*D
    case 191: a |= header[pc++]; break; // A|=N
    case 192: a ^= a; break; // A^=A
    case 193: a ^= b; break; // A^=B
    case 194: a ^= c; break; // A^=C
    case 195: a ^= d; break; // A^=D
    case 196: a ^= m(b); break; // A^=*B
    case 197: a ^= m(c); break; // A^=*C
    case 198: a ^= h(d); break; // A^=*D
    case 199: a ^= header[pc++]; break; // A^=N
    case 200: a <<= a; break; // A<<=A
    case 201: a <<= b; break; // A<<=B
    case 202: a <<= c; break; // A<<=C
    case 203: a <<= d; break; // A<<=D
    case 204: a <<= m(b); break; // A<<=*B
    case 205: a <<= m(c); break; // A<<=*C
    case 206: a <<= h(d); break; // A<<=*D
    case 207: a <<= header[pc++]; break; // A<<=N
    case 208: a >>= a; break; // A>>=A
    case 209: a >>= b; break; // A>>=B
    case 210: a >>= c; break; // A>>=C
    case 211: a >>= d; break; // A>>=D
    case 212: a >>= m(b); break; // A>>=*B
    case 213: a >>= m(c); break; // A>>=*C
    case 214: a >>= h(d); break; // A>>=*D
    case 215: a >>= header[pc++]; break; // A>>=N
    case 216: f = (a == a); break; // A==A
    case 217: f = (a == b); break; // A==B
    case 218: f = (a == c); break; // A==C
    case 219: f = (a == d); break; // A==D
    case 220: f = (a == m(b)); break; // A==*B
    case 221: f = (a == m(c)); break; // A==*C
    case 222: f = (a == h(d)); break; // A==*D
    case 223: f = (a == header[pc++]); break; // A==N
    case 224: f = (a < a); break; // A<A
    case 225: f = (a < b); break; // A<B
    case 226: f = (a < c); break; // A<C
    case 227: f = (a < d); break; // A<D
    case 228: f = (a < m(b)); break; // A<*B
    case 229: f = (a < m(c)); break; // A<*C
    case 230: f = (a < h(d)); break; // A<*D
    case 231: f = (a < header[pc++]); break; // A<N
    case 232: f = (a > a); break; // A>A
    case 233: f = (a > b); break; // A>B
    case 234: f = (a > c); break; // A>C
    case 235: f = (a > d); break; // A>D
    case 236: f = (a > m(b)); break; // A>*B
    case 237: f = (a > m(c)); break; // A>*C
    case 238: f = (a > h(d)); break; // A>*D
    case 239: f = (a > header[pc++]); break; // A>N
    default: err();
  }
  return 1;
}

// Print illegal instruction error message and exit
void ZPAQL::err() {
  fprintf(stderr, "\nExecution aborted: pc=%d a=%d b=%d->%d c=%d->%d d=%d->%d\n",
    pc-hbegin, a, b, m(b), c, m(c), d, h(d));
  if (pc>=hbegin && pc<hend) fprintf(stderr, "opcode = %d %s\n",
    header[pc-hbegin], opcodelist[header[pc-hbegin]]);
  else
    fprintf(stderr, "pc out of range. Program size is %d\n", hend-hbegin);
  exit(1);
}

///////////////////////////// Predictor ///////////////////////////

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
  Component();    // initialize to all 0
};

Component::Component(): limit(0), cxt(0), a(0), b(0), c(0) {}

// A predictor guesses the next bit
class Predictor {
public:
  Predictor(ZPAQL&);    // build model
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
  void train(Component& cr, int y);  // reduce prediction error in cr.cm
  int dt[1024];         // division table for cm: dt[i] = 2^18/(i+1.5)
  U16 squasht[4096];    // squash() lookup table
  short stretcht[4096]; // stretch() lookup table
  int squash(int x) {
    if (x>=2048) return 4095;
    else if (x<-2048) return 0;
    return squasht[x+2048];
  }
  int stretch(int x) {
    assert(x>=0 && x<4096);
    return stretcht[x];
  }
  int find(Array<U8>& ht, int sizebits, U32 cxt); // get cxt in ht
};

// Initailize the model
Predictor::Predictor(ZPAQL& zr): c8(1), hmap4(1), z(zr) {
  assert(sizeof(U8)==1);
  assert(sizeof(U16)==2);
  assert(sizeof(U32)==4);
  assert(sizeof(int)==4);
  assert(sizeof(long)==sizeof(char*));  // 4 or 8

  // Initialize tables
  for (int i=0; i<1024; ++i)
    dt[i]=(1<<19)/(i*2+3);
  for (int i=0; i<4096; ++i) {
    squasht[i]=int(0.5+4095.5/(1+exp((i-2048)*(-1.0/256))));
    stretcht[i]=int(log((i+0.5)/(4095.5-i))*256+0.5+10000)-10000;
    if (stretcht[i]>2047) stretcht[i]=2047;
    if (stretcht[i]<-2048) stretcht[i]=-2048;
  }

  // Verify floating point math for squash() and stretch()
  U32 sq=0, st=0;
  for (int i=4095; i>=0; --i) {
    st=st*3+stretch(i);
    sq=sq*3+squash(i-2048);
  }
  assert(st==2467703605u);
  assert(sq==1032925551u);

  // Initialize context hash function
  z.inith();

  // Initialize predictions
  for (int i=0; i<256; ++i) p[i]=0;

  // Initialize components
  int n=z.header[6]; // hsize[0..1] hh hm ph pm n (comp)[n] END 0[128] (hcomp) END
  if (n<1 || n>255) error("n must be 1..255 components");
  const U8* cp=&z.header[7];  // start of component list
  for (int i=0; i<n; ++i) {
    assert(cp<&z.header[z.cend]);
    assert(cp>&z.header[0] && cp<&z.header[z.header.size()-8]);
    Component& cr=comp[i];
    switch(cp[0]) {
      case CONST:  // c
        p[i]=(cp[1]-128)*16;
        break;
      case CM: // sizebits limit
        cr.cm.resize(1, cp[1]);
        cr.limit=cp[2]*4;
        for (int j=0; j<cr.cm.size(); ++j)
          cr.cm[j]=0x80000000;
        break;
      case ICM: // sizebits
        cr.limit=1023;
        cr.cm.resize(256);
        cr.ht.resize(64, cp[1]);
        for (int j=0; j<cr.cm.size(); ++j)
          cr.cm[j]=0x80000000;
        break;
      case MATCH:  // sizebits
        cr.cm.resize(1, cp[1]);  // index
        cr.ht.resize(4, cp[1]);  // buf
        cr.ht(0)=1;
        break;
      case AVG: // j k wt
        break;
      case MIX2:  // sizebits j k rate mask
        if (cp[3]>=i) error("MIX2 k >= i");
      case MIX: {  // sizebits j m rate mask
        if (cp[2]>=i) error("MIX j >= i");
        if (cp[0]==MIX && (cp[3]<1 || cp[3]>i-cp[2])) error("MIX m not in 1..i-j");
        int m=cp[3];  // number of inputs
        if (cp[0]==MIX2) m=2;
        assert(m>=1);
        cr.c=(1<<cp[1]); // size (number of contexts)
        cr.cm.resize(m, cp[1]);  // wt[size][m]
        for (int j=0; j<cr.cm.size(); ++j)
          cr.cm[j]=65536/m;
        break;
      }
      case IMIX2:  // sizebits j k wt rate
        if (cp[2]>=i) error("ISSE j >= i");
        if (cp[3]>=i) error("ISSE k >= i");
        cr.ht.resize(64, cp[1]);
        cr.cm.resize(512);
        for (int j=0; j<512; j+=2) {
          cr.cm[j]=256*cp[4];
          cr.cm[j+1]=256*(256-cp[4]);
        }
        break;
      case SSE: // sizebits j start limit mask
        if (cp[2]>=i) error("SSE j >= i");
        if (cp[3]>cp[4]*4) error("SSE start > limit*4");
        cr.cm.resize(32, cp[1]);
        cr.limit=cp[4]*4;
        for (int j=0; j<cr.cm.size(); ++j)
          cr.cm[j]=squash((j&31)*256-3968)<<20|cp[3];
        break;
      default: error("unknown component type");
    }
    assert(compsize[*cp]>0);
    cp+=compsize[*cp];
    assert(cp>=&z.header[7] && cp<&z.header[z.cend]);
  }
}

int Predictor::predict() {
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
      case CONST:  // c
        break;
      case CM:  // sizebits limit
        cr.cxt=z.h(i)^hmap4;
        p[i]=stretch(cr.cm(cr.cxt)>>20);
        break;
      case ICM: // sizebits
        assert((hmap4&15)>0);
        if (c8==1 || (c8&0xf0)==16) cr.c=find(cr.ht, cp[1]+2, z.h(i)+16*c8);
        cr.cxt=cr.ht[cr.c+(hmap4&15)];
        p[i]=stretch(cr.cm(cr.cxt)>>20);
        break;
      case MATCH: // sizebits: a=len, b=offset, c=bit, cxt=256/len,
                  //           ht=buf, limit=8*pos+bp
        assert(cr.a>=0 && cr.a<=255);
        if (cr.a==0) p[i]=0;
        else {
          cr.c=cr.ht((cr.limit>>3)-cr.b)>>7-(cr.limit&7)&1; // predicted bit
          p[i]=stretch(cr.cxt*(cr.c*-2+1)&4095);  // bit ? 4096-256/len : 256/len
        }
        break;
      case AVG: // j k wt
        p[i]=(p[cp[1]]*cp[3]+p[cp[2]]*(256-cp[3]))>>8;
        break;
      case MIX2: { // sizebits j k rate mask
                   // c=size cm=wt[size][m] cxt=input
        cr.cxt=(z.h(i)+(c8&cp[5])&cr.c-1)*2;
        assert(int(cr.cxt)>=0 && int(cr.cxt)<=cr.cm.size()-2);
        int* wt=(int*)&cr.cm[cr.cxt];
        p[i]=wt[0]*p[cp[2]]+wt[1]*p[cp[3]]>>16;
      }
        break;
      case MIX: {  // sizebits j m rate mask
                   // c=size cm=wt[size][m] cxt=index of wt in cm
        int m=cp[3];
        assert(m>=1 && m<=i);
        cr.cxt=z.h(i)+(c8&cp[5]);
        cr.cxt=(cr.cxt&cr.c-1)*m; // pointer to row of weights
        assert(int(cr.cxt)>=0 && int(cr.cxt)<=cr.cm.size()-m);
        int* wt=(int*)&cr.cm[cr.cxt];
        p[i]=0;
        for (int j=0; j<m; ++j)
          p[i]+=wt[j]*p[cp[2]+j]>>8;
        p[i]>>=8;
      }
        break;
      case IMIX2:  // sizebits j k wt rate -- c=hi, cxt=bh
        assert((hmap4&15)>0);
        if (c8==1 || (c8&0xf0)==16) cr.c=find(cr.ht, cp[1]+2, z.h(i)+16*c8);
        cr.cxt=cr.ht[cr.c+(hmap4&15)];  // bit history
        p[i]=int(cr.cm[cr.cxt*2])*p[cp[2]]+int(cr.cm[cr.cxt*2+1])*p[cp[3]]>>16;
        break;
      case SSE: { // sizebits j start limit mask
        cr.cxt=(z.h(i)+(c8&cp[5]))*32;
        int pr=p[cp[2]]+3968;
        if (pr<0) pr=0;
        if (pr>7935) pr=7935;
        int wt=pr&255;
        pr>>=8;
        assert(pr>=0 && pr<=30);
        cr.cxt+=pr;
        p[i]=stretch((cr.cm(cr.cxt)>>10)*(256-wt)+(cr.cm(cr.cxt+1)>>10)*wt>>18);
        cr.cxt+=wt>>7;
      }
        break;
      default:
        error("component predict not implemented");
    }
    cp+=compsize[cp[0]];
    assert(cp<&z.header[z.cend]);
  }
  assert(cp[0]==NONE);
  return squash(p[n-1]);
}

// Update model with decoded bit y (0...1)
void Predictor::update(int y) {
  assert(y==0 || y==1);
  assert(c8>=1 && c8<=255);
  assert(hmap4>=1 && hmap4<=511);

// bit history next state table for ICM
static const U8 next[256][2]={
{  1,  2},{  3,  5},{  4,  6},{  7, 10},{  8, 12},{  9, 13},{ 11, 14}, // 0
{ 15, 19},{ 16, 23},{ 17, 24},{ 18, 25},{ 20, 27},{ 21, 28},{ 22, 29}, // 7
{ 26, 30},{ 31, 33},{ 32, 35},{ 32, 35},{ 32, 35},{ 32, 35},{ 34, 37}, // 14
{ 34, 37},{ 34, 37},{ 34, 37},{ 34, 37},{ 34, 37},{ 36, 39},{ 36, 39}, // 21
{ 36, 39},{ 36, 39},{ 38, 40},{ 41, 43},{ 42, 45},{ 42, 45},{ 44, 47}, // 28
{ 44, 47},{ 46, 49},{ 46, 49},{ 48, 51},{ 48, 51},{ 50, 52},{ 53, 43}, // 35
{ 54, 57},{ 54, 57},{ 56, 59},{ 56, 59},{ 58, 61},{ 58, 61},{ 60, 63}, // 42
{ 60, 63},{ 62, 65},{ 62, 65},{ 50, 66},{ 67, 55},{ 68, 57},{ 68, 57}, // 49
{ 70, 73},{ 70, 73},{ 72, 75},{ 72, 75},{ 74, 77},{ 74, 77},{ 76, 79}, // 56
{ 76, 79},{ 62, 81},{ 62, 81},{ 64, 82},{ 83, 69},{ 84, 71},{ 84, 71}, // 63
{ 86, 73},{ 86, 73},{ 44, 59},{ 44, 59},{ 58, 61},{ 58, 61},{ 60, 49}, // 70
{ 60, 49},{ 76, 89},{ 76, 89},{ 78, 91},{ 78, 91},{ 80, 92},{ 93, 69}, // 77
{ 94, 87},{ 94, 87},{ 96, 45},{ 96, 45},{ 48, 99},{ 48, 99},{ 88,101}, // 84
{ 88,101},{ 80,102},{103, 69},{104, 87},{104, 87},{106, 57},{106, 57}, // 91
{ 62,109},{ 62,109},{ 88,111},{ 88,111},{ 80,112},{113, 85},{114, 87}, // 98
{114, 87},{116, 57},{116, 57},{ 62,119},{ 62,119},{ 88,121},{ 88,121}, // 105
{ 90,122},{123, 85},{124, 97},{124, 97},{126, 57},{126, 57},{ 62,129}, // 112
{ 62,129},{ 98,131},{ 98,131},{ 90,132},{133, 85},{134, 97},{134, 97}, // 119
{136, 57},{136, 57},{ 62,139},{ 62,139},{ 98,141},{ 98,141},{ 90,142}, // 126
{143, 95},{144, 97},{144, 97},{ 68, 57},{ 68, 57},{ 62, 81},{ 62, 81}, // 133
{ 98,147},{ 98,147},{100,148},{149, 95},{150,107},{150,107},{108,151}, // 140
{108,151},{100,152},{153, 95},{154,107},{108,155},{100,156},{157, 95}, // 147
{158,107},{108,159},{100,160},{161,105},{162,107},{108,163},{110,164}, // 154
{165,105},{166,117},{118,167},{110,168},{169,105},{170,117},{118,171}, // 161
{110,172},{173,105},{174,117},{118,175},{110,176},{177,105},{178,117}, // 168
{118,179},{110,180},{181,115},{182,117},{118,183},{120,184},{185,115}, // 175
{186,127},{128,187},{120,188},{189,115},{190,127},{128,191},{120,192}, // 182
{193,115},{194,127},{128,195},{120,196},{197,115},{198,127},{128,199}, // 189
{120,200},{201,115},{202,127},{128,203},{120,204},{205,115},{206,127}, // 196
{128,207},{120,208},{209,125},{210,127},{128,211},{130,212},{213,125}, // 203
{214,137},{138,215},{130,216},{217,125},{218,137},{138,219},{130,220}, // 210
{221,125},{222,137},{138,223},{130,224},{225,125},{226,137},{138,227}, // 217
{130,228},{229,125},{230,137},{138,231},{130,232},{233,125},{234,137}, // 224
{138,235},{130,236},{237,125},{238,137},{138,239},{130,240},{241,125}, // 231
{242,137},{138,243},{130,244},{245,135},{246,137},{138,247},{140,248}, // 238
{249,135},{250, 69},{ 80,251},{140,252},{249,135},{250, 69},{ 80,251}, // 245
{140,252},{  0,  0},{  0,  0},{  0,  0}};  // 252

  // Update components
  const U8* cp=&z.header[7];
  int n=z.header[6];
  assert(n>=1 && n<=255);
  assert(cp[-1]==n);
  for (int i=0; i<n; ++i) {
    Component& cr=comp[i];
    switch(cp[0]) {
      case CONST:  // c
        break;
      case CM:  // sizebits limit
        train(cr, y);
        break;
      case ICM: // sizebits: cxt=ht[b]=bh, ht[c][0..15]=bh row, cxt=bh
        cr.ht[cr.c+(hmap4&15)]=next[cr.ht[cr.c+(hmap4&15)]][y];
        train(cr, y);
        break;
      case MATCH: // sizebits: a=len, b=offset, c=bit, cm=index, cxt=256/len
                  //           ht=buf, limit=8*pos+bp
      {
        assert(cr.a>=0 && cr.a<=255);
        assert(cr.c==0 || cr.c==1);
        if (cr.c!=y) cr.a=0;  // mismatch?
        cr.ht(cr.limit>>3)+=cr.ht(cr.limit>>3)+y;
        if ((++cr.limit&7)==0) {
          int pos=cr.limit>>3;
          if (cr.a==0) {  // look for a match
            cr.b=pos-cr.cm(z.h(i));
            if (cr.b&cr.ht.size()-1)
              while (cr.a<255 && cr.ht(pos-cr.a-1)==cr.ht(pos-cr.a-cr.b-1))
                ++cr.a;
          }
          else cr.a+=cr.a<255;
          cr.cm(z.h(i))=pos;
          if (cr.a>0) cr.cxt=256/cr.a;
        }
      }
        break;
      case AVG:  // j k wt
        break;
      case MIX2: { // sizebits j k rate mask
                   // cm=input[2],wt[size][2], cxt=weight row
        assert(cr.cm.size()==2*cr.c);
        assert(int(cr.cxt)>=0 && int(cr.cxt)<=cr.cm.size()-2);
        int err=((y<<12)-squash(p[i]))*cp[4];
        int* wt=(int*)&cr.cm[cr.cxt];
        wt[0]+=((1<<15)+err*p[cp[2]])>>16;
        wt[1]+=((1<<15)+err*p[cp[3]])>>16;
      }
        break;
      case MIX: {   // sizebits j m rate mask
                    // cm=wt[size][m], cxt=input
        int m=cp[3];
        assert(m>0 && m<=i);
        assert(cr.cm.size()==m*cr.c);
        assert(int(cr.cxt)>=0 && int(cr.cxt)<=cr.cm.size()-m);
        int err=((y<<12)-squash(p[i]))*cp[4];
        int* wt=(int*)&cr.cm[cr.cxt];
        for (int j=0; j<m; ++j)
          wt[j]+=((1<<15)+err*p[cp[2]+j])>>16;
      }
        break;
      case IMIX2: { // sizebits j k wt rate -- c=hi, cxt=bh
        assert(cr.cxt==cr.ht[cr.c+(hmap4&15)]);
        cr.ht[cr.c+(hmap4&15)]=next[cr.cxt][y];
        int err=((y<<12)-squash(p[i]))*cp[5];
        cr.cm[cr.cxt*2]+=(1<<15)+err*p[cp[2]]>>16;
        cr.cm[cr.cxt*2+1]+=(1<<15)+err*p[cp[3]]>>16;
      }
        break;
      case SSE:  // sizebits j start limit mask
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
    hmap4=hmap4&0x1f0|(hmap4&0xf)*2+y&0xf;
}

// cr.cm(cr.cxt) has a prediction in the high 22 bits and a count in the
// low 10 bits.  Reduce the prediction error by error/(count+1.5) and
// count up to cr.limit. cm.size() must be a power of 2.
inline void Predictor::train(Component& cr, int y) {
  assert(y==0 || y==1);
  U32& pn=cr.cm(cr.cxt);
  int count=pn&0x3ff;
  int error=(y<<12)-(cr.cm(cr.cxt)>>20);
  pn+=(error*(128+dt[count]>>8)<<10)+(count<cr.limit);
}

// Find cxt row in hash table ht. ht has rows of 16 indexed by the
// low sizebits of cxt with element 0 having the next higher 8 bits for
// collision detection. If not found after 3 adjacent tries, replace the
// row with lowest element 1 as priority. Return index of row.
int Predictor::find(Array<U8>& ht, int sizebits, U32 cxt) {
  assert(ht.size()==16<<sizebits);
  int chk=cxt>>sizebits&255;
  int h0=cxt*16&ht.size()-16;
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

////////////////////////////// Decoder ////////////////////////////

// Decoder decompresses using an arithmetic code
class Decoder {
  FILE* in;  // destination
  U32 low, high; // range
  U32 curr;  // last 4 bytes of archive
  Predictor pr;  // to get p
  int decode(int p); // return decoded bit (0..1) with probability p (0..8191)
public:
  Decoder(FILE* f, ZPAQL& z);
  int decompress();  // return a byte or EOF
};

Decoder::Decoder(FILE* f, ZPAQL& z):
  in(f), low(1), high(0xFFFFFFFF), curr(0), pr(z) {}

inline int Decoder::decode(int p) {
  assert(p>=0 && p<8192);
  assert(high>low && low>0);
  assert(curr>=low && curr<=high);
  U32 mid=low+(high-low>>13)*p+((high-low&0x1fff)*p>>13); // split range here
  assert(high>mid && mid>=low);
  int y=curr<=mid;
  if (y) high=mid; else low=mid+1; // pick half
  while ((high^low)<0x1000000) { // shift out identical leading bytes
    high=high<<8|255;
    low=low<<8;
    low+=(low==0);
    int c=getc(in);
    if (c==EOF) error("unexpected end of file");
    curr=curr<<8|c;
  }
  return y;
}

int Decoder::decompress() {
  if (curr==0) {  // finish initialization
    for (int i=0; i<4; ++i)
      curr=curr<<8|getc(in);
  }
  if (decode(0)) {
    if (curr!=0) error("decoding end of stream");
    return EOF;
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

/////////////////////////// PostProcessor ////////////////////

class PostProcessor {
  int state;
public:
  PostProcessor(): state(-2) {}
  void write(int c, FILE* out);
};

void PostProcessor::write(int c, FILE* out) {
  assert(out);
  if (state<0) state=c;
  else if (state==0)  // PASS
    putc(c, out);
  else
    error("post processing not implemented");
}

/////////////////////////// Decompress ///////////////////////

// Decompress archive argv[2] to stored filenames or argv[3..argc-1]
void decompress(int argc, char** argv) {
  assert(argc>=3);

  // Open archive
  FILE* in=fopen(argv[2], "rb");
  if (!in) perror(argv[2]), exit(1);

  // number of files extracted
  int filecount=0;

  // Read the archive
  int c;
  while ((c=getc(in))=='z') {
    if (getc(in)!='P' || getc(in) != 'Q' || getc(in)!=LEVEL)
      error("missing ZPAQ level 0 block header");

    // Read block header
    ZPAQL z;
    z.read(in);

    // PostProcessor and Decoder is created and and destroyed for each block
    PostProcessor pp;
    Decoder dec(in, z);

    // Read segments
    while ((c=getc(in))==1) {

      // Read the filename
      char filename[512]={0};
      int i;
      for (i=0; (c=getc(in))>0; ++i)
        if (i<511) filename[i]=c;
      if (i>0 && i<512) filename[i]=0;

      // Skip comment
      while ((c=getc(in))!=EOF && c!=0);
      if (getc(in)) error("reserved");  // reserved 0

      // If the user gave an output file starting at argv[3], use it instead.
      // If the user doesn't name all the files, then stop after the last
      // named file.
      FILE* out=0;
      if (argc>3) {
        if (filecount+3 < argc) {
          out=fopen(argv[filecount+3], "wb");
          if (!out) {
            perror(argv[filecount+3]);
            printf("skipping %s -> %s ...\n", filename, argv[filecount+3]);
          }
          else
            printf("Decompressing %s -> %s\n", filename, argv[filecount+3]);
        }
        else {
          printf("Skipping %s and remaining files\n", filename);
          goto end;
        }
      }

      // Otherwise, use the names in the archive, but don't clobber.
      else {
        out=fopen(filename, "rb");
        if (out) {
          printf("Won't overwrite %s, skipping...\n", filename);
          fclose(out);
          out=0;
        }
        out=fopen(filename, "wb");
        if (!out) {
          perror(filename);
          printf("skipping %s ...\n", filename);
        }
        else
          printf("Decompressing %s\n", filename);
      }

      // Decompress
      assert(out);
      if (argv[1][0]=='t')  // don't postprocess
        while ((c=dec.decompress())!=EOF)
          putc(c, out);
      else {
        while ((c=dec.decompress())!=EOF)
          pp.write(c, out);
      }
      ++filecount;

      // Check for end of segment and block markers
      if (getc(in)!=254) error("missing end of segment marker");
    }
    if (c!=255) error("missing end of block marker");
  }
  if (c!=EOF) error("extra data after last block");

  // Close the archive
end:
  printf("%d file(s) extracted\n", filecount);
  fclose(in);
}

//////////////////////////// Compressor ////////////////////////////

//////////////////////////// Encoder ///////////////////////////////

// Encoder compresses using an arithmetic code
class Encoder {
  FILE* out;  // destination
  U32 low, high; // range
  Predictor pr;  // to get p
  void encode(int y, int p); // encode bit y (0..1) with probability p (0..8191)
public:
  Encoder(FILE* f, ZPAQL& z);
  void compress(int c);  // c is 0..255 or EOF
};

Encoder::Encoder(FILE* f, ZPAQL& z): 
  out(f), low(1), high(0xFFFFFFFF), pr(z) {}

inline void Encoder::encode(int y, int p) {
  assert(p>=0 && p<8192);
  assert(y==0 || y==1);
  assert(high>low && low>0);
  U32 mid=low+(high-low>>13)*p+((high-low&0x1fff)*p>>13); // split range here
  assert(high>mid && mid>=low);
  if (y) high=mid; else low=mid+1; // pick half
  while ((high^low)<0x1000000) { // write identical leading bytes
    putc(high>>24, out);  // same as low>>24
    high=high<<8|255;
    low=low<<8;
    low+=(low==0); // so we don't code 4 0 bytes in a row
  }
}

void Encoder::compress(int c) {
  if (c==EOF)
    encode(1, 0);
  else {
    encode(0, 0);
    for (int i=7; i>=0; --i) {
      int p=pr.predict()*2+1;
      assert(p>0 && p<8192);
      int y=c>>i&1;
      encode(y, p);
      pr.update(y);
    }
  }
}

//////////////////////////// PreProcessor ////////////////////////

class PreProcessor {
  Encoder* encp;
  int state;
public:
  PreProcessor(Encoder* p);
  void compress(int c);
};

PreProcessor::PreProcessor(Encoder* p): encp(p), state(0) {}

// Implement PASS by appending a 0 to the front of the input
void PreProcessor::compress(int c) {
  if (state==0) {
    encp->compress(0);  // PASS
    state=1;
  }
  encp->compress(c);
}

//////////////////////////// Compress ////////////////////////////

// Compress files in argv[3..argc-1] to argv[2]
void compress(int argc, char** argv) {
  assert(argc>=3);
  assert(argv[1][0]=='a' || argv[1][0]=='c');

  // Compile config file
  FILE* cfg=0;
  ZPAQL z;
  if (argv[1][1]) {
    cfg=fopen(argv[1]+1, "rb");
    if (!cfg) perror(argv[1]+1), exit(1);
    z.compile(cfg);
  }
  else
    error("no config file");

  // Open archive
  FILE* out=fopen(argv[2], argv[1][0]=='a' ? "ab" : "wb");
  if (!out) perror(argv[2]), exit(1);

  // Write block header
  long mark=ftell(out)-1;  // last reported size (-1 byte for trailer)
  fprintf(out, "zPQ%c", LEVEL);
  z.write(out);

  // Create PreProcessor chain that writes to Encoder
  assert(out);
  Encoder enc(out, z);
  PreProcessor pp(&enc);

  // Compress files argv[3..argc-1]
  for (int i=3; i<argc; ++i) {
    FILE* in=fopen(argv[i], "rb");
    if (!in)
      perror(argv[i]);  // skip file not found
    else {

      // Write filename and size to segment header
      fseek(in, 0, SEEK_END);
      long size=ftell(in);
      fprintf(out, "%c%s%c%ld%c%c", 1, argv[i], 0, size, 0, 0);
      fseek(in, 0, SEEK_SET);

      // Compress 
      int c;
      while ((c=getc(in))!=EOF)
        pp.compress(c);
      pp.compress(EOF);

      // Write trailer
      fprintf(out, "%c%c%c%c%c", 0, 0, 0, 0, 254);
      fclose(in);
      printf("%s %ld -> %ld\n", argv[i], size, ftell(out)-mark);
      mark=ftell(out);
    }
  }
  putc(255, out);  // block trailer
}

////////////////////////// List //////////////////////////

// List archive contents
void list(int argc, char** argv) {
  assert(argc>2 && argv[2]);

  // Open archive
  FILE* in=fopen(argv[2], "rb");
  if (!in) perror(argv[2]), exit(1);

  // File offsets to get compressed sizes
  long mark=0;

  // Read the file
  int c, blocks=0;
  while ((c=getc(in))=='z') {

    // Read block header
    if (getc(in)!='P' || getc(in)!='Q' || getc(in)!=LEVEL)
      error("not ZPAQ level 0");
    ZPAQL z;
    z.read(in);
    printf("Block %d: requires %1.3f MB memory\n",
     ++blocks, z.memory()/1000000);
    if (argv[1][0]=='v')
      z.list();

    // Read segments
    while ((c=getc(in))==1) {

      // Print filename and private data
      printf("  ");
      while ((c=getc(in))!=EOF && c) putchar(c);
      printf("  ");
      while ((c=getc(in))!=EOF && c) putchar(c);
      if (getc(in)!=0) error("reserved data");
      
      // Skip to end of data
      U32 c4=0xFFFFFFFF;  // last 4 bytes will be all 0
      while ((c=getc(in))!=EOF && (c4=c4<<8|c)!=0);
      if (c==EOF) error("unexpected end of file");
      while ((c=getc(in))==0);
      if (c!=254) error("missing end of segment marker");
      printf(" -> %ld\n", 1+ftell(in)-mark);
      mark=1+ftell(in);
    }
    if (c!=255) error("missing end of block marker");
  }
  if (c!=EOF) error("extra data at end");
}

// Run HCOMP with input argv[2...]
void hstep(int argc, char** argv) {
  ZPAQL z;
  FILE* in=fopen(argv[1]+1, "r");
  if (!in) perror(argv[1]+1), exit(1);
  z.compile(in);
  z.inith();
  for (int i=2; i<argc; ++i)
    z.step(atoi(argv[i]));
  fclose(in);
}

// Run PCOMP from argv[2] to argv[3]
void prun(int argc, char** argv) {
  ZPAQL z;
  FILE* in=stdin;
  z.output=stdout;
  FILE* cfg=fopen(argv[1]+1, "r");
  if (!cfg) perror(argv[1]+1), exit(1);
  if (argc>2) {
    in=fopen(argv[2], "rb");
    if (!in) perror(argv[2]), exit(1);
  }
  if (argc>3) {
    z.output=fopen(argv[3], "wb");
    if (!z.output) perror(argv[3]), exit(1);
  }
  z.verbose=false;
  z.compile(cfg);
  z.initp();
  int c;
  while ((c=getc(in))!=EOF)
    z.run(c);
}

///////////////////////////// Main ///////////////////////////

// Print help message and exit
void usage() {
  printf("ZPAQ v0.01 archiver.\n"
    "(C) 2009, Ocarina Networks Inc. Written by Matt Mahoney, Feb. 15, 2009.\n"
    "This is free software under GPL v3, http://www.gnu.org/copyleft/gpl.html\n"
    "\n"
    "Usage: zpaq command archive files...  Commands are:\n"
    "  c        Create new archive (or overwrite existing archive).\n"
    "  cconfig  Create using compression options in file config.\n"
    "  a        Append to archive.\n"
    "  aconfig  Append using compression options in file config.\n"
    "  x        Extract all files using stored names (does not clobber).\n"
    "           Or if file names are given, rename in that order (clobbers).\n"
    "  l        List contents of archive.\n"
    "  v        Verbose listing.\n"
    "For debugging:\n"
    "  t                 Extract without postprocessing (for debugging).\n"
    "  hconfig args...   Run HCOMP in config with numeric args (no archive).\n"
    "  pconfig in out    Run PCOMP on files (default stdin/stdout).\n"); 
  exit(0);
}

// Command syntax: zpaq1 (afile|cfile|x|l) archive files...
int main(int argc, char** argv) {

  if (LEVEL==0) {
    fprintf(stderr, "Warning: ZPAQ Level 0 is experimental. Different versions\n"
      "are not compatible with each other or with level 1. This format will be\n"
      "obsolete with the release of level 1.\n\n");
  }

  // Check usage
  if (argc<2) 
    usage();

  // Do the command
  char cmd=argv[1][0];
  if ((cmd=='a' || cmd=='c') && argc>=3) {
    compress(argc, argv);
    printf("Used %1.2f seconds\n", clock()/double(CLOCKS_PER_SEC));
  }
  else if ((cmd=='x' || cmd=='t') && argc>=2) {
    decompress(argc, argv);
    printf("Used %1.2f seconds\n", clock()/double(CLOCKS_PER_SEC));
  }
  else if ((cmd=='l' || cmd=='v') && argc>=2)
    list(argc, argv);
  else if (cmd=='h')
    hstep(argc, argv);
  else if (cmd=='p')
    prun(argc, argv);
  else
    usage();
  return 0;
}
