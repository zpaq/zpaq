/* zpaqd v6.24 - ZPAQ compression development tool - Apr. 12, 2013.

  This software is provided as-is, with no warranty.
  I, Matt Mahoney, on behalf of Dell Inc., release this software into
  the public domain.   This applies worldwide.
  In some countries this may not be legally possible; if so:
  I grant anyone the right to use this software for any purpose,
  without any conditions, unless such conditions are required by law.

zpaqd is a tool for developing custom compression algorithms in
the ZPAQ format. It can compress, list, and decompress files in the
ZPAQ format. Compression requires a configuration file specifying
the compression algorithm. The program can also run or single-step
configuration files for debugging. To compress:

  zpaqd {a|c}[inst] config.cfg [args...] archive.zpaq files...

Compress files to archive using the algorithm specified in config
to a single block. See the specification at http://mattmahoney/zpaq
for a description of the archive format. See libzpaq.h for config
file syntax. config can also be 1, 2, or 3 to select built-in
compression levels fast, mid, or max without a config file.

The first letter of the command is "a" to append to archive or "c"
to overwrite it. The letters following are:

  i - Don't save input file size as a comment.
  n - Don't save file names.
  s - Don't  SHA-1 checksums or test post-processor.
  t - Don't 13 byte header locator tag.

Up to 9 numeric arguments may be passed to the config file as $1
through $9. The archive filename must not start with a digit or "-"
to distinguish it from an argument. In Windows, file names must
have only ASCII characters (no international characters). For example:

  zpaqd aist max 2 arc file1 file2

compresses file1 and file2 and appends one block to arc.zpaq without
comments, checksums, or tags, but with file names saved. The
compression algorithm is described in max.cfg with $1 set to 2. If
max.cfg has a PCOMP section then it will specify an external post-processor
command that expects an input and output file name as its last 2 arguments.
The output will be passed as "zpaq.tmp". This file will be passed to the
compression algorithm and then deleted.

  zpaqd l archive

Lists the contents of archive. Compression algorithms saved in the
headers are displayed in ZPAQL format suitable for pasting into
a config file, with the exception of the external post-processing
command, which is not saved. If the archive is in journaling
format (created by zpaq), then the contents of the header, hash
table, and index blocks is shown.

  zpaqd d[s] archive [output [B [N [S]]]]

Decompress N (default all) blocks or S (default all) segments of
archive.zpaq (whichever comes first) to output (default: discard output)
starting at block B (default 1 = first block). For each block decompressed,
list the offset, block number and memory required to decompress. For each
segment, list the first 32 bits of the stored and computed SHA1 checksums,
file name, comment, compressed size, output size, and whether the
stored and computed checksums match (if both exist). Command "ds" does
not verify the checksum, which is a little faster.

  zpaqd r config [args...] {h|p} [input [output]]

Run the HCOMP or PCOMP (h or p) section of config.cfg. input and output
default to stdin and stdout. The section is run once for each byte
of input in the A register. The PCOMP section (if present) is run
once more with EOF (-1) in A.

  zpaqd t config [args...] {h|p} [N|xN]

Trace the HCOMP or PCOMP section of config.cfg once for each decimal
or hexadecimal argument. After each ZPAQL instruction is executed,
the instruction and register contents are displayed in the same base.
After a HALT instruction, any nonzero memory contents are displayed.
Hex arguments are identified by a leading "x", for example:

  zpaqd t max.cfg h 255 xff

runs the HCOMP section of max.cfg twice with 255 as input, but
displays the second output in hex.

  zpaqd s files...

Compute file sizes and SHA-1 checksums.

To compile: g++ -O3 zpaqd.cpp libzpaq.cpp -o zpaqd
For non x86 architectures, compile with -DNOJIT
For Linux, Unix, Mac, etc., compile with -Dunix

*/
#define _FILE_OFFSET_BITS 64  // In Linux make sizeof(off_t) == 8
#define _CRT_DISABLE_PERFCRIT_LOCKS  // make getc() faster
#include "libzpaq.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>
#include <time.h>
#include <string>
#include <vector>
#include <map>

using std::string;
using std::vector;
using std::map;

