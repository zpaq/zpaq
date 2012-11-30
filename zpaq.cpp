/* zpaq v5.00 - ZPAQ compressor and development tool

  Copyright (C) 2012, Dell Inc. Written by Matt Mahoney.

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

zpaq is a tool for developing compression algorithms in the ZPAQ format
described in the level 2 specification at http://mattmahoney.net/zpaq/
Commands:

  zpaq c|a config[,N]... archive [files...]  Compress/append archive.zpaq
  zpaq d|e|x archive [out]                   Extract to out+none/file/path
  zpaq l archive                             List archive.zpaq contents
  zpaq r[h|p] config[,N]... [in [out]]       Run hcomp/pcomp
  zpaq t[h|p] config[,N]... [N|xN]...        Trace with decimal/hex args

Commands c and a compress 1 or more files to archive.zpaq in a single
block, saving filenames as specified on the command line, file sizes as
decimal comments, and SHA-1 checksums. Command "c" creates a new
archive or overwrites an existing archive without adding a 13 byte
header locator tag. Command "a" appends with a header locator tag (which
allows the block to be found even if preceded by non-ZPAQ data). An
extension .zpaq is added to archive unless already specified.

config may be 0 which stores the input uncompressed, or 1, 2, or 3
to store with fast, mid, or max compression. Otherwise, a .cfg
filename extension is added (even if present). config.cfg should contain
a compression algorithm description in the ZPAQL language as described
in libzpaq.h. If numeric arguments are passed to it, then they should
be appended to the name preceded by commas without spaces. Up to 9
arguments may be passed. (Levels 2 and 3 can also take a positive argument
to increase memory usage or negative to decrease, e.g. "3,-1").

If config.cfg has a PCOMP section then each input file is preprocessed
by the external program specified after the PCOMP keyword. It should
take the input and output file as its last 2 command line arguments.
These will be named file and file.zpaqtmp respectively. file.zpaqtmp
will be input to the postprocessor during compression and the postprocessor
output SHA-1 hash will be compared to the input and a warning printed
in case of a mismatch (meaning decompression would be incorrect).
file.zpaqtmp is then deleted.

During compression, input and output size for each file is displayed
with updates every 100 KB. After completion, run time and memory usage
by component is displayed.

Commands d, e, and x extract archive.zpaq but treat filenames
stored in the archive differently. d ignores stored names and
concatenates everything into the specified output file "out".
(This is convenient for archives containing only 1 file).
e appends the saved name after the last / or \ to the optional "out".
x appends the entire saved name to out. Out may be omitted for
e or x, in which case e extracts to the current directory and x extracts
to the original directories. Output directories must already exist.
Existing files are overwritten without warning. File attributes such
as timestamps and permissions are not preserved.

Command l lists the contents of archive.zpaq. If the .zpaq extension
is omitted, then it is assumed as usual. The contents display for each block
the block number, the memory required to decompress, and either the
stored ZPAQL decompression code or an indication that it is identical
to an earlier block. The code is displayed in a format that can be
pasted into a config file for compression, although the preprocessor
command will be omitted because it is not saved. Also, structure
such as IF-THEN, DO-WHILE, etc. is lost and will be displayed as jump
instructions with labels shown as comments. Then for each segment,
the first 32 bits of the SHA-1 checksum (in hex) if present, filename,
comment (normally the uncompressed size), and compressed size will be shown.
If the filename is blank then it is a continuation of the previous file.

Commands rh and rp run the HCOMP or PCOMP section of config,
respectively. The section is executed once for each byte of input
and for PCOMP, once more with EOF. input and output default to
stdin and stdout.

Commands th and tp trace the HCOMP or PCOMP section of config,
respectively. Arguments may be given in decimal or hex with a
leading x, and the display will be in the same base. The display
shows each instruction and ZPAQL registers as it is executed, and
dumps nonzero memory after a HALT instruction.

To compile: g++ -O3 -msse2 zpaq.cpp libzpaq.cpp -o zpaq

*/

#include "libzpaq.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <assert.h>
#include <stdint.h>
#include <string>
#include <map>
using std::string;
using std::map;

