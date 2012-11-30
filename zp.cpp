/* zp.cpp v1.03 - Parallel ZPAQ compressor

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

zp is a ZPAQ file compressor and archiver. Usage:

  zp [-options] files...

The default behavior is to compress each file to file.zpaq.
Options:

  -c  = compress or decompress to standard output.
  -f  = force overwrite of existing files.
  -l  = list file.zpaq contents. Don't compress or decompress.
  -r  = remove input files when done.
  -tN = use N threads. Default = number of processors.
  -v  = verbose. Default: no output except for errors.

For compression:

  -bN = compress using block size of about N MB. Default -b32
  -mN = compress with method N (1=fastest...4=best). Default -m1

For decompression:

  -d  = decompress file.zpaq to file, ignoring saved names.
  -e  = extract to current directory using saved names.
  -x  = extract to original directory using saved paths.
  -k  = keep temporary zp*.cpp optimization source.

By default, zp compresses each file to a single file archive and
adds a .zpaq extension. The file name is saved as specified on
the command line. The size is saved as a comment as a decimal string.
Checksums are saved. Archives begin with a locator tag so that it
can be found by ZPAQ decompressers even if embedded in other data.
Large files are split into blocks that can be compressed and
decompressed in parallel by separate threads on multi-core
processors.

-c causes all output to be concatenated to standard output in the
same format to create a multi-file archive. For example:

  zp -c calgary\* > calgary.zpaq

creates an archive. File names are saved as calgary\book1, etc.

  zp -l calgary.zpaq

shows the archive contents.

  zp -x -f calgary.zpaq

creates subdirectory calgary if it does not already exist, and
extracts the files there. If the files exist, they are overwritten.

Option -bN specifies a block size of N*1048576 - 256 bytes
(about N MB). Larger blocks compress better but reduce opportunities
for parallelism. Compression levels are as follows:

  Level Model    Memory required per thread
  ----- -----    --------------------------
  -m1   bwtrle1  5x block size rounded up (default 160 MB)
  -m2   bwt2     5x block size rounded up (default 160 MB)
  -m3   mid      111 MB
  -m4   max      246 MB

For levels -m1 and -m2, the memory required is 5 times the block
size after rounding up to the nearest power of 2. The default
is -b32 which requires 160 MB per thread. However -b17 also
requires this amount.

-tN (e.g. -t2) sets the number of threads to compress at one time.
By default it is the number of processors, detected in Windows from
the environment variable NUMBER_OF_PROCESSORS, or in Linux by
counting the number of "processor" sections in /proc/cpuinfo.
Using a larger value generally will not run faster. Using a smaller
value reduce memory required to compress and can reduce
CPU power and ease CPU load for other programs. -t can also be
used during decompression if there is not enough memory available
otherwise.

If a file is spread over more than one block, then subsequent
blocks are decompressed to temporary files and appended after
all threads are finished. The temporary directory is named
by the environment variable TEMP (normally set in Windows),
or if not set, then by TMPDIR (sometimes set in Linux), or if
not set, then in /tmp (normally used in Linux). Temporary
filenames have the form "zptmp{pid}_{N}" where {pid} is the
process ID and N is a number 1 or more. They are normally
deleted after use.

If an external C++ compiler is available, then zp can be
configured for just-in-time (JIT) optimization, which speeds
up decompression. Optimization is built in for some common
configurations (fast, mid, max, bwtrle1, bwt2), but others
will run faster if JIT is enabled when zp.cpp is compiled. If
enabled, then zp will generate a temporary file containing
C++ code (zptmp{pid}_0.cpp), and attempt to compile, link,
and run it with the same arguments passed to unzp. After
decompression, the source and executable is deleted. The source
can be preserved with -k.

If JIT fails because of a compiler error or because there is
no compiler installed, then unzp will detect this and decompress
by interpreting the code. This works but takes about twice as
long depending on the algorithm.

Source configuration files for the 4 compression levels are
available from http://mattmahoney.net/dc/zpaq.html
A brief description of the algorithms:

-m1 bwtrle1. Input blocks are context sorted using a Burrows-
Wheeler transform (BWT) followed by run length encoding (RLE).
Runs of 2 to 257 are encoded as 2 bytes plus a count byte.
The result is compressed using a bytewise order 0 indirect context
model (ICM) that includes the RLE state (literal or count)
as context. An ICM maps a context (RLE state plus the previous
bits of the current byte) to an 8-bit bit history (a count of
0s and 1s and the last bit), and then to a bit prediction.
After a bit is arithmetic coded, the prediction is updated to
reduce the prediction error.

-m2 bwt2. BWT without RLE. Removing RLE improves compression but
takes longer because there is more subsequent data to compress.
The result is modeled in an order 0-1 ISSE chain. The order
0 ICM prediction is adjusted by an order 1 indirect secondary
symbol estimator (ISSE). An ISSE takes a predition, p, and
a context (order 1, consisting of the previous byte and the
previous bits of the current byte), and outputs a new prediction,
which is arithmetic coded. It computes the new prediction
squash(w1 + w2*stretch(p)) where p is input, stretch(p) =
log(p/(1-p)), squash(x) = 1/(1 + e^-x) is the inverse of
stretch(), and w1 and w2 are weights that are updated to reduce
the prediction error after coding. The pair of weights
(initially (0, 1)) are selected by the bit history selected
by the context.

-m3 mid. Input is modeled directly (no BWT) by an order
0-1-2-3-4-5 ISSE chain, an order 7 match model, and an order 0 mixer.
A match model looks up the most recent match to the (order 7)
context, predicts whatever bit follows (with confidence depending
on the match length), and extends the match until the prediction
fails. All of the components (ICM, ISSE x 5, match) are combined
using a mixer for the final bit prediction. A mixer computes
squash(SUM w[i]*stretch(p[i])) where p[i] are the input predictions,
w[i] are weights updated to reduce prediction errors, the summation
is over i, and the weight vector w[] is selected by the order
0 context.

-m4 max. Uses a 22 component model that includes a longer ISSE
chain, 3 sparse order 1 ICM (context gaps of 1, 2, 3 bytes),
a text model, and a model for FAX images (216 bytes) that includes
the previous raster scan as context. The text model is an order
0-1 ISSE chain in which the contexts are hashes of whole words,
converting to case insensitive and ignoring non-letters. The
predictions are combined using an order 1-0 mixer chain followed
by an order 0-1 SSE chain in which each output is mixed (order -1)
with the input. An SSE is like an ISSE but takes a direct context
(not a bit history) and a quantized and interpolated prediction
as input, and outputs a new prediction that is updated after
coding to reduce the prediction error.

To compile, you need libzpaq from http://mattmahoney.net/dc/zpaq
and libdivsufsort-lite from http://code.google.com/p/libdivsufsort/

  g++ -O3 -s -DNDEBUG zp.cpp libzpaq.cpp divsufsort.c

In Linux, also use the options -lpthread -fopenmp
For compatibility with very old machines, you can use
-march=pentiumpro without much performance penalty.
If you don't distribute the executable, use -march=native.
-DNDEBUG turns off run time debugging checks in libzpaq.cpp.
-s strips debugging info. -fopenmp improves speed for divsufsort.c
on multi-processor machines.

To enable JIT, the following files must be installed at some fixed location
and zp must be compiled with an option (-DOPT) telling it where they
can be found:

  zp.o         (compiled with -DNOOPT and not -DOPT)
  libzpaq.o    (compiled from libzpaq.cpp)
  libzpaq.h    (from libzpaq)

The option -DOPT should be set to a command string to compile a
file %1.cpp to %1.exe, where zp will substitute %1 with the name
of a temporary file. This file will be linked with the two .o
(or .obj) files and #include the above header file. For example:

  g++ -O3 -c -march=native -DNDEBUG libzpaq.cpp
  g++ -O3 -s -march=native zp.cpp divsufsort.c libzpaq.o -o zp \
    -DOPT="g++ -O3 -march=native %1.cpp zp.o libzpaq.o -o %1.exe"
  g++ -O3 -c -DNOOPT zp.cpp

-DNOOPT excludes code needed to compress, such as divsufsort,
and excludes optimization code that will be generated and linked
later at run time. When run, zp will expect to find the above 3 files in
the current directory. If they are elsewhere, then the -DOPT string should
specify their exact location and include a -I option to find libzpaq.h.
Note that zp.exe is not made from the same zp.o described in -DOPT.
They are compiled with different options.

For example, suppose in Windows that c:\bin is in your PATH. A possible
installation with MinGW g++ is:

  c:\bin\zp.exe
  c:\bin\zpaq\zp.o
  c:\bin\zpaq\libzpaq.o
  c:\bin\zpaq\libzpaq.h

with JIT enabled as follows:

  -DOPT="\"g++ -O3 -Ic:\\bin\\zpaq c:\\bin\\zpaq\\zp.o c:\\bin\\zpaq\\libzpaq.o %1.cpp -o %1.exe\""

In Linux, you also need to compile zp.cpp with -lpthread. Also, -Dunix
is assumed. A possible installation is:

  /usr/bin/zp
  /usr/lib/zpaq/zp.o
  /usr/lib/zpaq/libzpaq.o
  /usr/include/libzpaq.h


History:

zp 1.01 is derived from pzpaq (not from zp 1.00).
zp and unzp together replace pzpaq.

zp 1.02 - fixed -t option.

zp 1.03 - merged with unzp.

*/
#define NDEBUG 1
#include "libzpaq.h"
#ifndef NOOPT
#include "divsufsort.h"
#endif

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
#include <sys/stat.h>
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

int numberOfProcessors();  // Default for -t