#ifndef unix
#ifdef _MSC_VER  // Microsoft C++
#define fseeko(a,b,c) _fseeki64(a,b,c)
#define ftello(a) _ftelli64(a)
#else
#ifndef fseeko
#define fseeko(a,b,c) fseeko64(a,b,c)
#endif
#ifndef ftello
#define ftello(a) ftello64(a)
#endif
#endif
#endif

// Handle errors in libzpaq and elsewhere
void libzpaq::error(const char* msg) {
  fprintf(stderr, "zpaq error: %s\n", msg);
  exit(1);
}
using libzpaq::error;

// For libzpaq output to a string
struct StringWriter: public libzpaq::Writer {
  string s;
  void put(int c) {s+=char(c);}
};

// For libzpaq output to stdout
class Stdout: public libzpaq::Writer {
  void put(int c) {putchar(c);}
};

// Base class of InputFile and OutputFile (OS independent)
class File {
protected:
  enum {BUFSIZE=1<<16};  // buffer size
  int ptr;  // next byte to read or write in buf
  libzpaq::Array<char> buf;  // I/O buffer
  File(): ptr(0), buf(BUFSIZE) {}
};

class InputFile: public File, public libzpaq::Reader {
  FILE* in;
  int n;  // number of bytes in buf
public:
  InputFile(): in(0), n(0) {}

  // Open file for reading. Return true if successful
  bool open(const char* filename) {
    in=fopen(filename, "rb");
    if (!in) perror(filename);
    n=ptr=0;
    return in!=0;
  }

  // True if open
  bool isopen() {return in!=0;}

  // Read and return 1 byte (0..255) or EOF
  int get() {
    assert(in);
    if (ptr>=n) {
      assert(ptr==n);
      n=fread(&buf[0], 1, BUFSIZE, in);
      ptr=0;
      if (!n) return EOF;
    }
    assert(ptr<n);
    return buf[ptr++]&255;
  }

  // Return file position
  int64_t tell() {
    return ftello(in)-n+ptr;
  }

  // Set file position
  void seek(int64_t pos, int whence) {
    if (whence==SEEK_CUR) {
      whence=SEEK_SET;
      pos+=tell();
    }
    fseeko(in, pos, whence);
    n=ptr=0;
  }

  // Close file if open
  void close() {if (in) fclose(in), in=0;}
  ~InputFile() {close();}
};

class OutputFile: public File, public libzpaq::Writer {
  FILE* out;
public:
  OutputFile(): out(0) {}

  // Return true if file is open
  bool isopen() {return out!=0;}

  // Open for append/update or create if needed.
  bool open(const char* filename, const char* mode) {
    assert(!isopen());
    ptr=0;
    out=fopen(filename, mode);
    if (!out) perror(filename);
    return isopen();
  }

  // Flush pending output
  void flush() {
    if (ptr) {
      assert(isopen());
      assert(ptr>0 && ptr<=BUFSIZE);
      int n=fwrite(&buf[0], 1, ptr, out);
      if (n!=ptr) error("write failed");
      ptr=0;
    }
  }

  // Write 1 byte
  void put(int c) {
    assert(isopen());
    if (ptr>=BUFSIZE) {
      assert(ptr==BUFSIZE);
      flush();
    }
    assert(ptr>=0 && ptr<BUFSIZE);
    buf[ptr++]=c;
  }

  // Write bufp[0..size-1]
  void write(const char* bufp, int size);

  // Seek to pos. whence is SEEK_SET, SEEK_CUR, or SEEK_END
  void seek(int64_t pos, int whence) {
    assert(isopen());
    flush();
    fseeko(out, pos, whence);
  }

  // return position
  int64_t tell() {
    assert(isopen());
    return ftello(out)+ptr;
  }

  // Close file
  void close() {
    if (out) {
      flush();
      fclose(out);
    }
  }
  ~OutputFile() {close();}
};

// Write bufp[0..size-1]
void OutputFile::write(const char* bufp, int size) {
  if (ptr==BUFSIZE) flush();
  while (size>0) {
    assert(ptr>=0 && ptr<BUFSIZE);
    int n=BUFSIZE-ptr;  // number of bytes to copy to buf
    if (n>size) n=size;
    memcpy(&buf[ptr], bufp, n);
    size-=n;
    bufp+=n;
    ptr+=n;
    if (ptr==BUFSIZE) flush();
  }
}

