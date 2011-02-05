/* pzpaq.cpp v0.04 - Parallel ZPAQ compressor

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

pzpaq is a parallel ZPAQ compatible compressor. It compresses
or decompresses multiple files in parallel for better speed.
It can also compress a file or a solid archive in smaller blocks
for better speed at some cost in compressed size.

Command interface is similar to compress, gzip, or bzip2.
See usage() below or type "pzpaq -h" for brief description.

See http://mattmahoney.net/dc/pzpaq.html for complete documenation.

See http://mattmahoney.net/dc/zpaq.html for the latest version
of this software, for libzpaq which you will need to compile,
and for the ZPAQ specification.

To compile in Linux without optimized decompression:

  g++ -O3 -DNDEBUG pzpaq.cpp libzpaq.cpp libzpaqo.cpp -lpthread

Or to compile in Windows:

  g++ -O3 -DNDEBUG pzpaq.cpp libzpaq.cpp libzpaqo.cpp

Version 0.03 and higher will speed up decompression of archives created
with non-default compression (other than levels -1 -2 -3 created by
other programs) by translating the headers to pzpaqopt.cpp,
compiling with g++ to pzpaqopt.exe, and running it with the same arguments.
This feature is disabled by default. It will still work, but more slowly.
To enable this feature, compile with -DOPT="..." option, where the value
is a command string to create pzpaqopt.exe. For example:

  -DOPT="\"g++ -O3 pzpaqopt.cpp pzpaq.o libzpaq.o -lpthread -o pzpaqopt.exe\""

plus -I and -L or full paths as appropriate depending on where you put
the .o files, libzpaq.h, and pthread files. pzpaq.o and libzpaq.o should
be prepared in advance so that they only need to be linked when pzpaq is run:

  g++ -O3 -DNDEBUG -c pzpaq.cpp libzpaq.cpp

Recommended optimization options (everywhere in place of -O3):

  -O3 -s -march=native -fomit-frame-pointer

But something else might be appropriate for your computer.

-DNDEBUG turns off run time checks for better speed in libzpaq.cpp
and pzpaq.cpp, but has no effect on pzpaqopt.cpp. Thus, you don't
need to embed -DNDEBUG in the -DOPT string.

Recommended installation for Linux:

  /usr/bin/pzpaq
  /usr/lib/zpaq/pzpaq.o
  /usr/lib/zpaq/libzpaq.o
  /usr/include/libzpaq.h

Recommended installation for Windows, assuming that MinGW g++
is installed in c:\mingw and that c:\bin is in your PATH.

  c:\bin\pzpaq.exe
  c:\bin\zpaq\pzpaq.o
  c:\bin\zpaq\libzpaq.o
  c:\mingw\include\libzpaq.h or c:\bin\zpaq\libzpaq.h

For Windows without g++, you only need pzpaq.exe.

If you also have zpaq installed and configured to work with g++, then
libzpaq.h and libzpaq.o are already installed and may be shared by both
programs.

History:

v0.01 - initial release.
v0.02 - fix >2GB file handling for Windows.
v0.03 - adds decompression optimization for nonstandard compression levels.
v0.04 - native thread support in Windows, no longer needs pthread-win32.

*/

#include "libzpaq.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <assert.h>
#include <string>
#include <vector>
#include <fcntl.h>

#ifdef unix
#define PTHREAD 1
#include <sys/types.h>
#include <unistd.h>
#else
#include <windows.h>
#include <io.h>
#endif

// Compile with -DPTHREAD to use http://sourceware.org/pthreads-win32/
// instead of Windows native threads.
#ifdef PTHREAD
#include <pthread.h>
#endif

// Borland: compile with -Dint64_t=__int64
#ifndef int64_t
#include <stdint.h>
#endif

void usage() {
  fprintf(stderr,
  "pzpaq 0.04 - Parallel ZPAQ compressor\n"
  "(C) 2011, Dell Inc. Written by Matt Mahoney\n"
  "This is free software under GPL v3. http://www.gnu.org/copyleft/gpl.html\n"
  "\n"
  "Usage: pzpaq [-options]... [files]...\n"
  "Default is to compress, replacing each file with file.zpaq\n"
  "If no files are specified, then compress stdin to stdout. Options:\n"
  "-123  Compress fast, mid, or max (default -2 = mid)\n"
  "-bN   Compress in N byte blocks, -b0=infinite (default = size/threads)\n"
  "-c    Concatenate to standard output, keep input files\n"
  "-d    Decompress, replacing file.zpaq with file\n"
  "-e    Extract to current directory using saved names, keep input files\n"
  "-h    Help (print this message)\n"
  "-k    Keep (don't delete) input files\n"
  "-l    List compressed file contents\n"
  "-mN   Memory limit of N MB (default -m500)\n"
  "-sS   Suffix S1,S2... for temporary files (default -s.tmp)\n"
  "-tN   (De)compress blocks in parallel using N Threads (default -t2)\n"
  "-v    Verbose\n"
  "-x    Extract to original directory using saved paths, keep input files\n"
  "--    Stop option processing\n"
  );
#ifdef unix
  if (sizeof(off_t)!=8)
    fprintf(stderr, "Does not work with files larger than 2 GB\n");
#endif
#ifdef OPT
  fprintf(stderr, "Decompression optimization enabled with:\n  %s\n", OPT);
#endif
  exit(1);
}

// Seek f to 64 bit pos, return true if successful
int fseek64(FILE* f, int64_t pos) {
#ifdef unix
  return fseeko(f, pos, SEEK_SET)==0;
#else
  rewind(f);
  HANDLE h=(HANDLE)_get_osfhandle(_fileno(f));
  LONG low=pos, high=pos>>32;
  errno=0;
  SetFilePointer(h, low, &high, FILE_BEGIN);
  return GetLastError()==ERROR_SUCCESS;
#endif
}

// Return size of an open file as a 64 bit number
int64_t filesize(FILE* f) {
#ifdef unix
  int64_t pos=ftello(f);
  fseeko(f, 0, SEEK_END);
  int64_t result=ftello(f);
  fseeko(f, pos, SEEK_SET);
  return result;
#else
  HANDLE h=(HANDLE)_get_osfhandle(_fileno(f));
  DWORD high;
  DWORD low=GetFileSize(h, &high);
  return int64_t(high)<<32|low;
#endif
}

// Call f and check that the return code is 0
#define check(f) { \
  int rc=f; \
  if (rc) fprintf(stderr, "Line %d: %s: error %d\n", __LINE__, #f, rc); \
}

// signed size of a string or vector
template <typename T> int size(const T& x) {
  return x.size();
}

// Options readable by all threads
int command='2';     // -123dexl (compress, decompress, list)
const int MIN_BOPT=0x1000;      // minimum bopt
const int MAX_BOPT=0x7fffffff;  // maximum bopt
int bopt=-1;         // -b block size, 0 = infinite, -1 = default size/topt
bool copt=false;     // -c (output to stdout)
bool kopt=false;     // -k (keep input files)
int mopt=500;        // -m memory limit in MB
const char* sopt=".tmp";  // -s (temp file extension)
int topt=2;          // -t, at least 1 (number of threads)
bool verbose=false;  // -v

// Possible job states. A thread is initialized as READY. When main()
// is ready to start the thread it is set to RUNNING and runs  it. When
// the thread finishes, it sets its state to FINISHED or FINISHED_ERR
// if there is an error, signals main (using cv, protected by mutex),
// and exits. main then waits on the thread, receives the return status, and
// updates the state to OK or ERR.
typedef enum {READY, RUNNING, FINISHED_ERR, FINISHED, ERR, OK} State;

#ifdef PTHREAD
pthread_cond_t cv=PTHREAD_COND_INITIALIZER;  // to signal FINISHED
pthread_mutex_t mutex=PTHREAD_MUTEX_INITIALIZER; // protects cv
#else
HANDLE mutex;  // protects Job::state
typedef HANDLE pthread_t;
#endif

// A filename and a size
struct FileSize {
  const char* filename;  // input file, "" for stdin
  int64_t size;  // input size, -1 if unknown
  FileSize(const char* f=0, int s=-1): filename(f), size(s) {}
};

// Instructions to thread to compress or decompress one block.
struct Job {
  State state;        // job state, protected by mutex
  std::vector<FileSize> input;  // list of files to input
  std::string output; // output file, "" for stdout, saved names override
  int64_t start;      // where to start input of first file
  int memory;         // how much memory is needed in MB (for scheduler)
  int part;           // position in sequence for concatenation, 0=first
  pthread_t tid;      // thread ID (for scheduler)
  Job();
  void print(int i) const;
};