// libzpaq error handler
void libzpaq::error(const char* msg) {
  fprintf(stderr, "zpaq: %s\n", msg);
  exit(1);
}

// Convert non-negative decimal number x to string of at least n digits
string itos(int64_t x, int n=1) {
  assert(x>=0);
  assert(n>=0);
  string r;
  for (; x || n>0; x/=10, --n) r=char('0'+x%10)+r;
  return r;
}

// For libzpaq file I/O
struct File: public libzpaq::Reader, public libzpaq::Writer {
  FILE* f;
  int64_t offset;
  File(): f(0), offset(0) {}
  int get() {++offset; return getc(f);}
  void put(int c) {++offset; putc(c, f);}
  int read(char* buf, int n) {n=fread(buf, 1, n, f); offset+=n; return n;}
  void write(const char* buf, int n) {offset+=n; fwrite(buf, 1, n, f);}
};

// For libzpaq output to a string
struct StringWriter: public libzpaq::Writer {
  string s;
  void put(int c) {s+=char(c);}
};

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

  // For commands a,c,t,r read config[,N]... into config and args[9]
  string config;  // file name for now
  int args[9]={0};
  if (argc>2 && strchr("actr", argv[1][0])) {
    config=argv[2];
    for (int i=0, j=0; argv[2][i] && j<9; ++i) {  // read and chop args
      if (argv[2][i]==',') {
        if (j==0) config=config.substr(0, i);
        args[j++]=ntoi(argv[2]+i+1);
      }
    }
  }

  // If config is 0,1,2,3 then set to built-in store, fast, mid, max.
  if (config=="0") config=  // store
    "comp 0 0 0 0 0 hcomp post 0 end ";

  // fast.cfg
  else if (config=="1") config=
    "comp 1 2 0 0 2 (hh hm ph pm n)\n"
    "  0 icm 16    (order 2)\n"
    "  1 isse 19 0 (order 4)\n"
    "hcomp\n"
    "  *b=a a=0 (save in rotating buffer M)\n"
    "  d=0 hash b-- hash *d=a\n"
    "  d++ b-- hash b-- hash *d=a\n"
    "  halt\n"
    "post\n"
    "  0\n"
    "end\n";

  // mid.cfg
  else if (config=="2") config=
    "comp 3 3 0 0 8 (hh hm ph pm n)\n"
    "  0 icm 5        (order 0...5 chain)\n"
    "  1 isse 13 0\n"
    "  2 isse $1+17 1\n"
    "  3 isse $1+18 2\n"
    "  4 isse $1+18 3\n"
    "  5 isse $1+19 4\n"
    "  6 match $1+22 $1+24  (order 7)\n"
    "  7 mix 16 0 7 24 255  (order 1)\n"
    "hcomp\n"
    "  c++ *c=a b=c a=0 (save in rotating buffer M)\n"
    "  d= 1 hash *d=a   (orders 1...5 for isse)\n"
    "  b-- d++ hash *d=a\n"
    "  b-- d++ hash *d=a\n"
    "  b-- d++ hash *d=a\n"
    "  b-- d++ hash *d=a\n"
    "  b-- d++ hash b-- hash *d=a (order 7 for match)\n"
    "  d++ a=*c a<<= 8 *d=a       (order 1 for mix)\n"
    "  halt\n"
    "post\n"
    "  0\n"
    "end\n";

  // max.cfg
  else if (config=="3") config=
    "comp 5 9 0 0 22 (hh hm ph pm n)\n"
    "  0 const 160\n"
    "  1 icm 5  (orders 0-6)\n"
    "  2 isse 13 1 (sizebits j)\n"
    "  3 isse $1+16 2\n"
    "  4 isse $1+18 3\n"
    "  5 isse $1+19 4\n"
    "  6 isse $1+19 5\n"
    "  7 isse $1+20 6\n"
    "  8 match $1+22 $1+24\n"
    "  9 icm $1+17 (order 0 word)\n"
    "  10 isse $1+19 9 (order 1 word)\n"
    "  11 icm 13 (sparse with gaps 1-3)\n"
    "  12 icm 13\n"
    "  13 icm 13\n"
    "  14 icm 14 (pic)\n"
    "  15 mix 16 0 15 24 255 (mix orders 1 and 0)\n"
    "  16 mix 8 0 16 10 255 (including last mixer)\n"
    "  17 mix2 0 15 16 24 0\n"
    "  18 sse 8 17 32 255 (order 0)\n"
    "  19 mix2 8 17 18 16 255\n"
    "  20 sse 16 19 32 255 (order 1)\n"
    "  21 mix2 0 19 20 16 0\n"
    "hcomp\n"
    "  c++ *c=a b=c a=0 (save in rotating buffer)\n"
    "  d= 2 hash *d=a b-- (orders 1,2,3,4,5,7)\n"
    "  d++ hash *d=a b--\n"
    "  d++ hash *d=a b--\n"
    "  d++ hash *d=a b--\n"
    "  d++ hash *d=a b--\n"
    "  d++ hash b-- hash *d=a b--\n"
    "  d++ hash *d=a b-- (match, order 8)\n"
    "  d++ a=*c a&~ 32 (lowercase words)\n"
    "  a> 64 if\n"
    "    a< 91 if (if a-z)\n"
    "      d++ hashd d-- (update order 1 word hash)\n"
    "      *d<>a a+=*d a*= 20 *d=a (order 0 word hash)\n"
    "      jmp 9\n"
    "    endif\n"
    "  endif\n"
    "  (else not a letter)\n"
    "    a=*d a== 0 ifnot (move word order 0 to 1)\n"
    "      d++ *d=a d--\n"
    "    endif\n"
    "    *d=0  (clear order 0 word hash)\n"
    "  (end else)\n"
    "  d++\n"
    "  d++ b=c b-- a=0 hash *d=a (sparse 2)\n"
    "  d++ b-- a=0 hash *d=a (sparse 3)\n"
    "  d++ b-- a=0 hash *d=a (sparse 4)\n"
    "  d++ a=b a-= 212 b=a a=0 hash\n"
    "    *d=a b<>a a-= 216 b<>a a=*b a&= 60 hashd (pic)\n"
    "  d++ a=*c a<<= 9 *d=a (mix)\n"
    "  d++\n"
    "  d++\n"
    "  d++ d++\n"
    "  d++ *d=a (sse)\n"
    "  halt\n"
    "post\n"
    "  0\n"
    "end\n";

  // Read config file contents into config
  else if (config!="") {
    config+=".cfg";
    FILE* in=fopen(config.c_str(), "r");
    if (!in) perror(config.c_str()), exit(1);
    config=""; // file contents
    for (int c; (c=getc(in))!=EOF; config+=char(c));
    fclose(in);
  }

  // For commands a,c,d,e,x,l append .zpaq to archive
  string archive;
  if (argc>3 && strchr("ac", argv[1][0])) archive=argv[3];
  if (argc>2 && strchr("dexl", argv[1][0])) archive=argv[2];
  if (archive!="")
    if (archive.size()<5 || archive.substr(archive.size()-5)!=".zpaq")
      archive+=".zpaq";

  // Compress: a|c config archive files...
  const string cmd=argc>1 ? argv[1] : "";
  if (argc>4 && (cmd=="a" || cmd=="c")) {
    clock_t start=clock();

    // Open archive
    File out;
    if (cmd=="c") out.f=fopen(archive.c_str(), "wb");
    else out.f=fopen(archive.c_str(), "ab");
    if (!out.f) perror(archive.c_str()), exit(1);

    // Start block
    libzpaq::Compressor co;
    StringWriter pcomp_cmd;
    co.setOutput(&out);
    if (cmd=="a") co.writeTag();
    co.startBlock(config.c_str(), args, &pcomp_cmd);
    co.setVerify(pcomp_cmd.s.size()>0);
    int64_t offset=0;

    // Compress files
    for (int i=4; i<argc; ++i) {
      File in;
      in.f=fopen(argv[i], "rb");
      if (!in.f) {
        perror(argv[i]);
        continue;
      }

      // Get input size and checksum
      libzpaq::SHA1 sha1;
      for (int c; (c=getc(in.f))!=EOF; sha1.put(c));
      int64_t sz=sha1.usize();  // input size
      char sha1result[20];
      memcpy(sha1result, sha1.result(), 20);  // checksum
      rewind(in.f);

      // Preprocess
      string pre;
      if (pcomp_cmd.s!="") {
        fclose(in.f);
        pre=argv[i];
        pre+=".zpaqtmp";
        string pcmd=pcomp_cmd.s+" "+argv[i]+" "+pre;
        printf("%s\n", pcmd.c_str());
        system(pcmd.c_str());
        in.f=fopen(pre.c_str(), "rb");
        if (!in.f) {
          perror(pre.c_str());
          continue;
        }
      }

      // Compress
      in.offset=0;
      co.startSegment(argv[i], itos(sz).c_str());
      co.setInput(&in);
      while (co.compress(100000)) {
        printf("%s %1.0f -> %1.0f\r", argv[i], double(in.offset),
               double(out.offset-offset));
        fflush(stdout);
      }
      fclose(in.f);
      co.endSegment(sha1result);
      printf("%s %1.0f -> %1.0f -> %1.0f  \n", argv[i], double(sz),
             double(in.offset), double(out.offset-offset));
      offset=out.offset;

      // Verify pre/post processor
      if (pcomp_cmd.s!="") {
        int64_t postsz=co.getSize();
        if (memcmp(sha1result, co.getChecksum(), 20)) {
          printf("WARNING: pre/post test failed: restored size = %1.0f\n",
                 double(postsz));
        }
        remove(pre.c_str());
      }
    }
    co.endBlock();
    co.stat(0);
    printf("%s -> %1.0f in %1.2f sec.\n", archive.c_str(),
           double(out.offset), double(clock()-start)/CLOCKS_PER_SEC);
  }

  // Decompress: d|e|x archive prefix
  else if (argc>2 && (cmd=="d" || cmd=="e" || cmd=="x")) {
    clock_t start=clock();
    File in, out;
    in.f=fopen(archive.c_str(), "rb");
    if (!in.f) perror(archive.c_str()), exit(1);
    libzpaq::Decompresser de;
    de.setInput(&in);
    StringWriter filename;
    bool first=true;  // first segment of archive?
    while (de.findBlock()) {
      while (de.findFilename(&filename)) {
        libzpaq::SHA1 sha1;
        de.setSHA1(&sha1);
        char sha1result[21];
        if (cmd=="d") filename.s="";
        if (cmd=="e") { // strip path
          for (int i=filename.s.size()-1; i>=0; --i) {
            if (filename.s[i]=='/' || filename.s[i]=='\\') {
              filename.s=filename.s.substr(i+1);
              break;
            }
          }
        }
        if (first || filename.s!="") {  // new output file?
          if (argc>3) filename.s=argv[3]+filename.s;
          printf("%s\n", filename.s.c_str());
          if (out.f) fclose(out.f), out.f=0, de.setOutput(0);
          out.f=fopen(filename.s.c_str(), "wb");
          if (out.f) de.setOutput(&out);
          else perror(filename.s.c_str());
          first=false;
        }
        de.readComment();
        de.decompress();
        de.readSegmentEnd(sha1result);  // Verify checksum
        if (sha1result[0] && memcmp(sha1result+1, sha1.result(), 20))
          printf("WARNING: checksum error\n");
        filename.s="";
      }
    }
    fclose(in.f);
    if (out.f) fclose(out.f);
    printf("%s -> %1.2f sec.\n", archive.c_str(),
           double(clock()-start)/CLOCKS_PER_SEC);
  }

  // List: l archive
  else if (argc>2 && cmd=="l") {
    File in;
    in.f=fopen(archive.c_str(), "rb");
    if (!in.f) perror(archive.c_str()), exit(1);
    libzpaq::Decompresser d;
    d.setInput(&in);
    double mem;
    StringWriter filename, comment;
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
          d.decompress(0);
          d.pcomp(&pcomp);
          int& b=m[hcomp.s+pcomp.s];
          if (b==0)
            decompile(hcomp.s, pcomp.s), b=block;
          else
            printf(" (same model as block %d)\n", b);
          first=false;
        }
        d.readSegmentEnd(sha1result);
        printf("  ");
        for (int i=0; i<4; ++i) {
          if (sha1result[0]) printf("%02x", sha1result[i+1]&255);
          else printf("  ");
        }
        printf(" %s %s -> %1.0f\n", filename.s.c_str(), comment.s.c_str(),
               double(in.offset-offset));
        offset=in.offset;
        filename.s="";
        comment.s="";
      }
      offset=in.offset;
      printf("\n");
    }
    fclose(in.f);
  }

  // Run:   r{h|p} config input output
  // Trace: t{h|p} config N...
  else if (argc>2 && (cmd=="rh" || cmd=="rp" || cmd=="th" || cmd=="tp")) {

    // Compile config.cfg, args[] to hz, pz, pcomp_cmd
    libzpaq::ZPAQL hz, pz, *z;
    StringWriter pcomp_cmd;
    libzpaq::Compiler(config.c_str(), args, hz, pz, &pcomp_cmd);

    // Initialize either hz or pz to execute instructions
    if (cmd[1]=='h') {  // hcomp
      z=&hz;
      z->inith();
    }
    else {  // pcomp
      if (pcomp_cmd.s.size()==0)
        fprintf(stderr, "No PCOMP section\n"), exit(1);
      z=&pz;
      z->initp();
    }

    // Trace from decimal or hex command line arguments
    if (cmd[0]=='t') {
      for (int i=3; i<argc; ++i)
        z->step(ntoi(argv[i]), tolower(argv[i][0])=='x');
    }

    // Run from input to output
    else {

      // open input and output (default stdin, stdout)
      FILE* in=stdin;
      File out;
      out.f=stdout;
      if (argc>3) {
        in=fopen(argv[3], "rb");
        if (!in) perror(argv[3]), exit(1);
      }
      if (argc>4) {
        out.f=fopen(argv[4], "wb");
        if (!out.f) perror(argv[4]), exit(1);
      }
      z->output=&out;

      // run once per input byte, plus EOF if pcomp
      int c;
      while ((c=getc(in))!=EOF) z->run(c);
      if (argv[1][1]=='p') z->run(-1);
      z->flush();
      if (out.f!=stdout) fclose(out.f);
      if (in!=stdin) fclose(in);
    }
  }    

  // Invalid command: print help message
  else printf(
  "zpaq v5.00 - ZPAQ compression development tool\n"
  "(C) 2012, Dell Inc. Written by Matt Mahoney\n"
  "License: GPL v3. http://www.gnu.org/copyleft/gpl.html\n"
  "\n"
  "Usage: zpaq command\n"
  "l archive                          List contents of archive.zpaq\n"
  "c|a config[,N]... archive files... Compress/append level 0..3 or config.cfg\n"
  "d|e|x archive [out]                Extract to out+none/saved file/path\n"
  "r[h|p] config[,N]... [in [out]]    Run HCOMP/PCOMP\n"
  "t[h|p] config[,N]... [N|xN]...     Trace HCOMP/PCOMP\n"
  "See zpaq.cpp for details and libzpaq.h for config file syntax.\n");
  return 0;
}

// trace command: Execute ZPAQL input and show virtual register contents
// after each instruction. After HALT, dump memory.
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

// Show compression component statistics
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
      assert(cr.cm.size()>0);
      assert(cr.ht.size()>0);
      size_t count=0;
      for (size_t j=0; j<cr.cm.size(); ++j)
        if (cr.cm[j]) ++count;
      printf(": buffer=%1.0f/%1.0f index=%1.0f/%1.0f (%1.2f%%)",
        cr.limit/8.0, double(cr.ht.size()), double(count), double(cr.cm.size()),
        count*100.0/cr.cm.size());
    }
    else if (type==SSE) {
      assert(cr.cm.size()>0);
      size_t count=0;
      for (size_t j=0; j<cr.cm.size(); ++j) {
        if (int(cr.cm[j])!=(squash((j&31)*64-992)<<17|z.header[cp+3]))
          ++count;
      }
      printf(": %1.0f/%1.0f (%1.2f%%)", double(count),
        double(cr.cm.size()), count*100.0/cr.cm.size());
    }
    else if (type==CM) {
      assert(cr.cm.size()>0);
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
