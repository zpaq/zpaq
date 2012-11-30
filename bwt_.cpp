// bwt.cpp - BWT transform (does not compress)
// (C) 2006-2009, Matt Mahoney,
// This is free software under GPL, http://www.gnu.org/licenses/gpl.txt

// Derived from BBB, without the compression part. Commands are the same.


#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <algorithm>
#define NDEBUG  // remove for debugging
#include <cassert>
using namespace std;

// 8, 16, 32 bit unsigned types
typedef unsigned char U8;
typedef unsigned short U16;
typedef unsigned int U32;

//////////////////////////// Array ////////////////////////////

// Array<T, ALIGN> a(n); creates n elements of T initialized to 0 bits.
// Constructors for T are not called.
// Indexing is bounds checked if assertions are on.
// a.size() returns n.
// a.resize(n) changes size to n, padding with 0 bits or truncating.
// Copy and assignment are not supported.
// Memory is aligned on a ALIGN byte boundary (power of 2), default is none.

template <class T, int ALIGN=0> class Array {
private:
  int n;     // user size
  int reserved;  // actual size
  char *ptr; // allocated memory, zeroed
  T* data;   // start of n elements of aligned data
  void create(int i);  // create with size i
public:
  explicit Array(int i=0) {create(i);}
  ~Array();
  T& operator[](int i) {
#ifndef NDEBUG
    if (i<0 || i>=n) fprintf(stderr, "%d out of bounds %d\n", i, n), exit(1);
#endif
    return data[i];
  }
  const T& operator[](int i) const {
#ifndef NDEBUG
    if (i<0 || i>=n) fprintf(stderr, "%d out of bounds %d\n", i, n), exit(1);
#endif
    return data[i];
  }
  int size() const {return n;}
  void resize(int i);  // change size to i
private:
  Array(const Array&);  // no copy or assignment
  Array& operator=(const Array&);
};

template<class T, int ALIGN> void Array<T, ALIGN>::resize(int i) {
  if (i<=reserved) {
    n=i;
    return;
  }
  char *saveptr=ptr;
  T *savedata=data;
  int saven=n;
  create(i);
  if (savedata && saveptr) {
    memcpy(data, savedata, sizeof(T)*min(i, saven));
    free(saveptr);
  }
}

template<class T, int ALIGN> void Array<T, ALIGN>::create(int i) {
  n=reserved=i;
  if (i<=0) {
    data=0;
    ptr=0;
    return;
  }
  const int sz=ALIGN+n*sizeof(T);
  ptr = (char*)calloc(sz, 1);
  if (!ptr) fprintf(stderr, "Out of memory\n"), exit(1);
  data = (ALIGN ? (T*)(ptr+ALIGN-(((long)ptr)&(ALIGN-1))) : (T*)ptr);
  assert((char*)data>=ptr && (char*)data<=ptr+ALIGN);
}

template<class T, int ALIGN> Array<T, ALIGN>::~Array() {
  free(ptr);
}

///////////////////////////// Encoder ///////////////////////////

typedef enum {COMPRESS, DECOMPRESS} Mode;
class Encoder {
  Mode mode;
  FILE *archive;
public:
  Encoder(Mode m, FILE* f): mode(m), archive(f) {}
  Mode getMode() const {return mode;}
  long size() const {return ftell(archive);}  // length of archive so far
  void flush() {}  // call this when compression is finished

  // Compress one byte
  void compress(int c) {
    assert(mode==COMPRESS);
    putc(c, archive);
  }

  // Decompress and return one byte
  int decompress() {
    assert(mode==DECOMPRESS);
    return getc(archive);
  }
};

///////////////////////////////// BWT //////////////////////////////

// Globals
bool fast=false;  // transform method: fast uses 5x blocksize memory, slow uses 5x/4
int blockSize=0x400000;  // max BWT block size
int n=0;          // number of elements in block, 0 < n <= blockSize
Array<U8> block;  // [n] text to transform
Array<int> ptr;   // [n] or [n/16] indexes into block to sort
const int PAD=72; // extra bytes in block (copy of beginning)
int pos=0;        // bytes compressed/decompressed so far
bool quiet=false; // q option?

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

// read 4 byte value LSB first, or -1 at EOF
int read4(FILE* f) {
  unsigned int r=getc(f);
  r|=getc(f)<<8;
  r|=getc(f)<<16;
  r|=getc(f)<<24;
  return r;
}

