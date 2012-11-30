/* bwtpre.cpp - BWT transform (does not compress or invert)
   (C) 2009, Matt Mahoney,
   This is free software under GPL, http://www.gnu.org/licenses/gpl.txt
   Derived from BBB.

To BWT encode: bwtpre size input output

Block size is 2^(size+10). Output format is [n,p,block]...0
where n is the block size, p is the index of the first byte,
and block is n bytes after the BWT transform, i.e. the
input byte sorted by right context with wraparound. Numbers
are 4 bytes, MSB first. The output ends with 4 0 bytes.
*/

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>
using namespace std;

// Globals
typedef unsigned char U8;  // byte
int blockSize=0;  // max BWT block size
int n=0;          // number of elements in block, 0 < n <= blockSize
U8* block;        // [n] text to transform
int* ptr;         // [n] indexes into block to sort
const int PAD=72; // extra bytes in block (copy of beginning)

// true if block[a+1...] < block[b+1...] wrapping at n
inline bool lessthan(int a, int b) {
  if (a<0) return false;
  if (b<0) return true;
  int r=block[a+1]-block[b+1];  // an optimization
  if (r) return r<0;
  r=memcmp(&block[a+2], &block[b+2], PAD-8);
  if (r) return r<0;
  if (a<b) {
    int r=memcmp(&block[a+1], &block[b+1], n-b-1);
    if (r) return r<0;
    r=memcmp(&block[a+n-b], &block[0], b-a);
    if (r) return r<0;
    return memcmp(&block[0], &block[b-a], a)<0;
  }
  else {
    int r=memcmp(&block[a+1], &block[b+1], n-a-1);
    if (r) return r<0;
    r=memcmp(&block[0], &block[b+n-a], a-b);
    if (r) return r<0;
    return memcmp(&block[a-b], &block[0], b)<0;
  }

}

// read n<=blockSize bytes from in to block, BWT, write to out
int encodeBlock(FILE* in, FILE* out) {
  n=fread(&block[0], 1, blockSize, in);  // n = actual block size
  if (n<1) return 0;  // EOF
  for (int i=0; i<PAD; ++i) block[i+n]=block[i];

  // sort the pointers to the block
  for (int i=0; i<n; ++i) ptr[i]=i;
  stable_sort(&ptr[0], &ptr[n], lessthan);  // faster than sort() or qsort()
  int p=min_element(&ptr[0], &ptr[n])-&ptr[0];
  putc(n>>24, out);  // block size
  putc(n>>16, out),
  putc(n>>8, out);
  putc(n, out);
  putc(p>>24, out);  // original location of first byte
  putc(p>>16, out);
  putc(p>>8, out);
  putc(p, out);
  for (int i=0; i<n; ++i)
    putc(block[ptr[i]], out);  // BWT transformed data
  return n;
}

// forward BWT
void encode(FILE* in, FILE* out) {
  block=(U8*)malloc(blockSize+PAD);
  ptr=(int*)malloc((blockSize+1)*sizeof(int));
  if (!block || !ptr)
    fprintf(stderr, "out of memory\n"), exit(1);
  while (encodeBlock(in, out)) ;
  putc(0, out);  // mark EOF
  putc(0, out);
  putc(0, out);
  putc(0, out);
  free(ptr);
  free(block);
}

/////////////////////////////// main ////////////////////////////

int main(int argc, char** argv) {

  // check for args
  if (argc<4) {
    printf("BWT preprocessor, ver. 1.1\n"
      "(C) 2009, Matt Mahoney.  Free under GPL, http://www.gnu.org/licenses/gpl.txt\n"
      "\n"
      "To BWT encode a file: bbb size input output\n"
      "Uses block size 2^(size+10) - 256. Memory used is 5 x block size\n"
      "\n");
    exit(0);
  }
  
  // BWT transform
  blockSize=(1024<<atoi(argv[1]))-256;
  printf("block size = %d\n", blockSize);
  if (blockSize<=0) fprintf(stderr, "block size too big\n"), exit(1);
  FILE* in=fopen(argv[2], "rb");
  if (!in) perror(argv[2]), exit(1);
  FILE* out=fopen(argv[3], "wb");
  if (!out) perror(argv[3]), exit(1);
  encode(in, out);
  return 0;
}

