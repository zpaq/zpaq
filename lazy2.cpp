/* lazy2.cpp v1.0 - LZ77 compressor

(C) 2012 Dell Inc. Written by Matt Mahoney
Licensed under GPL v3, http://www.gnu.org/copyleft/gpl.html

lazy2 compresses and decompresses files in the LZ77 format with e8e9
preprocessing. Uncompressed files must be smaller than 2^30 bytes.
Uses 1 GB memory.

  Usage: lazy2 cmd input output

where cmd = 1..5 compresses faster..better, d decompresses.

lazy2 differs from lazy in that it uses an e8e9 preprocessor and
limits the file size so that it can all be read into 1 GB memory.
The e8e9 preprocessor improves compression of x86 code in .exe and .dll
files. To preprocess, the input is scanned in reverse order and for
any 5 byte sequence of the form (E8|E9 xx xx xx 00|FF) beginning
at offset i, the 3 middle bytes are interpreted as a 24 bit number
LSB first and i is added modulo 2^24. This transform is applied
prior to LZ77 compression. During decompression, the inverse transform
is applied.

In LZ77, data is compressed by coding strings that match previous
strings by giving an offset of 1 or more to the previous copy, and
the number of bytes to copy. Characters not matched to previous
output are coded uncompressed as literals, preceded by a length
code of 1 or more.

In a typical file, the number of bits needed to code an offset
tends to have a uniform distribution, but the number of bits needed to
code a literal or match length tends to have a geometric distribution.
About 3/4 of the codes are matches. Thus, we use a 2 bit code (00) to
indicate a literal, and a fixed length code of 5 bits, where the first
2 bits are not 00, to code the number of bits in the offset (0..23).
Literal and match lengths are coded using an interleaved Elias Gamma format,
where the leading 1 bit is removed and the remaining bits are preceded
by a 1, and a 0 marks the end. For example, a literal length of 1abc
(where a,b,c are 0 or 1) is coded as 1,a,1,b,1,c,0. For example,
1101 is coded as 1,1,1,0,1,1,0.

The minimum match length is 4. Thus, we code only the upper bits of
the match length as a marked binary number and code the last 2 bits
normally. For example, a match length of 1abcd would be coded as
1,a,1,b,0,cd.

The fields separated by commas are packed LSB (low bits) first. The
last byte is padded with 0. For example, ab,cde,fghij would be
coded into 2 bytes as hijcdeab 000000fg. Zero padding is possible
because no sequence of 7 or fewer 0 bits forms a complete code for
any output.

A literal is coded as 00,n,L[n], where n is the number of literals in
marked binary, and L[n] is a sequence of literals. For example, a
single literal 0 byte is coded as 00,0,00000000.

Match offsets may range from 1 to 2^24-1 (1..16777215).
A match is coded as jj,kkk,u,nn,m[jjkkk-8] where the 2 bits jj > 0.
The match offset is coded by removing the leading 1 bit and coding
the remaining 0..23 bits as m. The number of bits in m is given as
(jj - 1)*8 + kkk, where jj is the first 2 bit code and kkk is the
next 3 bit code. The low 2 bits of the match length are coded as nn.
The upper bits are coded as the marked binary number, u. For example,
a match with offset 1abc and length 1def is coded as
01,011,1,d,0,ef,abc.

A compressed file should never have a match length
greater than 16777215 or an offset starting before the beginning
of the file, even though it is possile to produce such codes.
The decompresser does not check for these cases, and would produce
undefined output.

Compression is achieved by reading input into a 16 MB rotating buffer
and using a hash table indexed by a hash of the next 4 bytes to
find matches in the past input. A hash indexes a bucket containing
2^level (2, 4, 8, 16, or 32) possible pointers to previous occurrences
of the same hash in a table of size 2^19 buckets (using 4, 8, 16, 32,
64 MB memory). The rotating buffer is implemented by a concatenated
pair of 16 MB input buffers. Every 16 MB, the upper buffer is moved
to the lower buffer, and a new upper buffer is read from input.
Thus, memory usage by level is:

  1: 36 MB
  2: 40 MB
  3: 48 MB
  4: 64 MB
  5: 96 MB

Decompression uses memory equal to the output size. Higher compression
levels compress slower but make decompression sligtly faster because there
is less input to decompress. Some examples on silesia.tar, timed on a 2.0
GHz T3200:
                    lazy                    lazy2
                  --------                --------  
  1: 211948544 -> 77291986 in 11.01 sec   76952753 in 11.22 sec
  2: 211948544 -> 75132954 in 13.68 sec   74794978 in 14.57 sec
  3: 211948544 -> 72760459 in 17.32 sec   72422267 in 18.28 sec
  4: 211948544 -> 70852680 in 24.51 sec   70514689 in 25.02 sec
  5: 211948544 -> 69397506 in 36.91 sec   69059876 in 37.47 sec
  5:              decompress   3.87 sec   decompress   4.10 sec

During compression, the program searches each pointer in the hash
bucket to find the longest match, and taking the closest in case
of ties, and stopping immediately if it finds a match of length
at least 128. If no match satisfies the minimum length, then the
input is coded as a literal. The minimum match length is 4, except
in the case where the offset is at least 65536 and at least one
literal has been coded since the last match, in which case the minimum
is 5. Otherwise, the match code would take more space than 4 more
literals.

As each byte is output, the hash table is updated by choosing one
element in the bucket selected by the current hash and replacing it
with a pointer to the current buffer location. The element within
the bucket is selected by the low bits of the current location.

As an optimization, each element contains a 24 bit pointer and the
8 bit value of the byte it points to. This allows the first
byte to be compared in the hash table rather than the buffer
in order to avoid a second cache miss if they differ. The hash table
is also aligned on a 64 byte address so that buckets fit within a
single cache line (or 2 lines for level 5).

To compile: g++ -O3 lazy.cpp

Use option -DNDEBUG to turn off run time checks, although this
doesn't seem to have much effect on speed.

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <assert.h>

int tab[34][3]={{0}}; // literals, match length, offset counts by log n

// return 0..32 bits in x not counting leading 0 bits
int lg(unsigned x) {
  for (unsigned i=0; i<32; ++i)
    if ((1u<<i)>x) return i;
  return 32;
}

// Writes variable length bit codes packed LSB first
struct BitWriter {
  FILE* out;
  unsigned buf;  // pending output in low bits
  int n;  // bits in buf (0..32)
  void put(unsigned x, int k) {  // write k (0..25) bits of x
    assert(k>=0 && k<=25);
    assert(k+n<=32);
    assert(n>=0 && n<=7);
    x&=(1<<k)-1;
    buf|=x<<n;
    n+=k;
    while (n>7) putc(buf, out), buf>>=8, n-=8;
  }
  void flush() {  // write last byte
    assert(n<=7);
    if (n>0) putc(buf, out), buf=0, n=0;
  }
  BitWriter(): out(0), buf(0), n(0) {}
};

// Write literal sequence buf[i-lit..i-1], set lit=0
// 00,u,L[u*8] = L literals
void write_literal(unsigned char* buf, int i, int& lit, BitWriter& out) {
  assert(lit>=0);
  if (lit<1) return;
  int ll=lg(lit);
  assert(ll>=1 && ll<=32);
  ++tab[ll][0];
  ++tab[33][0];
  out.put(0, 2);
  --ll;
  while (--ll>=0) {
    out.put(1, 1);
    out.put((lit>>ll)&1, 1);
  }
  out.put(0, 1);
  while (lit) out.put(buf[i-lit--], 8);
}

// Write match sequence of given length and offset (off in 1..2^24)
// jj,kkk,u,nn,m[jjkkk-8] = match length unn, offset 1m
void write_match(int len, int off, BitWriter& out) {
  assert(len>=4);
  assert(off>=1 && off<(1<<24));
  int ll=lg(len);
  assert(ll>=3 && ll<=32);
  int lo=lg(off);
  assert(lo>=1 && lo<=24);
  ++tab[ll][1];
  ++tab[33][1];
  ++tab[lo][2];
  --lo;
  out.put((lo>>3)+1, 2);
  out.put(lo&7, 3);
  --ll;
  while (--ll>=2) {
    out.put(1, 1);
    out.put((len>>ll)&1, 1);
  }
  out.put(0, 1);
  out.put(len&3, 2);
  out.put(off, lo);
}

// Args are c|d input output
int main(int argc, char** argv) {

  // Start timer
  clock_t start=clock();

  // Check args
  int cmd=-1;  // 0=decompress, 1..5=compression level
  if (argc!=4 || (cmd=argv[1][0])!='d' && (cmd<'1' || cmd>'5')) {
    fprintf(stderr,
      "lazy2 v1.0 (C) 2012, Dell Inc. Written by Matt Mahoney\n"
      "Licensed under GPL v3. http://www.gnu.org/copyleft/gpl.html\n"
      "To compress:   lazy2 N input output  (N = 1..5 = fastest..best)\n"
      "To decompress: lazy2 d input output\n");
    return 1;
  }
  if (cmd=='d') cmd=0;
  else cmd-='0';
  assert(cmd>=0 && cmd<=5);

  // Open files
  FILE* in=fopen(argv[2], "rb");
  if (!in) return perror(argv[2]), 1;
  FILE* out=fopen(argv[3], "wb");
  if (!out) return perror(argv[3]), 1;
  const unsigned MAXBUF=1<<30;

  // Compress
  if (cmd) {

    // Allocate buf (uncompressed data) and hashtable ht (compression only)
    // ht[h] low 24 bits points to buf[i..i+HASHORDER-1], high 8 bits is buf[i]
    const int BUFSIZE=1<<24;
    const int HTSIZE=1<<(19+cmd);   // hashtable size, must be a power of 2
    unsigned char* buf=(unsigned char*)calloc(MAXBUF, 1);
    int* ht=(int*)calloc(HTSIZE+16, sizeof(int));
    if (!buf || !ht) return fprintf(stderr, "Out of memory\n"), 1;
    ht+=16-((ht-(int*)0)&15);  // align on 64 byte address
    unsigned h=0;  // context hash of buf[i..]
    BitWriter bw;
    bw.out=out;

    // Read block into buf
    const int n=fread(buf, 1, MAXBUF, in);
    if (n<0 || n>=int(MAXBUF)) fprintf(stderr, "File too big\n"), exit(1);

    // e8e9 filter
    int e8e9=0;
    for (int i=n-5; i>=0; --i) {
      if (((buf[i]&254)==0xe8) && ((buf[i+4]+1)&254)==0) {
        unsigned a=(buf[i+1]|buf[i+2]<<8|buf[i+3]<<16)+i;
        buf[i+1]=a;
        buf[i+2]=a>>8;
        buf[i+3]=a>>16;
        ++e8e9;
      }
    }
    printf("%d e8e9 transforms\n", e8e9);

    // Scan the block just read in. ht may point to previous block
    const unsigned int HASHBUCKET=(1<<cmd); // searches per hash
    int lit=0;  // number of output literals pending
    for (int i=0; i<n;) {

      // Search for longest match, or pick closest in case of tie
      int blen=0, bp=0, len=0;
      for (int k=0; k<int(HASHBUCKET); ++k) {
        int p=ht[h+k];
        if ((p>>24&255)==buf[i]) {  // compare in ht first
          p=(p&BUFSIZE-1)|(i&-BUFSIZE);
          if (p>=i) p-=BUFSIZE;
          if (p>0 && p<i && p+BUFSIZE>i) {
            for (len=0; i+len<n && buf[p+len]==buf[i+len]; ++len);
            if (len>blen || len==blen && p>bp) blen=len, bp=p;
          }
        }
        if (blen>=128) break;
      }

      // If match is long enough, then output any pending literals first,
      // and then the match. blen is the length of the match.
      const int off=i-bp;  // offset
      if (blen>=4+(lit!=0 && off>=(1<<16)) && off>0 && off<(1<<24)) {
        write_literal(buf, i, lit, bw);
        write_match(blen, off, bw);
      }

      // Otherwise add to literal length
      else {
        blen=1;
        ++lit;
      }

      // Update index, advance blen bytes
      while (blen--) {
        ht[h+(i&HASHBUCKET-1)]=(i&BUFSIZE-1)+(buf[i]<<24);
        ++i;
        if (i+3<n) {
          h>>=cmd-1;
          h*=96;
          h+=buf[i+3]+1;
          h<<=cmd-1;
          h&=HTSIZE-1;
        }
      }
    }

    // Write pending literals at end of block
    write_literal(buf, n, lit, bw);
    bw.flush();
  }

  // Decode. Syntax is:
  // 00,u,L[u*8] = u literals
  // jj,kkk,u,nn,m[jjkkk-8] = match length u*4+nn, offset 1m, jj>0
  // u is marked binary
  if (cmd==0) {

    // Allocate output buffer
    unsigned char* buf=(unsigned char*)calloc(MAXBUF, 1);
    if (!buf) fprintf(stderr, "Out of memory\n"), exit(1);
    
    unsigned state=0; // 0..4=expect new code, match len, offset, lit len, lit
    unsigned len=0;   // match or literal length
    unsigned ptr=0;   // next buf location to write
    unsigned bits=0;  // input bits, LSB first
    unsigned m=0;     // expected offset bits
    unsigned nb=0;    // number of bits in bits (0..32)
    int c;            // input byte
    while ((c=getc(in))!=EOF) {
      bits+=c<<nb;
      nb+=8;
      if (state==0) {  // expect new code
        len=1;
        if (bits&3) {  // match code jj,kkk
          m=((bits&3)-1)*8;  // read offset bits (0..23)
          bits>>=2;
          m+=bits&7;
          bits>>=3;
          nb-=5;
          state=1;
        }
        else {  // literal, discard 00
          bits>>=2;
          nb-=2;
          state=3;
        }
      }
      while (state==1 && nb>=3) {  // expect match length u,nn
        if (bits&1) {
          bits>>=1;
          len+=len+(bits&1);
          bits>>=1;
          nb-=2;
        }
        else {
          bits>>=1;
          len<<=2;
          len+=bits&3;
          bits>>=2;
          nb-=3;
          state=2;
        }
      }
      if (state==2 && nb>=m) {  // expect m offset bits
        assert(m<24);
        assert(len>0);
        unsigned p=ptr-(bits&((1<<m)-1)|(1<<m));  // match location
        assert(p<MAXBUF);
        assert(p+len<=MAXBUF);
        while (len--)  // output match
          buf[ptr++]=buf[p++];
        bits>>=m;
        nb-=m;
        state=0;
      }
      while (state==3 && nb>=2) {  // expect literal length
        assert(len>0);
        if (bits&1) {
          bits>>=1;
          len+=len+(bits&1);
          bits>>=1;
          nb-=2;
        }
        else {
          bits>>=1;
          --nb;
          state=4;
        }
      }
      if (state==4 && nb>=8) {  // expect len literals
        assert(len>0);
        assert(ptr<MAXBUF);
        assert(ptr+len<=MAXBUF);
        buf[ptr++]=bits&255;
        bits>>=8;
        nb-=8;
        if (--len<1) state=0;
        assert(nb<8);
      }
    }

    // e8e9 filter
    for (unsigned i=0; i<ptr; ++i) {
      if (i+4<ptr && ((buf[i]&254)==0xe8) && ((buf[i+4]+1)&254)==0) {
        unsigned a=(buf[i+1]|buf[i+2]<<8|buf[i+3]<<16)-i;
        buf[i+1]=a;
        buf[i+2]=a>>8;
        buf[i+3]=a>>16;
      }
      putc(buf[i], out);
    }
  }

  // Print compression statistics
  if (cmd) {
    printf("\n"
           "       Range To            Literals    Matches     Offsets\n"
           "------------ ------------- --------- ---------- ----------\n");
    for (int i=1; i<33; ++i)
      printf("%12u %-12u %10d %10d %10d\n",
             1<<(i-1), (1<<i)-1, tab[i][0], tab[i][1], tab[i][2]);
    printf("Total                     %10d %10d\n\n", tab[33][0], tab[33][1]);
  }
  printf("%ld -> %ld in %1.2f sec\n", ftell(in), ftell(out),
    double(clock()-start)/CLOCKS_PER_SEC);
  fclose(out);
  fclose(in);
  return 0;
}