// Read a file into a string
string getFile(const char* filename) {
  FILE* in=fopen(filename, "rb");
  if (!in) perror(filename), exit(1);
  string s;
  int c;
  while ((c=getc(in))!=EOF) s+=char(c);
  return s;
}

// Read 4 byte little-endian int and advance s
int btoi(const char* &s) {
  s+=4;
  return (s[-4]&255)|((s[-3]&255)<<8)|((s[-2]&255)<<16)|((s[-1]&255)<<24);
}

// Read 8 byte little-endian int and advance s
int64_t btol(const char* &s) {
  int64_t r=unsigned(btoi(s));
  return r+(int64_t(btoi(s))<<32);
}

// Display hcomp or pcomp section as ZPAQL source code
void decompile_comp(string s) {
  assert(s.size()<65536);
  s+=char(0);
  s+=char(0);

  // Get a list of jump targets to print labels
  libzpaq::Array<unsigned char> a(1<<16);
  for (unsigned i=0; i+2<s.size(); ++i) {
    if (s[i]==39 || s[i]==47 || s[i]==63)  // JT, JF, JMP
      a[(i+2+s[i+1])&0xffff]=true;
    if ((s[i]&255)==255)  // LJ
      a[(s[i+1]&255)|((s[i+2]<<8)&0xff00)]=true, i+=2;
    else if ((s[i]&7)==7)  // 2 byte opcode
      ++i;
  }

  // Print ZPAQL source
  for (unsigned i=0, j=0; i+2<s.size(); ++i) {
    int c=s[i]&255;
    if (a[i]) {  // print jump label as comment
      if (j) printf("\n"), j=0;
      printf(" (%d)", i);
    }
    printf(" %s", libzpaq::opcodelist[c]);
    if (c==255)  // LJ
      printf(" %d", (s[i+1]&255)|((s[i+2]<<8)&0xff00)), i+=2;
    else if (c==39 || c==47 || c==63)  // JT, JF, JMP
      printf(" %d (to %d)", s[i+1], i+2+s[i+1]), ++i;
    else if (c%8==7)  // 2 byte opcode
      printf(" %d", s[++i]&255);
    if (++j>8 || i+3>=s.size())
      printf("\n"), j=0;
  }
}

// Display hcomp and pcomp as ZPAQL source code
void decompile(const string& hcomp, const string& pcomp) {
  if (hcomp.size()>6) {
    int n=hcomp[6]&255;  // number of components
    printf("\ncomp %d %d %d %d %d\n",
      hcomp[2]&255, hcomp[3]&255, hcomp[4]&255, hcomp[5]&255, hcomp[6]&255);
    int j=7;
    for (int i=0; i<n && j<int(hcomp.size()); ++i) {
      const int c=hcomp[j]&255;
      printf("  %d %s", i, libzpaq::compname[c]);
      for (int k=j+1; k<j+libzpaq::compsize[c] && k<int(hcomp.size()); ++k)
        printf(" %d", hcomp[k]&255);
      printf("\n");
      j+=libzpaq::compsize[c];
    }
    printf("hcomp\n");
    if (j<int(hcomp.size())-2)
      decompile_comp(hcomp.substr(j+1, hcomp.size()-j-2));
  }
  if (pcomp.size()>=3) {
    printf("pcomp ;\n");
    decompile_comp(pcomp.substr(2, pcomp.size()-3));
    printf("end\n");
  }
  else
    printf("post 0 end\n");
}

