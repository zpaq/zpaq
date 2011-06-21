/* wbpe.cpp v1.1 - Preprocessor for text compression

(C) 2011, Dell Inc. Written by Matt Mahoney

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

Preprocessing with wbpe often improves compression of text files
with other compressors like zip, gzip, bzip2, 7zip, ppmd, ppmonstr,
bsc, etc. It does not help if the compressor already has optimizations
for text, for example nanozip, lpaq1, or hook. Nor does it help with
binary files (.bmp, .exe, etc.)
It may also save time because the preprocessed file is usually smaller.

To encode/decode: wbpe command input output
commands:
  c = encode with capitalization modeling
  e = encode without capitalization modeling
  d = decode

The encoded format is a 256 word dictionary followed by a byte sequence
where each byte is either a dictionary code, ESC, CAP, or UPPER. ESC means
that the next byte should be output without decoding. CAP means that
bit 5 of the next byte to be output should be toggled first. UPPER
means that all of the characters decoded by the the next code should
be converted as with CAP. This has the effect of switching between
upper case and lower case letters. The dictionary consists of the
bytes ESC, CAP, and UPPER followed by 256 strings (3 unused)
beginning with a length byte.

A dictionary is built by byte pair encoding. The idea is to encode
the most frequent pair of bytes with the least frequent single
byte. Occurrences of the single byte must be coded as 2 bytes by
preceding it with ESC. The process is repeated until no more
space is saved. The original 3 least frequent bytes are reserved
to code ESC, CAP, and UPPER.

For text files, it is advantageous to not combine bytes of different
types. Letters are paired only with letters, digits with digits,
white space with white space, and punctuation only with identical
characters. UTF-8 characters in the range 128..255 are considered
to be letters.

As an optimization, the input is parsed into strings of characters
of the same type with a maximum length of 19 and then stored in
a hash table with a count associated with each string. Then the
byte pair encoding is performed on this table rather than the
original input. Furthermore, each string of letters cannot contain
a lowercase letter followed by an upper case letter. For example,
"PowerPC" would be parsed into 2 words, "Power" and "PC". Thus, byte
pair encoding would not count "rP" as a possibility. The hash table
stores up to 256K different words. Only the first 2 GB of input is
used to construct the dictionary.

The dictionary is then sorted in the order whitepace, punctuation,
digits, upper case letters, and lower case letters.
Codes 0..2 are reserved for ESC, CAP, and UPPER, and the remaining
253 codes are assigned strings. In a second pass, the input is coded
using greedy parsing to find the longest match. A match may differ
in the case (bit 5) of the first letter, in which case a CAP is
emitted followed by the code, or in the case of all letters, in which
an UPPER is emitted followed by the code.
If no match is found, then the next byte is output preceded by an ESC.

If encoded with the "c" command, then the hash table and dictionary
are built with all upper case converted to lower case. This usually
improves compression although the initial size is larger.

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
using namespace std;

// words are strings of the same chartype
int chartype(int c) {
  c&=255;
  if (c<=32) return 1;  // whitespace
  if (c>='A' && c<='Z') return 257;  // uppercase letter
  if (c>='a' && c<='z') return 257;  // lowercase letter
  if (c>=128) return 257;  // unicode letter
  if (c>='0' && c<='9') return 256;  // digit
  return c;  // punctuation
}

// Upper case?
int isupper(int c) {
  return c>='A' && c<='Z';
}

const int LEN=19;  // maximum string length
enum {TEXT=256, ESC, CAP, UPPER};  // decoding states

// Compare s[0..len-1] and t[0..len-1]. If equal return TEXT.
// If equal except case of first byte then return CAP.
// If equal except case of all bytes is opposite then return UPPER.
// Otherwise return ESC.
int match(const unsigned char* s, const unsigned char* t, int len) {
  if (len<1 || !memcmp(s, t, len))
     return TEXT;
  if ((s[0]^t[0])==32 && (len<2 || !memcmp(s+1, t+1, len-1)))
     return CAP;
  for (int i=0; i<len; ++i)
    if ((s[i]^t[i])!=32)
      return ESC;
  return UPPER;
}

// Decode c in dict[dn][3] to out where out[0] is current length
// dict[i][0..2] means that dict[i][0] decodes recursively to
// the byte pair dict[i][1], dict[i][2]. If no dict[i][0] matches
// c for lower i, then c decodes to itself.
void printto(unsigned char dict[][3], int dn, int c, unsigned char* out) {
  int i;
  for (i=dn-1; i>=0 && dict[i][0]!=c; --i);  // find dict[i][0] == c
  if (i>=0) {  // found?
    printto(dict, i, dict[i][1], out);
    printto(dict, i, dict[i][2], out);
  }
  else
    out[++out[0]]=c;
}    

// print dictionary entry n
void print(unsigned char dict[][3], int dn, int c) {
  unsigned char s[LEN+1]={0};
  printto(dict, dn, c, s);
  for (int i=1; i<=s[0]; ++i) {
    if (s[i]<32) printf("^%c", s[i]+64);
    else putchar(s[i]);
  }
}

// Hash table or dictionary element containing a count and a word
struct Element {
  int count;
  unsigned char s[LEN+1];  // length in first byte
  void print();
};

// true if Element a < b (for sorting)
bool less(const Element& a, const Element& b) {
  for (int i=1; i<=a.s[0] && i<=b.s[0]; ++i) {
    int ac=chartype(a.s[i]), bc=chartype(b.s[i]);
    if (ac<bc) return true;
    if (ac>bc) return false;
    if (a.s[i]<b.s[i]) return true;
    if (a.s[i]>b.s[i]) return false;
  }
  return a.s[0]<b.s[0];
}

// Print an element (for debugging)
void Element::print() {
  printf("%10d \"", count);
  for (int j=1; j<=s[0]; ++j) {
    if (s[j]<32) printf("^%c", s[j]+64);
    else putchar(s[j]);
  }
  printf("\"\n");
}

// For counting strings
struct Hashtable {
  enum {N=1<<18};  // N strings of length LEN
  Element* t;  // table of N
  void count(const unsigned char* s, int len);
  Hashtable();
  void print();
};

// Allocate elements
Hashtable::Hashtable() {
  t=(Element*)calloc(N, sizeof(Element));
  if (!t) fprintf(stderr, "Out of memory\n"), exit(1);
}

// Count string s which has length len unless table is full.
void Hashtable::count(const unsigned char* s, int len) {
  if (len>LEN) len=len;
  if (len<1) return;
  if (!s) return;

  // Compute hash
  unsigned int h=0;
  for (int i=0; i<len; ++i) {
    h+=s[i]+1;
    h*=773;
  }
  h&=N-1;

  // Look for s in t. If found then increment count. If not found then
  // insert in first of 4 empty slots. If full then give up.
  for (int i=0; i<4; ++i) {
    if (t[h^i].s[0]==len && memcmp(t[h^i].s+1, s, len)==0) {
      ++t[h^i].count;
      return;
    }
    else if (t[h^i].count==0) {
      t[h^i].count=1;
      t[h^i].s[0]=len;
      memcpy(t[h^i].s+1, s, len);
      return;
    }
  }
}

// Print hash table statistics.
void Hashtable::print() {
  int types=0, tokens=0, chars=0;
  for (int i=0; i<N; ++i) {
    if (t[i].count) {
      ++types;
      tokens+=t[i].count;
      chars+=t[i].count*t[i].s[0];
    }
  }
  printf("Parsed %d bytes into %d tokens.\n", chars, tokens);
  printf("%d of %d hash table entries used.\n", types, N);
}

// Encode. cmd is 'c' or 'e'. If 'c' then use capitalization modeling.
void encode(FILE* in, FILE* out, int cmd) {

  // Parse input into a Hashtable and count. A token consists
  // of up to LEN characters of the same type such that all
  // letters are the same case or only the first letter is upper case.
  // If cmd is 'c' then store as lower case.
  int c;        // input byte
  int chars=0;  // input limited to 2 GB to prevent int overflows
  Hashtable ht; // list of input words with counts
  unsigned char s[LEN+1]={0};  // input buffer
  int len=0;    //length of s in 0..LEN
  int n1[256]={0};  // char count
  printf("Pass 1, building dictionary...");
  while ((c=getc(in))!=EOF && ++chars<2000000000) {
    ++n1[c];
    if (len==0)
      s[len++]=c;
    else if (len<LEN && chartype(c)==chartype(s[len-1])
        && isupper(c)<=isupper(s[0])
        && (len==1 || isupper(c)==isupper(s[len-1])))
      s[len++]=c;
    else {
      if (cmd=='c')  // convert to lower case
        for (int i=0; i<len && isupper(s[i]); ++i)
          s[i]^=32;
      ht.count(s, len);
      len=1;
      s[0]=c;
    }
    if ((chars&0xffffff)==0) putchar('.');
  }
  if (len>0) ht.count(s, len);
  printf("\nRead first %d characters.\n", chars);
  ht.print();

  // Find 3 least freqent bytes to represent ESC, CAP, UPPER
  int esc=-1, cap=-1, upper=-1;
  for (int i=0; i<256; ++i)
    if (esc<0 || n1[i]<n1[esc]) esc=i;
  for (int i=0; i<256; ++i)
    if (i!=esc && (cap<0 || n1[i]<n1[cap])) cap=i;
  for (int i=0; i<256; ++i)
    if (i!=esc && i!=cap && (upper<0 || n1[i]<n1[upper])) upper=i;
  printf("Assigned codes ESC=%d (count %d) CAP=%d (%d) UPPER=%d (%d)\n",
    esc, n1[esc], cap, n1[cap], upper, n1[upper]);

  // Reduce using byte code pairing
  printf("Byte pair encoding...\n");
  int escaped=0;
  unsigned char dict[512][3]={{0}};  // BPE [0] expands to [1],[2]
  int dn;  // size of dict
  for (dn=0; dn<512; ++dn) {

    // Count bytes and pairs
    int c1=0, n2[256][256]={{0}};
    memset(n1, 0, sizeof(n1));
    for (int i=0; i<Hashtable::N; ++i) {
      if (ht.t[i].count) {
        for (int j=1; j<=ht.t[i].s[0]; ++j) {
          int c0=ht.t[i].s[j]&255;
          n1[c0]+=ht.t[i].count;
          if (j>1) n2[c1][c0]+=ht.t[i].count;
          c1=c0;
        }
      }
    }

    // Find least frequent byte
    int min0=-1;
    for (int i=0; i<256; ++i)
      if (n1[i]>=0 && (min0<0 || n1[i]<n1[min0])
          && i!=cap && i!=esc && i!=upper)
        min0=i;
    if (min0<0) break;

    // Find most frequent pair
    int max0=0, max1=0;
    for (int i=0; i<256; ++i)
      for (int j=0; j<256; ++j)
        if (n2[i][j]>n2[max1][max0])
          max1=i, max0=j;

    // Quit if encoding would make the string larger
    if (n1[min0]>=n2[max1][max0]) break;

    // Update dict
    escaped+=n1[min0];  // size of data in ht
    dict[dn][0]=min0;
    dict[dn][1]=max1;
    dict[dn][2]=max0;

    // Update ht with pair substitution
    int size=escaped;
    for (int i=0; i<Hashtable::N; ++i) {
      if (ht.t[i].count) {
        int k=1;  // output pointer
        for (int j=1; j<=ht.t[i].s[0]; ++j, ++k) {
          if (j<ht.t[i].s[0] && ht.t[i].s[j]==max1 && ht.t[i].s[j+1]==max0)
            ht.t[i].s[k]=min0, ++j;
          else if (k<j)
            ht.t[i].s[k]=ht.t[i].s[j];
        }
        ht.t[i].s[0]=k-1;
        size+=ht.t[i].count*(k-1);
      }
    }
  }
  printf("%d pairs encoded, %d escaped. Sorting dictionary...\n",
    dn, escaped);

  // Create expanded dictionary
  Element dict2[256];
  memset(dict2, 0, sizeof(dict2));
  for (int i=0; i<256; ++i)
    printto(dict, dn, i, dict2[i].s);
  dict2[cap].s[0]=0;  // empty
  dict2[esc].s[0]=0;
  dict2[upper].s[0]=0;

  // Bubble sort alphabetically. ESC and CAP will move to the front.
  for (int i=255; i>=0; --i) {
    for (int j=0; j<i; ++j) {
      if (less(dict2[j+1], dict2[j])) {
        Element t=dict2[j];
        dict2[j]=dict2[j+1];
        dict2[j+1]=t;
      }
    }
  }

  // Build index for fast lookup.
  // idx[c] = location of first word in dict2 starting with c
  // or 256 if not found.
  int idx[256];
  for (int i=0; i<256; ++i)
    idx[i]=256;
  for (int i=256; i>=0; --i)
    if (dict2[i].s[0])
      idx[dict2[i].s[1]]=i;

  // Write output header
  esc=0;
  cap=1;
  upper=2;
  putc(esc, out);
  putc(cap, out);
  putc(upper, out);
  for (int i=0; i<256; ++i) {
    for (int j=0; j<=dict2[i].s[0]; ++j)
      putc(dict2[i].s[j], out);
  }

  // Encode
  printf("Pass 2: encoding...\n");
  rewind(in);
  len=0;
  int tokens=0;
  while (true) {

    // fill input buffer s
    while (len<LEN && (c=getc(in))!=EOF)
      s[len++]=c;
    if (len<1)
      break;

    // Find longest match in dict2. Try both upper and lower case.
    int bi=256, bestmatch=0, bestmode=ESC, mode=ESC;
    for (int i=0; i<=32; i+=32) {
      for (int j=idx[s[0]^i]; j<256 && dict2[j].s[0]>0
           && dict2[j].s[1]==(s[0]^i); ++j) {
        if (dict2[j].s[0]<=len && dict2[j].s[0]>bestmatch
            && (mode=match(s, dict2[j].s+1, dict2[j].s[0]))!=ESC) {
          bestmatch=dict2[j].s[0];
          bi=j;
          bestmode=mode;
        }
      }
    }

    // Encode match
    if (bi<256) {
      if (bestmode==CAP) putc(cap, out), ++dict2[cap].count;
      if (bestmode==UPPER) putc(upper, out), ++dict2[upper].count;
      putc(bi, out);
      ++dict2[bi].count;
      len-=bestmatch;
    }
    else {  // encode escaped literal
      bestmatch=1;
      putc(esc, out);
      putc(s[0], out);
      ++dict2[esc].count;
      --len;
    }
    if (len>0) memmove(s, s+bestmatch, len);
    if ((++tokens&0xfffff)==0) {
      int len=printf("%ld -> %ld ", ftell(in), ftell(out));
      for (int i=0; i<len; ++i)
        putchar('\b');
      fflush(stdout);
    }
  }

  // Print dictionary with counts
  printf("\n\n"
    "Code   Count   Meaning\n"
    "---  --------- -------\n");
  for (int i=0; i<256; ++i) {
    if (dict2[i].count>=0) {
      printf("%3d ", i);
      if (i==cap) printf("%10d CAP\n", dict2[i].count);
      else if (i==esc) printf("%10d ESC\n", dict2[i].count);
      else if (i==upper) printf("%10d UPPER\n", dict2[i].count);
      else dict2[i].print();
    }
  }
  printf("\n%d strings encoded\n", tokens);
}

// Decode file
void decode(FILE* in, FILE* out) {
  int i=0, j=0, c;
  int esc=getc(in);
  int cap=getc(in);
  int upper=getc(in);
  unsigned char dict[256][256];
  while ((c=getc(in))!=EOF) {
    if (i<TEXT) {  // load dict
      dict[i][j]=c;
      if (j==dict[i][0]) j=0, ++i;
      else ++j;
    }
    else if (i==ESC) {
      putc(c, out);
      i=TEXT;
    }
    else if (c==esc) i=ESC;
    else if (c==cap) i=CAP;
    else if (c==upper) i=UPPER;
    else {
      for (j=1; j<=dict[c][0]; ++j) {
        putc(dict[c][j]^(i==TEXT?0:32), out);
        if (i==CAP) i=TEXT;
      }
      i=TEXT;
    }
  }
}

int main(int argc, char** argv) {

  // Check args
  int cmd;
  if (argc<4 || ((cmd=argv[1][0])!='c' && cmd!='d' && cmd!='e')) {
    fprintf(stderr, 
      "wbpe v1.1 preprocessor for text compression\n"
      "(C) 2011, Dell Inc. Written by Matt Mahoney on %s\n"
      "This program is licensed under GPL v3,"
      " http://www.gnu.org/licenses/gpl.html\n"
      "\n"
      "Usage: wbpe command input output\n"
      "Commands:\n"
      "c = encode with capitalization modeling (usually works better)\n"
      "e = encode without capitalization modeling\n"
      "d = decode\n", __DATE__);
    return 1;
  }

  // Open files
  FILE* in=fopen(argv[2], "rb");
  if (!in) perror(argv[2]), exit(1);
  FILE* out=fopen(argv[3], "wb");
  if (!out) perror(argv[3]), exit(1);

  // encode/decode
  if (cmd=='d')
    decode(in, out);
  else
    encode(in, out, cmd);
  printf("%ld -> %ld\n", ftell(in), ftell(out));
  return 0;
}