// Initialize
Job::Job(): state(READY), start(0), memory(0), part(0) {
  // tid is not initialized until state==RUNNING
}

// Print contents
void Job::print(int i=0) const {
  fprintf(stderr, "Job %d: state=%d start=%1.0f memory=%d part=%d output=%s\n",
       i, state, double(start), memory, part, output.c_str());
  for (int j=0; j<size(input); ++j)
    fprintf(stderr, "  %s %1.0f\n", input[j].filename, double(input[j].size));
}

// Thread exit. A msg of 0 means OK and any other pointer means ERR.
void libzpaq::error(const char* msg) {
  if (msg) fprintf(stderr, "pzpaq error: %s\n", msg);
  throw msg;
}

// File for libzpaq (de)compression
struct File: public libzpaq::Reader, public libzpaq::Writer {
  FILE* f;
  int get() {return getc(f);}
  void put(int c) {putc(c, f);}
  File(FILE* f_=0): f(f_) {}
};

// To count bytes read or written
struct FileCount: public libzpaq::Reader, public libzpaq::Writer {
  FILE* f;
  int64_t count;
  FileCount(FILE* f_): f(f_), count(0) {}
  int get() {int c=getc(f); count+=(c!=EOF); return c;}
  void put(int c) {putc(c, f); count+=1;}
};

// To output to a string
struct StringWriter: public libzpaq::Writer {
  std::string s;
  void put(int c) {s+=char(c);}
};

// File that automatically computes size and checksum of each
// byte of input or output
struct FileSHA1: public libzpaq::Reader, public libzpaq::Writer {
  FILE* f;
  libzpaq::SHA1 sha1;
  int get() {int c=getc(f); if (c!=EOF) sha1.put(c); return c;}
  void put(int c) {sha1.put(c); putc(c, f);}
  FileSHA1(FILE* f_=0): f(f_) {}
};

// Remove path from filename
std::string strip(const std::string& filename) {
  for (int i=size(filename)-1; i>=0; --i) {
    if (filename[i]=='/' || filename[i]=='\\' || (i==1 && filename[i]==':'))
      return filename.substr(i+1);
  }
  return filename;
}

// Convert int to string
std::string itos(int64_t x) {
  std::string s;
  if (x==0) return "0";
  if (x<0) return "-"+itos(-x);
  while (x>0) {
    s=char(x%10+'0')+s;
    x/=10;
  }
  return s;
}

// Append file2 to file1 and delete file2. Return true if the append
// is successful. "" means stdout, stdin.
bool append(const char* file1, const char* file2) {
  if (verbose)
    fprintf(stderr, "Appending to %s from %s\n", file1, file2);
  FILE* in=stdin;
  if (file2 && *file2) in=fopen(file2, "rb");
  if (!in) {
    perror(file2);
    return false;
  }
  FILE* out=stdout;
  if (file1 && *file1) out=fopen(file1, "ab");
  if (!out) {
    perror(file1);
    if (in!=stdin) fclose(in);
    return false;
  }
  const int BUFSIZE=4096;
  char buf[BUFSIZE];
  int n;
  while ((n=fread(buf, 1, BUFSIZE, in))>0)
    fwrite(buf, 1, n, out);
  if (out!=stdout) fclose(out);
  if (in!=stdin) fclose(in);
  if (in!=stdin && remove(file2))
    perror(file2);
  return true;
}

/////////////////////////// optimize ///////////////////////

// This code is to convert ZPAQL to C++.
#ifdef OPT

// Pad pcomp string with an empty COMP header with ph,pm from hcomp
void fix_pcomp(const std::string& hcomp, std::string& pcomp) {
  if (size(hcomp)>=8 && size(pcomp)>=2) {
    pcomp=hcomp.substr(0, 8)+pcomp.substr(2);
    pcomp[0]=(size(pcomp)-2)&255;  // new length of PCOMP
    pcomp[1]=(size(pcomp)-2)>>8;
    pcomp[6]=pcomp[7]=0;  // n=0 components
  }
}

// Read little-endian 2 byte number at models[p..p+1]
int get2(const std::string& models, int p) {
  assert(p+1<size(models));
  return (models[p]&255)+256*(models[p+1]&255);
}

// Test if filename is readable
bool exists(const char* filename) {
  FILE* in=fopen(filename, "rb");
  if (in) {
    fclose(in);
    return true;
  }
  else
    return false;
}

// Test if a file exists and exit with error if not
void testfile(const char* filename) {
  if (!exists(filename)) {
    fprintf(stderr, "File not found: %s\n", filename);
    exit(1);
  }
}

// Delete a file
void delete_file(const char* filename) {
  if (verbose && exists(filename))
    fprintf(stderr, "Deleting %s\n", filename);
  unlink(filename);
}

// Print and run a command
int run_cmd(const std::string& cmd) {
  if (verbose)
    fprintf(stderr, "%s\n", cmd.c_str());
  return system(cmd.c_str());
}

// Return '/' in Linux or '\' in Windows
char slash() {
#ifdef unix
  return '/';
#else
  return '\\';
#endif
}

typedef enum {NONE,CONS,CM,ICM,MATCH,AVG,MIX2,MIX,ISSE,SSE,
  JT=39,JF=47,JMP=63,LJ=255} CompType;