// List contents
void list(const char* archive) {

  InputFile in;
  in.open(archive);
  if (!in.isopen()) perror(archive), exit(1);
  libzpaq::Decompresser d;
  d.setInput(&in);
  double mem;
  StringWriter filename, comment, buf;
  char sha1result[21];
  map<string, int> m;
  int block=0;
  int64_t offset=0;
  while (d.findBlock(&mem)) {
    printf("Block %d at %1.0f: %1.3f MB", ++block,
           double(offset), mem/1000000.0);
    bool first=true;
    while (d.findFilename(&filename)) {
      d.readComment(&comment);
      if (first) {  // Print ZPAQL in header
        StringWriter hcomp, pcomp;
        d.hcomp(&hcomp);
        d.setOutput(0);
        d.decompress(0);
        d.pcomp(&pcomp);
        int& b=m[hcomp.s+pcomp.s];
        if (b==0)
          decompile(hcomp.s, pcomp.s), b=block;
        else
          printf(" (same model as block %d)\n", b);
        first=false;

         // Decompress JIDAC index
        if (comment.s.size()>=5
            && comment.s.substr(comment.s.size()-5)==" jDC\x01"
            && filename.s.size()==28
            && filename.s.substr(0, 3)=="jDC"
            && strchr("chi", filename.s[17])) {
          d.setOutput(&buf);
          d.decompress();
        }
      }
      d.readSegmentEnd(sha1result);
      printf("  ");
      for (int i=0; i<4; ++i) {
        if (sha1result[0]) printf("%02x", sha1result[i+1]&255);
        else printf("  ");
      }
      printf(" %s %s -> %1.0f\n", filename.s.c_str(),
             comment.s.c_str(), double(in.tell()-offset));
      offset=in.tell();

      // Display JIDAC index blocks
      if (buf.s.size()) {
        assert(filename.s.size()==28);
        const char* p=buf.s.c_str();
        const char* end=p+buf.s.size();
        if (filename.s[17]=='c')  // header
          printf("  csize = %1.0f\n", double(btol(p)));
        else if (filename.s[17]=='h') {  // fragment table
          printf("  bsize = %d\n", btoi(p));
          int n=atoi(filename.s.c_str()+18);  // frag ID
          while (p<=end-24) {
            printf("%10d ", n++);
            for (int i=0; i<20; ++i)
              printf("%02x", *p++&255);  // sha1 hash
            printf(" %10d\n", btoi(p));  // fsize
          }
        }
        else if (filename.s[17]=='i') {  // index
          while (p<end-8) {
            const int64_t fdate=btol(p);
            printf("  %14.0f %s", double(fdate), p);
            while (p<end && *p) ++p;  // skip filename
            ++p;
            if (fdate) {
              putchar(' ');
              if (p>end-4) break;
              int n=btoi(p);  // na
              while (n-->0 && p<end)
                printf("%02x", *p++&255); // attr
              if (p>end-4) break;
              n=btoi(p);  // ni
               // print fragment pointers
              vector<unsigned> ptr;
              for (; n>0 && p<=end-4; --n)
                ptr.push_back(btoi(p));
              bool hyphen=false;
              for (int i=0; i<int(ptr.size()); ++i) {
                if (i==0 || i==int(ptr.size())-1 || ptr[i]!=ptr[i-1]+1
                 || ptr[i]!=ptr[i+1]-1) {
                  if (!hyphen) printf(" ");
                  hyphen=false;
                  printf("%d", ptr[i]);
                }
                else {
                  if (!hyphen) printf("-");
                  hyphen=true;
                }
              }
            }
            printf("\n");
          }
        }
      }
      buf.s="";
      filename.s="";
      comment.s="";
    }
    offset=in.tell();
    printf("\n");
  }
  in.close();
  return;
}

