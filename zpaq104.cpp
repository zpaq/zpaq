/*  zpaq v1.04 archiver and file compressor.

(C) 2009, Ocarina Networks, Inc.
    Written by Matt Mahoney, matmahoney@yahoo.com, Sept. 18, 2009.

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
      Store file names without a path or drive letter.

  c - Create, overwriting archive.

  b - Append without storing SHA1 checksum.

  ra, rc, rb - Store paths as given on command line.

  aconfig, cconfig, bconfig, raconfig, rcconfig, rbconfig - 
      Use compression options in file config

  x - Extract and uncompress files. If no file names are given, then
      extract all files using the stored names. Skips (does not overwrite)
      existing files. If one or more file names are given, then extract
      that number of files and rename them, overwriting existing files,
      and ignoring any remaining files in the archive.

  l - List contents.

  v - List contents verbosely.

  [r]k{abc}[config] archive file [offset [length]] - Compress a single file
      or part of a file. If offset is specified, then skip the first
      offset bytes (default 0). If length is specified, then compress
      length bytes (default is to end of file). Also, if offset is not
      0, then the filename is not stored in the archive to signal
      the decompressor to append to the previous file. The commands
      ka, kb, kc otherwise work like a, b, c: ka appends, kb appends
      without a checksum, and kc creates a new archive. For example:

        zpaq kccfg1 arc.zp file1 0 100   - compress first 100 bytes using cfg1
        zpaq kacfg2 arc.zp file1 100 300 - compress next 300 bytes using cfg2
        zpaq kacfg3 arc.zp file1 400     - compress the rest using cfg3
        zpaq x arc.zp                    - extract file1


Each compression command appends one block with one segment per file. Archives
are created in ZPAQ level 1 format. See http://mattmahoney.net/dc/zpaq.pdf

New in version 1.04: An archive can either be a regular archive
or a self extracting program created with zpaqsfx.

Options for developers:

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

  sconfig - Compile config and output the header in the format of a C array
      initialization list.

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
    *d<>a a+=*d a*= 192 *d=a
    halt
  post
    0
  end

The meaning is as follows:

- COMP hh hm ph pm n

hh and hm are the log2 sizes of H and M in HCOMP for computing contexts.
ph and pm are the log2 sizes of H and M in PCOMP for post-processing.
There are n components numbered i = 0 to n-1. Possible components are:

- i CONST c
- i CM sizebits limit
- i ICM sizebits
- i MATCH sizebits bufbits
- i AVG j k wt
- i MIX2 sizebits j k rate mask
- i MIX sizebits j m rate mask
- i ISSE sizebits j
- i SSE sizebits j start limit

All argments are numbers in 0...255 except sizebits in (0...31),
j, k in (0...i-1), m in (1...i-j).

- HCOMP - describes the program that computes context hashes.
Instructions have the forms:

- L=R
  - where L is A B C D *B *C *D
  - where R is A B C D *B *C *D (0...255)
- AxR
  - where x is += -= *= /= %= &= &~ |= ^= <<= >>= == < >
- Ly
  - where y is <>A ++ -- ! =0
  - except A<>A is not valid.
- J Z
  - where J is JT JF JMP
  - where Z is a number in (-128...127)
- LJ M
  - where M is in (0...65535) (2 bytes, LSB first).
- S=R N
  - where S is A B C D
- R=A N
- ERROR
- HALT
- OUT
- HASH
- HASHD

Numeric operands for 2 byte instructions must be separated by
a space, as in "A= 3". Note that "L=0" is a 1 byte instruction,
and "L= 0" is a 2 byte instruction. "LJ M" is a 3 byte instruction
with M coded as 2 bytes, LSB first. Negative operands to JT, JF, and
JMP are coded mod 256.

Possible POST commands are
  0 (no post-processing)
  p esc maxlen hmul (LZP with hash table size 2^ph, buffer size 2^pm, pm>=8)
  x (EXE (E8E9) transform. Use ph=0, pm=3)

where esc, maxlen, and hmul are numbers in the range (0...255). LZP matches
of length (maxlen+1...maxlen+255) are encoded as (esc, len-maxlen). The
esc byte is encoded as (esc, 0). A match means to go back to the last
position within 2^pm with the same context hash and copy the next len bytes.
The context hash is updated with byte c as hash=(hash*hmul+c)%(2^ph).
LZP removes long duplicate strings to speed compression.

The x transform replaces sequences of the form (0xE8|0xe9 aaaa) by adding
the file offset of the start of the sequence to aaaa interpreted as a 32
bit number LSB first. It improves compression of x86 .exe and .dll files.

To compile: g++ -O2 -march=pentiumpro -fomit-frame-pointer -s zpaq.cpp -o zpaq
To turn off assertion checking (faster), compile with -DNDEBUG
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <math.h>
#include <assert.h>

const int LEVEL=1;  // ZPAQ level 0=experimental 1=final

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

//////////////////////////// SHA-1 //////////////////////////////

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

int SHA1::result(int i) {
  assert(i>=0 && i<20);
  if (!Computed && shaSuccess != SHA1Result(result_buf))
    error("SHA1 failed\n");
  return result_buf[i];
}

/*
 *  SHA1Reset
 *
 *  Description:
 *      This function will initialize the SHA1Context in preparation
 *      for computing a new SHA1 message digest.
 *
 *  Parameters: none
 *
 *  Returns:
 *      sha Error Code.
 *
 */
int SHA1::SHA1Reset()
{
    Length_Low             = 0;
    Length_High            = 0;
    Message_Block_Index    = 0;

    Intermediate_Hash[0]   = 0x67452301;
    Intermediate_Hash[1]   = 0xEFCDAB89;
    Intermediate_Hash[2]   = 0x98BADCFE;
    Intermediate_Hash[3]   = 0x10325476;
    Intermediate_Hash[4]   = 0xC3D2E1F0;

    Computed   = 0;
    Corrupted  = 0;

    return shaSuccess;
}

/*
 *  SHA1Result
 *
 *  Description:
 *      This function will return the 160-bit message digest into the
 *      Message_Digest array  provided by the caller.
 *      NOTE: The first octet of hash is stored in the 0th element,
 *            the last octet of hash in the 19th element.
 *
 *  Parameters:
 *      Message_Digest: [out]
 *          Where the digest is returned.
 *
 *  Returns:
 *      sha Error Code.
 *
 */
int SHA1::SHA1Result(U8 Message_Digest[SHA1HashSize])
{
    int i;

    if (!Message_Digest)
    {
        return shaNull;
    }

    if (Corrupted)
    {
        return Corrupted;
    }

    if (!Computed)
    {
        SHA1PadMessage();
        for(i=0; i<64; ++i)
        {
            /* message may be sensitive, clear it out */
            Message_Block[i] = 0;
        }
        Length_Low = 0;    /* and clear length */
        Length_High = 0;
        Computed = 1;

    }

    for(i = 0; i < SHA1HashSize; ++i)
    {
        Message_Digest[i] = Intermediate_Hash[i>>2]
                            >> 8 * ( 3 - ( i & 0x03 ) );
    }

    return shaSuccess;
}