// Generate one case of predict()
void opt_predict(FILE *out, const std::string& models, int p, int select) {
  assert(size(models)>6);
  int n=models[p+6]&255;
  fprintf(out,
    "    case %d: {\n"
    "      // %d components\n", select, n);

  // Code each component
  p+=7;
  for (int i=0; i<n; ++i) {
    int cp[10]={0};
    for (int j=0; j<10 && p+j<size(models); ++j)
      cp[j]=models[p+j]&255;
    switch(cp[0]) {
      case CONS:  // c
        fprintf(out, "\n      // %d CONST %d\n", i, cp[1]);
        break;
      case CM:  // sizebits limit
        fprintf(out, "\n      // %d CM %d %d\n", i, cp[1], cp[2]);
        fprintf(out,
          "      comp[%d].cxt=z.H(%d)^hmap4;\n"
          "      p[%d]=stretch(comp[%d].cm(comp[%d].cxt)>>17);\n",
          i, i, i, i, i);
        break;
      case ICM: // sizebits
        fprintf(out, "\n      // %d ICM %d\n", i, cp[1]);
        fprintf(out,
          "      if (c8==1 || (c8&0xf0)==16)\n"
          "        comp[%d].c=find(comp[%d].ht, %d+2, z.H(%d)+16*c8);\n"
          "      comp[%d].cxt=comp[%d].ht[comp[%d].c+(hmap4&15)];\n"
          "      p[%d]=stretch(comp[%d].cm(comp[%d].cxt)>>8);\n",
          i, i, cp[1], i, i, i, i, i, i, i);
        break;
      case MATCH: // sizebits bufbits: a=len, b=offset, c=bit, cxt=256/len,
                  //                   ht=buf, limit=8*pos+bp
        fprintf(out, "\n      // %d MATCH %d %d\n", i, cp[1], cp[2]);
        fprintf(out,
          "      if (comp[%d].a==0) p[%d]=0;\n"
          "      else {\n"
          "        comp[%d].c=comp[%d].ht((comp[%d].limit>>3)\n"
          "           -comp[%d].b)>>(7-(comp[%d].limit&7))&1;\n"
          "        p[%d]=stretch(comp[%d].cxt*(comp[%d].c*-2+1)&32767);\n"
          "      }\n",
          i, i, i, i, i, i, i, i, i, i);
        break;
      case AVG: // j k wt
          fprintf(out, "\n      // %d AVG %d %d %d\n", i, cp[1], cp[2], cp[3]);
          fprintf(out,
          "      p[%d]=(p[%d]*%d+p[%d]*(256-%d))>>8;\n",
          i, cp[1], cp[3], cp[2], cp[3]);
        break;
      case MIX2:   // sizebits j k rate mask
                   // c=size cm=wt[size][m] cxt=input
        fprintf(out, "\n      // %d MIX2 %d %d %d %d %d\n", 
                     i, cp[1], cp[2], cp[3], cp[4], cp[5]);
        fprintf(out,
          "      {\n"
          "        comp[%d].cxt=((z.H(%d)+(c8&%d))&(comp[%d].c-1));\n"
          "        int w=comp[%d].a16[comp[%d].cxt];\n"
          "        p[%d]=(w*p[%d]+(65536-w)*p[%d])>>16;\n"
          "      }\n",
          i, i, cp[5], i, i, i, i, cp[2], cp[3]);

        break;
      case MIX:    // sizebits j m rate mask
                   // c=size cm=wt[size][m] cxt=index of wt in cm
        fprintf(out, "\n      // %d MIX %d %d %d %d %d\n", 
                     i, cp[1], cp[2], cp[3], cp[4], cp[5]);
        fprintf(out,
          "      {\n"
          "        comp[%d].cxt=z.H(%d)+(c8&%d);\n"
          "        comp[%d].cxt=(comp[%d].cxt&(comp[%d].c-1))*%d;\n"
          "        int* wt=(int*)&comp[%d].cm[comp[%d].cxt];\n",
          i, i, cp[5], i, i, i, cp[3], i, i);
          for (int j=0; j<cp[3]; ++j)  // unrolled for-loop
            fprintf(out, 
              "        p[%d]%s=(wt[%d]>>8)*p[%d];\n", 
              i, j?"+":"", j, cp[2]+j);
        fprintf(out,
          "        p[%d]=clamp2k(p[%d]>>8);\n"
          "      }\n",
          i, i);
        break;
      case ISSE:   // sizebits j -- c=hi, cxt=bh
        fprintf(out, "\n      // %d ISSE %d %d\n", i, cp[1], cp[2]);
        fprintf(out,
          "      {\n"
          "        if (c8==1 || (c8&0xf0)==16)\n"
          "          comp[%d].c=find(comp[%d].ht, %d, z.H(%d)+16*c8);\n"
          "        comp[%d].cxt=comp[%d].ht[comp[%d].c+(hmap4&15)];\n"
          "        int *wt=(int*)&comp[%d].cm[comp[%d].cxt*2];\n"
          "        p[%d]=clamp2k((wt[0]*p[%d]+wt[1]*64)>>16);\n"
          "      }\n",
          i, i, cp[1]+2, i, i, i, i, i, i, i, cp[2]);
        break;
      case SSE:   // sizebits j start limit
        fprintf(out, "\n      // %d SSE %d %d %d %d\n", 
                     i, cp[1], cp[2], cp[3], cp[4]);
        fprintf(out,
          "      {\n"
          "        comp[%d].cxt=(z.H(%d)+c8)*32;\n"
          "        int pq=p[%d]+992;\n"
          "        if (pq<0) pq=0;\n"
          "        if (pq>1983) pq=1983;\n"
          "        int wt=pq&63;\n"
          "        pq>>=6;\n"
          "        comp[%d].cxt+=pq;\n"
          "        p[%d]=stretch(((comp[%d].cm(comp[%d].cxt)>>10)*(64-wt)\n"
          "           +(comp[%d].cm(comp[%d].cxt+1)>>10)*wt)>>13);\n"
          "        comp[%d].cxt+=wt>>5;\n"
          "      }\n",
          i, i, cp[2], i, i, i, i, i, i, i);
        break;
      default:
        fprintf(stderr, "unknown component type %d\n", cp[0]);
        exit(1);
    }
    assert(libzpaq::compsize[cp[0]]>0);
    p+=libzpaq::compsize[cp[0]];
    assert(p<size(models));
  }
  assert(models[p]==NONE);
  if (n<1)
    fprintf(out,
      "      return predict0();\n"
      "    }\n");
  else
    fprintf(out,
      "      return squash(p[%d]);\n"
      "    }\n", n-1);
}

void opt_update(FILE *out, const std::string& models, int p, int select) {
  assert(size(models)>p+7);
  int n=models[p+6]&255;
  fprintf(out,
    "    case %d: {\n"
    "      // %d components\n", select, n);

  // Code each component
  p+=7;
  for (int i=0; i<n; ++i) {
    int cp[10]={0};
    for (int j=0; j<10 && p+j<size(models); ++j)
      cp[j]=models[p+j]&255;
    switch(cp[0]) {
      case CONS:  // c
        fprintf(out, "\n      // %d CONST %d\n", i, cp[1]);
        break;
      case CM:  // sizebits limit
        fprintf(out, "\n      // %d CM %d %d\n", i, cp[1], cp[2]);
        fprintf(out,
          "      train(comp[%d], y);\n",
          i);
        break;
      case ICM:   // sizebits: cxt=ht[b]=bh, ht[c][0..15]=bh row, cxt=bh
        fprintf(out, "\n      // %d ICM %d\n", i, cp[1]);
        fprintf(out,
          "      {\n"
          "        comp[%d].ht[comp[%d].c+(hmap4&15)]=\n"
          "            st.next(comp[%d].ht[comp[%d].c+(hmap4&15)], y);\n"
          "        U32& pn=comp[%d].cm(comp[%d].cxt);\n"
          "        pn+=int(y*32767-(pn>>8))>>2;\n"
          "      }\n",
          i, i, i, i, i, i);
        break;
      case MATCH: // sizebits bufbits:
                  //   a=len, b=offset, c=bit, cm=index, cxt=256/len
                  //   ht=buf, limit=8*pos+bp
        fprintf(out, "\n      // %d MATCH %d %d\n", i, cp[1], cp[2]);
        fprintf(out,
          "      {\n"
          "        if (comp[%d].c!=y) comp[%d].a=0;\n"
          "        comp[%d].ht(comp[%d].limit>>3)+=comp[%d].ht(comp[%d].limit>>3)+y;\n"
          "        if ((++comp[%d].limit&7)==0) {\n"
          "          int pos=comp[%d].limit>>3;\n"
          "          if (comp[%d].a==0) {\n"
          "            comp[%d].b=pos-comp[%d].cm(z.H(%d));\n"
          "            if (comp[%d].b&(comp[%d].ht.size()-1))\n"
          "              while (comp[%d].a<255 && comp[%d].ht(pos-comp[%d].a-1)\n"
          "                     ==comp[%d].ht(pos-comp[%d].a-comp[%d].b-1))\n"
          "                ++comp[%d].a;\n"
          "          }\n"
          "          else comp[%d].a+=comp[%d].a<255;\n"
          "          comp[%d].cm(z.H(%d))=pos;\n"
          "          if (comp[%d].a>0) comp[%d].cxt=2048/comp[%d].a;\n"
          "        }\n"
          "      }\n",
          i, i, i, i, i, i, i, i, i, i, i, i, i, i,
          i, i, i, i, i, i, i, i, i, i, i, i, i, i);
        break;
      case AVG:  // j k wt
        fprintf(out, "\n      // %d AVG %d %d %d\n", i, cp[1], cp[2], cp[3]);
        break;
      case MIX2:   // sizebits j k rate mask
                   // cm=input[2],wt[size][2], cxt=weight row
        fprintf(out, "\n      // %d MIX2 %d %d %d %d %d\n", 
                     i, cp[1], cp[2], cp[3], cp[4], cp[5]);
        fprintf(out,
          "      {\n"
          "        int err=(y*32767-squash(p[%d]))*%d>>5;\n"
          "        int w=comp[%d].a16[comp[%d].cxt];\n"
          "        w+=(err*(p[%d]-p[%d])+(1<<12))>>13;\n"
          "        if (w<0) w=0;\n"
          "        if (w>65535) w=65535;\n"
          "        comp[%d].a16[comp[%d].cxt]=w;\n"
          "      }\n",
          i, cp[4], i, i, cp[2], cp[3], i, i);
        break;
      case MIX:     // sizebits j m rate mask
                    // cm=wt[size][m], cxt=input
        fprintf(out, "\n      // %d MIX %d %d %d %d %d\n", 
                     i, cp[1], cp[2], cp[3], cp[4], cp[5]);
        fprintf(out,
          "      {\n"
          "        int err=(y*32767-squash(p[%d]))*%d>>4;\n"
          "        int* wt=(int*)&comp[%d].cm[comp[%d].cxt];\n",
          i, cp[4], i, i);
        for (int j=0; j<cp[3]; ++j) // unrolled
          fprintf(out,
            "          wt[%d]=clamp512k(wt[%d]+((err*p[%d]+(1<<12))>>13));\n",
            j, j, cp[2]+j);
        fprintf(out,
          "      }\n");
        break;
      case ISSE:   // sizebits j  -- c=hi, cxt=bh
        fprintf(out, "\n      // %d ISSE %d %d\n", i, cp[1], cp[2]);
        fprintf(out,
          "      {\n"
          "        int err=y*32767-squash(p[%d]);\n"
          "        int *wt=(int*)&comp[%d].cm[comp[%d].cxt*2];\n"
          "        wt[0]=clamp512k(wt[0]+((err*p[%d]+(1<<12))>>13));\n"
          "        wt[1]=clamp512k(wt[1]+((err+16)>>5));\n"
          "        comp[%d].ht[comp[%d].c+(hmap4&15)]=st.next(comp[%d].cxt, y);\n"
          "      }\n",
          i, i, i, cp[2], i, i, i);
        break;
      case SSE:  // sizebits j start limit
        fprintf(out, "\n      // %d SSE %d %d %d %d\n", 
                     i, cp[1], cp[2], cp[3], cp[4]);
        fprintf(out,
          "      train(comp[%d], y);\n",
          i);
        break;
      default:
        fprintf(stderr, "unknown component type %d\n", cp[0]);
        exit(1);
    }
    assert(libzpaq::compsize[cp[0]]>0);
    p+=libzpaq::compsize[cp[0]];
    assert(p<size(models));
  }
  assert(models[p]==NONE);
  fprintf(out,
    "      break;\n"
    "    }\n");
}