// read n<=blockSize bytes from in to block, BWT, write to out
int encodeBlock(FILE* in, Encoder& en) {
  n=fread(&block[0], 1, blockSize, in);  // n = actual block size
  if (n<1) return 0;
  assert(block.size()>=n+PAD);
  for (int i=0; i<PAD; ++i) block[i+n]=block[i];

  // fast mode: sort the pointers to the block
  if (fast) {
    if (!quiet) printf("sorting     %10d to %10d  \r", pos, pos+n);
    assert(ptr.size()>=n);
    for (int i=0; i<n; ++i) ptr[i]=i;
    stable_sort(&ptr[0], &ptr[n], lessthan);  // faster than sort() or qsort()
    int p=min_element(&ptr[0], &ptr[n])-&ptr[0];
    en.compress(n>>24);
    en.compress(n>>16);
    en.compress(n>>8);
    en.compress(n);
    en.compress(p>>24);
    en.compress(p>>16);
    en.compress(p>>8);
    en.compress(p);
    if (!quiet) printf("compressing %10d to %10d  \r", pos, pos+n);
    for (int i=0; i<n; ++i) {
      en.compress(block[ptr[i]]);
      if (!quiet && i && (i&0xffff)==0) 
        printf("compressed  %10d of %10d  \r", pos+i, pos+n);
    }
    pos+=n;
    return n;
  }

  // slow mode: divide the block into 16 parts, sort them, write the pointers
  // to temporary files, then merge them.
  else {

    // write header
    if (!quiet) printf("writing header at %10d          \r", pos);
    int p=0;
    for (int i=1; i<n; ++i)
      if (lessthan(i, 0)) ++p;
    en.compress(n>>24);
    en.compress(n>>16);
    en.compress(n>>8);
    en.compress(n);
    en.compress(p>>24);
    en.compress(p>>16);
    en.compress(p>>8);
    en.compress(p);

    // sort pointers in 16 parts to temporary files
    const int subBlockSize = (n-1)/16+1;  // max size of sub-block
    int start=0, end=subBlockSize;  // range of current sub-block
    FILE* tmp[16];  // temporary files
    for (int i=0; i<16; ++i) {
      if (!quiet) printf("sorting      %10d to %10d  \r", pos+start, pos+end);
      tmp[i]=tmpfile();
      if (!tmp[i]) perror("tmpfile()"), exit(1);
      for (int j=start; j<end; ++j) ptr[j-start]=j;
      stable_sort(&ptr[0], &ptr[end-start], lessthan);
      for (int j=start; j<end; ++j) {  // write pointers
        int c=ptr[j-start];
        fprintf(tmp[i], "%c%c%c%c", c, c>>8, c>>16, c>>24);
      }
      start=end;
      end+=subBlockSize;
      if (end>n) end=n;
    }

    // merge sorted pointers
    if (!quiet) printf("merging      %10d to %10d  \r", pos, pos+n);
    unsigned int t[16];  // current pointers
    for (int i=0; i<16; ++i) {  // init t
      rewind(tmp[i]);
      t[i]=read4(tmp[i]);
    }
    for (int i=0; i<n; ++i) {  // merge and compress
      int j=min_element(t, t+16, lessthan)-t;
      en.compress(block[t[j]]);
      if (!quiet && i && (i&0xffff)==0) 
        printf("compressed  %10d of %10d  \r", pos+i, pos+n);
      t[j]=read4(tmp[j]);
    }
    for (int i=0; i<16; ++i)  // delete tmp files
      fclose(tmp[i]);
    pos+=n;
    return n;
  }
}

// forward BWT
void encode(FILE* in, Encoder& en) {
  block.resize(blockSize+PAD);
  if (fast) ptr.resize(blockSize+1);
  else ptr.resize((blockSize-1)/16+2);
  while (encodeBlock(in, en));
/*  en.compress(0);  // mark EOF
  en.compress(0);
  en.compress(0);
  en.compress(0);*/
}