/*
 *  SHA1Input
 *
 *  Description:
 *      This function accepts an array of octets as the next portion
 *      of the message.
 *
 *  Parameters:
 *      message_array: [in]
 *          An array of characters representing the next portion of
 *          the message.
 *      length: [in]
 *          The length of the message in message_array
 *
 *  Returns:
 *      sha Error Code.
 *
 */
int SHA1::SHA1Input(const U8  *message_array, unsigned length)
{
    if (!length)
    {
        return shaSuccess;
    }

    if (!message_array)
    {
        return shaNull;
    }

    if (Computed)
    {
        Corrupted = shaStateError;

        return shaStateError;
    }

    if (Corrupted)
    {
         return Corrupted;
    }
    while(length-- && !Corrupted)
    {
    Message_Block[Message_Block_Index++] =
                    (*message_array & 0xFF);

    Length_Low += 8;
    if (Length_Low == 0)
    {
        Length_High++;
        if (Length_High == 0)
        {
            /* Message is too long */
            Corrupted = 1;
        }
    }

    if (Message_Block_Index == 64)
    {
        SHA1ProcessMessageBlock();
    }

    message_array++;
    }

    return shaSuccess;
}

/*
 *  SHA1ProcessMessageBlock
 *
 *  Description:
 *      This function will process the next 512 bits of the message
 *      stored in the Message_Block array.
 *
 *  Parameters:
 *      None.
 *
 *  Returns:
 *      Nothing.
 *
 *  Comments:

 *      Many of the variable names in this code, especially the
 *      single character names, were used because those were the
 *      names used in the publication.
 *
 *
 */
void SHA1::SHA1ProcessMessageBlock()
{
    const U32 K[] =    {       /* Constants defined in SHA-1   */
                            0x5A827999,
                            0x6ED9EBA1,
                            0x8F1BBCDC,
                            0xCA62C1D6
                            };
    int      t;                 /* Loop counter                */
    U32      temp;              /* Temporary word value        */
    U32      W[80];             /* Word sequence               */
    U32      A, B, C, D, E;     /* Word buffers                */

    /*
     *  Initialize the first 16 words in the array W
     */
    for(t = 0; t < 16; t++)
    {
        W[t] = Message_Block[t * 4] << 24;
        W[t] |= Message_Block[t * 4 + 1] << 16;
        W[t] |= Message_Block[t * 4 + 2] << 8;
        W[t] |= Message_Block[t * 4 + 3];
    }

    for(t = 16; t < 80; t++)
    {
       W[t] = SHA1CircularShift(1,W[t-3] ^ W[t-8] ^ W[t-14] ^ W[t-16]);
    }

    A = Intermediate_Hash[0];
    B = Intermediate_Hash[1];
    C = Intermediate_Hash[2];
    D = Intermediate_Hash[3];
    E = Intermediate_Hash[4];

    for(t = 0; t < 20; t++)
    {
        temp =  SHA1CircularShift(5,A) +
                ((B & C) | ((~B) & D)) + E + W[t] + K[0];
        E = D;
        D = C;
        C = SHA1CircularShift(30,B);

        B = A;
        A = temp;
    }

    for(t = 20; t < 40; t++)
    {
        temp = SHA1CircularShift(5,A) + (B ^ C ^ D) + E + W[t] + K[1];
        E = D;
        D = C;
        C = SHA1CircularShift(30,B);
        B = A;
        A = temp;
    }

    for(t = 40; t < 60; t++)
    {
        temp = SHA1CircularShift(5,A) +
               ((B & C) | (B & D) | (C & D)) + E + W[t] + K[2];
        E = D;
        D = C;
        C = SHA1CircularShift(30,B);
        B = A;
        A = temp;
    }

    for(t = 60; t < 80; t++)
    {
        temp = SHA1CircularShift(5,A) + (B ^ C ^ D) + E + W[t] + K[3];
        E = D;
        D = C;
        C = SHA1CircularShift(30,B);
        B = A;
        A = temp;
    }

    Intermediate_Hash[0] += A;
    Intermediate_Hash[1] += B;
    Intermediate_Hash[2] += C;
    Intermediate_Hash[3] += D;
    Intermediate_Hash[4] += E;

    Message_Block_Index = 0;
}

/*
 *  SHA1PadMessage
 *

 *  Description:
 *      According to the standard, the message must be padded to an even
 *      512 bits.  The first padding bit must be a '1'.  The last 64
 *      bits represent the length of the original message.  All bits in
 *      between should be 0.  This function will pad the message
 *      according to those rules by filling the Message_Block array
 *      accordingly.  It will also call the ProcessMessageBlock function
 *      provided appropriately.  When it returns, it can be assumed that
 *      the message digest has been computed.
 *
 *  Parameters:
 *      ProcessMessageBlock: [in]
 *          The appropriate SHA*ProcessMessageBlock function
 *  Returns:
 *      Nothing.
 *
 */

void SHA1::SHA1PadMessage()
{
    /*
     *  Check to see if the current message block is too small to hold
     *  the initial padding bits and length.  If so, we will pad the
     *  block, process it, and then continue padding into a second
     *  block.
     */
    if (Message_Block_Index > 55)
    {
        Message_Block[Message_Block_Index++] = 0x80;
        while(Message_Block_Index < 64)
        {
            Message_Block[Message_Block_Index++] = 0;
        }

        SHA1ProcessMessageBlock();

        while(Message_Block_Index < 56)
        {
            Message_Block[Message_Block_Index++] = 0;
        }
    }
    else
    {
        Message_Block[Message_Block_Index++] = 0x80;
        while(Message_Block_Index < 56)
        {

            Message_Block[Message_Block_Index++] = 0;
        }
    }

    /*
     *  Store the message length as the last 8 octets
     */
    Message_Block[56] = Length_High >> 24;
    Message_Block[57] = Length_High >> 16;
    Message_Block[58] = Length_High >> 8;
    Message_Block[59] = Length_High;
    Message_Block[60] = Length_Low >> 24;
    Message_Block[61] = Length_Low >> 16;
    Message_Block[62] = Length_Low >> 8;
    Message_Block[63] = Length_Low;

    SHA1ProcessMessageBlock();
}

//////////////////////////// ZPAQL //////////////////////////////

// Symbolic constants, instruction size, and names
typedef enum {NONE,CONST,CM,ICM,MATCH,AVG,MIX2,MIX,ISSE,SSE} CompType;
static const int compsize[256]={0,2,3,2,3,4,6,6,3,5};
static const char* compname[]=
  {"","const","cm","icm","match","avg","mix2","mix","isse","sse",0};