// Generate optimization code for the HCOMP section of models[p...]
void opt_hcomp(FILE *out, const std::string& models, int p, int select) {

  // Instruction translation table
  static const char* inst[256]={
    "err();",                  // 0  ERROR
    "++a;",                    // 1  A++
    "--a;",                    // 2  A--
    "a = ~a;",                 // 3  A!
    "a = 0;",                  // 4  A=0
    "err();",
    "err();",
    "a = r[%d];",              // 7  A=R N
    "swap(b);",                // 8  B<>A
    "++b;",                    // 9  B++
    "--b;",                    // 10  B--
    "b = ~b;",                 // 11  B!
    "b = 0;",                  // 12  B=0
    "err();",
    "err();",
    "b = r[%d];",              // 15  B=R N
    "swap(c);",                // 16  C<>A
    "++c;",                    // 17  C++
    "--c;",                    // 18  C--
    "c = ~c;",                 // 19  C!
    "c = 0;",                  // 20  C=0
    "err();",
    "err();",
    "c = r[%d];",              // 23  C=R N
    "swap(d);",                // 24  D<>A
    "++d;",                    // 25  D++
    "--d;",                    // 26  D--
    "d = ~d;",                 // 27  D!
    "d = 0;",                  // 28  D=0
    "err();",
    "err();",
    "d = r[%d];",              // 31  D=R N
    "swap(m(b));",             // 32  *B<>A
    "++m(b);",                 // 33  *B++
    "--m(b);",                 // 34  *B--
    "m(b) = ~m(b);",           // 35  *B!
    "m(b) = 0;",               // 36  *B=0
    "err();",
    "err();",
    "if (f) goto L%d;",        // 39  JT N
    "swap(m(c));",             // 40  *C<>A
    "++m(c);",                 // 41  *C++
    "--m(c);",                 // 42  *C--
    "m(c) = ~m(c);",           // 43  *C!
    "m(c) = 0;",               // 44  *C=0
    "err();",
    "err();",
    "if (!f) goto L%d;",       // 47  JF N
    "swap(h(d));",             // 48  *D<>A
    "++h(d);",                 // 49  *D++
    "--h(d);",                 // 50  *D--
    "h(d) = ~h(d);",           // 51  *D!
    "h(d) = 0;",               // 52  *D=0
    "err();",
    "err();",
    "r[%d] = a;",              // 55  R=A N
    "return;",                 // 56  HALT
    "if (output) output->put(a); if (sha1) sha1->put(a);", // 57  OUT
    "err();",
    "a = (a+m(b)+512)*773;",   // 59  HASH
    "h(d) = (h(d)+a+512)*773;",// 60  HASHD
    "err();",
    "err();",
    "goto L%d;",               // 63  JMP N
    "a = a;",                  // 64  A=A
    "a = b;",                  // 65  A=B
    "a = c;",                  // 66  A=C
    "a = d;",                  // 67  A=D
    "a = m(b);",               // 68  A=*B
    "a = m(c);",               // 69  A=*C
    "a = h(d);",               // 70  A=*D
    "a = %d;",                 // 71  A= N
    "b = a;",                  // 72  B=A
    "b = b;",                  // 73  B=B
    "b = c;",                  // 74  B=C
    "b = d;",                  // 75  B=D
    "b = m(b);",               // 76  B=*B
    "b = m(c);",               // 77  B=*C
    "b = h(d);",               // 78  B=*D
    "b = %d;",                 // 79  B= N
    "c = a;",                  // 80  C=A
    "c = b;",                  // 81  C=B
    "c = c;",                  // 82  C=C
    "c = d;",                  // 83  C=D
    "c = m(b);",               // 84  C=*B
    "c = m(c);",               // 85  C=*C
    "c = h(d);",               // 86  C=*D
    "c = %d;",                 // 87  C= N
    "d = a;",                  // 88  D=A
    "d = b;",                  // 89  D=B
    "d = c;",                  // 90  D=C
    "d = d;",                  // 91  D=D
    "d = m(b);",               // 92  D=*B
    "d = m(c);",               // 93  D=*C
    "d = h(d);",               // 94  D=*D
    "d = %d;",                 // 95  D= N
    "m(b) = a;",               // 96  *B=A
    "m(b) = b;",               // 97  *B=B
    "m(b) = c;",               // 98  *B=C
    "m(b) = d;",               // 99  *B=D
    "m(b) = m(b);",            // 100  *B=*B
    "m(b) = m(c);",            // 101  *B=*C
    "m(b) = h(d);",            // 102  *B=*D
    "m(b) = %d;",              // 103  *B= N
    "m(c) = a;",               // 104  *C=A
    "m(c) = b;",               // 105  *C=B
    "m(c) = c;",               // 106  *C=C
    "m(c) = d;",               // 107  *C=D
    "m(c) = m(b);",            // 108  *C=*B
    "m(c) = m(c);",            // 109  *C=*C
    "m(c) = h(d);",            // 110  *C=*D
    "m(c) = %d;",              // 111  *C= N
    "h(d) = a;",               // 112  *D=A
    "h(d) = b;",               // 113  *D=B
    "h(d) = c;",               // 114  *D=C
    "h(d) = d;",               // 115  *D=D
    "h(d) = m(b);",            // 116  *D=*B
    "h(d) = m(c);",            // 117  *D=*C
    "h(d) = h(d);",            // 118  *D=*D
    "h(d) = %d;",              // 119  *D= N
    "err();",
    "err();",
    "err();",
    "err();",
    "err();",
    "err();",
    "err();",
    "err();",
    "a += a;",                 // 128  A+=A
    "a += b;",                 // 129  A+=B
    "a += c;",                 // 130  A+=C
    "a += d;",                 // 131  A+=D
    "a += m(b);",              // 132  A+=*B
    "a += m(c);",              // 133  A+=*C
    "a += h(d);",              // 134  A+=*D
    "a += %d;",                // 135  A+= N
    "a -= a;",                 // 136  A-=A
    "a -= b;",                 // 137  A-=B
    "a -= c;",                 // 138  A-=C
    "a -= d;",                 // 139  A-=D
    "a -= m(b);",              // 140  A-=*B
    "a -= m(c);",              // 141  A-=*C
    "a -= h(d);",              // 142  A-=*D
    "a -= %d;",                // 143  A-= N
    "a *= a;",                 // 144  A*=A
    "a *= b;",                 // 145  A*=B
    "a *= c;",                 // 146  A*=C
    "a *= d;",                 // 147  A*=D
    "a *= m(b);",              // 148  A*=*B
    "a *= m(c);",              // 149  A*=*C
    "a *= h(d);",              // 150  A*=*D
    "a *= %d;",                // 151  A*= N
    "div(a);",                 // 152  A/=A
    "div(b);",                 // 153  A/=B
    "div(c);",                 // 154  A/=C
    "div(d);",                 // 155  A/=D
    "div(m(b));",              // 156  A/=*B
    "div(m(c));",              // 157  A/=*C
    "div(h(d));",              // 158  A/=*D
    "div(%d);",                // 159  A/= N
    "mod(a);",                 // 160  A=A
    "mod(b);",                 // 161  A=B
    "mod(c);",                 // 162  A=C
    "mod(d);",                 // 163  A=D
    "mod(m(b));",              // 164  A=*B
    "mod(m(c));",              // 165  A=*C
    "mod(h(d));",              // 166  A=*D
    "mod(%d);",                // 167  A= N
    "a &= a;",                 // 168  A&=A
    "a &= b;",                 // 169  A&=B
    "a &= c;",                 // 170  A&=C
    "a &= d;",                 // 171  A&=D
    "a &= m(b);",              // 172  A&=*B
    "a &= m(c);",              // 173  A&=*C
    "a &= h(d);",              // 174  A&=*D
    "a &= %d;",                // 175  A&= N
    "a &= ~ a;",               // 176  A&~A
    "a &= ~ b;",               // 177  A&~B
    "a &= ~ c;",               // 178  A&~C
    "a &= ~ d;",               // 179  A&~D
    "a &= ~ m(b);",            // 180  A&~*B
    "a &= ~ m(c);",            // 181  A&~*C
    "a &= ~ h(d);",            // 182  A&~*D
    "a &= ~ %d;",              // 183  A&~ N
    "a |= a;",                 // 184  A|=A
    "a |= b;",                 // 185  A|=B
    "a |= c;",                 // 186  A|=C
    "a |= d;",                 // 187  A|=D
    "a |= m(b);",              // 188  A|=*B
    "a |= m(c);",              // 189  A|=*C
    "a |= h(d);",              // 190  A|=*D
    "a |= %d;",                // 191  A|= N
    "a ^= a;",                 // 192  A^=A
    "a ^= b;",                 // 193  A^=B
    "a ^= c;",                 // 194  A^=C
    "a ^= d;",                 // 195  A^=D
    "a ^= m(b);",              // 196  A^=*B
    "a ^= m(c);",              // 197  A^=*C
    "a ^= h(d);",              // 198  A^=*D
    "a ^= %d;",                // 199  A^= N
    "a <<= (a&31);",           // 200  A<<=A
    "a <<= (b&31);",           // 201  A<<=B
    "a <<= (c&31);",           // 202  A<<=C
    "a <<= (d&31);",           // 203  A<<=D
    "a <<= (m(b)&31);",        // 204  A<<=*B
    "a <<= (m(c)&31);",        // 205  A<<=*C
    "a <<= (h(d)&31);",        // 206  A<<=*D
    "a <<= (%d&31);",          // 207  A<<= N
    "a >>= (a&31);",           // 208  A>>=A
    "a >>= (b&31);",           // 209  A>>=B
    "a >>= (c&31);",           // 210  A>>=C
    "a >>= (d&31);",           // 211  A>>=D
    "a >>= (m(b)&31);",        // 212  A>>=*B
    "a >>= (m(c)&31);",        // 213  A>>=*C
    "a >>= (h(d)&31);",        // 214  A>>=*D
    "a >>= (%d&31);",          // 215  A>>= N    
    "f = (a == a);",           // 216  A==A
    "f = (a == b);",           // 217  A==B
    "f = (a == c);",           // 218  A==C
    "f = (a == d);",           // 219  A==D
    "f = (a == U32(m(b)));",   // 220  A==*B
    "f = (a == U32(m(c)));",   // 221  A==*C
    "f = (a == h(d));",        // 222  A==*D
    "f = (a == U32(%d));",     // 223  A== N
    "f = (a < a);",            // 224  A<A
    "f = (a < b);",            // 225  A<B
    "f = (a < c);",            // 226  A<C
    "f = (a < d);",            // 227  A<D
    "f = (a < U32(m(b)));",    // 228  A<*B
    "f = (a < U32(m(c)));",    // 229  A<*C
    "f = (a < h(d));",         // 230  A<*D
    "f = (a < U32(%d));",      // 231  A< N
    "f = (a > a);",            // 232  A>A
    "f = (a > b);",            // 233  A>B
    "f = (a > c);",            // 234  A>C
    "f = (a > d);",            // 235  A>D
    "f = (a > U32(m(b)));",    // 236  A>*B
    "f = (a > U32(m(c)));",    // 237  A>*C
    "f = (a > h(d));",         // 238  A>*D
    "f = (a > U32(%d));",      // 239  A> N
    "err();",
    "err();",
    "err();",
    "err();",
    "err();",
    "err();",
    "err();",
    "err();",
    "err();",
    "err();",
    "err();",
    "err();",
    "err();",
    "err();",
    "err();",
    "goto L%d;"};              // 255 LJ NN

  // Find start and end of code
  assert(size(models)>p+8);
  const int end=p+get2(models, p)+2;
  assert(size(models)>=end+2);
  int n=models[p+6]&255;
  p+=7;
  for (int i=0; i<n; ++i) {
    assert((models[p]&255)>0 && libzpaq::compsize[models[p]&255]>0);
    p+=libzpaq::compsize[models[p]&255];
    assert(p<size(models)-1 && p<end);
  }
  assert(models[p]==0);
  ++p;
  assert(p<=end);

  // Generate a map of jump targets
  if (p==end) return;
  libzpaq::Array<char> targets(0x10000);
  for (int i=p; i<end-1; ++i) {
    int op=models[i]&255;
    if (op==LJ && p<end-2)
      targets[get2(models, i+1)]=1, ++i;
    if (op==JT || op==JF || op==JMP) {
      int addr=i+2+((models[i+1]&255)<<24>>24)-p;
      if (addr>=0 && addr<0x10000) targets[addr]=1;
      else fprintf(stderr, "goto target %d out of range\n", addr);
    }
    if (op%8==7) ++i;  // 2 byte instruction (LJ is 3)
  }

  // Generate instructions. The output code will not compile
  // if any ZPAQL instructions jump to the middle of a 2 or 3
  // byte instruction (legal) or out of range (legal if not executed).
  fprintf(out, "      a = input;\n");
  for (int i=p; i<end-1; ++i) {
    int op=models[i]&255;
    assert(i-p<0x10000);
    if (targets[i-p]) {
      fprintf(out, "L%d:\n", select*100000+(i-p)); // goto label
      targets[i-p]=0;
    }
    int operand=0;
    operand=models[i+1]&255;  // numeric operand
    if (op==JT || op==JF || op==JMP)  // label
      operand=select*100000+i+2+(operand<<24>>24)-p;
    if (op==LJ) {
      if (i<end-2)
        operand=select*100000+get2(models, i+1);  // label
      ++i;
    }
    if (op%8==7) ++i; // 2 byte instruction
    fprintf(out, "      ");
    fprintf(out, inst[op], operand);
    fprintf(out, "\n");
  }
}

