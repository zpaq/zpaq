/* lazy.cpp v1.0 - LZ77 compressor

(C) 2012 Dell Inc. Written by Matt Mahoney
Licensed under GPL v3, http://www.gnu.org/copyleft/gpl.html

lazy compresses and decompresses files in the LZ77 format.

  Usage: lazy cmd input output

where cmd = 1..5 compresses faster..better, d decompresses.

Compression prints statistics on the length distribution of literals and
the length and offset distribution of matches. For example, a tar
file of the Silesia corpus has the following statistics:

> lazy 3 silesia.tar x

   Range To        Literals    Matches    Offsets
-------- -------- --------- ---------- ----------
       1 1           1979387          0      73405
       2 3           1052317          0      25667
       4 7            463352   13062258      48235
       8 15           524935    3839590     280674
      16 31            58529    1490365     798551
      32 63             9642     591038     687947
      64 127            3445     118826     852638
     128 255            1775      26516     925461
     256 511            1162      20403     994744
     512 1023            891       3662    1087207
    1024 2047            547       1260    1151070
    2048 4095            328        391    1328039
    4096 8191            133        228    1393830
    8192 16383            42         41    1441973
   16384 32767             5         13    1449732
   32768 65535             2          1    1419433
   65536 131071            0          0    1239571
  131072 262143            0          0    1108786
  262144 524287            0          0     967952
  524288 1048575           0          0     740373
 1048576 2097151           0          0     505221
 2097152 4194303           0          0     324415
 4194304 8388607           0          0     202399
 8388608 16777215          0          0     107269
Total                4096492   19154592

211948544 -> 72760459 in 17.32 sec

> lazy d x y
72760459 -> 211948544 in 3.71 sec

> fc/b silesia.tar y
Comparing files silesia.tar and Y
FC: no differences encountered

(Times are on a 2.0 GHz T3200, Win32, on 1 of 2 cores).

In LZ77, data is compressed by coding strings that match previous
strings by giving an offset of 1 or more to the previous copy, and
the number of bytes to copy. Characters not matched to previous
output are coded uncompressed as literals, preceded by a length
code of 1 or more.

This distribution is typical. The number of bits needed to code an offset
tends to have a uniform distribution, but the number of bits needed to
code a literal or match length tends to have a geometric distribution.
About 3/4 of the codes are matches. Thus, we use a 2 bit code (00) to
indicate a literal, and a fixed length code of 5 bits, where the first
2 bits are not 00, to code the number of bits in the offset (0..23).
Literal and match lengths are coded using a marked binary format,
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

Decompression always uses 16 MB. Higher compression levels compress
slower but make decompression sligtly faster because there is less
input to decompress. Some examples on silesia.tar:

  1: 211948544 -> 77291986 in 11.01 sec
  2: 211948544 -> 75132954 in 13.68 sec
  3: 211948544 -> 72760459 in 17.32 sec
  4: 211948544 -> 70852680 in 24.51 sec
  5: 211948544 -> 69397506 in 36.91 sec

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

int tab[26][3]={{0}}; // literals, match length, offset counts by log n

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
  assert(ll>=1 && ll<=24);
  ++tab[ll][0];
  ++tab[25][0];
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
//  printf("match %d %d\n", off, len);
  assert(len>=4 && len<(1<<24));
  assert(off>=1 && off<(1<<24));
  int ll=lg(len);
  assert(ll>=3 && ll<=24);
  int lo=lg(off);
  assert(lo>=1 && lo<=24);
  ++tab[ll][1];
  ++tab[25][1];
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
      "lazy v1.0 (C) 2012, Dell Inc. Written by Matt Mahoney\n"
      "Licensed under GPL v3. http://www.gnu.org/copyleft/gpl.html\n"
      "To compress:   lazy N input output  (N = 1..5 = fastest..best)\n"
      "To decompress: lazy d input output\n");
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
  BitWriter bw;
  if (cmd) bw.out=out;

  // Allocate buf (uncompressed data) and hashtable ht (compression only)
  // ht[h] low 24 bits points to buf[i..i+HASHORDER-1], high 8 bits is buf[i]
  const int HTSIZE=1<<(19+cmd);   // hashtable size, must be a power of 2
  const int BUFSIZE=1<<24;  // buffer size (each of 2), max 2^24
  unsigned char* buf=(unsigned char*)calloc(BUFSIZE, 1+(cmd!=0));
  int* ht=(int*)(cmd?calloc(HTSIZE+16, sizeof(int)):buf);
  if (!buf || !ht) return fprintf(stderr, "Out of memory\n"), 1;
  ht+=16-((ht-(int*)0)&15);  // align on 64 byte address
  unsigned h=0;  // context hash of buf[i..]

  // Compress
  while (cmd) {

    // Read block into second half of buf
    const int n=fread(buf+BUFSIZE, 1, BUFSIZE, in)+BUFSIZE;
    if (n<=BUFSIZE) {
      bw.flush();
      break;
    }

    // Scan the block just read in. ht may point to previous block
    const unsigned int HASHBUCKET=(1<<cmd); // searches per hash
    int lit=0;  // number of output literals pending
    for (int i=BUFSIZE; i<n;) {

      // Search for longest match, or pick closest in case of tie
      int blen=0, bp=0, len=0;
      for (int k=0; k<int(HASHBUCKET); ++k) {
        int p=ht[h+k];
        if ((p>>24&255)==buf[i]) {  // compare in ht first
          p=(p&BUFSIZE-1)+BUFSIZE;
          if (p>=i) p-=BUFSIZE;
          if (p>0 && p<i && p+(1<<24)>i) {
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

    // Move data from second half of buf to first half if more input
    // is expected.
    if (n==BUFSIZE*2) memmove(buf, buf+BUFSIZE, BUFSIZE);
  }

  // Decode. Syntax is:
  // 00,u,L[u*8] = u literals
  // jj,kkk,u,nn,m[jjkkk-8] = match length u*4+nn, offset 1m, jj>0
  // u is marked binary
  if (cmd==0) {
    unsigned state=0; // 0..4=expect new code, match len, offset, lit len, lit
    unsigned len=0;   // match or literal length
    unsigned ptr=0;   // next buf location to write
    unsigned bits=0;  // input bits, LSB first
    unsigned m=0;     // expected offset bits
    unsigned n=0;     // number of bits in bits (0..32)
    int c;            // input byte
    while ((c=getc(in))!=EOF) {
      bits+=c<<n;
      n+=8;
      if (state==0) {  // expect new code
        len=1;
        if (bits&3) {  // match code jj,kkk
          m=((bits&3)-1)*8;  // read offset bits (0..23)
          bits>>=2;
          m+=bits&7;
          bits>>=3;
          n-=5;
          state=1;
        }
        else {  // literal, discard 00
          bits>>=2;
          n-=2;
          state=3;
        }
      }
      while (state==1 && n>=3) {  // expect match length u,nn
        if (bits&1) {
          bits>>=1;
          len+=len+(bits&1);
          bits>>=1;
          n-=2;
        }
        else {
          bits>>=1;
          len<<=2;
          len+=bits&3;
          bits>>=2;
          n-=3;
          state=2;
        }
      }
      if (state==2 && n>=m) {  // expect m offset bits
        assert(m<24);
        assert(len>0);
        unsigned p=ptr-(bits&((1<<m)-1)|(1<<m));  // match location
        while (len--)  // output match
          putc(buf[ptr++&(BUFSIZE-1)]=buf[p++&(BUFSIZE-1)], out);
        bits>>=m;
        n-=m;
        state=0;
      }
      while (state==3 && n>=2) {  // expect literal length
        assert(len>0);
        if (bits&1) {
          bits>>=1;
          len+=len+(bits&1);
          bits>>=1;
          n-=2;
        }
        else {
          bits>>=1;
          --n;
          state=4;
        }
      }
      if (state==4 && n>=8) {  // expect len literals
        assert(len>0);
        putc(buf[ptr++&(BUFSIZE-1)]=bits&255, out);
        bits>>=8;
        n-=8;
        if (--len<1) state=0;
        assert(n<8);
      }
    }
  }

  // Print compression statistics
  if (cmd) {
    printf("\n"
           "   Range To        Literals    Matches    Offsets\n"
           "-------- -------- --------- ---------- ----------\n");
    for (int i=1; i<25; ++i)
      printf("%8d %-8d %10d %10d %10d\n",
             1<<(i-1), (1<<i)-1, tab[i][0], tab[i][1], tab[i][2]);
    printf("Total             %10d %10d\n\n", tab[25][0], tab[25][1]);
  }
  printf("%ld -> %ld in %1.2f sec\n", ftell(in), ftell(out),
    double(clock()-start)/CLOCKS_PER_SEC);
  fclose(out);
  fclose(in);
  return 0;
}