// Opcodes from ZPAQ spec, table 1, without operands (N, M)".
static const char* opcodelist[258]={
"error","a++",  "a--",  "a!",   "a=0",  "",     "",     "a=r",
"b<>a", "b++",  "b--",  "b!",   "b=0",  "",     "",     "b=r",
"c<>a", "c++",  "c--",  "c!",   "c=0",  "",     "",     "c=r",
"d<>a", "d++",  "d--",  "d!",   "d=0",  "",     "",     "d=r",
"*b<>a","*b++", "*b--", "*b!",  "*b=0", "",     "",     "jt",
"*c<>a","*c++", "*c--", "*c!",  "*c=0", "",     "",     "jf",
"*d<>a","*d++", "*d--", "*d!",  "*d=0", "",     "",     "r=a",
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
"",     "",     "",     "",     "",     "",     "",     "lj",
"post", 0};

// A ZPAQL machine HCOMP or PCOMP.
class ZPAQL {
public:
  ZPAQL();
  void load(int cn, int hn, const U8* data); // init from data[cn+hn]
  void read(FILE* in);    // Read header from archive
  void write(FILE* out);  // Write header to archive
  U32 compile(FILE* in);  // Create header from config file
  void list();            // Display header contents
  void inith();           // Initialize as HCOMP
  void initp();           // Initialize as PCOMP
  void run(U32 input);    // Execute with input
  void step(U32 input);   // Execute while displaying progress
  void prints();          // Print header as an array initialization
  double memory();        // Return memory requirement in bytes
  int ph() {return header[4];}  // ph
  int pm() {return header[5];}  // pm
  FILE* output;           // Destination for OUT instruction, or 0 to suppress
  SHA1* sha1;             // Points to checksum computer
  bool verbose;           // Show config file during compile?
  friend class Predictor;
  friend class PostProcessor;
private:

  // ZPAQ1 block header
  int hsize;          // Header size
  Array<U8> header;   // hsize[2] hh hm ph pm n COMP (guard) HCOMP (guard)
  int cend;           // COMP in header[7...cend-1] (empty for PCOMP)
  int hbegin, hend;   // HCOMP in header[hbegin...hend-1]

  // Machine state for executing HCOMP
  Array<U8> m;        // memory array M for HCOMP
  Array<U32> h;       // hash array H for HCOMP
  Array<U32> r;       // 256 element register array
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
  sha1=0;
}