// Search list of models for comp, return true if a match is found
bool findModel(const std::string& models, const std::string& comp) {
  if (size(comp)<8) return false;
  for (int p=0; p<size(models)-1; p+=get2(models, p)+2) {
    bool mismatch=false;
    for (int i=0; !mismatch && i<size(comp); ++i)
      mismatch=i+p>=size(models) || models[i+p]!=comp[i];
    if (!mismatch) return true;
  }
  return false;
}

// Combine hcomp and pcomp into 1 or 2 models suitable for libzpaq::models[]
std::string combine(std::string hcomp, std::string pcomp) {
  if (pcomp!="") {
    fix_pcomp(hcomp, pcomp);
    hcomp+=pcomp;
  }
  hcomp+=std::string(2, '\0');
  return hcomp;
}

// Print models[p..] for model i
void dump(FILE* out, const std::string& models, int p, int n) {
  assert(size(models)>p+1);
  const int len=get2(models, p)+2;
  assert(size(models)>=p+len);
  fprintf(out,
  "\n"
  "  // Model %d\n  ", n);
  for (int i=0; i<len; ++i) {
    fprintf(out, "%d,", char(models[p+i]));
    if (i%16==15) fprintf(out, "\n  ");
  }
  fprintf(out, "\n");
}

// Generate C++ source code from a list of models
// Then compile and run it with argc, argv
void optimize(const std::string& models, int argc, char** argv) {

  // Open output file
  FILE* out=fopen("pzpaqopt.cpp", "w");
  if (!out) perror("pzpaqopt.cpp"), exit(1);

  // Print models[]
  fprintf(out,
  "// pzpaqopt.cpp generated by pzpaq\n"
  "\n"
  "#include \"libzpaq.h\"\n"
  "namespace libzpaq {\n"
  "\n"
  "const char models[]={\n");
  int p, i;
  for (p=0, i=1; p<size(models)-2; p+=get2(models, p)+2, ++i)
    dump(out, models, p, i);
  assert(p==size(models)-2);
  assert(models[p]==0 && models[p+1]==0);
  fprintf(out, "\n  0,0};\n");  // end of list

  // Print predict()
  // Write Predictor::predict()
  fprintf(out,
    "\n"
    "int Predictor::predict() {\n"
    "  switch(z.select) {\n");
  for (p=0, i=1; p<size(models)-2; p+=get2(models, p)+2, ++i)
    opt_predict(out, models, p, i);
  fprintf(out,
    "    default: return predict0();\n"
    "  }\n"
    "}\n"
    "\n");

  // Write Predictor::update()
  fprintf(out,
    "void Predictor::update(int y) {\n"
    "  switch(z.select) {\n");
  for (p=0, i=1; p<size(models)-2; p+=get2(models, p)+2, ++i)
    opt_update(out, models, p, i);
  fprintf(out,
    "    default: return update0(y);\n"
    "  }\n"
    "  c8+=c8+y;\n"
    "  if (c8>=256) {\n"
    "    z.run(c8-256);\n"
    "    hmap4=1;\n"
    "    c8=1;\n"
    "  }\n"
    "  else if (c8>=16 && c8<32)\n"
    "    hmap4=(hmap4&0xf)<<5|y<<4|1;\n"
    "  else\n"
    "    hmap4=(hmap4&0x1f0)|(((hmap4&0xf)*2+y)&0xf);\n"
    "}\n"
    "\n");

  // Write ZPAQL::run()
  fprintf(out,
    "void ZPAQL::run(U32 input) {\n"
    "  switch(select) {\n");
  for (p=0, i=1; p<size(models)-2; p+=get2(models, p)+2, ++i) {
    fprintf(out, "    case %d: {\n", i);
    opt_hcomp(out, models, p, i);
    fprintf(out,
      "      break;\n"
      "    }\n");
  }
  fprintf(out,
    "    default: run0(input);\n"
    "  }\n"
    "}\n"
    "}\n"
    "\n");

  // Close output and make sure it exists
  fclose(out);
  testfile("pzpaqopt.cpp");
  if (verbose)
    fprintf(stderr, "Created pzpaqopt.cpp\n");

  // Compile
  unlink("pzpaqopt.exe");
  run_cmd(OPT);

  // Run it
  testfile("pzpaqopt.exe");
  std::string command=".";
  command+=slash();
  command+="pzpaqopt.exe";
  for (int i=1; i<argc; ++i) {
    command+=" ";
    command+=argv[i];
  }
  run_cmd(command);

  // Clean up and quit
  delete_file("pzpaqopt.exe");
  delete_file("pzpaqopt.cpp");
  delete_file("pzpaqopt.obj");
  delete_file("pzpaqopt.map");
  delete_file("pzpaqopt.tds");
  exit(0);
}