// trace: Execute ZPAQL input and show virtual register contents after
// each instruction. After HALT, dump memory.
namespace libzpaq {
int ZPAQL::step(U32 input, int ishex) {
  assert(cend>6);
  assert(hbegin>=cend+128);
  assert(hend>=hbegin);
  assert(hend<header.isize()-130);
  assert(m.size()>0);
  assert(h.size()>0);
  pc=hbegin;
  a=input;
  printf("\n"
  "  pc   opcode  f      a          b      *b      c      *c      d         *d\n"
  "----- -------- - ---------- ---------- --- ---------- --- ---------- ----------\n");
  printf(ishex ?
    "               %d   %08X   %08X  %02X   %08X  %02X   %08X   %08X\n" :
    "               %d %10u %10u %3u %10u %3u %10u %10u\n",
    f, a, b, m(b), c, m(c), d, h(d));
  while (1) {
    assert(pc>=cend && pc<header.isize());
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
    printf(ishex ?
      " %d   %08X   %08X  %02X   %08X  %02X   %08X   %08X\n" :
      " %d %10u %10u %3u %10u %3u %10u %10u\n",
      f, a, b, m(b), c, m(c), d, h(d));
  }

  // Print R, skipping rows of 4 zeros
  printf("\n\nR (size %1.0f) = (rows of all 0 omitted)\n", double(r.size()));
  for (int i=0; i<r.isize(); i+=4) {
    if (r(i) || r(i+1) || r(i+2) || r(i+3))
      printf(ishex ? "%8X: %08X %08X %08X %08X\n"
                   : "%10u: %10u %10u %10u %10u\n",
        i, r(i), r(i+1), r(i+2), r(i+3));
  }

  // Print H, skipping rows of 4 zeros
  printf("\nH (size %1.0f) = (rows of all 0 omitted)\n", double(h.size()));
  for (int i=0; i<h.isize(); i+=4) {
    if (h(i) || h(i+1) || h(i+2) || h(i+3))
      printf(ishex ? "%8X: %08X %08X %08X %08X\n"
                   : "%10u: %10u %10u %10u %10u\n",
        i, h(i), h(i+1), h(i+2), h(i+3));
  }

  // Print M, skipping rows of 16 zeros
  printf("\nM (size %1.0f) = (rows of all 0 omitted)\n", double(m.size()));
  for (int i=0; i<m.isize(); i+=16) {
    bool found=false;
    for (int j=0; j<16; ++j)
      if (m(i+j)) found=true;
    if (found) {
      printf(ishex ? "%8X:" : "%10u:", i);
      for (int j=0; j<16; ++j) {
        printf(ishex ? " %02X" : " %3d", m(i+j));
        if (j%4==3) printf(" ");
      }
      printf("\n");
    }
  }
  printf("\n\n");
  return 0;
}

// show compression component statistics
int Predictor::stat(int id) {
  printf("Memory utilization:\n");
  int cp=7;
  for (int i=0; i<z.header[6]; ++i) {
    assert(cp<z.header.isize());
    int type=z.header[cp];
    assert(compsize[type]>0);
    printf("%2d %s", i, compname[type]);
    for (int j=1; j<compsize[type]; ++j)
      printf(" %d", z.header[cp+j]);
    Component& cr=comp[i];
    if (type==MATCH) {
      size_t count=0;
      for (size_t j=0; j<cr.cm.size(); ++j)
        if (cr.cm[j]) ++count;
      printf(": buffer=%1.0f/%1.0f index=%1.0f/%1.0f (%1.2f%%)",
        cr.limit/8.0, double(cr.ht.size()), double(count), double(cr.cm.size()),
        count*100.0/cr.cm.size());
    }
    else if (type==SSE) {
      size_t count=0;
      for (size_t j=0; j<cr.cm.size(); ++j) {
        if (int(cr.cm[j])!=(squash((j&31)*64-992)<<17|z.header[cp+3]))
          ++count;
      }
      printf(": %1.0f/%1.0f (%1.2f%%)", double(count),
        double(cr.cm.size()), count*100.0/cr.cm.size());
    }
    else if (type==CM) {
      size_t count=0;
      for (size_t j=0; j<cr.cm.size(); ++j)
        if (cr.cm[j]!=0x80000000) ++count;
      printf(": %1.0f/%1.0f (%1.2f%%)", double(count),
        double(cr.cm.size()), count*100.0/cr.cm.size());
    }
    else if (type==MIX) {
      size_t count=0;
      int m=z.header[cp+3];
      assert(m>0);
      for (size_t j=0; j<cr.cm.size(); ++j)
        if (int(cr.cm[j])!=65536/m) ++count;
      printf(": %1.0f/%1.0f (%1.2f%%)", double(count),
        double(cr.cm.size()), count*100.0/cr.cm.size());
    }
    else if (type==MIX2) {
      size_t count=0;
      for (size_t j=0; j<cr.a16.size(); ++j)
        if (int(cr.a16[j])!=32768) ++count;
      printf(": %1.0f/%1.0f (%1.2f%%)", double(count),
        double(cr.a16.size()), count*100.0/cr.a16.size());
    }
    else if (cr.ht.size()>0) {
      double hcount=0;
      for (size_t j=0; j<cr.ht.size(); ++j)
        if (cr.ht[j]>0) ++hcount;
      printf(": %1.0f/%1.0f (%1.2f%%)",
          double(hcount), double(cr.ht.size()), hcount*100.0/cr.ht.size());
    }
    cp+=compsize[type];
    printf("\n");
  }
  printf("\n");
  return 0;
}
}  // end namespace libzpaq