// Copy cn bytes of COMP and hn bytes of HCOMP from data to header
void ZPAQL::load(int cn, int hn, const U8* data) {
  assert(header.size()==0);
  assert(cn>=7);
  assert(hn>=1);
  assert(data);
  cend=cn;
  hbegin=cend+128;
  hend=hbegin+hn;
  header.resize(hend+144);
  for (int i=0; i<cn; ++i)
    header[i]=data[i];
  for (int i=0; i<hn; ++i)
    header[hbegin+i]=data[cn+i];
  hsize=cn+hn-2;
  assert(header[0]+256*header[1]==hsize);
  assert(header[cend-1]==0);
  assert(header[hend-1]==0);
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

// Compile a configuration file and store the result in header.
// Return the POST command as a 32 bit value with a single
// letter in the low byte and up to 3 numeric parameters packed
// into the higher bytes. For example: "POST A 100" returns
// 'A' + 100*256.
U32 ZPAQL::compile(FILE* in) {

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
    int operand2=-1;  // 0...255 if 3 bytes
    if ((op&7)==7) { // 2 byte operand, read N
      if (op==255) {  // LJ
        operand=rtoken(in, 0, 65535);
        operand2=operand>>8;
        operand&=255;
        if (verbose) printf("(to %d) ", operand+256*operand2);
      }
      else if (op==39 || op==47 || op==63) { // JT, JF, JMP
        operand=rtoken(in, -128, 127);
        if (verbose) printf("(to %d) ", hend-hbegin+2+operand);
        operand&=255;
      }
      else
        operand=rtoken(in, 0, 255);
    }
    if (verbose) {
      if (operand2>=0)
        printf("(%d %d %d)\n", op, operand, operand2);
      else if (operand>=0)
        printf("(%d %d)\n", op, operand);
      else
        printf("(%d)\n", op);
    }
    header[hend++]=op;
    if (operand>=0)
      header[hend++]=operand;
    if (operand2>=0)
      header[hend++]=operand2;
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

  // Compile POST section: cmd (0...255)[0..3]
  U32 result=0;
  const char* tok=token(in);
  if (tok && strcmp(tok, "end")) result=tok[0];
  for (int i=1; i<4 && ((tok=token(in)))!=0 && strcmp(tok, "end"); ++i)
    result+=atoi(tok)<<i*8;
  return result;
}

// Display header contents. Assume it is constructed correctly.
void ZPAQL::list() {
  assert(cend>=7 && cend<header.size());
  assert(hbegin==cend+128 && hbegin<header.size());
  assert(hend>hbegin && hend<header.size());
  assert(hsize==header[0]+256*header[1]);

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
    if (op==255) { // LJ
      printf(" %d %d (to %d)", header[h], header[h+1],
          header[h]+256*header[h+1]);
      h+=2;
    }
    else if ((op&7)==7) {
      printf(" %d", header[h++]);
      if (op==39 || op==47 || op==63) // JT, JF, JMP
        printf(" (to %d) ", h-hbegin+(int(header[h-1])<<24>>24));
    }
    printf("\n");
  }
  assert(header[h]==0);
  assert(h+1==hend);
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
  r.resize(256);
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
  while (execute()) ;
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
  printf("\n"
  "  pc   opcode  f      a          b      *b      c      *c      d         *d\n"
  "----- -------- - ---------- ---------- --- ---------- --- ---------- ----------\n");
  printf("               %d %10u %10u %3u %10u %3u %10u %10u\n",
    f, a, b, m(b), c, m(c), d, h(d));
  while (1) {
    assert(pc>=cend && pc<header.size());
    int op=header[pc];
    printf("%5d ", pc-hbegin);
    char inst[16];
    if (op==255)
      sprintf(inst, "%s %d", opcodelist[op], header[pc+1]+256*header[pc+2]);
    else if ((op&7)==7)
      sprintf(inst, "%s %d", opcodelist[op], header[pc+1]);
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
  int rsize=r.size(); // don't print trailing zeros
  while (rsize>5 && r[rsize-1]==0) --rsize;
  printf("\n\nR (size %d) =", r.size());
  for (int i=0; i<rsize; ++i) {
    if (i%5==0) printf("\n%8d:", i);
    printf(" %10u", r[i]);
  }
  printf("\n\n");
}

// Print header as an array initialization in C
void ZPAQL::prints() {
  assert(header.size()>hend);
  assert(hend>=hbegin);
  assert(hbegin>=0);
  printf("\n\n[%d]={ // COMP %d bytes\n", cend+hend-hbegin, cend);
  for (int i=0; i<cend; ++i) {
    printf("%d,", header[i]);
    if (i%16==15) printf("\n");
  }
  printf("\n  // HCOMP %d bytes\n", hend-hbegin);
  for (int i=hbegin; i<hend; ++i) {
    printf("%d", header[i]);
    if (i<hend-1) printf(",");
    if ((i-hbegin)%16==15) printf("\n");
  }
  printf("}\n");
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

// Read a token and return it, or return 0 at EOF. Skip (comments).
// Convert to lower case. Tokens are separated by white space.
// In verbose mode, print the token.
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

  // read token separated by whitespace
  do {
    if (isupper(c)) c=tolower(c);
    s[len++]=c;
  }
  while (len<15 && (c=getc(in))!=EOF && c>' ');
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
# Generate ZPAQL interpreter from ZPAQ1.PDF table 1
$go="pc+=(header[pc]+128&255)-127";
$code=-1;
print"  switch(header[pc++]) {\n";
while (<>) {
 chomp;
 $code++;
 if ($_ ne "") {
  $comment=$_;
  s/ N$/N/;
  if    (/^([ABCD])(=)(R)/) {($a,$op,$b)=($1,$2,$3);}
  elsif (/^(R)(=)(A)/) {($a,$op,$b)=($1,$2,$3);}
  elsif (/^(\*?[ABCD])(\W*)(\*[ABCDN0])$/) {($a,$op,$b)=($1,$2,$3);}
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
  if ($a eq "jtn") {print"if (f) $go; else ++pc;";}
  elsif ($a eq "lj n m") {print"if((pc=hbegin+header[pc]+256*header[pc+1])>=hend)err();";}
  elsif ($a eq "jfn") {print"if (!f) $go; else ++pc;";}
  elsif ($a eq "jmpn") {print"$go;";}
  elsif ($a eq "out") {print"if (output) putc(a, output); if (sha1) sha1->put(a);";}
  elsif ($a eq "hash") {print"a = (a+m(b)+512)*773;"}
  elsif ($a eq "hashd") {print"h(d) = (h(d)+a+512)*773;"}
  elsif ($op eq "<>") {print"swap($a);";}
  elsif ($op eq "==" || $op eq "<" || $op eq ">") {print"f = ($a $op $b);";}
  elsif ($op eq "++" || $op eq "--") {print"$op$a;";}
  elsif ($op eq "!") {print"$a = ~$a;";}
  elsif ($op eq ".=") {print"$a = ($a<<8)+$b;";}
  elsif ($op eq "/=") {print"div($b);";}
  elsif ($op eq "%=") {print"mod($b);";}
  elsif ($b eq "r") {print"$a = r[header[pc++]];";}
  elsif ($a eq "r") {print"r[header[pc++]] = $b;";}
  elsif ($a) {print("$a $op $b;");}
  else {print"err();";}
  if ($a ne "return 0") {print" break;"}
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
    case 57: if (output) putc(a, output); if (sha1) sha1->put(a); break; // OUT
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
    case 200: a <<= a; break; // A<<=A
    case 201: a <<= b; break; // A<<=B
    case 202: a <<= c; break; // A<<=C
    case 203: a <<= d; break; // A<<=D
    case 204: a <<= m(b); break; // A<<=*B
    case 205: a <<= m(c); break; // A<<=*C
    case 206: a <<= h(d); break; // A<<=*D
    case 207: a <<= header[pc++]; break; // A<<= N
    case 208: a >>= a; break; // A>>=A
    case 209: a >>= b; break; // A>>=B
    case 210: a >>= c; break; // A>>=C
    case 211: a >>= d; break; // A>>=D
    case 212: a >>= m(b); break; // A>>=*B
    case 213: a >>= m(c); break; // A>>=*C
    case 214: a >>= h(d); break; // A>>=*D
    case 215: a >>= header[pc++]; break; // A>>= N
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
  Array<U16> a16; // multi-use
  Component();    // initialize to all 0
};

Component::Component(): limit(0), cxt(0), a(0), b(0), c(0) {}

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

U8 StateTable::ns[1024]={0};
const int StateTable::bound[B]={20,48,15,8,6,5}; // n0 -> max n1, n1 -> max n0

// How many states with count of n0 zeros, n1 ones (0...2)
int StateTable::num_states(int n0, int n1) {
  if (n0<n1) return num_states(n1, n0);
  if (n0<0 || n1<0 || n0>=N || n1>=N || n1>=B || n0>bound[n1]) return 0;
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
  void train(Component& cr, int y);  // reduce prediction error in cr.cm
  int dt[1024];         // division table for cm: dt[i] = 2^16/(i+1.5)
  U16 squasht[4096];    // squash() lookup table
  short stretcht[32768];// stretch() lookup table
  StateTable st;        // next, cminit functions

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

// Print component statistics
void Predictor::stat() {
  printf("\nMemory utilization:\n");
  int cp=7;
  for (int i=0; i<z.header[6]; ++i) {
    assert(cp<z.header.size());
    int type=z.header[cp];
    assert(compsize[type]>0);
    printf("%2d %s", i, compname[type]);
    for (int j=1; j<compsize[type]; ++j)
      printf(" %d", z.header[cp+j]);
    Component& cr=comp[i];
    if (type==MATCH) {
      assert(cr.cm.size()>0);
      assert(cr.ht.size()>0);
      int count=0;
      for (int j=0; j<cr.cm.size(); ++j)
        if (cr.cm[j]) ++count;
      printf(": buffer=%d/%d index=%d/%d (%1.2f%%)", cr.limit/8, cr.ht.size(),
        count, cr.cm.size(), count*100.0/cr.cm.size());
    }
    else if (type==SSE) {
      assert(cr.cm.size()>0);
      int count=0;
      for (int j=0; j<cr.cm.size(); ++j) {
        if (int(cr.cm[j])!=(squash((j&31)*64-992)<<17|z.header[cp+3]))
          ++count;
      }
      printf(": %d/%d (%1.2f%%)", count, cr.cm.size(),
        count*100.0/cr.cm.size());
    }
    else if (type==CM) {
      assert(cr.cm.size()>0);
      int count=0;
      for (int j=0; j<cr.cm.size(); ++j)
        if (cr.cm[j]!=0x80000000) ++count;
      printf(": %d/%d (%1.2f%%)", count, cr.cm.size(),
        count*100.0/cr.cm.size());
    }
    else if (type==MIX) {
      int count=0;
      int m=z.header[cp+3];
      assert(m>0);
      for (int j=0; j<cr.cm.size(); ++j)
        if (int(cr.cm[j])!=65536/m) ++count;
      printf(": %d/%d (%1.2f%%)", count, cr.cm.size(),
        count*100.0/cr.cm.size());
    }
    else if (type==MIX2) {
      int count=0;
      for (int j=0; j<cr.a16.size(); ++j)
        if (int(cr.a16[j])!=32768) ++count;
      printf(": %d/%d (%1.2f%%)", count, cr.a16.size(),
        count*100.0/cr.a16.size());
    }
    else if (cr.ht.size()>0) {
      int hcount=0;
      for (int j=0; j<cr.ht.size(); ++j)
        if (cr.ht[j]>0) ++hcount;
      printf(": %d/%d (%1.2f%%)",
          hcount, cr.ht.size(), hcount*100.0/cr.ht.size());
    }
    cp+=compsize[type];
    printf("\n");
  }
}     

// Initailize the model
Predictor::Predictor(ZPAQL& zr): c8(1), hmap4(1), z(zr) {
  assert(sizeof(U8)==1);
  assert(sizeof(U16)==2);
  assert(sizeof(U32)==4);
  assert(sizeof(short)==2);
  assert(sizeof(int)==4);
  assert(sizeof(long)==sizeof(char*));  // 4 or 8

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
        p[i]=stretch(cr.cm(cr.cxt)>>17);
        break;
      case ICM: // sizebits
        assert((hmap4&15)>0);
        if (c8==1 || (c8&0xf0)==16) cr.c=find(cr.ht, cp[1]+2, z.h(i)+16*c8);
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
        cr.cxt=((z.h(i)+(c8&cp[5]))&(cr.c-1));
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
        cr.cxt=z.h(i)+(c8&cp[5]);
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
          cr.c=find(cr.ht, cp[1]+2, z.h(i)+16*c8);
        cr.cxt=cr.ht[cr.c+(hmap4&15)];  // bit history
        int *wt=(int*)&cr.cm[cr.cxt*2];
        p[i]=clamp2k((wt[0]*p[cp[2]]+wt[1]*64)>>16);
      }
        break;
      case SSE: { // sizebits j start limit
        cr.cxt=(z.h(i)+c8)*32;
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
      case CONST:  // c
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
            cr.b=pos-cr.cm(z.h(i));
            if (cr.b&(cr.ht.size()-1))
              while (cr.a<255 && cr.ht(pos-cr.a-1)==cr.ht(pos-cr.a-cr.b-1))
                ++cr.a;
          }
          else cr.a+=cr.a<255;
          cr.cm(z.h(i))=pos;
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
        assert(cr.cxt==cr.ht[cr.c+(hmap4&15)]);
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

// cr.cm(cr.cxt) has a prediction in the high 22 bits and a count in the
// low 10 bits.  Reduce the prediction error by error/(count+1.5) and
// count up to cr.limit. cm.size() must be a power of 2.
inline void Predictor::train(Component& cr, int y) {
  assert(y==0 || y==1);
  U32& pn=cr.cm(cr.cxt);
  int count=pn&0x3ff;
  int error=y*32767-(cr.cm(cr.cxt)>>17);
  pn+=(error*dt[count]&-1024)+(count<cr.limit);
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
  assert(p>=0 && p<65536);
  assert(high>low && low>0);
  assert(curr>=low && curr<=high);
  U32 mid=low+((high-low)>>16)*p+((((high-low)&0xffff)*p)>>16); // split range
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
  int state;   // input parse state
  int ph, pm;  // sizes of H and M in z
  ZPAQL z;     // holds PCOMP
public:
  PostProcessor(ZPAQL& hz);
  void set(FILE* out, SHA1* p) {z.output=out; z.sha1=p;}  // Set output
  void write(int c);  // Input a byte
};

// Copy ph, pm from block header
PostProcessor::PostProcessor(ZPAQL& hz) {
  state=0;
  ph=hz.header[4];
  pm=hz.header[5];
}

// (PASS=0 | PROG=1 psize[0..1] pcomp[0..psize-1]) data... EOB=-1
void PostProcessor::write(int c) {
  assert(c>=-1 && c<=255);
  switch (state) {
    case 0:  // initial state
      if (c<0) error("Unexpected EOS");
      state=c+1;  // 1=PASS, 2=PROG
      if (state>2) error("unknown post processing type");
      break;
    case 1:  // PASS
      if (z.output && c>=0) putc(c, z.output);  // data
      if (z.sha1 && c>=0) z.sha1->put(c);
      break;
    case 2: // PROG
      if (c<0) error("Unexpected EOS");
      z.hsize=c;  // low byte of psize
      state=3;
      break;
    case 3:  // PROG psize[0]
      if (c<0) error("Unexpected EOS");
      z.hsize+=c*256+1;  // high byte of psize
      z.header.resize(z.hsize+300);
      z.cend=8;
      z.hbegin=z.hend=136;
      z.header[0]=z.hsize&255;
      z.header[1]=z.hsize>>8;
      z.header[4]=ph;
      z.header[5]=pm;
      state=4;
      break;
    case 4:  // PROG psize[0..1] pcomp[0...]
      if (c<0) error("Unexpected EOS");
      assert(z.hend<z.header.size());
      z.header[z.hend++]=c;  // one byte of pcomp
      if (z.hend-z.hbegin==z.hsize-1) {  // last byte of pcomp?
        z.header[z.hend++]=0;
        z.initp();
        state=5;
      }
      break;
    case 5:  // PROG ... data
      z.run(c);
      break;
  }
}

/////////////////////////// Decompress ///////////////////////

// Reject archive filenames that might cause problems
bool validate_filename(const char* filename) {
  int len=strlen(filename);
  if (len<1) return true;  // No name is OK
  if (len>511) return false;  // name too long
  if (strstr(filename, "../")) return false; // no backward paths
  if (strstr(filename, "..\\")) return false;
  if (filename[0]=='/' || filename[0]=='\\') return false; // no absolute path
  for (int i=0; i<len; ++i)  // no control chars, drive letters, or devices
    if ((filename[i]&255)<32 || filename[i]==':') return false;
  return true;
}

// Position in at the start of the archive, either at the beginning
// starting with "zPQ",LEVEL or after zpaqsfx.tag.
// Error if not found.
void find_start(FILE *in) {
  U32 h1=1, h2=2, h3=3, h4=4;
  int c=0;
  if (ftell(in)==0 && getc(in)=='z' && getc(in)=='P' && getc(in)=='Q'
      && (c=getc(in))>=1 && c<=LEVEL) {
    rewind(in);
    return;
  }
  rewind(in);
  while ((c=getc(in))!=EOF) {
    h1=h1*12+c;
    h2=h2*20+c;
    h3=h3*28+c;
    h4=h4*44+c;
    if (h1==0xBD49B113 && h2==0x29EB7F93 && h3==0x6614BE13 && h4==0xB828EB13){
      return;
    }
  }
  error("Start of archive data not found");
}

// Decompress archive argv[2] to stored filenames or argv[3..argc-1]
// If a segment has no filename, then append to the previous file.
void decompress(int argc, char** argv) {
  assert(argc>=3);
  assert(argv[1][0]=='x' || argv[1][0]=='t');

  // Open archive
  FILE* in=fopen(argv[2], "rb");
  if (!in) perror(argv[2]), exit(1);
  find_start(in);

  // Read the archive
  int filecount=0;  // number of files extracted
  FILE *out=0;  // file to extract
  int c;
  while ((c=getc(in))=='z') {
    if (getc(in)!='P' || getc(in) != 'Q' || getc(in)!=LEVEL || getc(in)!=1)
      error("Not ZPAQ");

    // Read block header
    ZPAQL z;
    z.read(in);

    // PostProcessor and Decoder is created and and destroyed for each block
    PostProcessor pp(z);
    Decoder dec(in, z);

    // Read segments
    while ((c=getc(in))==1) {

      // Read the filename
      char filename[512]={0};
      int i;
      for (i=0; (c=getc(in))>0; ++i)
        if (i<511) filename[i]=c;
      if (i>0 && i<512) filename[i]=0;
      printf("%s ", filename);

      // Get comment
      char comment[20]={0};
      i=0;
      while ((c=getc(in))!=EOF && c!=0) {
        if (i<19) comment[i]=c;
        ++i;
      }
      printf("%s -> ", comment);
      if (getc(in)) error("reserved");  // reserved 0

      // If a segment is named, or no output file is open, then
      // create a new output file.
      if (filename[0] || !out) {
        if (out) fclose(out), out=0;

        // If the user gave an output file starting at argv[3], use it instead.
        // If the user doesn't name all the files, then stop after the last
        // named file.
        if (argc>3) {
          if (filecount+3 < argc) {
            out=fopen(argv[filecount+3], "wb");
            if (!out) {
              perror(argv[filecount+3]);
              goto end;
            }
            else
              printf("%s ", argv[filecount+3]);
          }
          else {
            printf("\nSkipping %s and remaining files\n", filename);
            goto end;
          }
        }

        // Otherwise, use the names in the archive, but don't clobber
        // or use suspicious filenames
        else {
          if (!validate_filename(filename)) {
            printf("Error: bad filename\n");
            goto end;
          }
          out=fopen(filename, "rb");
          if (out) {
            printf("Error: won't overwrite\n");
            goto end;
          }
          else {
            out=fopen(filename, "wb");
            if (!out) {
              perror(filename);
              goto end;
            }
          }
        }
        ++filecount;
      }

      // Decompress
      SHA1 sha1;
      long len=0;
      if (argv[1][0]=='t') { // don't postprocess
        while ((c=dec.decompress())!=EOF) {
          if (out) putc(c, out);
          sha1.put(c);
          if (!(len&0xffff))
            printf("%-12ld\b\b\b\b\b\b\b\b\b\b\b\b", len);
          ++len;
        }
      }
      else {
        pp.set(out, &sha1);
        while ((c=dec.decompress())!=EOF) {
          pp.write(c);
          if (!(len&0xffff))
            printf("%-12ld\b\b\b\b\b\b\b\b\b\b\b\b", len);
          ++len;
        }
        pp.write(-1);
      }

      // Check for end of segment and block markers
      int eos=getc(in);  // 253=SHA1 follows, 254=EOS
      if (eos==253) {
        U8 hash[20];
        bool match=true;
        for (int i=0; i<20; ++i) {
          hash[i]=getc(in);
          if (hash[i]!=sha1.result(i))
            match=false;
        }
        if (match)
          printf("Checksum OK ");
        else {
          printf("CHECKSUM FAILED: FILE IS NOT IDENTICAL\n  Archive SHA1: ");
          for (int i=0; i<20; ++i)
            printf("%02x", hash[i]);
          printf("\n  File SHA1:    ");
          for (int i=0; i<20; ++i)
            printf("%02x", sha1.result(i));
        }
      }
      else if (eos!=254)
        error("missing end of segment marker");
      else
        printf("OK, no checksum");
      printf("\n");
    }
    if (c!=255) error("missing end of block marker");
  }
  if (c!=EOF) error("extra data after last block");

  // Close the archive
end:
  if (out) fclose(out);
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
  void stat() {pr.stat();}  // print predictor statistics
};

Encoder::Encoder(FILE* f, ZPAQL& z): 
  out(f), low(1), high(0xFFFFFFFF), pr(z) {}

inline void Encoder::encode(int y, int p) {
  assert(p>=0 && p<65536);
  assert(y==0 || y==1);
  assert(high>low && low>0);
  U32 mid=low+((high-low)>>16)*p+((((high-low)&0xffff)*p)>>16); // split range
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

//////////////////////////// PreProcessor ////////////////////////

const U32 EOS=U32(-1);

class PreProcessor {
  Encoder* encp;
  U32 cmd;  // command
  int ph, pm; // memory sizes for H, M from config file
  int state;  // 0 = init, 1 = after
  U32 b, c, d;  // general purpose state for transforms
  Array<U8> m;
  Array<U32> h;
  void exe(U32 a);  // (x) EXE transform
  void lzp(U32 a);  // (p) LZP transform
  void lzp_flush(); // used by lzp()
public:
  PreProcessor(Encoder* p, U32 cm, int ph_, int pm_);
  void compress(U32 a);
};

PreProcessor::PreProcessor(Encoder* p, U32 cm, int ph_, int pm_):
    encp(p), cmd(cm), ph(ph_), pm(pm_) {
  state=0;
  b=c=d=0;
  m.resize(1, pm);
  h.resize(1, ph);
}

// EXE transform. Replace x86 CALL and JMP relative addresses with
// absolute addresses. The opcode is 0xE8 or 0xE9, followed by a 
// 4 byte address LSB first. Add the offset of the instruction from
// the beginning of the file to the address. Append a program to
// reverse the transform.
void PreProcessor::exe(U32 a) {
  if (pm<3) error("x transform requires at least ph=0, pm=3");

  /* EXE transform. Assume ph=0, pm=3. Decoding is as follows:
  (e8e9 transform. M=queue with C tail and B at head,
   max size 4. If size < 4 then add to buffer. Else if
   *C is xE8 or xE9 then add B to next 4 bytes LSB first
   and output all 5 bytes. Otherwise output *C only.)
  a> 255 jt 65 (EOS)
  *b=a (save input)
  a=b a-=c a== 4 jt 2
    b++ halt (buffer not full)
  a=*c a&= 254 a== 232 jt 5
    a=*c out c++ b++ halt (buffer full and not E8/E9)
  a=*b b-- a<<= 8 a+=*b b-- a<<= 8 a+=*b b-- a<<= 8 a+=*b (load addr)
  a-=c (convert to relative)
  *b=a a>>= 8 b++ *b=a a>>= 8 b++ *b=a a>>= 8 b++ *b=a (save addr)
  a=*c out c++  a=*c out c++  a=*c out c++  a=*c out c++  a=*c out c++
  b++ 
  halt
  (flush buffer at EOS)
  a=b a==c jt 5
    a=*c out c++
  jmp -9 b=0 c=0 halt
  */

  if (state==0) {  // Initialize
    static const U8 prog[85]={  // Generated by "zpaq s" from above program
      1,82,0,239,255,39,65,96,65,138,223,4,39,2,9,56,69,175,
      254,223,232,39,5,69,57,17,9,56,68,10,207,8,132,10,207,8,132,
      10,207,8,132,138,96,215,8,9,96,215,8,9,96,215,8,9,96,69,
      57,17,69,57,17,69,57,17,69,57,17,69,57,17,9,56,65,218,39,
      5,69,57,17,63,247,12,20,56,0};
    for (int i=0; i<85; ++i)
      encp->compress(prog[i]);
    state=1;
  }

  // EXE transform, exactly like the ZPAQL program above except
  // for replacing "a-=c" with "a+=c" (convert address to absolute).
  assert(b-c<=4);
  if (a==EOS) {
    while (c!=b)
      encp->compress(m(c++));  // flush buffer
    encp->compress(EOS);
    b=c=0;
  }
  else {
    m(b)=a;
    if (b-c!=4)
      ++b;
    else if ((m(c)&254)!=232)
      encp->compress(m(c++)), ++b;
    else {
      a=m(b--)<<8;  // read relative address, LSB first
      a=(a+m(b--))<<8;
      a=(a+m(b--))<<8;
      a+=m(b);
      a+=c; // convert to absolute address (opposite of above)
      m(b++)=a;  // put it back
      a>>=8;
      m(b++)=a;
      a>>=8;
      m(b++)=a;
      a>>=8;
      m(b++)=a;
      encp->compress(m(c++));  // compress it, empty buffer
      encp->compress(m(c++));
      encp->compress(m(c++));
      encp->compress(m(c++));
      encp->compress(m(c++));
    }
  }
}

// POST p esc minlen hmul
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
  const int ESC=cmd>>8&255;
  const int MINLEN=cmd>>16&255;
  const int HMUL=cmd>>24&255;

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
*/

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

  // Forward transform uses similar state information:
  // b = number of bytes input
  // c = number of bytes output
  // d = hash of context at c
  // m = buffer with pending output at m(c...b-1)
  // h = index of context hashes h(d) -> m(0...c-1)

  if (a==EOS) {
    while (b!=c)
      lzp_flush();
    encp->compress(EOS);
  }
  else {
    const int MINLEN=cmd>>16&255;
    m(b++)=a;
    if (b>256+MINLEN+c || b==(1<<pm)+c)
      lzp_flush();
  }
}

void PreProcessor::lzp_flush() {
  assert(c!=b);
  const int ESC=cmd>>8&255;
  const int MINLEN=cmd>>16&255;
  const int HMUL=cmd>>24&255;

  // Look for a match
  int len=0;
  U32 p=h(d);
  if (c-p>0 && c-p+258+MINLEN<U32(1<<pm))
    while (len<255+MINLEN && m(p+len)==m(c+len) && c+len!=b)
      ++len;
  if (len>MINLEN) {
    encp->compress(ESC);
    encp->compress(len-MINLEN);
    while (len-->0) {
      assert(c!=b);
      h(d)=c;
      d=d*HMUL+m(c++);
    }
  }

  // Encode a literal
  else {
    encp->compress(m(c));
    if (m(c)==ESC)
      encp->compress(0);
    h(d)=c;
    d=d*HMUL+m(c++);
  }
}

// Compress one byte (0...255) or EOS
void PreProcessor::compress(U32 a) {
  assert(encp);
  assert(state==0 || state==1);
  assert(cmd);
  assert(a<=255 || a==EOS);

  switch(cmd&255) {
    case '0':  // no transform
      if (state==0)
        encp->compress(0), state=1;  // append header
      encp->compress(a);
      break;
    case 'x':
      exe(a); break;
    case 'p':
      lzp(a); break;
    default:
      error("unknown POST command");
  }
}

//////////////////////////// Compress ////////////////////////////

void usage();  // print help message and exit.

// Remove path from filename
const char* strip(const char* filename) {
  assert(filename);
  int len=strlen(filename);
  const char *result=filename;
  for (int i=0; i<len; ++i) {
    if (filename[i]=='/' || filename[i]=='\\' || (i==1 && filename[i]==':'))
      result=filename+i+1;
  }
  return result;
}

// Compress files
// argv = [r](a|b|c)config archive files... 
// or     [r][k](a|b|c)[config] archive file [offset [length]]
// a=append, b=append without checksum, c=create archive,
// k=compress file[offset..offset+length-1], r=store full path.
void compress(int argc, char** argv) {
  assert(argc>=3);
  const char *command=argv[1];
  int rcmd=command[0]=='r';
  if (rcmd) ++command;
  int kcmd=command[0]=='k';
  if (kcmd) ++command;
  if (!(command[0]=='a' || command[0]=='b' || command[0]=='c')) usage();

  // Compile config file
  FILE* cfg=0;   // config file
  U32 cmd=0;     // postprocessor command
  ZPAQL z;       // HCOMP
  if (command[1]) {
    cfg=fopen(command+1, "rb");
    if (!cfg) perror(command+1), exit(1);
    z.verbose=false;
    cmd=z.compile(cfg);
    printf("%1.3f MB memory required.\n", z.memory()/1000000);
  }
  else {
    static U8 header[71]={ // COMP 34 bytes from mid.cfg
      69,0,3,3,0,0,8,3,5,8,13,0,8,17,1,8,
      18,2,8,18,3,8,19,4,4,22,24,7,16,0,7,24,
      255,0,
      // HCOMP 37 bytes
      17,104,74,4,95,1,59,112,10,25,59,112,10,25,59,112,
      10,25,59,112,10,25,59,112,10,25,59,10,59,112,25,69,
      207,8,112,56,0};
    z.load(34, 37, header);
    cmd='0';
  }

  // Fail if first input file does not exist
  FILE *in=0;
  if (argc<=3 || (in=fopen(argv[3], "rb"))==0)
    perror(argv[3]), exit(1);

  // Open archive
  FILE* out=fopen(argv[2], command[0]=='c' ? "wb" : "ab");
  if (!out) perror(argv[2]), exit(1);

  // Write block header
  fprintf(out, "zPQ%c%c", LEVEL, 1);
  long mark=ftell(out)-6;  // last reported size (adjusted for header/trailer)
  z.write(out);

  // Create PreProcessor chain that writes to Encoder
  assert(out);
  Encoder enc(out, z);
  PreProcessor pp(&enc, cmd, z.ph(), z.pm());

  // Compress files argv[3..argc-1]
  for (int i=3; i<(kcmd?4:argc); ++i) {
    if (!in) in=fopen(argv[i], "rb");
    if (!in) perror(argv[i]);  // skip file not found
    else {

      // Write filename (unless kcmd) and size+offset to segment header
      fseek(in, 0, SEEK_END);
      long size=ftell(in);  // file size (-1 if fails)
      long offset=0, length=size;  // from k command
      if (kcmd && size>=0) {
        if (argc>4) offset=atol(argv[4]);
        if (argc>5) length=atoi(argv[5]);
        if (offset<0) offset=0;
        if (offset>size) offset=size;
        if (length<0) length=0;
        if (length>size-offset) length=size-offset;
      }
      fprintf(out, "%c%s%c%ld",
        1, offset?"":rcmd?argv[i]:strip(argv[i]), 0, length);
      if (kcmd && offset>0) fprintf(out, "+%ld", offset);
      fprintf(out, "%c%c", 0, 0);

      // Compress 
      fseek(in, offset, SEEK_SET);
      int c;
      SHA1 sha1;
      size=length;
      printf("%s %ld ", argv[i], length);
      if (kcmd && offset>0) printf("+%ld", offset);
      long i=0;
      while ((c=getc(in))!=EOF) {
        if (kcmd && size--<=0) break;
        if (command[0]!='b') sha1.put(c);
        pp.compress(c);
        if (!(++i&0xffff)) {
          printf("%12ld -> %-12ld"
            "\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b",
            i, ftell(out)-mark);
        }
      }
      pp.compress(EOS);

      // Write segment trailer
      if (command[0]=='b')
        fprintf(out, "%c%c%c%c%c", 0, 0, 0, 0, 254);
      else {
        fprintf(out, "%c%c%c%c%c", 0, 0, 0, 0, 253);
        for (int j=0; j<20; ++j)
          putc(sha1.result(j), out);
      }
      fclose(in);
      in=0;
      printf("-> %ld                        \n", ftell(out)-mark);
      mark=ftell(out);
    }
  }
  putc(255, out);  // block trailer
  printf("-> %ld\n", ftell(out));
  fclose(out);
  enc.stat();  // print statistics
}

////////////////////////// Misc. commands //////////////////////////

// List archive contents
void list(int argc, char** argv) {
  assert(argc>2 && argv[2]);

  // Open archive
  FILE* in=fopen(argv[2], "rb");
  if (!in) perror(argv[2]), exit(1);
  find_start(in);

  // File offsets to get compressed sizes
  long mark=0;

  // Read the file
  int c, blocks=0;
  while ((c=getc(in))=='z') {

    // Read block header
    if (getc(in)!='P' || getc(in)!='Q' || getc(in)!=LEVEL || getc(in)!=1)
      error("not ZPAQ");
    ZPAQL z;
    z.read(in);
    printf("Block %d: requires %1.3f MB memory\n",
     ++blocks, z.memory()/1000000);
    if (argv[1][0]=='v')
      z.list();

    // Read segments
    while ((c=getc(in))==1) {

      // Print filename and comments
      printf("  ");
      while ((c=getc(in))!=EOF && c) putchar(c);
      printf("  ");
      while ((c=getc(in))!=EOF && c) putchar(c);
      if (getc(in)!=0) error("reserved data");
      
      // Skip to end of data
      U32 c4=0xFFFFFFFF;  // last 4 bytes will be all 0
      while ((c=getc(in))!=EOF && (c4=c4<<8|c)!=0) ;
      if (c==EOF) error("unexpected end of file");
      while ((c=getc(in))==0) ;
      if (c==253) {  // print SHA1 in verbose mode
        if (argv[1][0]=='v') {
          printf(" SHA1=");
          for (int i=0; i<20; ++i)
            printf("%02x", getc(in));
        }
        else {
          for (int i=0; i<20; ++i)  // skip SHA1
            getc(in);
        }
      }
      else if (c!=254) error("missing end of segment marker");
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
  z.run(U32(-1));
}

// Compile HCOMP to a C array initialization list
void scompile(int argc, char** argv) {
  ZPAQL z;
  FILE* in=fopen(argv[1]+1, "r");
  if (!in) perror(argv[1]+1), exit(1);
  z.compile(in);
  z.prints();
  fclose(in);
}

///////////////////////////// Main ///////////////////////////

// Print help message and exit
void usage() {
  printf("ZPAQ v1.04 archiver, (C) 2009, Ocarina Networks Inc.\n"
    "Written by Matt Mahoney, " __DATE__ ".\n"
    "This is free software under GPL v3, http://www.gnu.org/copyleft/gpl.html\n"
    "\n"
    "Usage: zpaq command archive files...  Commands are:\n"
    "  a archive files... - Compress files and append to archive.\n"
    "  c archive files... - Compress files to new archive (clobbers).\n"
    "  x archive - Extract all files using stored names (does not clobber).\n"
    "  x archive files... - Extract and rename (clobbers).\n"
    "  l archive - List archive contents.\n"
    "\n"
    "Advanced options:\n"
    "  v archive - List archive contents verbosely.\n"
    "  b archive files... - Compress files and append with no checksum.\n"
    "  k{a|b|c} archive file [m [n]] - {Append|no checksum|create} archive\n"
    "    from n (default all) bytes of file skipping first m (default 0).\n"
    "  [k]{a|b|c}config - Use compression options in config file.\n"
    "  r[k]{a|b|c} - Store paths.\n"
    "  t archive [files...] - extract (like x) without postprocessing.\n"
    "  hconfig args... - Run HCOMP in config with numeric args (no archive).\n"
    "  pconfig in out  - Run PCOMP on files (default stdin/stdout).\n"
    "  sconfig - Compile header to a list of bytes to stdout.\n");
  exit(0);
}

// Command syntax as above
int main(int argc, char** argv) {

  // Check usage
  if (argc<2) 
    usage();

  // Do the command
  char cmd=argv[1][0];
  if ((cmd=='a' || cmd=='b' || cmd=='c' || cmd=='k' || cmd=='r') && argc>=3) {
    compress(argc, argv);
    printf("Used %1.2f seconds\n", clock()/double(CLOCKS_PER_SEC));
  }
  else if ((cmd=='x' || cmd=='t') && argc>=2) {
    decompress(argc, argv);
    printf("Used %1.2f seconds\n", clock()/double(CLOCKS_PER_SEC));
  }
  else if ((cmd=='l' || cmd=='v') && argc>2)
    list(argc, argv);
  else if (cmd=='h')
    hstep(argc, argv);
  else if (cmd=='p')
    prun(argc, argv);
  else if (cmd=='s')
    scompile(argc, argv);
  else
    usage();
  return 0;
}