#endif // ifdef OPT

// Decompress. The input is a list of files to decompress,
// a size for each, an output file name, and a starting offset for
// the first file. A size of -1 means that all blocks should be
// decompressed, or else just the first block of the first file
// starting at the specified offset. An input file name of ""
// means to read from standard input. An output file name of ""
// means standard output. If the command is -e or -x and not -c then
// filenames stored in the archive override the output filename.
// Decompresion fails and the rest of the job is abandoned under
// the following conditions: an output file cannot be created
// (for example the path does not exist), an input file is not readable,
// or the input is corrupted, or a bad checksum is detected,
// or no compressed input is found.

void decompress(const Job& job) {

  // Decompress each file
  for (int i=0; i<size(job.input); ++i) {

    // Open input
    File in(stdin);
    const FileSize& fs=job.input[i];
    if (!fs.filename) libzpaq::error("null filename");
    if (fs.filename && fs.filename[i])
      in.f=fopen(fs.filename, "rb");
    if (!in.f) {
      perror(fs.filename);
      libzpaq::error("cannot read file");
    }

    // Find start of block in first file
    if (i==0 && job.start>0 && !fseek64(in.f, job.start))
      libzpaq::error("fseek64");

    // Decompress file
    libzpaq::Decompresser d;
    d.setInput(&in);
    std::string output=job.output;
    if (job.part) output+=sopt+itos(job.part);
    File out(0);
    while (d.findBlock()) {
      StringWriter filename, comment;
      while (d.findFilename(&filename)) {
        d.readComment(&comment);
        libzpaq::SHA1 sha1;
        d.setSHA1(&sha1);

        // Get new output filename
        if (filename.s!="" && !copt && command!='d') {
          if (command=='x')
            output=filename.s;
          else if (command=='e')
            output=strip(filename.s);
          if (verbose) {
            fprintf(stderr, "Decompressing %s %s -> %s\n",
              filename.s.c_str(), comment.s.c_str(), output.c_str());
          }
          if (out.f && out.f!=stdout) {
            fclose(out.f);
            out.f=0;
          }
        }
        filename.s="";
        comment.s="";

        // Set output
        if (!out.f) {
          out.f=stdout;
          if (output!="") {
            out.f=fopen(output.c_str(), "wb");
            if (!out.f) {
              perror(output.c_str());
              libzpaq::error("file creation failed");
            }
          }
        }
        d.setOutput(&out);

        // Decompress segment
        d.decompress();
        if (verbose) {
          fprintf(stderr, "%s -> %s %1.0f\n",
              fs.filename, output.c_str(), sha1.size());
        }

        // Verify checksum
        char sha1string[21];
        d.readSegmentEnd(sha1string);
        if (sha1string[0] && memcmp(sha1string+1, sha1.result(), 20)) {
          fprintf(stderr, "%s -> %s checksum error\n",
              fs.filename, output.c_str());
          libzpaq::error("checksum mismatch");
        }
      }

      // End of block
      if (fs.size!=-1)
        break;
    }

    // End of input file
    if (out.f && out.f!=stdout)
      fclose(out.f);
    if (in.f && in.f!=stdin)
      fclose(in.f);
    if (!out.f) {
      fprintf(stderr, "%s: ", fs.filename);
      libzpaq::error("no compressed data found");
    }
  }
}        

// Compress job.input to job.output in 1 block with each input file
// in a separate segment. For the special case of compressing from
// an unknown size and a block size specified in bopt, compress
// to multiple blocks of size bopt.
void compress(const Job& job) {

  // Get output file name
  std::string output=job.output;
  if (job.part) output+=sopt+itos(job.part);

  // Open output file
  libzpaq::Compressor c;
  FileCount out(stdout);
  if (output!="") out.f=fopen(output.c_str(), "wb");
  if (!out.f) {
    perror(output.c_str());
    libzpaq::error("output open failed");
  }
  c.setOutput(&out);
  c.writeTag();

  // Compress multiple files in one block, or multiple blocks if
  // an input size is unknown and not finished.
  for (bool done=false; !done;) {
    c.startBlock(command-'0');

    // Compress one segment per input file. Save filename if start is 0.
    // The comment is file size or "size+start" if start > 0.
    for (int i=0; i<size(job.input); ++i) {
      if (job.start>0 && i==0)
        c.startSegment(0, 
             (itos(job.input[i].size)+"+"+itos(job.start)).c_str());
      else
        c.startSegment(job.input[i].filename,
             itos(job.input[i].size).c_str());
      if (i==0)
        c.postProcess();

      // Open input file unless "" (stdin)
      FileSHA1 in(stdin);
      if (job.input[i].filename[0]) in.f=fopen(job.input[i].filename, "rb");
      if (!in.f) {
        perror(job.input[i].filename);
        libzpaq::error("input open failed");
      }
      c.setInput(&in);
      if (i==0 && job.start>0) {
        if (!fseek64(in.f, job.start))
          libzpaq::error("fseek64 failed");
      }
      if (verbose) {
        fprintf(stderr, "Compressing %s", job.input[i].filename);
        if (i==0 && job.start>0)
          fprintf(stderr, "+%1.0f", double(job.start));
        fprintf(stderr, " %1.0f -> %s\n",
            double(job.input[i].size), output.c_str());
      }

      // Compress 1 block or to EOF if -b0
      if (bopt>0 && job.input[i].size<0) {
        c.compress(bopt);
        done=in.sha1.size()<bopt;
      }
      else {
        c.compress(job.input[i].size>bopt ? -1: int(job.input[i].size));
        done=true;
      }
      if (verbose) {
        fprintf(stderr, "%s %1.0f -> %s %1.0f\n", job.input[i].filename,
            in.sha1.size(), output.c_str(), double(out.count));
      }
      c.endSegment(in.sha1.result());
      if (in.f!=stdin) fclose(in.f);
    }
    c.endBlock();
  }

  // Close output
  if (out.f!=stdout) fclose(out.f);
}

