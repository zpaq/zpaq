/* lzpre.cpp v2.1 - LZ77 preprocessor for ZPAQ

(C) 2011 Dell Inc. Written by Matt Mahoney
Licensed under GPL v3, http://www.gnu.org/copyleft/gpl.html

Usage: lzpre c|d input output
c = compress, d = decompress

Compressed format is byte oriented LZ77. Lengths and offsets are MSB first:

  00xxxxxx                            x+1 (1..64) literals follow
  01xxxyyy yyyyyyyy                   copy x+5 (5..12), offset y+1 (1..2048)
  10xxxxxx yyyyyyyy yyyyyyyy          copy x+1 (1..64), offset y+1 (1..65536)
  11xxxxxx yyyyyyyy yyyyyyyy yyyyyyyy copy x+1 (1..64), offset y+1 (1..2^24)

Decompression needs 16 MB memory. The compressor uses 64 MB consisting
of 2 16 MB buffers and a 8M (32 MB) hash table as an index to find matches.
For each byte position in the input buffer, the table stores 2 hashes
of order 10 and 5 in buckets of size 16 and 8 respectively. The buckets
are searched in that order, taking the longest match found (greedy
matching), breaking ties by taking the smaller offset. If a match
is found, then the remaining buckets are skipped. To update, the bucket
entry indexed by the low bits of position is replaced.

The minimum match length is 5, 6, or 7 for a match code of length
2, 3, 4 respectively. Matches and literal strings longer than 64 are
coded as a series of length 64 codes.

As a speed optimization, each hash table entry contains a 24 bit pointer
and the byte pointed to, packed into 32 bits. If the byte in the hash
table mismatches, then this avoids a cache miss to compare the first
byte of the buffer. The hash table is aligned so that buckets do not
cross 64 byte cache lines.

To compile: g++ -O3 lzpre.cpp -o lzpre

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Write literal sequence buf[i-lit..i-1], set lit=0
void write_literal(unsigned char* buf, int i, int& lit, FILE* out) {
  while (lit>0) {
    int lit1=lit;
    if (lit1>64) lit1=64;
    putc(lit1-1, out);
    for (int j=i-lit; j<i-lit+lit1; ++j) putc(buf[j], out);
    lit-=lit1;
  }
}

// Write match sequence of given length and offset (off in 1..2^24)
void write_match(int len, int off, FILE* out) {
  --off;
  while (len>0) {
    int len1=len;
    if (len1>64) len1=64;
    if (off<2048 && len1>=5 && len1<=12) {
      putc(64+(len1-5)*8+(off>>8), out);
      putc(off, out);
    }
    else if (off<65536) {
      putc(128+len1-1, out);
      putc(off>>8, out);
      putc(off, out);
    }
    else {
      putc(192+len1-1, out);
      putc(off>>16, out);
      putc(off>>8, out);
      putc(off, out);
    }
    len-=len1;
  }
}

// Args are c|d input output
int main(int argc, char** argv) {

  // Start timer
  clock_t start=clock();

  // Check args
  int cmd;
  if (argc!=4 || (cmd=argv[1][0])!='c' && cmd!='d') {
    fprintf(stderr, "To compress/decompress: lzpre c/d input output\n");
    return 1;
  }

  // Open files
  FILE* in=fopen(argv[2], "rb");
  if (!in) return perror(argv[2]), 1;
  FILE* out=fopen(argv[3], "wb");
  if (!out) return perror(argv[3]), 1;

  // Tunable LZ77 parameters
  const int HTSIZE=1<<23;   // hashtable size, must be a power of 2
  const int BUFSIZE=1<<24;  // buffer size (each of 2), max 2^24
  const int HASHES=2;       // number of hashes computed per byte
  const int HASHORDER[HASHES]={10,5};  // context order per hash
  const int HASHMUL[HASHES]={44,48};   // hash multipliers
  const unsigned int HASHBUCKET[HASHES]={16,8}; // searches per hash

  // Allocate buf (uncompressed data) and hashtable ht (compression only)
  // ht[h] low 24 bits points to buf[i..i+HASHORDER-1], high 8 bits is buf[i]
  unsigned char* buf=(unsigned char*)calloc(BUFSIZE, 1+(cmd=='c'));
  int* ht=(int*)(cmd=='c'?calloc(HTSIZE+16, sizeof(int)):buf);
  if (!buf || !ht) return fprintf(stderr, "Out of memory\n"), 1;
  ht+=16-((ht-(int*)0)&15);  // align on 64 byte address
  int h[HASHES]={0};  // context hashes of buf[i..]

  // Compress
  while (cmd=='c') {

    // Read block into second half of buf
    const int n=fread(buf+BUFSIZE, 1, BUFSIZE, in)+BUFSIZE;
    if (n<=BUFSIZE) break;

    // Scan the block just read in. ht may point to previous block
    int lit=0;  // number of output literals pending
    for (int i=BUFSIZE; i<n;) {

      // Search for longest match, or pick closest in case of tie
      // Try the longest context orders first. If a match is found, then
      // skip the lower orders as a speed optimization.
      int blen=0, bp=0, len=0;
      for (int j=0; j<HASHES; ++j) {
        for (int k=0; k<int(HASHBUCKET[j]); ++k) {
          int p=ht[h[j]+k];
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
        if (blen>=HASHORDER[j]) break;
      }

      // If match is long enough, then output any pending literals first,
      // and then the match. blen is the length of the match.
      const int off=i-bp;  // offset
      if (blen>=5+(off>=2048)+(off>=65536) && off>0 && off<(1<<24)) {
        write_literal(buf, i, lit, out);
        write_match(blen, off, out);
      }

      // Otherwise add to literal length
      else {
        blen=1;
        ++lit;
      }

      // Update index, advance blen bytes
      while (blen--) {
        for (int j=0; j<HASHES; ++j) {
          ht[h[j]+(i&HASHBUCKET[j]-1)]=(i&BUFSIZE-1)+(buf[i]<<24);
        }
        ++i;
        for (int j=0; j<HASHES; ++j) {
          if (i+HASHORDER[j]<=n) {
            h[j]/=HASHBUCKET[j];
            h[j]*=HASHMUL[j];
            h[j]+=buf[i+HASHORDER[j]-1]+1;
            h[j]*=HASHBUCKET[j];
            h[j]&=HTSIZE-1;
          }
        }
      }
    }

    // Write pending literals at end of block
    write_literal(buf, n, lit, out);

    // Move data from second half of buf to first half if more input
    // is expected.
    if (n==BUFSIZE*2) memmove(buf, buf+BUFSIZE, BUFSIZE);
  }

  // Decode. state is as follows:
  // 0 = expecting a literal or match code.
  // 1 = decoding a literal with len bytes remaining.
  // 2 = expecting last offset byte of a match code.
  // 3,4 = expecting 2,3 match offset bytes.
  if (cmd=='d') {
    int c, i=0, state=0, len=0, off=0;
    while ((c=getc(in))!=EOF) {
      if (state==0) {
        state=1+(c>>6);
        if (state==1) off=0, len=c+1;  // literal
        else if (state==2) off=c&7, len=(c>>3)-3;  // short match
        else off=0, len=(c&63)+1;  // match
      }
      else if (state==1) { // literal
        putc(buf[i++&BUFSIZE-1]=c, out);
        if (--len==0) state=0;
      }
      else if (state>2) {  // reading offset
        off=off<<8|c;
        --state;
      }
      else { // state==2, match
        off=off<<8|c;
        off=i-off-1;
        while (len--)
          putc(buf[i++&BUFSIZE-1]=buf[off++&BUFSIZE-1], out);
        state=0;
      }
    }
  }

  // Print compression statistics
  printf("%ld -> %ld in %1.2f sec\n", ftell(in), ftell(out),
    double(clock()-start)/CLOCKS_PER_SEC);
  return 0;
}