//////////////////////////// main /////////////////////////

// Print help message
void usage() {
  printf(
    "zpaqd v6.24 ZPAQ development tool, " __DATE__ "\n"
    "To compress: zpaqd {a|c}[i|n|s|t]... config [arg]... archive files...\n"
    "  a - append to existing archive.zpaq\n"
    "  c - create new archive.zpaq\n"
    "  i - don't save file sizes in comments\n"
    "  n - don't save file names\n"
    "  s - don't save SHA-1 checksums or test post-processor\n"
    "  t - don't save header locator tag\n"
    "  config = 1..3 (compress faster..better)\n"
    "      or ZPAQL file config.cfg with args $1...$9 - see libzpaq.h\n"
    "To decompress:   zpaqd d[s] archive [output [block [blocks [segments]]]]\n"
    "  s - don't verify SHA-1 checksums\n"
    "To list:         zpaqd l archive\n"
    "To run:          zpaqd r config [arg]... {h|p} [input [output]]\n"
    "To trace:        zpaqd t config [arg]... {h|p} [N|xN]...\n"
    "To compute SHA1: zpaqd s files...\n"
    "See http://mattmahoney.net/zpaq/ for latest version\n");
  exit(1);
}

// Convert decimal or hex string to int
int ntoi(const char* s) {
  int n=0, base=10, sign=1;
  for (; *s; ++s) {
    int c=*s;
    if (isupper(c)) c=tolower(c);
    if (!n && c=='x') base=16;
    else if (!n && c=='-') sign=-1;
    else if (c>='0' && c<='9') n=n*base+c-'0';
    else if (base==16 && c>='a' && c<='f') n=n*base+c-'a'+10;
    else break;
  }
  return n*sign;
}