// List the contents of an archive to stdout
void list(const char* filename) {
  FileCount in(stdin);
  if (filename && *filename) {
    printf("%s\n", filename);
    in.f=fopen(filename, "rb");
    if (!in.f) {
      perror(filename);
      return;
    }
  }
  try {
    libzpaq::Decompresser d;
    in.count=1;
    d.setInput(&in);
    double memory=0;
    StringWriter name, comment;
    char s[21];  // checksum
    for (int i=1; d.findBlock(&memory); ++i) {
      printf("Block %d level %d needs %d MB\n",
          i, d.getModel(), int((memory+999999.5)/1000000));
      while (d.findFilename(&name)) {
        d.readComment(&comment);
        d.readSegmentEnd(s);
        if (s[0])
          printf("  %02x%02x%02x%02x ",
              s[1]&255, s[2]&255, s[3]&255, s[4]&255);
        else
          printf("           ");
        printf("%s %s -> %1.0f\n",
            name.s.c_str(), comment.s.c_str(), double(in.count));
        name.s="";
        comment.s="";
        in.count=0;
      }
    }
  }
  catch (const char* msg) {}
  if (in.f!=stdin) fclose(in.f);
  printf("\n");
}

// Worker thread
#ifdef PTHREAD
void*
#else
DWORD
#endif
thread(void *arg) {

  // Do the work and receive status in msg
  Job* job=(Job*)arg;
  const char* result=0;  // error message unless OK
  try {
    if (isdigit(command))
      compress(*job);
    else if (command=='d' || command=='x' || command=='e')
      decompress(*job);
  }
  catch (const char* msg) {
    result=msg;
  }

  // Let controlling thread know we're done and the result
#ifdef PTHREAD
  check(pthread_mutex_lock(&mutex));
  job->state=result?FINISHED_ERR:FINISHED;
  check(pthread_cond_signal(&cv));
  check(pthread_mutex_unlock(&mutex));
#else
  WaitForSingleObject(mutex, INFINITE);
  job->state=result?FINISHED_ERR:FINISHED;
  ReleaseMutex(mutex);
#endif
  return 0;
}

int main(int argc, char** argv) {

  // Start timer
  time_t start_time=time(0);

  // Process arguments
  bool opt=true;  // false after --
  std::vector<FileSize> files;  // list of files and sizes
  for (int i=1; i<argc; ++i) {
    if (opt && argv[i][0]=='-') {
      bool arg=false;  // option has an argument?
      for (int j=1; !arg && argv[i][j]; ++j) {
        switch(argv[i][j]) {
          case '1':
          case '2':
          case '3':
          case 'd':
          case 'e':
          case 'x':
          case 'l': command=argv[i][j]; break;
          case 'b': bopt=atoi(argv[i]+j+1); arg=true; break;
          case 'c': copt=true; break;
          case 'k': kopt=true; break;
          case 'm': mopt=atoi(argv[i]+j+1); arg=true; break;
          case 's': sopt=argv[i]+j+1; arg=true; break;
          case 't': topt=atoi(argv[i]+j+1); arg=true; break;
          case 'v': verbose=true; break;
          case '-': opt=false; break;
          default: usage();  // -h or others
        }
      }
    }
    else
      files.push_back(FileSize(argv[i]));
  }
  if (topt<1) usage();
  if (size(files)==0) {
    topt=1;  // can't multithread from stdin
    files.push_back("");  // add stdin to list
  }
  kopt |= copt || command=='e' || command=='x';
#ifndef PTHREAD
  if (topt>MAXIMUM_WAIT_OBJECTS)
    topt=MAXIMUM_WAIT_OBJECTS;  // max 64 threads in Windows
#endif

  // set stdin and stdout to binary mode in Windows
#ifndef unix
  if (command!='l')
    setmode(1, O_BINARY);  // stdout
  setmode(0, O_BINARY);  // stdin
#endif

  // Get file sizes, -1 = unknown. Remove nonexistent files
  for (int i=0; i<size(files); ++i) {
    assert(files[i].filename);
    if (files[i].filename[0]) {  // not stdin?
      FILE* f=fopen(files[i].filename, "rb");
      if (!f) {  // remove nonexistent files
        perror(files[i].filename);
        for (int j=i+1; j<size(files); ++j)
          files[j-1]=files[j];
        files.pop_back();
      }
      else {
        files[i].size=filesize(f);
        if (files[i].size==-1)
          perror(files[i].filename);
        fclose(f);
      }
    }
  }

  // Get default block size. If any sizes are unknown then
  // default is -b0 (no blocks)
  if (bopt<0 && isdigit(command)) {
    int64_t sum=0;
    for (int i=0; i<size(files); ++i) {
      if (files[i].size<0) { // unknown size
        sum=-1;
        break;
      }
      sum+=files[i].size;
    }
    if (sum<0)
      bopt=0;
    else {
      int64_t t=(sum+topt-1)/topt;
      bopt=t<MAX_BOPT ? int(t) : MAX_BOPT;
      if (bopt<MIN_BOPT) bopt=MIN_BOPT;
    }
  }

  // Print processed command line
  if (verbose) {
    fprintf(stderr, "%s -%c -b%d %s %s -m%d -s%s -t%d -v",
        argv[0], command, bopt, copt?"-c":"", kopt?"-k":"", mopt, sopt, topt);
    for (int i=0; i<size(files); ++i)
      fprintf(stderr, " %s", files[i].filename);
    fprintf(stderr, "\n\n");
  }

  // List
  if (command=='l') {
    for (int i=0; i<size(files); ++i)
      list(files[i].filename);
    return 0;
  }

  // List of jobs
  std::vector<Job> jobs;

  // Schedule decompression for commands -d, -e, or -x.
  // stdin is 1 job. Otherwise the input files are scanned for blocks
  // and each block of each input file is one job.
  // job.start is the offset of the start of the block.
  // job.input has one file. It is "" for stdin or else
  // the filename. The size is not important.
  // If the first segment is not named or ignored by -d or -c then
  // job.output and job.part determine the output file name.
  // job.part is the distance in blocks to the block in
  // the file that names the segment (at least 1) and
  // job.output is that name. The name comes from the
  // last named segment, with a path for -x or without for -e.
  // If no named segment, or names are ignored by -d then
  // job.output is derived by removing the .zpaq extension
  // from the input filename, or appending sopt (.tmp) if there
  // is no .zpaq extension, or is "" if -c or input is "" (stdin).
  // If OPT then build a list of models. If any non-default models
  // are found then generate and run pzpaqopt.
  if (command=='d' || command=='e' || command=='x') {
#ifdef OPT
    std::string model_list;
    bool non_default=false;
#endif
    int part=0;
    std::string output;
    if (copt) output="";  // -c
    for (int i=0; i<size(files); ++i) {
      try {

        // stdin
        if (files[i].size<0 || !files[i].filename || !files[i].filename[0]) {
          Job job;
          job.input.push_back(files[i]);
          jobs.push_back(job);
        }
        else {

          // Open input file
          FileCount in(fopen(files[i].filename, "rb"));
          if (!in.f)
            perror(files[i].filename);
          else {

            // Get initial output name by dropping .zpaq or adding .tmp
            if (!copt) {
              int l=strlen(files[i].filename);
              if (l>5 && !strcmp(files[i].filename+l-5, ".zpaq"))
                output=std::string(files[i].filename).substr(0, l-5);
              else if (l>0)
                output=std::string(files[i].filename)+sopt;
              if (command=='e')
                output=strip(output);
            }

            // Scan input for blocks
            int64_t offset=0;
            libzpaq::Decompresser d;
            d.setInput(&in);
            double memory;
            StringWriter filename;
            if (!copt) part=0;
            while (d.findBlock(&memory)) {

              // Schedule a job for this block
              Job job;
              job.input.push_back(files[i]);
              job.start=offset;
              job.output=output;
              job.memory=int((memory+999999.5)/1000000);
              job.part=part;

              // Update output by finding the last named segment
#ifdef OPT
              StringWriter hcomp, pcomp;
              d.hcomp(&hcomp);
              if (!findModel(model_list, hcomp.s))
                model_list+=hcomp.s;
              if (d.getModel()<1) non_default=true;
#endif
              bool first_segment=true;
              while (d.findFilename(&filename)) {
                d.readComment();
#ifdef OPT
                if (first_segment) {
                  d.decompress(0);
                  if (d.pcomp(&pcomp)) {
                    non_default=true;
                    fix_pcomp(hcomp.s, pcomp.s);
                    if (!findModel(model_list, pcomp.s))
                      model_list+=pcomp.s;
                  }
                }
#endif
                d.readSegmentEnd();
                offset=in.count+1;  // start of next block after EOB
                if (filename.s!="" && command!='d' && !copt) {
                  if (command=='e')
                    output=strip(filename.s);
                  else if (command=='x')
                    output=filename.s;
                  part=0;
                  if (first_segment) {
                    job.part=0;
                    job.output=output;
                  }
                }
                first_segment=false;
                filename.s="";
              }
              ++part;
              jobs.push_back(job);
            }
            fclose(in.f);
          }
        }
      }

      // In case of error, go on to the next input file
      catch (const char* msg) {
        fprintf(stderr, "%s: %s\n", files[i].filename, msg);
      }
    }
#ifdef OPT
    if (non_default) {
      model_list+=char(0);
      model_list+=char(0);
      optimize(model_list, argc, argv);
    }
#endif
  }

  // Schedule compression according to -c, -b (copt, bopt)
  // If -c -b0, then job is all input
  // If -c -b, then a job is 1 block, splitting input files
  // If -b0, then a job is 1 file
  // If -b then a job is 1 block of 1 file
  // Unknown file sizes like stdin are treated like size 0
  if (isdigit(command)) {
    const int memory[]={38, 112, 247};  // command -> MB needed
    int fn=0;    // number of files scheduled
    int64_t len=0; // number of bytes of files[fn] scheduled
    int part=0;  // number of jobs since start of file
    while (fn<size(files)) {  // until all input is scheduled
      Job job;  // Schedule 1 job per loop
      job.start=len;
      job.part=part++;
      job.memory=memory[command-'1'];
      if (!copt) job.output=files[fn].filename;
      if (job.output!="") job.output+=".zpaq";

      // If -c then add files to job until block is full
      // else job is 1 block or rest of file whichever is smaller.
      // If -b0 then block has infinite size.
      // If file size is unknown then pretend it is 0.
      for (int64_t remaining=bopt-(bopt==0); remaining && fn<size(files);) {
        job.input.push_back(files[fn]);
        job.input.back().size-=len;

        // Remaining space in block is at least as big as rest of file?
        if (bopt==0 || remaining>=job.input.back().size) { // add it
          remaining-=job.input.back().size;
          ++fn;
          len=0;
          if (!copt) part=0;
        }
        else {  // fill block with part of file
          len+=job.input.back().size=remaining;
          remaining=0;
        }
        if (!copt)
          break;
      }
      jobs.push_back(job);
    }
  }

  // Print list of jobs
  if (verbose) {
    for (int i=0; i<size(jobs); ++i)
      jobs[i].print(i);
  }

  // Loop until all jobs return OK or ERR: start a job whenever one
  // is eligible. If none is eligible then wait for one to finish and
  // try again. If none are eligible and none are running then it is
  // an error.
  int memory_count=0;  // MB in use, not to exceed mopt
  int thread_count=0;  // number RUNNING, not to exceed topt
  int job_count=0;     // number of jobs with state OK or ERR

  // Aquire lock on jobs[i].state.
  // Threads can access only while waiting on a FINISHED signal.
#ifdef PTHREAD
  pthread_attr_t attr; // thread joinable attribute
  check(pthread_attr_init(&attr));
  check(pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE));
  check(pthread_mutex_lock(&mutex));  // locked