// inverse BWT of one block
int decodeBlock(Encoder& en, FILE* out) {

  // read block size
  int n=en.decompress();
  n=n*256+en.decompress();
  n=n*256+en.decompress();
  n=n*256+en.decompress();
  if (n==0) return n;
  if (!blockSize) {  // first block?  allocate memory
    blockSize = n;
    if (!quiet) printf("block size = %d\n", blockSize);
    block.resize(blockSize+PAD);
    if (fast) ptr.resize(blockSize);
    else ptr.resize(blockSize/16+256);
  }
  else if (n<1 || n>blockSize) {
    printf("file corrupted: block=%d max=%d\n", n, blockSize);
    exit(1);
  }

  // read pointer to first byte
  int p=en.decompress();
  p=p*256+en.decompress();
  p=p*256+en.decompress();
  p=p*256+en.decompress();
  if (p<0 || p>=n) {
    printf("file corrupted: p=%d n=%d\n", p, n);
    exit(1);
  }

  // decompress and read block
  for (int i=0; i<n; ++i) {
    block[i]=en.decompress();
    if (!quiet && i && (i&0xffff)==0)
      printf("decompressed %10d of %10d  \r", pos+i, pos+n);
  }
  for (int i=0; i<PAD; ++i) block[i+n]=block[i];  // circular pad

  // count (sort) bytes
  if (!quiet) printf("unsorting    %10d to %10d  \r", pos, pos+n);
  Array<int> t(257);  // i -> number of bytes < i in block
  for (int i=0; i<n; ++i)
    ++t[block[i]+1];
  for (int i=1; i<257; ++i)
    t[i]+=t[i-1];
  assert(t[256]==n);

  // fast mode: build linked list
  if (fast) {
    for (int i=0; i<n; ++i)
      ptr[t[block[i]]++]=i;
    assert(t[255]==n);

    // traverse list
    for (int i=0; i<n; ++i) {
      assert(p>=0 && p<n);
      putc(block[p], out);
      p=ptr[p];
    }
    return n;
  }

  // slow: build ptr[t[c]+c+i] = position of i*16'th occurrence of c in block
  Array<int> count(256);  // i -> count of i in block
  for (int i=0; i<n; ++i) {
    int c=block[i];
    if ((count[c]++ & 15)==0)
      ptr[(t[c]>>4)+c+(count[c]>>4)]=i;
  }

  // decode
  int c=block[p];
  for (int i=0; i<n; ++i) {
    assert(p>=0 && p<n);
    putc(c, out);

    // find next c by binary search in t so that t[c] <= p < t[c+1]
    c=127;
    int d=64;
    while (d) {
      if (t[c]>p) c-=d;
      else if (t[c+1]<=p) c+=d;
      else break;
      d>>=1;
    }
    if (c==254 && t[255]<=p && p<t[256]) c=255;
    assert(c>=0 && c<256 && t[c]<=p && p<t[c+1]);

    // find approximate position of p
    int offset=p-t[c];
    const U8* q=&block[ptr[(t[c]>>4)+c+(offset>>4)]];  // start of linear search
    offset&=15;

    // find next p by linear search for offset'th occurrence of c in block
    while (offset--)
      if (*++q != c) q=(const U8*)memchr(q, c, &block[n]-q);
    assert(q && q>=&block[0] && q<&block[n]);
    p=q-&block[0];
  }
  pos+=n;
  return n;
}

// inverse BWT of file
void decode(Encoder& en, FILE* out) {
  while (decodeBlock(en, out));
}

/////////////////////////////// main ////////////////////////////

int main(int argc, char** argv) {
  clock_t start=clock();

  // check for args
  if (argc<4) {
    printf("bwt Big Block BWT file encoder, ver. 1\n"
      "(C) 2009, Matt Mahoney.  Free under GPL, http://www.gnu.org/licenses/gpl.txt\n"
      "\n"
      "To encode a file: bbb command input output\n"
      "\n"
      "Commands:\n"
      "c = code (default),  d = decode.\n"
      "f = fast mode, needs 5x block size memory, default uses 1.25x block size.\n"
      "q = quiet (no output except error messages).\n"
      "bN, kN, mN = use block size N bytes, KiB, MiB, default = m4 (compression only).\n"
      "\n"
      "Commands should be concatenated in any order, e.g. bwt cfm100q foo foo.bwt\n"
      "means code foo to foo.bwt in fast mode using 100 MiB block size in quiet\n"
      "mode.\n");
    exit(0);
  }

  // read options
  Mode mode=COMPRESS;
  const char* p=argv[1];
  while (*p) {
    switch (*p) {
      case 'c': mode=COMPRESS; break;
      case 'd': mode=DECOMPRESS; break;
      case 'f': fast=true; break;
      case 'b': blockSize=atoi(p+1); break;
      case 'k': blockSize=atoi(p+1)<<10; break;
      case 'm': blockSize=atoi(p+1)<<20; break;
      case 'q': quiet=true; break;
    }
    ++p;
  }
  if (blockSize<1) printf("Block size must be at least 1\n"), exit(1);
  
  // open files
  FILE* in=fopen(argv[2], "rb");
  if (!in) perror(argv[2]), exit(1);
  FILE* out=fopen(argv[3], "wb");
  if (!out) perror(argv[3]), exit(1);

  // encode or decode
  if (mode==COMPRESS) {
    if (!quiet) printf("Compressing %s to %s in %s mode, block size = %d\n", 
      argv[2], argv[3], fast ? "fast" : "slow", blockSize);
    Encoder en(COMPRESS, out);
    encode(in, en);
    en.flush();
  }
  else if (mode==DECOMPRESS) {
    blockSize=0;
    if (!quiet) printf("Decompressing %s to %s in %s mode\n",
      argv[2], argv[3], fast ? "fast" : "slow");
    Encoder en(DECOMPRESS, in);
    decode(en, out);
  }
  if (!quiet) printf("%ld -> %ld in %1.2f sec                  \n", ftell(in), ftell(out),
    (clock()-start+0.0)/CLOCKS_PER_SEC);
  return 0;
}