int main(int argc, char** argv) {

  // Get start time
  clock_t start=clock();

  // Read command line
  if (argc<3) usage();
  char cmd=argv[1][0];
  if (strchr("acldrts", cmd)==0) usage();

  // Read config file and args
  string method;  // config file contents
  int args[9]={0};
  int i=2;
  if (strchr("acrt", cmd)) {
    string config=argv[2];
    if (config=="1" || config=="2" || config=="3")
      method=config;
    else {
      if (config.size()<4 || config.substr(config.size()-4)!=".cfg")
        config+=".cfg";
      FILE* in=fopen(config.c_str(), "rb");  // read config file
      if (!in) perror(config.c_str()), exit(1);
      int c;
      while ((c=getc(in))!=EOF) method+=c;
      fclose(in);
    }
    for (i=3; i<argc && i<12 && (argv[i][0]=='-' || isdigit(argv[i][0])); ++i)
      args[i-3]=atoi(argv[i]);  // read args
  }

  // Get archive name and append .zpaq unless present
  string archive;  // archive name
  if (strchr("acdl", cmd)) {
    if (i>=argc) usage();
    archive=argv[i++];
    if (archive.size()<5 || archive.substr(archive.size()-5)!=".zpaq")
      archive+=".zpaq";
  }

  // Do command
  switch (cmd) {

    // List
    case 'l':
      list(archive.c_str());
      break;

    // Compress: argv[i]... = files...
    case 'c':
    case 'a':
    {
      const char* options=argv[1]+1;
      OutputFile out;
      StringWriter pcomp_cmd;
      int errors=0;
      out.open(archive.c_str(), cmd=='c' ? "wb" : "ab");
      if (!out.isopen()) perror(archive.c_str()), exit(1);
      out.seek(0, SEEK_END);
      int64_t offset=out.tell(), start=offset, totalSize=0;
      printf("Appending %s at %1.0f\n", archive.c_str(), double(offset));
      libzpaq::Compressor co;
      co.setOutput(&out);
      if (!strchr(options, 't')) co.writeTag();
      if (method=="1" || method=="2" || method=="3")
        co.startBlock(method[0]-'0');
      else
        co.startBlock(method.c_str(), args, &pcomp_cmd);
      co.setVerify(!strchr(options, 's'));

      // Compress each file
      for (; i<argc; ++i) {
        libzpaq::SHA1 sha1;
        InputFile in;
        in.open(argv[i]);
        if (!in.isopen()) continue;
        co.setInput(&in);

        // Get file size
        char comment[32]={0};
        in.seek(0, SEEK_END);
        int64_t size=in.tell();
        totalSize+=size;
        in.seek(0, SEEK_SET);
        if (size>=0) sprintf(comment, "%1.0f", double(size));
        co.startSegment(!strchr(options, 'n') ? argv[i] : 0,
                        !strchr(options, 'i') ? comment : 0);

        // Get checksum
        if (!strchr(options,'s')) {
          for (int c; (c=in.get())!=EOF;) sha1.put(c);
          in.seek(0, SEEK_SET);
        }

        // Preprocess
        string tmpfile="";
        if (pcomp_cmd.s!="") {
          in.close();
          tmpfile="zpaq.tmp";
          string syscmd=pcomp_cmd.s+" \""+argv[i]+"\" "+tmpfile;
          printf("%s\n", syscmd.c_str());
          system(syscmd.c_str());
          in.open(tmpfile.c_str());
          if (!in.isopen()) perror(tmpfile.c_str()), exit(1);
        }

        // Compress
        while (co.compress(100000)) {
          printf("%s %1.0f -> %1.0f \r", argv[i], double(in.tell()),
                 double(out.tell()-offset));
          fflush(stdout);
        }

        // End segment and verify checksums
        int64_t size2=-1;
        const char* sha1result2=co.endSegmentChecksum(&size2);
        printf("%s %1.0f -> %1.0f\n", argv[i], double(in.tell()),
               double(out.tell()-offset));
        in.close();
        offset=out.tell();
        if (!strchr(options, 's')
            && memcmp(sha1.result(), sha1result2, 20)) {
          printf("WARNING: %s: post-processor mismatch: %1.0f -> %1.0f\n",
                   argv[i], double(size), double(size2));
          ++errors;
        }
        if (tmpfile!="") remove(tmpfile.c_str());
      }
      co.endBlock();
      printf("%s %1.0f -> %1.0f (%d errors)\n", archive.c_str(),
             double(totalSize), double(out.tell()-start), errors);
      co.stat(0);
      break;
    }

    // Run or trace
    case 'r':  // argv[i]... = {h|p} [N|xN]...
    case 't':  // argv[i]... = {h|p} [input [output]]
    {

      // Compile config.cfg, args[] to hz, pz, pcomp_cmd
      libzpaq::ZPAQL hz, pz, *z;
      StringWriter pcomp_cmd;
      libzpaq::Compiler(method.c_str(), args, hz, pz, &pcomp_cmd);

      // Initialize either hz or pz to execute instructions
      if (argv[i][0]=='h') {  // hcomp
        z=&hz;
        z->inith();
      }
      else if (argv[i][0]=='p') {  // pcomp
        if (pz.hend<=pz.hbegin) error("no PCOMP section");
        z=&pz;
        z->initp();
      }
      else
        usage();

      // Trace from decimal or hex command line arguments
      if (argv[1][0]=='t') {
        for (++i; i<argc; ++i)
          z->step(ntoi(argv[i]), tolower(argv[i][0])=='x');
      }

      // Run from input to output
      else {

        // open input and output (default stdin, stdout)
        FILE* in=stdin;
        OutputFile out;
        Stdout outc;
        if (i+1<argc) {
          in=fopen(argv[i+1], "rb");
          if (!in) perror(argv[i+1]), exit(1);
        }
        if (i+2<argc) {
          out.open(argv[i+2], "wb");
          if (!out.isopen()) perror(argv[i+2]), exit(1);
          z->output=&out;
        }
        else
          z->output=&outc;

        // run once per input byte, plus EOF if pcomp
        int c;
        while ((c=getc(in))!=EOF) z->run(c);
        if (argv[i][0]=='p') z->run(-1);
        z->flush();
        out.close();
        if (in!=stdin) fclose(in);
      }
      break;
    }

    // Compute file hashes
    case 's':
    {
      libzpaq::SHA1 sha1;
      for (i=2; i<argc; ++i) {
        FILE* in=fopen(argv[i], "rb");
        if (!in) {
          perror(argv[i]);
          continue;
        }
        unsigned char buf[4096];
        for (int n; (n=fread(buf, 1, 4096, in))>0;)
          for (int j=0; j<n; ++j) sha1.put(buf[j]);
        int64_t size=sha1.usize();
        const char* s=sha1.result();
        fclose(in);
        for (int j=0; j<20; ++j) printf("%02x", s[j]&255);
        printf(" %12.0f %s\n", double(size), argv[i]);
      }
      break;
    }

    // Decompress: d archive [output [firstblock [blocks [segments]]]]
    case 'd':
    {
      int firstblock=1;
      if (argc>4) firstblock=atoi(argv[4]);
      int blocks=-firstblock;
      if (argc>5) blocks=atoi(argv[5]);
      int segments=0x7fffffff;
      if (argc>6) segments=atoi(argv[6]);
      InputFile in;
      in.open(archive.c_str());
      if (!in.isopen()) perror(archive.c_str()), exit(1);
      libzpaq::Decompresser de;
      de.setInput(&in);
      OutputFile out;
      if (argc>3) {
        out.open(argv[3], "wb");
        if (!out.isopen()) perror(argv[3]), exit(1);
        de.setOutput(&out);
      }
      libzpaq::SHA1 sha1;
      if (argv[1][1]!='s')
        de.setSHA1(&sha1);
      StringWriter filename, comment;
      double memory=0;
      int64_t offset=0;
      int errors=0;
      for (int i=1; segments && i!=firstblock+blocks && de.findBlock(&memory);
           ++i) {
        printf("Block %d (%1.3f MB) at %1.0f\n",
               i, memory*0.000001, double(offset));
        while (segments && de.findFilename(&filename)) {
          de.readComment(&comment);
          if (i<firstblock) {
            printf("  Skipping %s %s\n", filename.s.c_str(),
                   comment.s.c_str());
            de.readSegmentEnd();
          }
          else {
            --segments;
            double inStart=in.tell();
            do {
              printf("%s %s %1.0f -> %1.0f \r", filename.s.c_str(),
                     comment.s.c_str(), in.tell()-inStart, sha1.size());
              fflush(stdout);
            } while (de.decompress(100000));
            double size=sha1.size();
            char sha1result[20], check[21];
            memcpy(sha1result, sha1.result(), 20);
            de.readSegmentEnd(check);
            printf("  ");
            if (argv[1][1]!='s')
              for (int j=0; j<4; ++j)
                printf("%02x", sha1result[j]&255);
            printf(" ");
            if (check[0]) {
              for (int j=0; j<4; ++j)
                printf("%02x", check[j+1]&255);
            }
            else
              printf("        ");
            printf(" %s %s %1.0f -> %1.0f", filename.s.c_str(),
                   comment.s.c_str(), in.tell()-inStart, size);
            if (check[0] && argv[1][1]!='s') {
              if (memcmp(check+1, sha1result, 20)) {
                printf(" VERIFY ERROR!\n");
                ++errors;
              }
              else
                printf(" OK\n");
            }
            else
              printf(" Not verified\n");
          }
          filename.s="";
          comment.s="";
        }
        offset=in.tell();
      }
      printf("%d errors in %1.0f bytes of %s\n", errors,
             double(offset), archive.c_str());
      break;
    }

    // Unknown command
    default:
      usage();
  }
  printf("%1.2f seconds\n", double(clock()-start)/CLOCKS_PER_SEC);
  return 0;
}