void usage() {
  fprintf(stderr,
  "zp v1.03 - Parallel ZPAQ compressor and decompresser\n"
  "(C) 2011, Dell Inc. Written by Matt Mahoney\n"
  "This is free software under GPL v3. http://www.gnu.org/copyleft/gpl.html\n"
  "\n"
  "Usage: zp [-options]... files...\n"
  "Default is to compress each file to file.zpaq. Options\n"
  "  -c  concatenate to standard output.\n"
  "  -l  list contents only.\n"
  "  -r  remove input files when done.\n"
  "  -tN use N threads. Default -t%d\n"
  "  -v  verbose.\n"
  "For compression:\n"
  "  -bN use block size of about N MB. Default -b32\n"
  "  -mN use method N (1=fastest...4=best). Default -m1\n"
  "For decompression:\n"
  "  -d  decompress each file.zpaq to file, ignoring saved names.\n"
  "  -e  extract to current directory using saved names.\n"
  "  -x  extract to original directory using saved paths.\n"
  "  -f  force overwrite of existing files.\n",
  numberOfProcessors());
#ifdef OPT
  fprintf(stderr, 
  "  -k  keep JIT optimization source code.\n"
  "JIT optimization enabled with:\n  %s\n", OPT);
#else
  fprintf(stderr, "JIT optimization not enabled with -DOPT\n");
#endif
#ifdef unix
  if (sizeof(off_t)!=8)
    fprintf(stderr, "Does not work with files larger than 2 GB\n");
#endif
#ifndef NDEBUG
  fprintf(stderr, "Debug (slow) version, not compiled with -DNDEBUG\n");
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

// Return size of an open file as a 64 bit number, or -1 if error
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
  SetLastError(NO_ERROR);
  DWORD low=GetFileSize(h, &high);
  if (GetLastError()!=NO_ERROR)
    return -1;
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

// Guess number of cores
int numberOfProcessors() {
  int rc=0;  // result
#ifdef unix

  // Count lines of the form "processor\t: %d\n" in /proc/cpuinfo
  // where %d is 0, 1, 2,..., rc-1
  FILE *in=fopen("/proc/cpuinfo", "r");
  if (!in) return 1;
  std::string s;
  int c;
  while ((c=getc(in))!=EOF) {
    if (c>='A' && c<='Z') c+='a'-'A';  // convert to lowercase
    if (c>' ') s+=c;  // remove white space
    if (c=='\n') {  // end of line?
      if (size(s)>10 && s.substr(0, 10)=="processor:") {
        c=atoi(s.c_str()+10);
        if (c==rc) ++rc;
      }
      s="";
    }
  }
  fclose(in);
#else

  // In Windows return %NUMBER_OF_PROCESSORS%
  const char* p=getenv("NUMBER_OF_PROCESSORS");
  if (p) rc=atoi(p);
#endif
  if (rc<1) rc=1;
  return rc;
}

// Options readable by all threads
int command=' ';      // -d, -e, -x, -l, else compress
unsigned int bopt=32; // -b block size
bool copt=false;      // -c output to stdout
bool fopt=false;      // -f force overwrite
bool kopt=false;      // -k keep JIT source
int mopt=1;           // -m compression level
bool ropt=false;      // -r remove input files
int topt=1;           // -t, at least 1 (number of threads)
bool verbose=false;   // -v

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

// Instructions to thread to compress or decompress one block.
struct Job {
  State state;        // job state, protected by mutex
  const char* input;  // input file name
  std::string output; // output file, "" for stdout, saved names override
  int64_t size;       // input block size (for scheduling)
  int64_t start;      // where to start input of first file
  int id;             // temporary output file name or 0 if none
  int part;           // position in sequence for concatenation, 0=first
  pthread_t tid;      // thread ID (for scheduler)
  Job();
  void print(int i) const;
};

// Initialize
Job::Job(): state(READY), input(0), size(-1), start(0), id(0), part(0) {
  // tid is not initialized until state==RUNNING
}

// Print contents
void Job::print(int i=0) const {
  fprintf(stderr,
      "Job %d: state=%d %s -> %s size=%1.0f start=%1.0f id=%d part=%d\n",
      i, state, input?input:"", output.c_str(), double(size), double(start),
      id, part);
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

// To output to a string
struct StringWriter: public libzpaq::Writer {
  std::string s;
  void put(int c) {s+=char(c);}
};

// Remove path from filename
std::string strip(const std::string& filename) {
  for (int i=size(filename)-1; i>=0; --i) {
    if (filename[i]=='/' || filename[i]=='\\' || (i==1 && filename[i]==':'))
      return filename.substr(i+1);
  }
  return filename;
}

// To count bytes read or written
struct FileCount: public libzpaq::Reader, public libzpaq::Writer {
  FILE* f;
  int64_t count;
  FileCount(FILE* f_): f(f_), count(0) {}
  int get() {int c=getc(f); count+=(c!=EOF); return c;}
  void put(int c) {putc(c, f); count+=1;}
};

#ifndef NOOPT

// File that automatically computes size and checksum of each
// byte of input and computes BWT or BWT-RLE depending on command.
class FileSHA1: public libzpaq::Reader {
  unsigned char* buf;  // holds BWT
  int len;    // size of BWT
  int i;      // next byte to read
  int rle;    // consecutive byte count
public:
  FILE* f;
  libzpaq::SHA1 sha1;
  int get();
  FileSHA1(FILE* f_=0);
  ~FileSHA1() {if (buf) free(buf);}
};

FileSHA1::FileSHA1(FILE* f_): buf(0), len(0), i(0), rle(0), f(f_) {}

// Read a byte
int FileSHA1::get() {

  // Return input byte not preprocessed
  if (mopt>=3) {
    int c=getc(f);
    if (c==EOF) return -1;
    sha1.put(c);
    return c;
  }

  // Store BWT of up to bopt bytes in buf[0..len-1]
  // Insert the EOF symbol at buf[idx] and store idx in last 4 bytes.
  if (len==0) {
    buf=(unsigned char*)malloc(bopt*5+5);
    if (!buf) libzpaq::error("out of memory for BWT");
    len=fread(buf, 1, bopt, f);
    for (int j=0; j<len; ++j) sha1.put(buf[j]);
    int idx=divbwt(buf, buf, (int*)(buf+len), len);
    if (len>idx) memmove(buf+idx+1, buf+idx, len-idx);
    buf[idx]=255;
    for (int j=0; j<4; ++j) buf[len+j+1]=idx>>(j*8);
    len+=5;
  }

  // Return BWT
  if (mopt==2) {
    if (i<len) return buf[i++];
    else return -1;
  }

  // Return BWT+RLE
  assert(mopt==1);
  if (rle<2 && i>=len) return -1;
  if (rle==2) {  // return RLE code
    int j;  // count run length
    for (j=0; j<255 && i+j<len && buf[i+j]==buf[i-1]; ++j);
    i+=j;
    rle=0;
    return j;
  }
  else {
    if (rle>0 && buf[i]==buf[i-1]) ++rle;
    else rle=1;
    return buf[i++];
  }
}

#endif  // ifndef NOOPT

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

// Delete a file
void delete_file(const char* filename) {
  if (verbose && exists(filename))
    fprintf(stderr, "Deleting %s\n", filename);
  remove(filename);
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
  if (in!=stdin) {
    fclose(in);
    delete_file(file2);
  }
  return true;
}

// Return '/' in Linux or '\' in Windows
char slash() {
#ifdef unix
  return '/';
#else
  return '\\';
#endif
}

// Construct a temporary file name
std::string tempname(int id) {

  // Get temporary directory
  std::string result;
  const char* env=getenv("TMPDIR");
  if (!env) env=getenv("TEMP");
  if (!env) env="/tmp";
  result=env;
  if (size(result)<1 || result[size(result)-1]!=slash())
    result+=slash();

  // Append base name
  result+="zptmp";

  // Append process ID
#ifdef unix
  result+=itos(getpid());
#else
  result+=itos(GetCurrentProcessId());
#endif

  // Append id
  result+="_";
  result+=itos(id);
  return result;
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

// Print and run a command
int run_cmd(const std::string& cmd) {
  if (verbose)
    fprintf(stderr, "%s\n", cmd.c_str());
  return system(cmd.c_str());
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

  // Get file name
  std::string basename=tempname(0);
  std::string sourcefile=basename+".cpp";
  std::string exefile=basename+".exe";

  // Open output file
  FILE* out=fopen(sourcefile.c_str(), "w");
  if (!out) perror(sourcefile.c_str()), exit(1);

  // Print models[]
  fprintf(out,
  "// generated by zp\n"
  "\n"
  "#define NDEBUG 1\n"
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
  if (verbose)
    fprintf(stderr, "Created %s\n", sourcefile.c_str());

  // Construct command by replacing "%1" with basename
  const char* opt=OPT;
  std::string command;
  for (int i=0; opt[i]; ++i) {
    if (opt[i]=='%' && opt[i+1]=='1') {
      command+=basename;
      ++i;
    }
    else
      command+=opt[i];
  }

  // Compile
  delete_file(exefile.c_str());
  run_cmd(command.c_str());

  // If compile failed, then run unoptimized
  if (!exists(exefile.c_str())) {
    if (verbose) fprintf(stderr, "Compile failed, skipping...\n");
    return;
  }

  // Run it
  command=exefile;
  for (int i=1; i<argc; ++i) {
    command+=" ";
    command+=argv[i];
  }
  run_cmd(command);

  // Clean up
  delete_file((basename+".obj").c_str());
  delete_file(exefile.c_str());
  if (!kopt) delete_file(sourcefile.c_str());
  exit(0);
}

#endif // ifdef OPT


/////////////////// compress ////////////////////////

#ifndef NOOPT

// Put n'th model into buf[258] (n starts at 1)
void copy_model(int n, char buf[]) {
  const unsigned char* p=(const unsigned char*)libzpaq::models;
  while (n>1 && p[0] && !p[1]) {
    p+=p[0]+2;
    --n;
  }
  if (n==1 && p[0] && !p[1])
    memcpy(buf, p, 2+*p);
}

// Remove COMP header from PCOMP string
void to_pcomp(char buf[]) {
  int len=(buf[0]&255)+256*(buf[1]&255);
  memmove(buf+2, buf+8, len-=6);
  buf[0]=len;
  buf[1]=len>>8;
}

// Compress job.input to job.output in 1 block.
void compress(const Job& job) {

  // Get output file name
  std::string output;
  if (job.part) output=tempname(job.id);
  else output=job.output;

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

  // Get header
  char buf[258];
  copy_model(mopt*2-(mopt==4), buf);  // 1..4 -> 2,4,6,7

  // Set postprocessor block size
  const bool bwt=mopt<=2;
  if (bwt) {
    int mem;
    for (mem=0; mem<32 && (1<<mem)-256<job.size; ++mem);
    buf[4]=buf[5]=mem;  // PCOMP H and M array sizes
  }
  c.startBlock(buf);

  // Compress one segment. Save filename if start is 0.
  // The comment is file size or "(part %d)" if start > 0.
  if (job.start>0)
    c.startSegment(0, ("(part "+itos(job.part+1)+")").c_str());
  else
    c.startSegment(job.input, itos(job.size).c_str());

  // Set postprocessor to bwtrle_post for 1, bwt_post for 2
  if (bwt) {
    copy_model(mopt==1?3:5, buf);
    to_pcomp(buf);
    c.postProcess(buf);
  }
  else
    c.postProcess(0);

  // Open input file
  FileSHA1 in(fopen(job.input, "rb"));
  if (!in.f) {
    perror(job.input);
    libzpaq::error("input open failed");
  }
  c.setInput(&in);
  if (job.start>0) {
    if (!fseek64(in.f, job.start))
      libzpaq::error("fseek64 failed");
  }
  if (verbose) {
    fprintf(stderr, "Compressing %s", job.input);
    if (job.start>0)
      fprintf(stderr, " part %d\n", job.part+1);
    else
      fprintf(stderr, " %1.0f -> %s\n", double(job.size), output.c_str());
  }

  // Compress 1 block or to EOF if -b0
  c.compress(bwt||job.size>bopt ? -1 : int(job.size));
  c.endSegment(in.sha1.result());
  c.endBlock();

  // Close output
  if (out.f!=stdout) fclose(out.f);
}

#endif // ifndef NOOPT

/////////////////// decompress ////////////////////////

// Create directories as needed. For example is path="/tmp/foo/bar"
// then create directories /, /tmp, and /tmp/foo unless they exist.
// Convert slashes to / in Linux or \ in Windows.
void makepath(std::string& path) {
  for (int i=0; i<size(path); ++i) {
    if (path[i]=='\\' || path[i]=='/') {
      path[i]=0;
#ifdef unix
      int ok=!mkdir(path.c_str(), 0777);
#else
      int ok=CreateDirectory(path.c_str(), 0);
#endif
      if (verbose && ok)
        fprintf(stderr, "Created directory %s\n", path.c_str());
      path[i]=slash();
    }
  }
}

// Decompress one block.
// job.input is the input filename.
// job.start is the starting offset of the block.
// job.output is the output filename if the first segment is
// not named or if the command is c or d, meaning that stored
// filenames are to be ignored. Otherwise, stored names in
// the segment headers override. If the output is "" then
// write to stdout.
// Decompresion fails and the rest of the job is abandoned under
// the following conditions: an output file cannot be created
// an input file is not readable,
// or the input is corrupted, or a bad checksum is detected,
// or no compressed input is found, or the output file exists
// and -f is not specified.

void decompress(const Job& job) {

  // Open input
  if (!job.input) libzpaq::error("null filename");
  File in(fopen(job.input, "rb"));
  if (!in.f) {
    perror(job.input);
    libzpaq::error("cannot read file ");
  }

  // Find start of block in first file
  if (job.start>0 && !fseek64(in.f, job.start))
    libzpaq::error("fseek64");

  // Decompress file
  libzpaq::Decompresser d;
  d.setInput(&in);
  std::string output=job.output;
  if (job.part) output=tempname(job.id);
  File out(0);
  if (d.findBlock()) {
    StringWriter filename, comment;
    while (d.findFilename(&filename)) {
      d.readComment(&comment);
      libzpaq::SHA1 sha1;
      d.setSHA1(&sha1);

      // Get new output filename
      if (filename.s!="" && (command=='e' || command=='x')) {
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
          if (!fopt && exists(output.c_str()))
            libzpaq::error((output+" exists, use -f to overwrite").c_str());
          makepath(output);
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
            job.input, output.c_str(), sha1.size());
      }

      // Verify checksum
      char sha1string[21];
      d.readSegmentEnd(sha1string);
      if (sha1string[0] && memcmp(sha1string+1, sha1.result(), 20)) {
        fprintf(stderr, "%s -> %s checksum error\n",
            job.input, output.c_str());
        libzpaq::error("checksum mismatch");
      }
    }
  }

  // End of input file
  if (out.f && out.f!=stdout)
    fclose(out.f);
  if (in.f && in.f!=stdin)
    fclose(in.f);
  if (!out.f) {
    fprintf(stderr, "%s: ", job.input);
    libzpaq::error("no compressed data found");
  }
}

////////////////// List /////////////////

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
      printf("Block %d model %d needs %1.3f MB\n", i, d.getModel(),
          memory*0.000001);
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
#ifndef NOOPT
    if (command==' ')
      compress(*job);
#endif
    if (command=='d' || command=='e' || command=='x')
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

/////////////////////////////// main //////////////////////////////

int main(int argc, char** argv) {

  // Start timer
  time_t start_time=time(0);

  // Process arguments
  if (argc<2) usage();
  topt=numberOfProcessors();
  int filelist=argc;  // index of first file name in argv
  for (int i=1; i<argc; ++i) {
    if (argv[i][0]=='-') {
      switch(argv[i][1]) {
        case 'd':
        case 'e':
        case 'x':
        case 'l': command=argv[i][1]; break;
        case 'm': mopt=atoi(argv[i]+2); break;
        case 'b': bopt=atoi(argv[i]+2); break;
        case 'c': copt=true; break;
        case 'f': fopt=true; break;
        case 'k': kopt=true; break;
        case 'r': ropt=true; break;
        case 't': topt=atoi(argv[i]+2); break;
        case 'v': verbose=true; break;
        default: usage();  // -h or others
      }
    }
    else {
      filelist=i;
      break;
    }
  }
  if (topt<1 || mopt<1 || mopt>4 || bopt<1 || filelist>=argc) usage();
  if (bopt>2047) bopt=2047;
  bopt=(bopt<<20)-256;
#ifndef PTHREAD
  if (topt>MAXIMUM_WAIT_OBJECTS)
    topt=MAXIMUM_WAIT_OBJECTS;  // max 64 threads in Windows
#endif

  // In Windows, if -c then set stdout to binary mode (Windows)
  // and make sure stdout is not a terminal
#ifndef unix
  if (copt) setmode(1, O_BINARY);  // stdout
  if (copt && command==' ' && filesize(stdout)<0) {
    fprintf(stderr, "Won't compress to a terminal\n");
    exit(1);
  }
#endif

  // List
  if (command=='l') {
    for (int i=filelist; i<argc; ++i)
      list(argv[i]);
    return 0;
  }

  // List of jobs
  std::vector<Job> jobs;

  // Schedule decompression for commands d, e, x.
  // The input files are scanned for blocks
  // and each block of each input file is one job.
  // job.start is the offset of the start of the block.
  // job.input is the filename. The size is the block size.
  // If the first segment is not named or ignored by -d or -c then
  // job.output and job.part determine the output file name.
  // job.part is the distance in blocks to the block in
  // the file that names the segment (at least 1) and
  // job.output is that name. The name comes from the
  // last named segment, with a path for -x or without for -e.
  // If no named segment, or names are ignored by -d or -c then
  // job.output is derived by removing the .zpaq extension
  // from the input filename, or adding .out if there is not a .zpaq
  // extension, or is "" if -c.
  // If OPT then build a list of models. If any non-default models
  // are found then generate and run unzpopt.
  if (command=='d' || command=='e' || command=='x') {
#ifdef OPT
    std::string model_list;
    bool non_default=false;
#endif
    int part=0;
    std::string output;
    for (int i=filelist; i<argc; ++i) {
      try {

        // Open input file
        FileCount in(fopen(argv[i], "rb"));
        if (!in.f)
          perror(argv[i]);
        else {

          // Get initial output name by dropping .zpaq or adding .out
          if (!copt) {
            int l=strlen(argv[i]);
            if (l>5 && !strcmp(argv[i]+l-5, ".zpaq"))
              output=std::string(argv[i]).substr(0, l-5);
            else
              output=std::string(argv[i])+".out";
          }

          // Scan input for blocks
          int64_t offset=0;
          libzpaq::Decompresser d;
          d.setInput(&in);
          StringWriter filename;
          part=0;
          while (d.findBlock()) {

            // Schedule a job for this block
            Job job;
            job.input=argv[i];
            job.start=offset;
            job.output=output;
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
                  if (d.getPostModel()<1)
                    non_default=true;
                  fix_pcomp(hcomp.s, pcomp.s);
                  if (!findModel(model_list, pcomp.s))
                    model_list+=pcomp.s;
                }
              }
#endif
              d.readSegmentEnd();
              offset=in.count+1;  // start of next block after EOB
              job.size=offset-job.start;
              if (filename.s!="" && (command=='e' || command=='x')) {
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
            }  // end while findFileName
            ++part;
            jobs.push_back(job);
          }  // end while findBlock
          fclose(in.f);
        }  // end for each file
      }  // end try

      // In case of error, go on to the next input file
      catch (const char* msg) {
        fprintf(stderr, "%s: %s\n", argv[i], msg);
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

  // Schedule compression
#ifndef NOOPT
  else if (command==' ') {
    int part=0;
    for (int i=filelist; i<argc; ++i) {

      // Get file size
      FILE* f=fopen(argv[i], "rb");
      if (!f) {
        perror(argv[i]);
        continue;
      }
      int64_t fs=filesize(f);
      fclose(f);
      if (fs<0) {
        fprintf(stderr, "File %s has unknown size, skipping...\n", argv[i]);
        continue;
      }

      // Schedule one job per block
      if (!copt) part=0;
      int64_t j=0;
      do {
        Job job;
        job.input=argv[i];
        if (!copt) job.output=std::string(argv[i])+".zpaq";
        job.start=j;
        job.size=bopt;
        if (job.start+job.size>fs) job.size=fs-job.start;
        job.part=part++;
        jobs.push_back(job);
        j+=bopt;
      } while (j<fs);
    }
  }
#endif // ifndef NOOPT

  // Assign job ids and print list of jobs
  int id=0;
  for (int i=0; i<size(jobs); ++i) {
    if (jobs[i].part) jobs[i].id=++id;
    if (verbose) jobs[i].print(i);
  }

  // Loop until all jobs return OK or ERR: start a job whenever one
  // is eligible. If none is eligible then wait for one to finish and
  // try again. If none are eligible and none are running then it is
  // an error.
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

    // If there is more than 1 thread then run the biggest jobs first.
    // If -t1 then take the next ready job.
    int bi=-1;  // find a job to start
    if (thread_count<topt) {
      for (int i=0; i<size(jobs); ++i) {
        if (jobs[i].state==READY 
            && (bi<0 || jobs[i].size>jobs[bi].size)) {
          bi=i;
          if (topt==1) break;
        }
      }
    }

    // If found then run it
    if (bi>=0) {
      jobs[bi].state=RUNNING;
      ++thread_count;
#ifdef PTHREAD
      check(pthread_create(&jobs[bi].tid, &attr, thread, &jobs[bi]));
#else
      jobs[bi].tid=CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)thread,
          &jobs[bi], 0, NULL);
#endif
    }

    // If no jobs can start then wait for one to finish
    else {
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
        }
      }
#else
      // Make a list of running jobs and wait on one to finish
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
      std::string tmp=tempname(jobs[i].id);
      if (jobs[i].state==OK) {
        if (jobs[i-part].state==OK)
          append(jobs[i].output.c_str(), tmp.c_str());
        else
          delete_file(tmp.c_str());
      }
    }
  }

  // Delete input files if there was no error
  if (ropt) {
    for (int i=0; i<size(jobs); ++i)
      if (jobs[i].state==OK && jobs[i].start==0)
        delete_file(jobs[i].input);
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

/////////////////////////// Optimized models ////////////////////////

#ifndef NOOPT

// generated by zpaq
namespace libzpaq {

const char models[]={

  // Model 1 fast
  26,0,1,2,0,0,2,3,16,8,19,0,0,96,4,28,
  59,10,59,112,25,10,59,10,59,112,56,0,

  // Model 2 bwtrle1 -1
  21,0,1,0,27,27,1,3,7,0,-38,80,47,3,9,63,
  1,12,65,52,60,56,0,

  // Model 3 bwtrle1 post -1
  -101,0,1,0,27,27,0,0,-17,-1,39,48,80,67,-33,0,
  47,6,90,25,98,9,63,34,67,2,-17,-1,39,16,-38,47,
  7,-121,-1,1,1,88,63,2,90,25,98,9,63,12,26,66,
  -17,0,47,5,99,9,18,63,-10,28,63,95,10,68,10,-49,
  8,-124,10,-49,8,-124,10,-49,8,-124,80,55,1,65,55,2,
  65,-17,0,47,10,10,68,1,-81,-1,88,27,49,63,-15,28,
  27,119,1,4,-122,112,26,24,3,-17,-1,3,24,47,-11,12,
  66,-23,47,9,92,27,49,94,26,113,9,63,-13,74,9,23,
  2,66,-23,47,9,92,27,49,94,26,113,9,63,-13,31,1,
  67,-33,0,39,6,94,75,68,57,63,-11,56,0,

  // Model 4 bwt2 -2
  17,0,1,0,27,27,2,3,5,8,12,0,0,95,1,52,
  60,56,0,

  // Model 5 bwt2 post -2
  111,0,1,0,27,27,0,0,-17,-1,39,4,96,9,63,95,
  10,68,10,-49,8,-124,10,-49,8,-124,10,-49,8,-124,80,55,
  1,65,55,2,65,-17,0,47,10,10,68,1,-81,-1,88,27,
  49,63,-15,28,27,119,1,4,-122,112,26,24,3,-17,-1,3,
  24,47,-11,12,66,-23,47,9,92,27,49,94,26,113,9,63,
  -13,74,9,23,2,66,-23,47,9,92,27,49,94,26,113,9,
  63,-13,31,1,67,-33,0,39,6,94,75,68,57,63,-11,56,
  0,

  // Model 6 mid -3
  69,0,3,3,0,0,8,3,5,8,13,0,8,17,1,8,
  18,2,8,18,3,8,19,4,4,22,24,7,16,0,7,24,
  -1,0,17,104,74,4,95,1,59,112,10,25,59,112,10,25,
  59,112,10,25,59,112,10,25,59,112,10,25,59,10,59,112,
  25,69,-49,8,112,56,0,

  // Model 7 max -4
  -60,0,5,9,0,0,22,1,-96,3,5,8,13,1,8,16,
  2,8,18,3,8,19,4,8,19,5,8,20,6,4,22,24,
  3,17,8,19,9,3,13,3,13,3,13,3,14,7,16,0,
  15,24,-1,7,8,0,16,10,-1,6,0,15,16,24,0,9,
  8,17,32,-1,6,8,17,18,16,-1,9,16,19,32,-1,6,
  0,19,20,16,0,0,17,104,74,4,95,2,59,112,10,25,
  59,112,10,25,59,112,10,25,59,112,10,25,59,112,10,25,
  59,10,59,112,10,25,59,112,10,25,69,-73,32,-17,64,47,
  14,-25,91,47,10,25,60,26,48,-122,-105,20,112,63,9,70,
  -33,0,39,3,25,112,26,52,25,25,74,10,4,59,112,25,
  10,4,59,112,25,10,4,59,112,25,65,-113,-44,72,4,59,
  112,8,-113,-40,8,68,-81,60,60,25,69,-49,9,112,25,25,
  25,25,25,112,56,0,

  0,0};

int Predictor::predict() {
  switch(z.select) {
    case 1: {
      // 2 components

      // 0 ICM 16
      if (c8==1 || (c8&0xf0)==16)
        comp[0].c=find(comp[0].ht, 16+2, z.H(0)+16*c8);
      comp[0].cxt=comp[0].ht[comp[0].c+(hmap4&15)];
      p[0]=stretch(comp[0].cm(comp[0].cxt)>>8);

      // 1 ISSE 19 0
      {
        if (c8==1 || (c8&0xf0)==16)
          comp[1].c=find(comp[1].ht, 21, z.H(1)+16*c8);
        comp[1].cxt=comp[1].ht[comp[1].c+(hmap4&15)];
        int *wt=(int*)&comp[1].cm[comp[1].cxt*2];
        p[1]=clamp2k((wt[0]*p[0]+wt[1]*64)>>16);
      }
      return squash(p[1]);
    }
    case 2: {
      // 1 components

      // 0 ICM 7
      if (c8==1 || (c8&0xf0)==16)
        comp[0].c=find(comp[0].ht, 7+2, z.H(0)+16*c8);
      comp[0].cxt=comp[0].ht[comp[0].c+(hmap4&15)];
      p[0]=stretch(comp[0].cm(comp[0].cxt)>>8);
      return squash(p[0]);
    }
    case 3: {
      // 0 components
      return predict0();
    }
    case 4: {
      // 2 components

      // 0 ICM 5
      if (c8==1 || (c8&0xf0)==16)
        comp[0].c=find(comp[0].ht, 5+2, z.H(0)+16*c8);
      comp[0].cxt=comp[0].ht[comp[0].c+(hmap4&15)];
      p[0]=stretch(comp[0].cm(comp[0].cxt)>>8);

      // 1 ISSE 12 0
      {
        if (c8==1 || (c8&0xf0)==16)
          comp[1].c=find(comp[1].ht, 14, z.H(1)+16*c8);
        comp[1].cxt=comp[1].ht[comp[1].c+(hmap4&15)];
        int *wt=(int*)&comp[1].cm[comp[1].cxt*2];
        p[1]=clamp2k((wt[0]*p[0]+wt[1]*64)>>16);
      }
      return squash(p[1]);
    }
    case 5: {
      // 0 components
      return predict0();
    }
    case 6: {
      // 8 components

      // 0 ICM 5
      if (c8==1 || (c8&0xf0)==16)
        comp[0].c=find(comp[0].ht, 5+2, z.H(0)+16*c8);
      comp[0].cxt=comp[0].ht[comp[0].c+(hmap4&15)];
      p[0]=stretch(comp[0].cm(comp[0].cxt)>>8);

      // 1 ISSE 13 0
      {
        if (c8==1 || (c8&0xf0)==16)
          comp[1].c=find(comp[1].ht, 15, z.H(1)+16*c8);
        comp[1].cxt=comp[1].ht[comp[1].c+(hmap4&15)];
        int *wt=(int*)&comp[1].cm[comp[1].cxt*2];
        p[1]=clamp2k((wt[0]*p[0]+wt[1]*64)>>16);
      }

      // 2 ISSE 17 1
      {
        if (c8==1 || (c8&0xf0)==16)
          comp[2].c=find(comp[2].ht, 19, z.H(2)+16*c8);
        comp[2].cxt=comp[2].ht[comp[2].c+(hmap4&15)];
        int *wt=(int*)&comp[2].cm[comp[2].cxt*2];
        p[2]=clamp2k((wt[0]*p[1]+wt[1]*64)>>16);
      }

      // 3 ISSE 18 2
      {
        if (c8==1 || (c8&0xf0)==16)
          comp[3].c=find(comp[3].ht, 20, z.H(3)+16*c8);
        comp[3].cxt=comp[3].ht[comp[3].c+(hmap4&15)];
        int *wt=(int*)&comp[3].cm[comp[3].cxt*2];
        p[3]=clamp2k((wt[0]*p[2]+wt[1]*64)>>16);
      }

      // 4 ISSE 18 3
      {
        if (c8==1 || (c8&0xf0)==16)
          comp[4].c=find(comp[4].ht, 20, z.H(4)+16*c8);
        comp[4].cxt=comp[4].ht[comp[4].c+(hmap4&15)];
        int *wt=(int*)&comp[4].cm[comp[4].cxt*2];
        p[4]=clamp2k((wt[0]*p[3]+wt[1]*64)>>16);
      }

      // 5 ISSE 19 4
      {
        if (c8==1 || (c8&0xf0)==16)
          comp[5].c=find(comp[5].ht, 21, z.H(5)+16*c8);
        comp[5].cxt=comp[5].ht[comp[5].c+(hmap4&15)];
        int *wt=(int*)&comp[5].cm[comp[5].cxt*2];
        p[5]=clamp2k((wt[0]*p[4]+wt[1]*64)>>16);
      }

      // 6 MATCH 22 24
      if (comp[6].a==0) p[6]=0;
      else {
        comp[6].c=comp[6].ht((comp[6].limit>>3)
           -comp[6].b)>>(7-(comp[6].limit&7))&1;
        p[6]=stretch(comp[6].cxt*(comp[6].c*-2+1)&32767);
      }

      // 7 MIX 16 0 7 24 255
      {
        comp[7].cxt=z.H(7)+(c8&255);
        comp[7].cxt=(comp[7].cxt&(comp[7].c-1))*7;
        int* wt=(int*)&comp[7].cm[comp[7].cxt];
        p[7]=(wt[0]>>8)*p[0];
        p[7]+=(wt[1]>>8)*p[1];
        p[7]+=(wt[2]>>8)*p[2];
        p[7]+=(wt[3]>>8)*p[3];
        p[7]+=(wt[4]>>8)*p[4];
        p[7]+=(wt[5]>>8)*p[5];
        p[7]+=(wt[6]>>8)*p[6];
        p[7]=clamp2k(p[7]>>8);
      }
      return squash(p[7]);
    }
    case 7: {
      // 22 components

      // 0 CONST 160

      // 1 ICM 5
      if (c8==1 || (c8&0xf0)==16)
        comp[1].c=find(comp[1].ht, 5+2, z.H(1)+16*c8);
      comp[1].cxt=comp[1].ht[comp[1].c+(hmap4&15)];
      p[1]=stretch(comp[1].cm(comp[1].cxt)>>8);

      // 2 ISSE 13 1
      {
        if (c8==1 || (c8&0xf0)==16)
          comp[2].c=find(comp[2].ht, 15, z.H(2)+16*c8);
        comp[2].cxt=comp[2].ht[comp[2].c+(hmap4&15)];
        int *wt=(int*)&comp[2].cm[comp[2].cxt*2];
        p[2]=clamp2k((wt[0]*p[1]+wt[1]*64)>>16);
      }

      // 3 ISSE 16 2
      {
        if (c8==1 || (c8&0xf0)==16)
          comp[3].c=find(comp[3].ht, 18, z.H(3)+16*c8);
        comp[3].cxt=comp[3].ht[comp[3].c+(hmap4&15)];
        int *wt=(int*)&comp[3].cm[comp[3].cxt*2];
        p[3]=clamp2k((wt[0]*p[2]+wt[1]*64)>>16);
      }

      // 4 ISSE 18 3
      {
        if (c8==1 || (c8&0xf0)==16)
          comp[4].c=find(comp[4].ht, 20, z.H(4)+16*c8);
        comp[4].cxt=comp[4].ht[comp[4].c+(hmap4&15)];
        int *wt=(int*)&comp[4].cm[comp[4].cxt*2];
        p[4]=clamp2k((wt[0]*p[3]+wt[1]*64)>>16);
      }

      // 5 ISSE 19 4
      {
        if (c8==1 || (c8&0xf0)==16)
          comp[5].c=find(comp[5].ht, 21, z.H(5)+16*c8);
        comp[5].cxt=comp[5].ht[comp[5].c+(hmap4&15)];
        int *wt=(int*)&comp[5].cm[comp[5].cxt*2];
        p[5]=clamp2k((wt[0]*p[4]+wt[1]*64)>>16);
      }

      // 6 ISSE 19 5
      {
        if (c8==1 || (c8&0xf0)==16)
          comp[6].c=find(comp[6].ht, 21, z.H(6)+16*c8);
        comp[6].cxt=comp[6].ht[comp[6].c+(hmap4&15)];
        int *wt=(int*)&comp[6].cm[comp[6].cxt*2];
        p[6]=clamp2k((wt[0]*p[5]+wt[1]*64)>>16);
      }

      // 7 ISSE 20 6
      {
        if (c8==1 || (c8&0xf0)==16)
          comp[7].c=find(comp[7].ht, 22, z.H(7)+16*c8);
        comp[7].cxt=comp[7].ht[comp[7].c+(hmap4&15)];
        int *wt=(int*)&comp[7].cm[comp[7].cxt*2];
        p[7]=clamp2k((wt[0]*p[6]+wt[1]*64)>>16);
      }

      // 8 MATCH 22 24
      if (comp[8].a==0) p[8]=0;
      else {
        comp[8].c=comp[8].ht((comp[8].limit>>3)
           -comp[8].b)>>(7-(comp[8].limit&7))&1;
        p[8]=stretch(comp[8].cxt*(comp[8].c*-2+1)&32767);
      }

      // 9 ICM 17
      if (c8==1 || (c8&0xf0)==16)
        comp[9].c=find(comp[9].ht, 17+2, z.H(9)+16*c8);
      comp[9].cxt=comp[9].ht[comp[9].c+(hmap4&15)];
      p[9]=stretch(comp[9].cm(comp[9].cxt)>>8);

      // 10 ISSE 19 9
      {
        if (c8==1 || (c8&0xf0)==16)
          comp[10].c=find(comp[10].ht, 21, z.H(10)+16*c8);
        comp[10].cxt=comp[10].ht[comp[10].c+(hmap4&15)];
        int *wt=(int*)&comp[10].cm[comp[10].cxt*2];
        p[10]=clamp2k((wt[0]*p[9]+wt[1]*64)>>16);
      }

      // 11 ICM 13
      if (c8==1 || (c8&0xf0)==16)
        comp[11].c=find(comp[11].ht, 13+2, z.H(11)+16*c8);
      comp[11].cxt=comp[11].ht[comp[11].c+(hmap4&15)];
      p[11]=stretch(comp[11].cm(comp[11].cxt)>>8);

      // 12 ICM 13
      if (c8==1 || (c8&0xf0)==16)
        comp[12].c=find(comp[12].ht, 13+2, z.H(12)+16*c8);
      comp[12].cxt=comp[12].ht[comp[12].c+(hmap4&15)];
      p[12]=stretch(comp[12].cm(comp[12].cxt)>>8);

      // 13 ICM 13
      if (c8==1 || (c8&0xf0)==16)
        comp[13].c=find(comp[13].ht, 13+2, z.H(13)+16*c8);
      comp[13].cxt=comp[13].ht[comp[13].c+(hmap4&15)];
      p[13]=stretch(comp[13].cm(comp[13].cxt)>>8);

      // 14 ICM 14
      if (c8==1 || (c8&0xf0)==16)
        comp[14].c=find(comp[14].ht, 14+2, z.H(14)+16*c8);
      comp[14].cxt=comp[14].ht[comp[14].c+(hmap4&15)];
      p[14]=stretch(comp[14].cm(comp[14].cxt)>>8);

      // 15 MIX 16 0 15 24 255
      {
        comp[15].cxt=z.H(15)+(c8&255);
        comp[15].cxt=(comp[15].cxt&(comp[15].c-1))*15;
        int* wt=(int*)&comp[15].cm[comp[15].cxt];
        p[15]=(wt[0]>>8)*p[0];
        p[15]+=(wt[1]>>8)*p[1];
        p[15]+=(wt[2]>>8)*p[2];
        p[15]+=(wt[3]>>8)*p[3];
        p[15]+=(wt[4]>>8)*p[4];
        p[15]+=(wt[5]>>8)*p[5];
        p[15]+=(wt[6]>>8)*p[6];
        p[15]+=(wt[7]>>8)*p[7];
        p[15]+=(wt[8]>>8)*p[8];
        p[15]+=(wt[9]>>8)*p[9];
        p[15]+=(wt[10]>>8)*p[10];
        p[15]+=(wt[11]>>8)*p[11];
        p[15]+=(wt[12]>>8)*p[12];
        p[15]+=(wt[13]>>8)*p[13];
        p[15]+=(wt[14]>>8)*p[14];
        p[15]=clamp2k(p[15]>>8);
      }

      // 16 MIX 8 0 16 10 255
      {
        comp[16].cxt=z.H(16)+(c8&255);
        comp[16].cxt=(comp[16].cxt&(comp[16].c-1))*16;
        int* wt=(int*)&comp[16].cm[comp[16].cxt];
        p[16]=(wt[0]>>8)*p[0];
        p[16]+=(wt[1]>>8)*p[1];
        p[16]+=(wt[2]>>8)*p[2];
        p[16]+=(wt[3]>>8)*p[3];
        p[16]+=(wt[4]>>8)*p[4];
        p[16]+=(wt[5]>>8)*p[5];
        p[16]+=(wt[6]>>8)*p[6];
        p[16]+=(wt[7]>>8)*p[7];
        p[16]+=(wt[8]>>8)*p[8];
        p[16]+=(wt[9]>>8)*p[9];
        p[16]+=(wt[10]>>8)*p[10];
        p[16]+=(wt[11]>>8)*p[11];
        p[16]+=(wt[12]>>8)*p[12];
        p[16]+=(wt[13]>>8)*p[13];
        p[16]+=(wt[14]>>8)*p[14];
        p[16]+=(wt[15]>>8)*p[15];
        p[16]=clamp2k(p[16]>>8);
      }

      // 17 MIX2 0 15 16 24 0
      {
        comp[17].cxt=((z.H(17)+(c8&0))&(comp[17].c-1));
        int w=comp[17].a16[comp[17].cxt];
        p[17]=(w*p[15]+(65536-w)*p[16])>>16;
      }

      // 18 SSE 8 17 32 255
      {
        comp[18].cxt=(z.H(18)+c8)*32;
        int pq=p[17]+992;
        if (pq<0) pq=0;
        if (pq>1983) pq=1983;
        int wt=pq&63;
        pq>>=6;
        comp[18].cxt+=pq;
        p[18]=stretch(((comp[18].cm(comp[18].cxt)>>10)*(64-wt)
           +(comp[18].cm(comp[18].cxt+1)>>10)*wt)>>13);
        comp[18].cxt+=wt>>5;
      }

      // 19 MIX2 8 17 18 16 255
      {
        comp[19].cxt=((z.H(19)+(c8&255))&(comp[19].c-1));
        int w=comp[19].a16[comp[19].cxt];
        p[19]=(w*p[17]+(65536-w)*p[18])>>16;
      }

      // 20 SSE 16 19 32 255
      {
        comp[20].cxt=(z.H(20)+c8)*32;
        int pq=p[19]+992;
        if (pq<0) pq=0;
        if (pq>1983) pq=1983;
        int wt=pq&63;
        pq>>=6;
        comp[20].cxt+=pq;
        p[20]=stretch(((comp[20].cm(comp[20].cxt)>>10)*(64-wt)
           +(comp[20].cm(comp[20].cxt+1)>>10)*wt)>>13);
        comp[20].cxt+=wt>>5;
      }

      // 21 MIX2 0 19 20 16 0
      {
        comp[21].cxt=((z.H(21)+(c8&0))&(comp[21].c-1));
        int w=comp[21].a16[comp[21].cxt];
        p[21]=(w*p[19]+(65536-w)*p[20])>>16;
      }
      return squash(p[21]);
    }
    default: return predict0();
  }
}

void Predictor::update(int y) {
  switch(z.select) {
    case 1: {
      // 2 components

      // 0 ICM 16
      {
        comp[0].ht[comp[0].c+(hmap4&15)]=
            st.next(comp[0].ht[comp[0].c+(hmap4&15)], y);
        U32& pn=comp[0].cm(comp[0].cxt);
        pn+=int(y*32767-(pn>>8))>>2;
      }

      // 1 ISSE 19 0
      {
        int err=y*32767-squash(p[1]);
        int *wt=(int*)&comp[1].cm[comp[1].cxt*2];
        wt[0]=clamp512k(wt[0]+((err*p[0]+(1<<12))>>13));
        wt[1]=clamp512k(wt[1]+((err+16)>>5));
        comp[1].ht[comp[1].c+(hmap4&15)]=st.next(comp[1].cxt, y);
      }
      break;
    }
    case 2: {
      // 1 components

      // 0 ICM 7
      {
        comp[0].ht[comp[0].c+(hmap4&15)]=
            st.next(comp[0].ht[comp[0].c+(hmap4&15)], y);
        U32& pn=comp[0].cm(comp[0].cxt);
        pn+=int(y*32767-(pn>>8))>>2;
      }
      break;
    }
    case 3: {
      // 0 components
      break;
    }
    case 4: {
      // 2 components

      // 0 ICM 5
      {
        comp[0].ht[comp[0].c+(hmap4&15)]=
            st.next(comp[0].ht[comp[0].c+(hmap4&15)], y);
        U32& pn=comp[0].cm(comp[0].cxt);
        pn+=int(y*32767-(pn>>8))>>2;
      }

      // 1 ISSE 12 0
      {
        int err=y*32767-squash(p[1]);
        int *wt=(int*)&comp[1].cm[comp[1].cxt*2];
        wt[0]=clamp512k(wt[0]+((err*p[0]+(1<<12))>>13));
        wt[1]=clamp512k(wt[1]+((err+16)>>5));
        comp[1].ht[comp[1].c+(hmap4&15)]=st.next(comp[1].cxt, y);
      }
      break;
    }
    case 5: {
      // 0 components
      break;
    }
    case 6: {
      // 8 components

      // 0 ICM 5
      {
        comp[0].ht[comp[0].c+(hmap4&15)]=
            st.next(comp[0].ht[comp[0].c+(hmap4&15)], y);
        U32& pn=comp[0].cm(comp[0].cxt);
        pn+=int(y*32767-(pn>>8))>>2;
      }

      // 1 ISSE 13 0
      {
        int err=y*32767-squash(p[1]);
        int *wt=(int*)&comp[1].cm[comp[1].cxt*2];
        wt[0]=clamp512k(wt[0]+((err*p[0]+(1<<12))>>13));
        wt[1]=clamp512k(wt[1]+((err+16)>>5));
        comp[1].ht[comp[1].c+(hmap4&15)]=st.next(comp[1].cxt, y);
      }

      // 2 ISSE 17 1
      {
        int err=y*32767-squash(p[2]);
        int *wt=(int*)&comp[2].cm[comp[2].cxt*2];
        wt[0]=clamp512k(wt[0]+((err*p[1]+(1<<12))>>13));
        wt[1]=clamp512k(wt[1]+((err+16)>>5));
        comp[2].ht[comp[2].c+(hmap4&15)]=st.next(comp[2].cxt, y);
      }

      // 3 ISSE 18 2
      {
        int err=y*32767-squash(p[3]);
        int *wt=(int*)&comp[3].cm[comp[3].cxt*2];
        wt[0]=clamp512k(wt[0]+((err*p[2]+(1<<12))>>13));
        wt[1]=clamp512k(wt[1]+((err+16)>>5));
        comp[3].ht[comp[3].c+(hmap4&15)]=st.next(comp[3].cxt, y);
      }

      // 4 ISSE 18 3
      {
        int err=y*32767-squash(p[4]);
        int *wt=(int*)&comp[4].cm[comp[4].cxt*2];
        wt[0]=clamp512k(wt[0]+((err*p[3]+(1<<12))>>13));
        wt[1]=clamp512k(wt[1]+((err+16)>>5));
        comp[4].ht[comp[4].c+(hmap4&15)]=st.next(comp[4].cxt, y);
      }

      // 5 ISSE 19 4
      {
        int err=y*32767-squash(p[5]);
        int *wt=(int*)&comp[5].cm[comp[5].cxt*2];
        wt[0]=clamp512k(wt[0]+((err*p[4]+(1<<12))>>13));
        wt[1]=clamp512k(wt[1]+((err+16)>>5));
        comp[5].ht[comp[5].c+(hmap4&15)]=st.next(comp[5].cxt, y);
      }

      // 6 MATCH 22 24
      {
        if (comp[6].c!=y) comp[6].a=0;
        comp[6].ht(comp[6].limit>>3)+=comp[6].ht(comp[6].limit>>3)+y;
        if ((++comp[6].limit&7)==0) {
          int pos=comp[6].limit>>3;
          if (comp[6].a==0) {
            comp[6].b=pos-comp[6].cm(z.H(6));
            if (comp[6].b&(comp[6].ht.size()-1))
              while (comp[6].a<255 && comp[6].ht(pos-comp[6].a-1)
                     ==comp[6].ht(pos-comp[6].a-comp[6].b-1))
                ++comp[6].a;
          }
          else comp[6].a+=comp[6].a<255;
          comp[6].cm(z.H(6))=pos;
          if (comp[6].a>0) comp[6].cxt=2048/comp[6].a;
        }
      }

      // 7 MIX 16 0 7 24 255
      {
        int err=(y*32767-squash(p[7]))*24>>4;
        int* wt=(int*)&comp[7].cm[comp[7].cxt];
          wt[0]=clamp512k(wt[0]+((err*p[0]+(1<<12))>>13));
          wt[1]=clamp512k(wt[1]+((err*p[1]+(1<<12))>>13));
          wt[2]=clamp512k(wt[2]+((err*p[2]+(1<<12))>>13));
          wt[3]=clamp512k(wt[3]+((err*p[3]+(1<<12))>>13));
          wt[4]=clamp512k(wt[4]+((err*p[4]+(1<<12))>>13));
          wt[5]=clamp512k(wt[5]+((err*p[5]+(1<<12))>>13));
          wt[6]=clamp512k(wt[6]+((err*p[6]+(1<<12))>>13));
      }
      break;
    }
    case 7: {
      // 22 components

      // 0 CONST 160

      // 1 ICM 5
      {
        comp[1].ht[comp[1].c+(hmap4&15)]=
            st.next(comp[1].ht[comp[1].c+(hmap4&15)], y);
        U32& pn=comp[1].cm(comp[1].cxt);
        pn+=int(y*32767-(pn>>8))>>2;
      }

      // 2 ISSE 13 1
      {
        int err=y*32767-squash(p[2]);
        int *wt=(int*)&comp[2].cm[comp[2].cxt*2];
        wt[0]=clamp512k(wt[0]+((err*p[1]+(1<<12))>>13));
        wt[1]=clamp512k(wt[1]+((err+16)>>5));
        comp[2].ht[comp[2].c+(hmap4&15)]=st.next(comp[2].cxt, y);
      }

      // 3 ISSE 16 2
      {
        int err=y*32767-squash(p[3]);
        int *wt=(int*)&comp[3].cm[comp[3].cxt*2];
        wt[0]=clamp512k(wt[0]+((err*p[2]+(1<<12))>>13));
        wt[1]=clamp512k(wt[1]+((err+16)>>5));
        comp[3].ht[comp[3].c+(hmap4&15)]=st.next(comp[3].cxt, y);
      }

      // 4 ISSE 18 3
      {
        int err=y*32767-squash(p[4]);
        int *wt=(int*)&comp[4].cm[comp[4].cxt*2];
        wt[0]=clamp512k(wt[0]+((err*p[3]+(1<<12))>>13));
        wt[1]=clamp512k(wt[1]+((err+16)>>5));
        comp[4].ht[comp[4].c+(hmap4&15)]=st.next(comp[4].cxt, y);
      }

      // 5 ISSE 19 4
      {
        int err=y*32767-squash(p[5]);
        int *wt=(int*)&comp[5].cm[comp[5].cxt*2];
        wt[0]=clamp512k(wt[0]+((err*p[4]+(1<<12))>>13));
        wt[1]=clamp512k(wt[1]+((err+16)>>5));
        comp[5].ht[comp[5].c+(hmap4&15)]=st.next(comp[5].cxt, y);
      }

      // 6 ISSE 19 5
      {
        int err=y*32767-squash(p[6]);
        int *wt=(int*)&comp[6].cm[comp[6].cxt*2];
        wt[0]=clamp512k(wt[0]+((err*p[5]+(1<<12))>>13));
        wt[1]=clamp512k(wt[1]+((err+16)>>5));
        comp[6].ht[comp[6].c+(hmap4&15)]=st.next(comp[6].cxt, y);
      }

      // 7 ISSE 20 6
      {
        int err=y*32767-squash(p[7]);
        int *wt=(int*)&comp[7].cm[comp[7].cxt*2];
        wt[0]=clamp512k(wt[0]+((err*p[6]+(1<<12))>>13));
        wt[1]=clamp512k(wt[1]+((err+16)>>5));
        comp[7].ht[comp[7].c+(hmap4&15)]=st.next(comp[7].cxt, y);
      }

      // 8 MATCH 22 24
      {
        if (comp[8].c!=y) comp[8].a=0;
        comp[8].ht(comp[8].limit>>3)+=comp[8].ht(comp[8].limit>>3)+y;
        if ((++comp[8].limit&7)==0) {
          int pos=comp[8].limit>>3;
          if (comp[8].a==0) {
            comp[8].b=pos-comp[8].cm(z.H(8));
            if (comp[8].b&(comp[8].ht.size()-1))
              while (comp[8].a<255 && comp[8].ht(pos-comp[8].a-1)
                     ==comp[8].ht(pos-comp[8].a-comp[8].b-1))
                ++comp[8].a;
          }
          else comp[8].a+=comp[8].a<255;
          comp[8].cm(z.H(8))=pos;
          if (comp[8].a>0) comp[8].cxt=2048/comp[8].a;
        }
      }

      // 9 ICM 17
      {
        comp[9].ht[comp[9].c+(hmap4&15)]=
            st.next(comp[9].ht[comp[9].c+(hmap4&15)], y);
        U32& pn=comp[9].cm(comp[9].cxt);
        pn+=int(y*32767-(pn>>8))>>2;
      }

      // 10 ISSE 19 9
      {
        int err=y*32767-squash(p[10]);
        int *wt=(int*)&comp[10].cm[comp[10].cxt*2];
        wt[0]=clamp512k(wt[0]+((err*p[9]+(1<<12))>>13));
        wt[1]=clamp512k(wt[1]+((err+16)>>5));
        comp[10].ht[comp[10].c+(hmap4&15)]=st.next(comp[10].cxt, y);
      }

      // 11 ICM 13
      {
        comp[11].ht[comp[11].c+(hmap4&15)]=
            st.next(comp[11].ht[comp[11].c+(hmap4&15)], y);
        U32& pn=comp[11].cm(comp[11].cxt);
        pn+=int(y*32767-(pn>>8))>>2;
      }

      // 12 ICM 13
      {
        comp[12].ht[comp[12].c+(hmap4&15)]=
            st.next(comp[12].ht[comp[12].c+(hmap4&15)], y);
        U32& pn=comp[12].cm(comp[12].cxt);
        pn+=int(y*32767-(pn>>8))>>2;
      }

      // 13 ICM 13
      {
        comp[13].ht[comp[13].c+(hmap4&15)]=
            st.next(comp[13].ht[comp[13].c+(hmap4&15)], y);
        U32& pn=comp[13].cm(comp[13].cxt);
        pn+=int(y*32767-(pn>>8))>>2;
      }

      // 14 ICM 14
      {
        comp[14].ht[comp[14].c+(hmap4&15)]=
            st.next(comp[14].ht[comp[14].c+(hmap4&15)], y);
        U32& pn=comp[14].cm(comp[14].cxt);
        pn+=int(y*32767-(pn>>8))>>2;
      }

      // 15 MIX 16 0 15 24 255
      {
        int err=(y*32767-squash(p[15]))*24>>4;
        int* wt=(int*)&comp[15].cm[comp[15].cxt];
          wt[0]=clamp512k(wt[0]+((err*p[0]+(1<<12))>>13));
          wt[1]=clamp512k(wt[1]+((err*p[1]+(1<<12))>>13));
          wt[2]=clamp512k(wt[2]+((err*p[2]+(1<<12))>>13));
          wt[3]=clamp512k(wt[3]+((err*p[3]+(1<<12))>>13));
          wt[4]=clamp512k(wt[4]+((err*p[4]+(1<<12))>>13));
          wt[5]=clamp512k(wt[5]+((err*p[5]+(1<<12))>>13));
          wt[6]=clamp512k(wt[6]+((err*p[6]+(1<<12))>>13));
          wt[7]=clamp512k(wt[7]+((err*p[7]+(1<<12))>>13));
          wt[8]=clamp512k(wt[8]+((err*p[8]+(1<<12))>>13));
          wt[9]=clamp512k(wt[9]+((err*p[9]+(1<<12))>>13));
          wt[10]=clamp512k(wt[10]+((err*p[10]+(1<<12))>>13));
          wt[11]=clamp512k(wt[11]+((err*p[11]+(1<<12))>>13));
          wt[12]=clamp512k(wt[12]+((err*p[12]+(1<<12))>>13));
          wt[13]=clamp512k(wt[13]+((err*p[13]+(1<<12))>>13));
          wt[14]=clamp512k(wt[14]+((err*p[14]+(1<<12))>>13));
      }

      // 16 MIX 8 0 16 10 255
      {
        int err=(y*32767-squash(p[16]))*10>>4;
        int* wt=(int*)&comp[16].cm[comp[16].cxt];
          wt[0]=clamp512k(wt[0]+((err*p[0]+(1<<12))>>13));
          wt[1]=clamp512k(wt[1]+((err*p[1]+(1<<12))>>13));
          wt[2]=clamp512k(wt[2]+((err*p[2]+(1<<12))>>13));
          wt[3]=clamp512k(wt[3]+((err*p[3]+(1<<12))>>13));
          wt[4]=clamp512k(wt[4]+((err*p[4]+(1<<12))>>13));
          wt[5]=clamp512k(wt[5]+((err*p[5]+(1<<12))>>13));
          wt[6]=clamp512k(wt[6]+((err*p[6]+(1<<12))>>13));
          wt[7]=clamp512k(wt[7]+((err*p[7]+(1<<12))>>13));
          wt[8]=clamp512k(wt[8]+((err*p[8]+(1<<12))>>13));
          wt[9]=clamp512k(wt[9]+((err*p[9]+(1<<12))>>13));
          wt[10]=clamp512k(wt[10]+((err*p[10]+(1<<12))>>13));
          wt[11]=clamp512k(wt[11]+((err*p[11]+(1<<12))>>13));
          wt[12]=clamp512k(wt[12]+((err*p[12]+(1<<12))>>13));
          wt[13]=clamp512k(wt[13]+((err*p[13]+(1<<12))>>13));
          wt[14]=clamp512k(wt[14]+((err*p[14]+(1<<12))>>13));
          wt[15]=clamp512k(wt[15]+((err*p[15]+(1<<12))>>13));
      }

      // 17 MIX2 0 15 16 24 0
      {
        int err=(y*32767-squash(p[17]))*24>>5;
        int w=comp[17].a16[comp[17].cxt];
        w+=(err*(p[15]-p[16])+(1<<12))>>13;
        if (w<0) w=0;
        if (w>65535) w=65535;
        comp[17].a16[comp[17].cxt]=w;
      }

      // 18 SSE 8 17 32 255
      train(comp[18], y);

      // 19 MIX2 8 17 18 16 255
      {
        int err=(y*32767-squash(p[19]))*16>>5;
        int w=comp[19].a16[comp[19].cxt];
        w+=(err*(p[17]-p[18])+(1<<12))>>13;
        if (w<0) w=0;
        if (w>65535) w=65535;
        comp[19].a16[comp[19].cxt]=w;
      }

      // 20 SSE 16 19 32 255
      train(comp[20], y);

      // 21 MIX2 0 19 20 16 0
      {
        int err=(y*32767-squash(p[21]))*16>>5;
        int w=comp[21].a16[comp[21].cxt];
        w+=(err*(p[19]-p[20])+(1<<12))>>13;
        if (w<0) w=0;
        if (w>65535) w=65535;
        comp[21].a16[comp[21].cxt]=w;
      }
      break;
    }
    default: return update0(y);
  }
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

void ZPAQL::run(U32 input) {
  switch(select) {
    case 1: {
      a = input;
      m(b) = a;
      a = 0;
      d = 0;
      a = (a+m(b)+512)*773;
      --b;
      a = (a+m(b)+512)*773;
      h(d) = a;
      ++d;
      --b;
      a = (a+m(b)+512)*773;
      --b;
      a = (a+m(b)+512)*773;
      h(d) = a;
      return;
      break;
    }
    case 2: {
      a = input;
      f = (a == c);
      c = a;
      if (!f) goto L200007;
      ++b;
      goto L200008;
L200007:
      b = 0;
L200008:
      a = b;
      h(d) = 0;
      h(d) = (h(d)+a+512)*773;
      return;
      break;
    }
    case 3: {
      a = input;
      f = (a > U32(255));
      if (f) goto L300052;
      c = a;
      a = d;
      f = (a == U32(0));
      if (!f) goto L300016;
      d = c;
      ++d;
      m(b) = c;
      ++b;
      goto L300050;
L300016:
      a = d;
      --a;
      f = (a > U32(255));
      if (f) goto L300038;
      f = (a == c);
      if (!f) goto L300032;
      a += 255;
      ++a;
      ++a;
      d = a;
      goto L300034;
L300032:
      d = c;
      ++d;
L300034:
      m(b) = c;
      ++b;
      goto L300050;
L300038:
      --d;
L300039:
      a = c;
      f = (a > U32(0));
      if (!f) goto L300049;
      m(b) = d;
      ++b;
      --c;
      goto L300039;
L300049:
      d = 0;
L300050:
      goto L300147;
L300052:
      --b;
      a = m(b);
      --b;
      a <<= (8&31);
      a += m(b);
      --b;
      a <<= (8&31);
      a += m(b);
      --b;
      a <<= (8&31);
      a += m(b);
      c = a;
      r[1] = a;
      a = b;
      r[2] = a;
L300072:
      a = b;
      f = (a > U32(0));
      if (!f) goto L300087;
      --b;
      a = m(b);
      ++a;
      a &= 255;
      d = a;
      d = ~d;
      ++h(d);
      goto L300072;
L300087:
      d = 0;
      d = ~d;
      h(d) = 1;
      a = 0;
L300092:
      a += h(d);
      h(d) = a;
      --d;
      swap(d);
      a = ~a;
      f = (a > U32(255));
      a = ~a;
      swap(d);
      if (!f) goto L300092;
      b = 0;
L300104:
      a = c;
      f = (a > b);
      if (!f) goto L300117;
      d = m(b);
      d = ~d;
      ++h(d);
      d = h(d);
      --d;
      h(d) = b;
      ++b;
      goto L300104;
L300117:
      b = c;
      ++b;
      c = r[2];
L300121:
      a = c;
      f = (a > b);
      if (!f) goto L300134;
      d = m(b);
      d = ~d;
      ++h(d);
      d = h(d);
      --d;
      h(d) = b;
      ++b;
      goto L300121;
L300134:
      d = r[1];
L300136:
      a = d;
      f = (a == U32(0));
      if (f) goto L300147;
      d = h(d);
      b = d;
      a = m(b);
      if (output) output->put(a); if (sha1) sha1->put(a);
      goto L300136;
L300147:
      return;
      break;
    }
    case 4: {
      a = input;
      d = 1;
      h(d) = 0;
      h(d) = (h(d)+a+512)*773;
      return;
      break;
    }
    case 5: {
      a = input;
      f = (a > U32(255));
      if (f) goto L500008;
      m(b) = a;
      ++b;
      goto L500103;
L500008:
      --b;
      a = m(b);
      --b;
      a <<= (8&31);
      a += m(b);
      --b;
      a <<= (8&31);
      a += m(b);
      --b;
      a <<= (8&31);
      a += m(b);
      c = a;
      r[1] = a;
      a = b;
      r[2] = a;
L500028:
      a = b;
      f = (a > U32(0));
      if (!f) goto L500043;
      --b;
      a = m(b);
      ++a;
      a &= 255;
      d = a;
      d = ~d;
      ++h(d);
      goto L500028;
L500043:
      d = 0;
      d = ~d;
      h(d) = 1;
      a = 0;
L500048:
      a += h(d);
      h(d) = a;
      --d;
      swap(d);
      a = ~a;
      f = (a > U32(255));
      a = ~a;
      swap(d);
      if (!f) goto L500048;
      b = 0;
L500060:
      a = c;
      f = (a > b);
      if (!f) goto L500073;
      d = m(b);
      d = ~d;
      ++h(d);
      d = h(d);
      --d;
      h(d) = b;
      ++b;
      goto L500060;
L500073:
      b = c;
      ++b;
      c = r[2];
L500077:
      a = c;
      f = (a > b);
      if (!f) goto L500090;
      d = m(b);
      d = ~d;
      ++h(d);
      d = h(d);
      --d;
      h(d) = b;
      ++b;
      goto L500077;
L500090:
      d = r[1];
L500092:
      a = d;
      f = (a == U32(0));
      if (f) goto L500103;
      d = h(d);
      b = d;
      a = m(b);
      if (output) output->put(a); if (sha1) sha1->put(a);
      goto L500092;
L500103:
      return;
      break;
    }
    case 6: {
      a = input;
      ++c;
      m(c) = a;
      b = c;
      a = 0;
      d = 1;
      a = (a+m(b)+512)*773;
      h(d) = a;
      --b;
      ++d;
      a = (a+m(b)+512)*773;
      h(d) = a;
      --b;
      ++d;
      a = (a+m(b)+512)*773;
      h(d) = a;
      --b;
      ++d;
      a = (a+m(b)+512)*773;
      h(d) = a;
      --b;
      ++d;
      a = (a+m(b)+512)*773;
      h(d) = a;
      --b;
      ++d;
      a = (a+m(b)+512)*773;
      --b;
      a = (a+m(b)+512)*773;
      h(d) = a;
      ++d;
      a = m(c);
      a <<= (8&31);
      h(d) = a;
      return;
      break;
    }
    case 7: {
      a = input;
      ++c;
      m(c) = a;
      b = c;
      a = 0;
      d = 2;
      a = (a+m(b)+512)*773;
      h(d) = a;
      --b;
      ++d;
      a = (a+m(b)+512)*773;
      h(d) = a;
      --b;
      ++d;
      a = (a+m(b)+512)*773;
      h(d) = a;
      --b;
      ++d;
      a = (a+m(b)+512)*773;
      h(d) = a;
      --b;
      ++d;
      a = (a+m(b)+512)*773;
      h(d) = a;
      --b;
      ++d;
      a = (a+m(b)+512)*773;
      --b;
      a = (a+m(b)+512)*773;
      h(d) = a;
      --b;
      ++d;
      a = (a+m(b)+512)*773;
      h(d) = a;
      --b;
      ++d;
      a = m(c);
      a &= ~ 32;
      f = (a > U32(64));
      if (!f) goto L700057;
      f = (a < U32(91));
      if (!f) goto L700057;
      ++d;
      h(d) = (h(d)+a+512)*773;
      --d;
      swap(h(d));
      a += h(d);
      a *= 20;
      h(d) = a;
      goto L700066;
L700057:
      a = h(d);
      f = (a == U32(0));
      if (f) goto L700065;
      ++d;
      h(d) = a;
      --d;
L700065:
      h(d) = 0;
L700066:
      ++d;
      ++d;
      b = c;
      --b;
      a = 0;
      a = (a+m(b)+512)*773;
      h(d) = a;
      ++d;
      --b;
      a = 0;
      a = (a+m(b)+512)*773;
      h(d) = a;
      ++d;
      --b;
      a = 0;
      a = (a+m(b)+512)*773;
      h(d) = a;
      ++d;
      a = b;
      a -= 212;
      b = a;
      a = 0;
      a = (a+m(b)+512)*773;
      h(d) = a;
      swap(b);
      a -= 216;
      swap(b);
      a = m(b);
      a &= 60;
      h(d) = (h(d)+a+512)*773;
      ++d;
      a = m(c);
      a <<= (9&31);
      h(d) = a;
      ++d;
      ++d;
      ++d;
      ++d;
      ++d;
      h(d) = a;
      return;
      break;
    }
    default: run0(input);
  }
}
}

#endif // ifndef NOOPT