#else
  mutex=CreateMutex(NULL, FALSE, NULL);  // not locked
#endif

  while(job_count<size(jobs)) {

    // If there is more than 1 thread then run the biggest jobs first
    // that satisfies the memory bound. If 1 then take the next ready job
    // that satisfies the bound. If no threads are running, then ignore
    // the memory bound.
    int bi=-1;  // find a job to start
    if (thread_count<topt) {
      for (int i=0; i<size(jobs); ++i) {
        if (jobs[i].state==READY 
            && (thread_count==0 || jobs[i].memory+memory_count<=mopt)
            && (bi<0 || jobs[i].input[0].size>jobs[bi].input[0].size)) {
          bi=i;
          if (topt==1) break;
        }
      }
    }

    // If found then run it
    if (bi>=0) {
      jobs[bi].state=RUNNING;
      ++thread_count;
      memory_count+=jobs[bi].memory;
#ifdef PTHREAD
      check(pthread_create(&jobs[bi].tid, &attr, thread, &jobs[bi]));
#else
      jobs[bi].tid=CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)thread,
          &jobs[bi], 0, NULL);
#endif
    }

    // If no jobs can start then wait for one to finish
    else {
      if (thread_count<1) { // no jobs to wait on?
        fprintf(stderr, "Not enough memory, try larger -m\n");
        break;
      }
#ifdef PTHREAD
      check(pthread_cond_wait(&cv, &mutex));  // wait on cv

      // Join any finished threads. Usually that is the one
      // that signaled it, but there may be others.
      for (int i=0; i<size(jobs); ++i) {
        if (jobs[i].state==FINISHED || jobs[i].state==FINISHED_ERR) {
          void* status=0;
          check(pthread_join(jobs[i].tid, &status));
          if (jobs[i].state==FINISHED) jobs[i].state=OK;
          if (jobs[i].state==FINISHED_ERR) jobs[i].state=ERR;
          ++job_count;
          --thread_count;
          memory_count-=jobs[i].memory;
        }
      }
#else
      // List of running jobs
      HANDLE joblist[MAXIMUM_WAIT_OBJECTS];
      int jobptr[MAXIMUM_WAIT_OBJECTS];
      DWORD njobs=0;
      WaitForSingleObject(mutex, INFINITE);
      for (int i=0; i<size(jobs) && njobs<MAXIMUM_WAIT_OBJECTS; ++i) {
        if (jobs[i].state==RUNNING || jobs[i].state==FINISHED
            || jobs[i].state==FINISHED_ERR) {
          jobptr[njobs]=i;
          joblist[njobs++]=jobs[i].tid;
        }
      }
      ReleaseMutex(mutex);
      DWORD id=WaitForMultipleObjects(njobs, joblist, FALSE, INFINITE);
      if (id>=WAIT_OBJECT_0 && id<WAIT_OBJECT_0+njobs) {
        id-=WAIT_OBJECT_0;
        id=jobptr[id];
        if (jobs[id].state==FINISHED) jobs[id].state=OK;
        if (jobs[id].state==FINISHED_ERR) jobs[id].state=ERR;
        ++job_count;
        --thread_count;
        memory_count-=jobs[id].memory;
      }
#endif
    }
  }
#ifdef PTHREAD
  check(pthread_mutex_unlock(&mutex));
#endif

  // Append temporary files if both tmp and destination are OK.
  // If destination is ERR and tmp is OK then delete tmp.
  for (int i=0; i<size(jobs); ++i) {
    const int part=jobs[i].part;
    if (part>0 && part<=i) {
      std::string tmp=jobs[i].output+sopt+itos(part);
      if (jobs[i].state==OK) {
        if (jobs[i-part].state==OK)
          append(jobs[i].output.c_str(), tmp.c_str());
        else {
          if (verbose)
            fprintf(stderr, "Deleting %s\n", tmp.c_str());
          if (remove(tmp.c_str()))
            perror(tmp.c_str());
        }
      }
    }
  }

  // Delete input files if there was no error in any output part
  if (!kopt) {
    for (int i=0; i<size(jobs); ++i) {
      if (jobs[i].state==OK) {
        for (int j=0; j<size(jobs[i].input); ++j) {
          if ((j>0 || jobs[i].start==0) && jobs[i].input[j].filename[0]) {
            if (verbose)
              fprintf(stderr, "Deleting %s\n", jobs[i].input[j].filename);
            if (remove(jobs[i].input[j].filename))
              perror(jobs[i].input[j].filename);
          }
        }
      }
    }
  }

  // Report unfinished jobs and time
  if (verbose) {
    for (int i=0; i<size(jobs); ++i) {
      if (jobs[i].state!=OK) {
        fprintf(stderr, "failed: ");
        jobs[i].print(i);
      }
    }
    fprintf(stderr, "%1.0f seconds\n", double(time(0)-start_time));
  }

  return 0;
}
