/* zpaq.cpp v3.00 - A parallel, self optimizing, configurable ZPAQ compressor

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

zpaq is a ZPAQ file compressor and archiver. zpaq 3.00 combines the
features of zpaq 2.04 (configuration file compression and development)
and zp 1.03 (multithreading and fast BWT methods). It supports JIT
self optimization if an external C++ compiler is available.

zpaq compresses, extracts, and lists archives in the ZPAQ level 1
format as described in http://mattmahoney.net/dc/zpaq.html
ZPAQ archives contain a description of the decoding algorithm,
which allows older versions of the program to read archives produced
using algorithms introduced in newer versions (and vice versa). zpaq
has 4 built in compression levels but allows you to specify others using
configuration (.cfg) files. zpaq includes tools for debugging
config files. Some config files are available at the above website.

Usage: zpaq [-options] command [arguments...]
Commands:
  c|a archive files...     Compress|append to archive.zpaq
  e|x archive [files...]   Extract to files or as saved without|with paths
  l archive                List contents
  b archive output [N[-N]...]  Append listed blocks to output.zpaq
  r [input [output]]       Run config file F.cfg (specified by -m)
  t [N...]                 Trace F.cfg with decimal/hex inputs
Options:
  -f   Force overwrite of output files
  -m1 ... -m4  Compress faster...smaller (default -m1)
  -mF[,N...]   Compress using F.cfg with up to 9 numeric arguments
  -bN  Compress in N MB blocks, -b0 = file, -bs = solid (default -b16)
  -v   Verbose
  -tN  Use N threads (default -t2)
  -p   Ignore/don't save paths
  -n   Ignore/don't save filenames
  -s   Ignore/don't save checksums
  -i   Don't save comments
  -h   Save locator tag. With r or t run HCOMP (default PCOMP)
  -j0 ... -j3  No JIT, JIT, keep source, exe (default -j1)
  -q   Don't test F.cfg postprocessor during compression

Command "a" compresses and appends to archive.zpaq or creates
it if it doesn't exist. The .zpaq extension is added automatically if
not specified. File names are saved in the archive as specified
on the command line. Existing files are not replaced. Rather, another
copy is appended.

Command "c" is the same as "-f a" overwriting archive.zpaq if it exists.

Command "x" extracts from archive.zpaq. Output files and directories are
created using the saved names. If no name is saved, then archive.zpaq
is decompressed to archive by dropping ".zpaq". Files are not clobbered
unless -f is specified. Otherwise it is an error if an output file exists.
If a list of files follow, then those files are created (and clobbered
implying -f) by extracting in the order they were saved.

Command "e" is the same as "-p x" extracting to the current directory
by default. It is exactly like "x" if a list of files follow.

Command "l" lists the contents of archive.zpaq showing files and
blocks. A block may contain a file, a part of a file, or multiple
files. Each file or segment shows the name (blank except for the
first segment of a file), comment (normally size, but may be absent),
compressed size, and first 32 bits of the 160 bit SHA1 checksum in hex
if present.

Blocks are independent so they can be compressed or
decompressed in parallel by separate threads at the same time.
The memory required to decompress each block is
shown. If multiple threads are running, then memory usage is is the
total of all blocks being processed at the same time. Larger blocks
usually improve compression but reduce speed by reducing the number
of threads that can run, and sometimes require more memory.

Command "b" extracts blocks from archive.zpaq and appends them to
output.zpaq. The blocks are listed as arguments in the form N or N-N.
For example "zpaq b in out 1-3 5" would append the first, second,
third, and fifth blocks of in.zpaq to out.zpaq. This is a faster way
to extract part of an archive than extracting all of it and deleting
the files you don't want. Blocks do not have to be extracted in order.
Reordering and concatenating blocks has the same effect as reordering
the data it represents. If the first segment of a block is not named
then the data is appended to the last named segment in an earlier block.

Command "r" runs the program in F.cfg specified by -m as a stand
alone program reading from input (default stdin) and writing to
output (default stdout). Normally F.cfg is used to specify a compression
algorithm. It contains two programs written in ZPAQL called HCOMP
and PCOMP. HCOMP is normally used to compute contexts for the
context mixing model and PCOMP (if present) post-processes the
output. The command runs PCOMP unless option -h specifies HCOMP.
Either program is run once for each byte of input. PCOMP is run
once more with EOF (-1) as input. The command is useful for testing
and debugging configuration files.

Command "t" traces F.cfg running it once for each numeric argument
which can be either a decimal or hex number. After each instruction
the register content is displayed in the same base (decimal or hex)
as the input. Hex numbers have a leading "x". When the program
halts, memory is dumped. -h selects the HCOMP section as with "r".
For example, "zpaq -mmin -h t 255 xff" would run the HCOMP section
of min.cfg twice with input 255 displaing the second run in hex.
The command is used for debugging configuration files.

Option -f allows the "x" command to overwrite existing files. When used
with "a", archive.zpaq is overwritten rather than appended. With
"e" or "x", output files are clobbered if they exist.

-m1 through -m4 select the compression level from fastest to smallest.
These require memory per thread as follows:

  Opt  Config     Memory
  ---  ------     ------
  -m1  -mbwtrle1  5x block size after rounding up to a power of 2.
  -m2  -mbwt2     5x block size after rounding up to a power of 2.
  -m3  -mmid      111 MB.
  -m4  -mmax      246 MB.

The default block size is 16 MB, so -m1 and -m2 use 80 MB per thread
by default.

Equivalent configurations are shown. Configuration files are available
from the above website. A .cfg extension is assumed.
Some can take arguments, separated by commas without spaces.
For example, -mmid,1 means to use mid.cfg with argument 1, which in
this case doubles memory usage.

Option -bN means to compress large files in blocks of size
N * 1000000 bytes. Blocks can be compressed and
decompressed in parallel in separate threads for better speed,
but small blocks make compression worse. For -m1 and -m2, larger
blocks use more memory, for example, -b100 would require 640 MB
memory per thread (because 100 rounds up to 128). Decompression
requires the same memory as compression. -b0 means don't
split files into blocks no matter how large. -bs means to pack
all files into one block. This improves compression for files of
similar type, but is slower because only one thread can be used.
Compression methods -m1 and -m2 require blocks not larger than
256 MB - 257 bytes, equivalent to -b268.435199
-b0, -bs, or a larger block size will automatically select this
value.

Option -v displays more output progress. It is only useful for
debugging. With "l" it displays the saved decompression algorithm
for each block in a format that can be saved to a .cfg file.

Option -t sets the number of threads. The default is equal to
the number of processors detected (for example -t2). Thus, a
higher number does not improve speed. A lower number will run
slower but can save memory during either compression or decompression
because memory usage is proportional to number of threads.

Option -p means to save filenames without a path or to ignore the
path when extracting. The result in either case is that files are
extracted to the current directory. Command "e" is equivalent
to "-p x".

Option -n means don't save filenames, or to ignore them when
extracting. The result in either case is that all of the data is
concatenated to a single file when extracting, and that the output
file name must be given or defaults by dropping the ".zpaq"
extension.

Option -i means don't save the file segment size as a commment.
This saves a few bytes and only affects the display when listing.

Option -s means don't save checksums. This saves 20 bytes, but
disables error detection during extraction.

Option -h with commands "c" or "a" means put a block header
locator tag at the beginning of the archive. This adds 13 bytes.
A tag is only needed if the block is to be appended to non-ZPAQ
data such as a self extracting archive stub. With commands "r"
or "t" the meaning is to run the HCOMP section rather than PCOMP.

Options -j0 through -j3 select whether to use JIT optimization
and save the generated code. Whenever zpaq reads a configuration
file or archive that does not use one of the built-in compression
levels -m1 through -m4, it will generate source code to build
an optimized version of itself, compile it with an external C++
compiler, link it to files prepared during installation, run the
new program with the same arguments, then delete this temporary
program. If the generated code does not compiler (for example,
no external C++ compiler is available), then zpaq will still
work but take about twice as long. Option -j0 says not to attempt
to compile. -j2 says not to delete the generated source code.
-j3 says not to delete the executable either.

Option -q says not to test the postprocessor when compressing
from a config file. Normally if a config file specifies pre/post
processing (like bwtrle1.cfg and bwt2.cfg), then zpaq will first
preprocess the input to a temporary file, save the postprocessing
code in the archive, and compress the temporary file. The
postprocessing code should restore the original program, but
might not if the code is incorrect. Normally, before compression,
zpaq will run the temporary file through the postprocessor and
compare the output checksum with the input checksum and raise
an error if they don't match. -q says not to do this check, which
can save time. Use it only if you are sure the code is correct.
Options -m1 and -m2 normally skip this check.


Installation:

To compile for Windows with MinGW:

  g++ -O3 zpaq.cpp libzpaq.cpp divsufsort.c -o zpaq

With Visual C++

  cl /O2 zpaq.cpp libzpaq.cpp divsufsort.c

In Linux, also use -lpthread -fopenmp
Other optimization options may be appropriate.

To enable JIT, The script zpaqopt (zpaqopt.bat in Windows) needs to be in
your PATH and configured along with an external C++ compiler and this
source code to enable acceleration. Otherwise, zpaq will still work but
won't be as fast when compressing with config files or decompressing archives
that were not compressed with one of the 4 built in levels.

To configure for acceleration, code needs to be prepared in advance
to link to the generated source code in a place where the script
can find it, along with the header file libzpaq.h. To produce
the two object files:

  g++ -O3 -c -DOPT zpaq.cpp libzpaq.cpp

-DOPT is meaningful only to zpaq.cpp. It excludes unneeded code
(like divsufsort) and leaves a "hole" in the program that will be
filled in by the generated source. Then if you have the following
files:

  c:\zpaq\zpaq.o
  c:\zpaq\libzpaq.o
  c:\zpaq\libzpaq.h

Then zpaqopt.bat should contain:

  g++ -O3 %1.cpp c:\zpaq\zpaq.o c:\zpaq\libzpaq.o -Ic:\zpaq\libzpaq.h -o %1.exe

A typical Linux configuration would look like this:

  /usr/lib/zpaq/zpaq.o     (g++ -O3 -c -DOPT -lpthread zpaq.cpp)
  /usr/lib/zpaq/libzpaq.o  (g++ -O3 -c libzpaq.cpp)
  /usr/include/libzpaq.h
  /usr/bin/zpaq
  /usr/bin/zpaqopt

where zpaqopt contains:

  g++ -O3 $1.cpp /usr/lib/zpaq/zpaq.o /usr/lib/zpaq/libzpaq.o -o $1.exe -lpthread

The argument %1 or $1 would be a temporary file in %TEMP%, $TMPDIR, or /tmp
(searching in that order) of the form "zpaqtmp{PID}_0 where {PID}
is the process ID, for example, /tmp/zpaqtmp2318_0

*/

// #define NDEBUG 1
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

// Forward declarations
#ifndef OPT
#include "divsufsort.h"
bool findModel(const std::string& models, const std::string& comp);
void optimize(const std::string& models, int argc, char** argv);
void compile_cmd(const char* cmd);
void list(const char* filename);
void block_append();
#endif
int numberOfProcessors();  // Default for -t
extern const char* pcomp_cmd_string;

void usage() {
  fprintf(stderr,
  "zpaq v3.00 - ZPAQ archiver and compression algorithm development tool.\n"
  "(C) 2011, Dell Inc. Written by Matt Mahoney. Compiled " __DATE__ ".\n"
  "This is free software under GPL v3. http://www.gnu.org/copyleft/gpl.html\n"
  "\n"
  "Usage: zpaq [-options] command [arguments...]\n"
  "Commands:\n"
  "  c|a archive files...     Compress|append to archive.zpaq\n"
  "  e|x archive [files...]   Extract to files or as saved without|with paths\n"
  "  l archive                List contents\n"
  "  b archive output [N[-N]...]  Append listed blocks to output.zpaq\n"
  "  r [input [output]]       Run config file F.cfg (specified by -m)\n"
  "  t [N...]                 Trace F.cfg with decimal/hex inputs\n"
  "Options:\n"
  "  -f   Force overwrite of output files\n"
  "  -m1 ... -m4  Compress faster...smaller (default -m1)\n"
  "  -mF[,N...]   Compress using F.cfg with up to 9 numeric arguments\n"
  "  -bN  Compress in N MB blocks, -b0 = file, -bs = solid (default -b16)\n"
  "  -v   Verbose\n"
  "  -tN  Use N threads (default -t%d)\n"
  "  -p   Ignore/don't save paths\n"
  "  -n   Ignore/don't save filenames\n"
  "  -s   Ignore/don't save checksums\n"
  "  -i   Don't save comments\n"
  "  -h   Save locator tag. With r or t run HCOMP (default PCOMP)\n"
  "  -j0 ... -j3  No JIT, JIT, keep source, exe (default -j1)\n"
  "  -q   Don't test F.cfg postprocessor during compression\n",
  numberOfProcessors());
#ifdef unix
  if (sizeof(off_t)!=8)
    fprintf(stderr, "Does not work with files larger than 2 GB\n");
#endif
#ifndef NDEBUG
  fprintf(stderr, "Debug (slow) version, not compiled with -DNDEBUG\n");
#endif
  exit(1);
}

// Portable thread handling
#ifdef PTHREAD  // unix or pthread_win32
pthread_cond_t cv=PTHREAD_COND_INITIALIZER;  // to signal FINISHED
pthread_mutex_t mutex=PTHREAD_MUTEX_INITIALIZER; // protects cv
#else  // Windows
HANDLE mutex;  // protects Job::state
typedef HANDLE pthread_t;
#endif

// The parsed command line is available globally
char** cmd=0;          // command, archive, files...
int ncmd=0;            // length of cmd
bool verbose=0;        // -v verbose option
bool fopt=false;       // -f force overwrite (c or x/e with filenames)
int mopt=1;            // -m compression method 1..4, 0 = config file
int64_t bopt=16000000; // -b in bytes, -1 = -bs
bool nopt=false;       // -n no names
bool popt=false;       // -p no paths or run/trace PCOMP
bool iopt=false;       // -i no comments
bool sopt=false;       // -s no checksums
bool hopt=false;       // -h no header locator tags, or rh, th commands
int jopt=1;            // -j0..-j3 = no JIT, JIT, save source, save exe
bool qopt=false;       // -q don't test postprocessor
int topt=1;            // -t number of threads
const char* config=0;  // config file name from -m, r, t
int args[9]={0};       // config file arguments
std::string archive;   // archive file name
const char* hcomp=0;   // COMP+HCOMP selected by -m, length in first 2 bytes
const char* pcomp=0;   // PCOMP with empty COMP header, selected by -m
extern const char* pcomp_cmd;  // preprocessor command set by OPT
bool iserror=false;    // return code, set true by error()
#ifndef OPT
const char* pcomp_cmd=0;  // preprocessor command from config file from -m
#endif

// State: Possible job states. A thread is initialized as READY. When main()
// is ready to start the thread it is set to RUNNING and runs  it. When
// the thread finishes, it sets its state to FINISHED or FINISHED_ERR
// if there is an error, signals main (using cv, protected by mutex),
// and exits. main then waits on the thread, receives the return status, and
// updates the state to OK or ERR.
typedef enum {READY, RUNNING, FINISHED_ERR, FINISHED, ERR, OK} State;

// A Job is a thread to compress or decompress a single ZPAQ block.
// For compression, the input is the name of a file, part of a file,
// or a list of files. The output is an archive or a temporary file that
// will be appended to the archive when all jobs are finished.
// For decompression, the input is the name of an archive and an offset
// to the start of the block, which may contain a file, part of a file,
// or a sequence of files (first one possibly partial). The output
// is a file for each named segment (possibly renamed according to
// command line options) and a temporary file in the case that the
// block starts with an unnamed segment (or names are ignored).

struct Job {
  Job();
  State state;        // job state, protected by mutex, initially READY
  pthread_t tid;      // thread ID (for scheduler)
  int id;             // unique job number and temporary output filename
  int nfile;          // number of files in previous blocks
  int64_t start;      // input file offset
  int64_t size;       // input size, -1 = to EOF
  std::string output; // last non-temporary output file or "" if none
  void print(int i);  // dump contents to stderr
};

// Initialize
Job::Job(): state(READY), id(0), nfile(0), start(0), size(-1) {
  // tid is not initialized until state==RUNNING
}

void Job::print(int i) {
  fprintf(stderr,
      "Job %d: state=%d id=%d output=%s nfile=%d start=%1.0f size=%1.0f\n",
      i, state, id, output.c_str(), nfile, double(start), double(size));
}

// Seek f to 64 bit pos, return true if successful
int fseek64(FILE* f, int64_t pos) {
#ifdef unix
  return fseeko(f, pos, SEEK_SET)==0;
#else
  rewind(f);
  HANDLE h=(HANDLE)_get_osfhandle(_fileno(f));
  LONG low=pos, high=pos>>32;
//  SetLastError(NO_ERROR);  // thread unsafe?
  SetFilePointer(h, low, &high, FILE_BEGIN);
//  return GetLastError()==ERROR_SUCCESS;
  return 1;
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
//  SetLastError(NO_ERROR);  // thread unsafe?
  DWORD low=GetFileSize(h, &high);
//  if (GetLastError()!=NO_ERROR) return -1;
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

// Thread exit. A msg of 0 means OK and any other pointer means ERR.
void libzpaq::error(const char* msg) {
  iserror=true;
  if (msg) fprintf(stderr, "zpaq error: %s\n", msg);
  throw msg;
}
using libzpaq::error;

// Component names
static const char* compname[256]=
  {"","const","cm","icm","match","avg","mix2","mix","isse","sse",0};

// Print compression component statistics
int libzpaq::Predictor::stat(int id) {
  fprintf(stderr, "\nMemory utilization for job [%d]:\n", id);
  int cp=7;
  for (int i=0; i<z.header[6]; ++i) {
    assert(cp<z.header.isize());
    int type=z.header[cp];
    assert(compsize[type]>0);
    fprintf(stderr, "%2d %s", i, compname[type]);
    for (int j=1; j<compsize[type]; ++j)
      fprintf(stderr, " %d", z.header[cp+j]);
    Component& cr=comp[i];
    if (type==MATCH) {
      assert(cr.cm.size()>0);
      assert(cr.ht.size()>0);
      int count=0;
      for (size_t j=0; j<cr.cm.size(); ++j)
        if (cr.cm[j]) ++count;
      fprintf(stderr, ": buffer=%d/%d index=%d/%d (%1.2f%%)",
        cr.limit/8, cr.ht.size(), count, cr.cm.size(),
        count*100.0/cr.cm.size());
    }
    else if (type==SSE) {
      assert(cr.cm.size()>0);
      int count=0;
      for (size_t j=0; j<cr.cm.size(); ++j) {
        if (int(cr.cm[j])!=(squash((j&31)*64-992)<<17|z.header[cp+3]))
          ++count;
      }
      fprintf(stderr, ": %d/%d (%1.2f%%)", count, cr.cm.size(),
        count*100.0/cr.cm.size());
    }
    else if (type==CM) {
      assert(cr.cm.size()>0);
      int count=0;
      for (size_t j=0; j<cr.cm.size(); ++j)
        if (cr.cm[j]!=0x80000000) ++count;
      fprintf(stderr, ": %d/%d (%1.2f%%)", count, cr.cm.size(),
        count*100.0/cr.cm.size());
    }
    else if (type==MIX) {
      int count=0;
      int m=z.header[cp+3];
      assert(m>0);
      for (size_t j=0; j<cr.cm.size(); ++j)
        if (int(cr.cm[j])!=65536/m) ++count;
      fprintf(stderr, ": %d/%d (%1.2f%%)", count, cr.cm.size(),
        count*100.0/cr.cm.size());
    }
    else if (type==MIX2) {
      int count=0;
      for (size_t j=0; j<cr.a16.size(); ++j)
        if (int(cr.a16[j])!=32768) ++count;
      fprintf(stderr, ": %d/%d (%1.2f%%)", count, cr.a16.size(),
        count*100.0/cr.a16.size());
    }
    else if (cr.ht.size()>0) {
      int hcount=0;
      for (size_t j=0; j<cr.ht.size(); ++j)
        if (cr.ht[j]>0) ++hcount;
      fprintf(stderr, ": %d/%d (%1.2f%%)",
          hcount, cr.ht.size(), hcount*100.0/cr.ht.size());
    }
    cp+=compsize[type];
    fprintf(stderr, "\n");
  }
  fprintf(stderr, "\n");
  return 0;
}

// Print and run a command
int run_cmd(const std::string& cmd) {
  fprintf(stderr, "%s\n", cmd.c_str());
  return system(cmd.c_str());
}

// File for libzpaq (de)compression
struct File: public libzpaq::Reader, public libzpaq::Writer {
  FILE* f;
  int get() {return getc(f);}
  void put(int c) {putc(c, f);}
  File(FILE* f_=0): f(f_) {}
  ~File() {if (f) fclose(f);}
};

// To count bytes read or written
struct FileCount: public libzpaq::Reader, public libzpaq::Writer {
  FILE* f;
  int64_t count;
  FileCount(FILE* f_): f(f_), count(0) {}
  ~FileCount() {if (f) fclose(f);}
  int get() {int c=getc(f); count+=(c!=EOF); return c;}
  void put(int c) {putc(c, f); count+=1;}
};

// To output to a string
struct StringWriter: public libzpaq::Writer {
  std::string s;
  void put(int c) {s+=char(c);}
  int len() const {return s.size();}
  int operator()(int i) const {return i>=0 && i<len() ? s[i]&255 : 0;}
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

// Test for regular file (Linux)
static bool is_file(const char* filename) {
#ifdef unix
  struct stat st;
  return stat(filename, &st)==0 && (st.st_mode & S_IFREG);
#endif
  return true;
}

// Test if filename is readable
bool exists(const char* filename) {
  if (!is_file(filename)) return false;
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
  if (exists(filename)) {
    if (verbose) fprintf(stderr, "Deleting %s\n", filename);
    if (remove(filename)) perror(filename);
  }
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
    remove(file2);
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
  result+="zpaqtmp";

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

// Get 2 byte little-endian number
int get2(const char* s) {
  if (!s) return -2;
  return (s[0]&255)+256*(s[1]&255);
}

// Input file for compression that preprocesses
class FileToCompress: public libzpaq::Reader {
public:
  FileToCompress(const char* filename, int64_t start, int64_t n, int id);
  ~FileToCompress();  // close
  int get();  // return n bytes, then EOF
  int64_t filesize() const {return inputsize;}  // input size
  const char* sha1() const {return sha1result;}  // checksum of input
private:
  FILE* in;  // input or temporary
  int64_t inputsize;  // n or actual input size if n < 0
  int64_t remaining;  // remaining bytes before EOF, no limit if < 0
  char sha1result[20];  // computed at initialization
  libzpaq::Array<unsigned char> buf;  // to hold BWT for -m1, -m2
  int64_t pos;  // next byte to read in buf
  int rle;  // RLE decode state for -m1
  std::string tmp_out;  // preprocessor output file name
};

// Initialize by opening filename for reading n bytes from start
// or to EOF if n < 0. id should be unique among threads.
// If mopt is 1 or 2 then compute the BWTRLE or BWT transform and
// read from that instead. If mopt is 0 and pcomp_cmd or pcomp_cmd_string
// is not empty then preprocess the input to a temporary file and read
// from that file instead. This may require a second temporary if the
// input is not a complete file (start is 0 and either bopt is 0 or
// n < bopt). If qopt is false then test the preprocessor by running it
// through the postprocessor desribed in pcomp or the second model
// in models and comparing the output checksum.
FileToCompress::FileToCompress(const char* filename, int64_t start,
                               int64_t n, int id) {

  // Initialize BWT buffer
#ifndef OPT
  if (mopt==1 || mopt==2) {
    assert(bopt>0);
    assert(n>=0);
    int len=n;
    assert(int64_t(len)==n);
    pos=0;
    rle=0;
    buf.resize(len+5);
  }
#endif

  // Open input
  remaining=n;
  if (!is_file(filename)) {
    fprintf(stderr, "%s: not a regular file\n", filename);
    error("cannot read file");
  }
  in=fopen(filename, "rb");
  if (!in) {
    perror(filename);
    error("file not found");
  }

  // Seek to start
  if (start && !fseek64(in, start))
    error("fseek64 failed");

  // Compute checksum and save in buf
  libzpaq::SHA1 sha1;
  int c;
  for (int64_t i=0; i!=n && (c=getc(in))!=EOF; ++i) {
    sha1.put(c);
#ifndef OPT
    if (mopt==1 || mopt==2) {
      assert(i>=0 && i<int64_t(buf.size()-5));
      buf[i]=c;
    }
#endif
  }
  inputsize=sha1.size();
  memcpy(sha1result, sha1.result(), 20);
  if (!fseek64(in, start))
    error("fseek64 failed");

  // For modes -m1 and -m2, close input and compute BWT in buf
#ifndef OPT
  if (mopt==1 || mopt==2) {
    fclose(in);
    in=0;
    int len=n;
    libzpaq::Array<int> w(len);
    int idx=divbwt(&buf[0], &buf[0], &w[0], len);
    if (len>idx) memmove(&buf[idx+1], &buf[idx], len-idx);
    buf[idx]=255;
    for (int j=0; j<4; ++j) buf[len+j+1]=idx>>(j*8);
  }
#endif

  // Preprocess with pcomp_cmd if any
  if (pcomp_cmd) {
    assert(mopt==0);
    assert(in);
    assert(hcomp);
    assert(pcomp);

    // If the input is not a whole file, then create a temporary
    // block for input to the preprocessor
    std::string tmp_in=filename;
    if (bopt>0 && (start>0 || n>=bopt)) {
      tmp_in=tempname(id)+".in";
      FILE* tmp=fopen(tmp_in.c_str(), "wb");
      if (!tmp) {
        perror(tmp_in.c_str());
        error("Cannot create preprocessor temporary block");
      }
      int c;
      int64_t i;
      for (i=0; i!=n && (c=getc(in))!=EOF; ++i)
        putc(c, tmp);
      fclose(tmp);
      if (verbose) {
        fprintf(stderr, "Copied %1.0f bytes of %s+%1.0f to %s\n",
          double(i), filename, double(start), tmp_in.c_str());
      }
    }
    fclose(in);
    in=0;

    // Run external preprocessor
    std::string tmp_out=tempname(id)+".out";
    run_cmd(std::string(pcomp_cmd)+" "+tmp_in+" "+tmp_out);
    in=fopen(tmp_out.c_str(), "rb");
    if (!in) {
      perror(tmp_out.c_str());
      error("preprocessing failed");
    }
    remaining=-1;
    if (tmp_in!=filename) delete_file(tmp_in.c_str());

    // Verify the postprocessor
    if (!qopt) {

      // Initialize a postprocessor with pcomp
      static libzpaq::PostProcessor pps;  // for solid mode (1 thread)
      libzpaq::PostProcessor ppb;  // for multithreaded testing
      libzpaq::PostProcessor& pp(bopt<0 ? pps : ppb);
      libzpaq::SHA1 sha2;
      pp.setSHA1(&sha2);
      if (pp.getState()==0) {
        int plen=get2(pcomp);
        pp.init(hcomp[4]&255, hcomp[5]&255);
        pp.write(1);
        pp.write((plen-6)&255);
        pp.write((plen-6)/256);
        for (int i=8; i<plen+2; ++i)
          pp.write(pcomp[i]&255);
      }

      // Postprocess temporary input and compare checksums
      int c;
      while ((c=getc(in))!=EOF)
        pp.write(c);
      pp.write(-1);
      rewind(in);
      if (memcmp(sha1result, sha2.result(), 20)) {
        fclose(in);
        in=0;
        fprintf(stderr, "pre/post test failed: %s+%1.0f\n", filename,
            double(start));
        error("pre/post test failed");
      }
      else if (verbose) {
        fprintf(stderr, "%s+%1.0f pre/post test passed\n",
            filename, double(start));
      }
    }
  }
}

// Read 1 byte or return EOF if there is no more input
int FileToCompress::get() {

  // Return BWT
  const int len=buf.size();
  if (mopt==2) {
    if (pos<len) return buf[pos++];
    else return -1;
  }

  // Return BWT+RLE
  else if (mopt==1) {
    if (rle<2 && pos>=len) return -1;
    if (rle==2) {  // return RLE code
      int j;  // count run length
      for (j=0; j<255 && pos+j<len && buf[pos+j]==buf[pos-1]; ++j);
      pos+=j;
      rle=0;
      return j;
    }
    else {
      if (rle>0 && buf[pos]==buf[pos-1]) ++rle;
      else rle=1;
      return buf[pos++];
    }
  }

  // Read from file
  assert(in);
  if (remaining--) return getc(in);
  return -1;
}

// Close files, delete temporaries, and free memory
FileToCompress::~FileToCompress() {
  if (in) fclose(in);
  if (tmp_out!="") delete_file(tmp_out.c_str());
}

/////////////////// compress ////////////////////////

// Compress 1 block
void compress(Job& job) {

  // Open output file
  libzpaq::Compressor c;
  std::string output=job.output;
  if (job.output=="")
    output=tempname(job.id);
  else
    fprintf(stderr, "%s archive %s\n", exists(output.c_str())
        ? (fopt?"Overwriting":"Appending to") : "Creating", output.c_str());
  FileCount out(fopen(output.c_str(), job.output==""||fopt?"wb":"ab"));
  if (!out.f) {
    perror(output.c_str());
    error("file creation failed");
  }
  double outsize=-1;

  // Write locator tag
  c.setOutput(&out);
  if (job.id==1 && hopt) c.writeTag();

  // Adjust postprocessor block size for bwt modes
  if (mopt==1 || mopt==2) {
    assert(hcomp);
    std::string s(hcomp, hcomp+get2(hcomp)+2);
    assert(size(s)>5);
    int mem;
    for (mem=0; mem<32 && (1<<mem)-257<job.size; ++mem);
    s[4]=s[5]=mem;  // PCOMP H and M array sizes
    c.startBlock(s.c_str());
  }
  else
    c.startBlock(hcomp);

  // Write segments
  for (int i=0; i<(bopt<0?ncmd-2:1); ++i) {

    // Get input file name
    assert(job.nfile+i+2<ncmd);
    const char* input=cmd[job.nfile+i+2];
    if (verbose) {
      fprintf(stderr, "%s", input);
      if (job.start>0) fprintf(stderr, "+%1.0f", double(job.start));
      fprintf(stderr, " %1.0f -> %s[%d]\n", double(job.size),
          output.c_str(), job.id);
    }

    // Compress segment
    assert((job.size>=0)==(bopt>=0));
    FileToCompress in(input, job.start, job.size, job.id);
    int64_t insize=in.filesize();
    c.setInput(&in);
    c.startSegment(
        nopt || job.start ? 0 : popt ? strip(input).c_str() : input, 
        iopt ? 0 : itos(insize).c_str());
    if (i==0) {
      if (pcomp) c.postProcess(pcomp+8, get2(pcomp)-6);
      else c.postProcess(0);
    }
    c.compress();
    c.endSegment(sopt ? 0 : in.sha1());
    fprintf(stderr, "[%d] %s", job.id, input);
    if (job.start>0) fprintf(stderr, "+%1.0f", double(job.start));
    fprintf(stderr, " %1.0f -> %1.0f\n", double(insize), out.count-outsize);
    outsize=out.count;
  }
  c.endBlock();
  fclose(out.f);
  out.f=0;
  if (verbose)
    c.stat(job.id);
}

/////////////////// decompress ////////////////////////

// Create directories as needed. For example if path="/tmp/foo/bar"
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
void decompress(Job& job) {

  // Open input
  File in(fopen(archive.c_str(), "rb"));
  if (!in.f) {
    perror(archive.c_str());
    error("cannot read file ");
  }

  // Find start of block in first file
  if (job.start>0 && !fseek64(in.f, job.start))
    error("fseek64");

  // Decompress file
  libzpaq::Decompresser d;
  d.setInput(&in);
  File out(0);
  bool first_segment=true;
  if (d.findBlock()) {
    StringWriter filename, comment;
    while (d.findFilename(&filename)) {
      d.readComment(&comment);
      libzpaq::SHA1 sha1;
      d.setSHA1(&sha1);

      // Get new output filename
      if (nopt) filename.s="";
      if (filename.s!="" || (job.id==1 && first_segment)) {
        ++job.nfile;
        if (ncmd>2) {  // rename output
          if (job.nfile+1>=ncmd) break;  // no more files
          job.output=cmd[job.nfile+1];
        }
        else if (filename.s=="") {  // no name at start of archive?
          if (size(archive)>5 && archive.substr(size(archive)-5)==".zpaq")
            job.output=archive.substr(0, size(archive)-5);
          else
            job.output=archive+".out";
        }
        else if (popt)
          job.output=strip(filename.s);
        else
          job.output=filename.s;
        if (out.f) {
          fclose(out.f);
          out.f=0;
        }
      }

      // Set output
      if (!out.f) {
        makepath(job.output);
        std::string output=job.output;
        if (output!="")
          fprintf(stderr, "Extracting %s\n", output.c_str());
        if (output=="")
          output=tempname(job.id);
        else if (!fopt && exists(output.c_str())) {
          fprintf(stderr, "Won't clobber %s\n", output.c_str());
          error("output file exists");
        }
        if (verbose)
          fprintf(stderr, "%s[%d] %s %s -> %s\n", archive.c_str(), job.id,
              filename.s.c_str(), comment.s.c_str(), output.c_str());
        out.f=fopen(output.c_str(), "wb");
        if (!out.f) {
          perror(output.c_str());
          error("file creation failed");
        }
      }
      d.setOutput(&out);

      // Decompress segment
      d.decompress();

      // Verify checksum
      char sha1string[21];
      d.readSegmentEnd(sha1string);
      if (sha1string[0] && memcmp(sha1string+1, sha1.result(), 20)) {
        fprintf(stderr, "%s -> %s checksum error\n",
            archive.c_str(), job.output.c_str());
        if (!sopt) error("checksum mismatch");
      }
      filename.s="";
      comment.s="";
      first_segment=false;
    }
  }

  // End of input block
  if (out.f && out.f!=stdout) {
    fclose(out.f);
    out.f=0;
  }
  if (in.f && in.f!=stdin) {
    fclose(in.f);
    in.f=0;
  }
}

/////////////////// run ////////////////////////

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

// Read one byte of a string
class StringReader: public libzpaq::Reader {
  const char* ptr;
  int n;
public:
  StringReader(const char* p, int len): ptr(p), n(len) {}
  int get() {return n>0 ? *ptr++&255 : -1;}
};

// Pad pcomp string with an empty COMP header with ph,pm from hcomp
void fix_pcomp(const std::string& hcomp, std::string& pcomp) {
  if (size(hcomp)>=8 && size(pcomp)>=2) {
    pcomp=hcomp.substr(0, 8)+pcomp.substr(2);
    pcomp[0]=(size(pcomp)-2)&255;  // new length of PCOMP
    pcomp[1]=(size(pcomp)-2)>>8;
    pcomp[6]=pcomp[7]=0;  // n=0 components
  }
}

// Run or trace config file
void run() {
  if (!config)
    fprintf(stderr, "Use -m to specify a config file\n"), exit(1);
  if (!pcomp && !hopt)
    fprintf(stderr, "No PCOMP section, use -h to run HCOMP\n"), exit(1);

  // Initialze virtual machine for HCOMP or PCOMP
  libzpaq::ZPAQL z;
  if (hopt) {
    StringReader s(hcomp, get2(hcomp)+2);
    z.read(&s);
    z.inith();
  }
  else {
    assert(pcomp);
    StringReader s(pcomp, get2(pcomp)+2);
    z.read(&s);
    z.initp();
  }

  // Run the program
  if (**cmd=='t') {  // trace with numeric args
#ifndef OPT
    for (int i=1; i<ncmd; ++i) 
      z.step(ntoi(cmd[i]), tolower(cmd[i][0])=='x');
#endif
  }
  else if (**cmd=='r') {  // run F.cfg input output
    FILE *in=stdin;
    File out(stdout);
    z.output=&out;
    if (ncmd>1) {
      in=fopen(cmd[1], "rb");
      if (!in) perror(cmd[1]), exit(1);
    }
    if (ncmd>2) {
      out.f=fopen(cmd[2], "wb");
      if (!out.f) perror(cmd[2]), exit(1);
    }
    int c;
    while ((c=getc(in))!=EOF)
      z.run(c);
    if (!hopt) z.run(-1);
  }
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
    if (**cmd=='a' || **cmd=='c') compress(*job);
    if (**cmd=='x' || **cmd=='e') decompress(*job);
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

// Put n'th model into p and return its length (including length code).
// If there is no n'th model, set p=0 and return 0.
int getmodel(int n, const char* &p) {
  if (n<1) return p=0,0;
  int len=0;
  for (p=libzpaq::models; (len=get2(p)) && n>1; --n, p+=len+2);
  return len ? len+2 : (p=0,0);
}

int main(int argc, char** argv) {

  // Start timer
  time_t start_time=time(0);

  // Process options
  cmd=argv+1;
  ncmd=argc-1;
  topt=numberOfProcessors();
  while (ncmd>0 && cmd[0][0]=='-') {
    switch(cmd[0][1]) {
      case 'v': verbose=true; break;
      case 'f': fopt=true; break;
      case 'm':
        if (isdigit(cmd[0][2])) mopt=atoi(cmd[0]+2);
        else config=cmd[0]+2, mopt=0;
        break;
      case 'b':
        if (isdigit(cmd[0][2])) bopt=atof(cmd[0]+2)*1000000+0.25;
        else if (cmd[0][2]=='s') bopt=-1;
        else usage();
        break;
      case 'n': nopt=true; break;
      case 'p': popt=true; break;
      case 'i': iopt=true; break;
      case 's': sopt=true; break;
      case 'h': hopt=true; break;
      case 'j': jopt=atoi(cmd[0]+2); break;
      case 'q': qopt=true; break;
      case 't': topt=atoi(cmd[0]+2); break;
      default: usage(); break;
    }
    ++cmd;
    --ncmd;
  }

  // Process command
  if (ncmd<1 || !cmd || !*cmd) usage();
  switch(cmd[0][0]) {
    case 'b':
      if (ncmd<4) usage();
    case 'c':
    case 'a':
      if (ncmd<3) usage();
    case 'x':
    case 'e':
    case 'l':
      if (ncmd<2) usage();
    case 'r':
    case 't':
      if (cmd[0][1]) usage();
      break;
    default: usage(); break;
  }

  // Check for valid -m, -t, -b, -j
  if (!config && (mopt<1 || mopt>4)) usage();
  if (topt<1) topt=1;
#ifndef PTHREAD
  if (topt>MAXIMUM_WAIT_OBJECTS)
    topt=MAXIMUM_WAIT_OBJECTS;  // max 64 threads in Windows
#endif
  if (mopt==1 || mopt==2) {
    const int64_t max_bopt=(1<<28)-257;  // limit -b to max bwt block size
    if (bopt<=0 || bopt>max_bopt) {
      fprintf(stderr, "Setting max block size for -m1 or -m2 to -b%1.6f\n",
          max_bopt*0.000001);
      bopt=max_bopt;
    }
  }
  if (**cmd=='e') popt=true;  // ignore paths
  if (**cmd=='c') fopt=true;  // force overwrite
  if ((**cmd=='e' || **cmd=='x') && ncmd>2) fopt=true;
  if (**cmd=='t') jopt=0;

  // Get archive name. Append .zpaq if not already.
  if (ncmd>1) {
    archive=cmd[1];
    if (size(archive)<5 || archive.substr(size(archive)-5)!=".zpaq")
      archive+=".zpaq";
  }

  // List of jobs
  std::vector<Job> jobs;  // one per thread

  // Initialize hcomp, pcomp, pcomp_cmd for commands a, c, t, r
  if (strchr("actr", **cmd)) {
#ifdef OPT
    getmodel(1, hcomp);
    getmodel(2, pcomp);
#else

    // Compile config file. Put result in hcomp, pcomp, pcomp_cmd
    if (config) {
      assert(mopt==0);
      try {
        compile_cmd(config);
        std::string model_list(hcomp, hcomp+get2(hcomp)+2);
        if (pcomp) model_list+=std::string(pcomp, pcomp+get2(pcomp)+2);
        model_list+=char(0);
        model_list+=char(0);
        if (jopt>0)
          optimize(model_list, argc, argv);
      }
      catch(const char* msg) {
        fprintf(stderr, "Error in %s\n", config);
        exit(1);
      }
    }

    // Set build in mode for -m1 ... -m4
    else {
      assert(mopt>=1 && mopt<=4);
      getmodel(mopt*2-(mopt==4), hcomp);  // 2, 4, 6, 7
      if (mopt<=2) getmodel(mopt*2+1, pcomp);  // 3, 5
    }
#endif
  }

  // Run
  if (**cmd=='r' || **cmd=='t') {
    run();
    return 0;
  }

#ifndef OPT
  // List
  if (**cmd=='l') {
    list(archive.c_str());
    return 0;
  }

  // Extract blocks: b archive output [N-[N]]...
  // Append blocks in list from archive to output
  if (**cmd=='b') {
    block_append();
    return 0;
  }
#endif

  // Schedule compression
  if (**cmd=='a' || **cmd=='c') {

    // Solid mode = 1 job
    if (bopt<0) {
      Job job;
      job.output=archive;
      jobs.push_back(job);
    }

    else {
      for (int i=2; i<ncmd; ++i) {

        // Get file size
        FILE* f=fopen(cmd[i], "rb");
        if (!f) {
          perror(cmd[i]);
          continue;
        }
        int64_t fs=filesize(f);
        fclose(f);
        if (fs<0) {
          fprintf(stderr, "File %s has unknown size, skipping...\n", cmd[i]);
          continue;
        }

        // Schedule one job per file or block
        int64_t start=0;
        do {
          Job job;
          job.nfile=i-2;
          job.start=start;
          job.size=bopt?bopt:fs;
          if (start+job.size>fs) job.size=fs-start;
          if (i==2 && !start && (fopt || !exists(archive.c_str())))
            job.output=archive;
          jobs.push_back(job);
          start=job.start+job.size;
        } while (start<fs);
      }
    }
  }

  // Schedule decompression
  if (**cmd=='x' || **cmd=='e') {
    fprintf(stderr, "Extracting from %s\n", archive.c_str());
#ifndef OPT
    std::string model_list;
    bool non_default=false;
#endif
    try {
      int64_t offset=0; // current location in archive
      int filecount=0;  // how many files in archive?
      bool done=false;

      // Open input file
      FileCount in(fopen(archive.c_str(), "rb"));
      if (!in.f) perror(archive.c_str()), exit(1);

      // Scan archive for blocks. Schedule 1 job per block.
      // Look for non-default models.
      libzpaq::Decompresser d;
      d.setInput(&in);
      StringWriter filename;
      while (!done && d.findBlock()) {
        Job job;
        job.start=offset;
        job.nfile=filecount;
#ifndef OPT
        StringWriter hcomp, pcomp;
        d.hcomp(&hcomp);
        if (!findModel(model_list, hcomp.s))
          model_list+=hcomp.s;
        if (d.getModel()<1) non_default=true;
#endif

        // Scan segments and count nonempty filenames
        bool first_segment=true;
        while (!done && d.findFilename(&filename)) {
          d.readComment();
          if (nopt) filename.s="";
          if (filename.s!="" || (offset==0 && first_segment)) {
            ++filecount;
            if (first_segment && ncmd>2 && filecount>ncmd-2) done=true;

            // Check for overwrite errors
            else if (!fopt) {
              if (filename.s=="" && size(archive)>5)
                filename.s=archive.substr(0, size(archive)-5);  // drop .zpaq
              else if (popt)
                filename.s=strip(filename.s);
              if (exists(filename.s.c_str())) {
                fprintf(stderr, "Rename or use -f to overwrite: %s\n",
                    filename.s.c_str());
                error("file exists");
              }
            }
          }

#ifndef OPT
          // Get postprocessor model
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
          filename.s="";
          first_segment=false;
        }
        if (!done) jobs.push_back(job);
      }  // end while findBlock
      fclose(in.f);
      in.f=0;
    }  // end try

      // In case of error, go on to the next input file
    catch (const char* msg) {
      fprintf(stderr, "%s extraction failed\n", archive.c_str());
      return 1;
    }
#ifndef OPT
    if (non_default && jopt>0) {
      model_list+=char(0);
      model_list+=char(0);
      optimize(model_list, argc, argv);
    }
#endif
  }  // end if *cmd=='x'

  // Assign job ids and print list of jobs
  for (int i=0; i<size(jobs); ++i) {
    jobs[i].id=i+1;
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

  // Report unfinished jobs
  if (verbose) {
    for (int i=0; i<size(jobs); ++i) {
      if (jobs[i].state!=OK) {
        fprintf(stderr, "failed: ");
        jobs[i].print(i);
      }
    }
  }

  // Append temporary files to the last successful job with a non-temp output.
  std::string output;
  if (**cmd=='c' || **cmd=='a') output=archive;
  for (int i=0; i<size(jobs); ++i) {
    if (jobs[i].output!="")
      output=jobs[i].output;
    if (jobs[i].state!=OK)
      output="";
    if (output!="" && jobs[i].state==OK && jobs[i].output=="")
      append(output.c_str(), tempname(jobs[i].id).c_str());
  }

  // Delete leftover temporary files due to errors
  for (int i=0; i<size(jobs); ++i) {
    std::string fn=tempname(jobs[i].id);
    delete_file(fn.c_str());
    delete_file((fn+".in").c_str());
    delete_file((fn+".out").c_str());
  }

  // Report finish time
  fprintf(stderr, "%1.0f seconds\n", double(time(0)-start_time));
  return iserror;
}

//////////////////////////////// compile ///////////////////////////

// The rest of this program is not needed in JIT optimized ZPAQ
#ifndef OPT

// This code is to read configuration files containing custom
// compression algorithms written in ZPAQL.

// Improved std:string with output by appending to it
struct String: public std::string {
  String(const std::string& s): std::string(s) {} // base to derived conversions
  String(const char* s=""): std::string(s) {}
  explicit String(char c): std::string(1, c) {}
  void put(int c) {*this+=char(c);}     // append 1 byte
  int len() const {return int(size());} // size as a signed int
  int operator()(unsigned int i) const; // i'th byte, bounds checked
  String sub(int i, int n) const;       // clipped substr(i, n)
  String sub(int i) const;              // clipped substr(i)
};

int String::operator()(unsigned int i) const {
  assert(i<size());
  if (i>=size()) return 0;
  return (*this)[i]&255;
}

String String::sub(int i, int n) const {
  if (i<0) n+=i, i=0;
  if (i+n>len()) n=len()-i;
  if (n<=0) return "";
  return substr(i, n);
}

String String::sub(int i) const {
  return sub(i, len()-i);
}

// Symbolic constants
typedef enum {NONE,CONS,CM,ICM,MATCH,AVG,MIX2,MIX,ISSE,SSE,
  JT=39,JF=47,JMP=63,LJ=255,
  POST=256,PCOMP,END,IF,IFNOT,ELSE,ENDIF,DO,
  WHILE,UNTIL,FOREVER,IFL,IFNOTL,ELSEL,SEMICOLON} CompType;

// Opcodes
static const char* opcodelist[272]={
"error","a++",  "a--",  "a!",   "a=0",  "",     "",     "a=r",
"b<>a", "b++",  "b--",  "b!",   "b=0",  "",     "",     "b=r",
"c<>a", "c++",  "c--",  "c!",   "c=0",  "",     "",     "c=r",
"d<>a", "d++",  "d--",  "d!",   "d=0",  "",     "",     "d=r",
"*b<>a","*b++", "*b--", "*b!",  "*b=0", "",     "",     "jt",
"*c<>a","*c++", "*c--", "*c!",  "*c=0", "",     "",     "jf",
"*d<>a","*d++", "*d--", "*d!",  "*d=0", "",     "",     "r=a",
"halt", "out",  "",     "hash", "hashd","",     "",     "jmp",
"a=a",  "a=b",  "a=c",  "a=d",  "a=*b", "a=*c", "a=*d", "a=",
"b=a",  "b=b",  "b=c",  "b=d",  "b=*b", "b=*c", "b=*d", "b=",
"c=a",  "c=b",  "c=c",  "c=d",  "c=*b", "c=*c", "c=*d", "c=",
"d=a",  "d=b",  "d=c",  "d=d",  "d=*b", "d=*c", "d=*d", "d=",
"*b=a", "*b=b", "*b=c", "*b=d", "*b=*b","*b=*c","*b=*d","*b=",
"*c=a", "*c=b", "*c=c", "*c=d", "*c=*b","*c=*c","*c=*d","*c=",
"*d=a", "*d=b", "*d=c", "*d=d", "*d=*b","*d=*c","*d=*d","*d=",
"",     "",     "",     "",     "",     "",     "",     "",
"a+=a", "a+=b", "a+=c", "a+=d", "a+=*b","a+=*c","a+=*d","a+=",
"a-=a", "a-=b", "a-=c", "a-=d", "a-=*b","a-=*c","a-=*d","a-=",
"a*=a", "a*=b", "a*=c", "a*=d", "a*=*b","a*=*c","a*=*d","a*=",
"a/=a", "a/=b", "a/=c", "a/=d", "a/=*b","a/=*c","a/=*d","a/=",
"a%=a", "a%=b", "a%=c", "a%=d", "a%=*b","a%=*c","a%=*d","a%=",
"a&=a", "a&=b", "a&=c", "a&=d", "a&=*b","a&=*c","a&=*d","a&=",
"a&~a", "a&~b", "a&~c", "a&~d", "a&~*b","a&~*c","a&~*d","a&~",
"a|=a", "a|=b", "a|=c", "a|=d", "a|=*b","a|=*c","a|=*d","a|=",
"a^=a", "a^=b", "a^=c", "a^=d", "a^=*b","a^=*c","a^=*d","a^=",
"a<<=a","a<<=b","a<<=c","a<<=d","a<<=*b","a<<=*c","a<<=*d","a<<=",
"a>>=a","a>>=b","a>>=c","a>>=d","a>>=*b","a>>=*c","a>>=*d","a>>=",
"a==a", "a==b", "a==c", "a==d", "a==*b","a==*c","a==*d","a==",
"a<a",  "a<b",  "a<c",  "a<d",  "a<*b", "a<*c", "a<*d", "a<",
"a>a",  "a>b",  "a>c",  "a>d",  "a>*b", "a>*c", "a>*d", "a>",
"",     "",     "",     "",     "",     "",     "",     "",
"",     "",     "",     "",     "",     "",     "",     "lj",
"post", "pcomp","end",  "if",   "ifnot","else", "endif","do",
"while","until","forever","ifl","ifnotl","elsel",";",    0};

// Read a token and return it, or return 0 at EOF. Skip (comments).
// Convert to lower case. Tokens are separated by white space.
// In verbose mode, print the token.
const char* token(FILE* in, bool lowercase=true) {
  static char s[512];  // result
  int len=0;  // length of s

  // skip to start of token
  int paren=0, c=0;
  while (c<=' ' || paren>0) {
    c=getc(in);
    if (c=='(') ++paren;
    if (c==')') --paren, c=' ';
    if (c==EOF) return 0;
  }

  // read token separated by whitespace
  do {
    if (lowercase && isupper(c)) c=tolower(c);
    s[len++]=c;
  }
  while (len<511 && (c=getc(in))!=EOF && c>' ');
  s[len++]=0;
  if (verbose) printf("%s ", s);

  // Substitute parameters $1..$9 with args[0..8], $i+n with args[i-1]+n
  if (s[0]=='$' && s[1]>='1' && s[1]<='9') {
    int i=s[1]-'1';
    assert(i>=0 && i<9);
    int val=args[i];
    if (s[2]=='+')
      val+=atoi(s+3);
    sprintf(s, "%d", val);
    if (verbose) printf("(%s) ", s);
  }
  return s;
}

// Read a token, which must be in the NULL terminated list or else
// exit with an error. If found, return its index.
int rtoken(FILE* in, const char* list[]) {
  assert(in);
  assert(list);
  const char* tok=token(in);
  if (!tok)
    fprintf(stderr, "\nUnexpected end of configuration file\n"), exit(1);
  for (int i=0; list[i]; ++i)
    if (!strcmp(list[i], tok))
      return i;
  fprintf(stderr, "\nConfiguration file error at %s\n", tok), exit(1);
  assert(0);
  return -1; // not reached
}

// Read a token which must be the specified value s
void rtoken(FILE* in, const char* s) {
  assert(s);
  const char* t=token(in);
  if (!t) fprintf(stderr, "\nExpected %s, found EOF\n", s), exit(1);
  if (strcmp(s, t))
    fprintf(stderr, "\nExpected %s, found %s\n", s, t), exit(1);
}

// Read a number in (low...high) or exit with an error
int rtoken(FILE* in, int low, int high) {
  const char* tok=token(in);
  if (!tok)
    fprintf(stderr, "\nUnexpected end of configuration file\n"), exit(1);
  int n=0;
  const char* p=tok;
  int sign=1;
  if (*p=='-') sign=-1, ++p;
  while (*p) {
    if (isdigit(*p))
      n=n*10+*p-'0';
    else
      fprintf(stderr,
        "\nConfiguration file error at %s: expected a number\n", tok),
      exit(1);
    ++p;
  }
  n*=sign;
  if (n>=low && n<=high)
    return n;
  fprintf(stderr,
    "\nConfiguration file error: expected (%d...%d), found %d\n",
    low, high, n);
  exit(1);
  return 0;
}

// Stack of n elements of type T
template<class T>
class Stack {
  libzpaq::Array<T> s;
  size_t top;
public:
  Stack(int n): s(n), top(0) {}
  void push(const T& x) {
    if (top>=s.size()) error("stack full");
    s[top++]=x;
  }
  T pop() {
    if (top<=0) error("stack empty");
    return s[--top];
  }
};

// Compile HCOMP or PCOMP code. Exit on error. Return
// code for end token (POST, PCOMP, END)
CompType compile_comp(FILE* in, String& comp) {
  int op=0;
  const int comp_begin=comp.len();
  Stack<unsigned short> if_stack(1000), do_stack(1000);  // IF, DO addresses
  if (verbose) printf("\n");
  int indent=0;  // program listing indentation
  while (comp.len()<0x10000) {
    if (verbose) {
      printf("(%4d) ", comp.len()-comp_begin);
      for (int i=0; i<indent; ++i) printf("  ");
    }
    op=rtoken(in, opcodelist);
    if (op==POST || op==PCOMP || op==END) break;
    int operand=-1; // 0...255 if 2 bytes
    int operand2=-1;  // 0...255 if 3 bytes
    if (op==IF) {
      op=JF;
      operand=0; // set later
      if_stack.push(comp.len()+1); // save jump target location
      ++indent;
    }
    else if (op==IFNOT) {
      op=JT;
      operand=0;
      if_stack.push(comp.len()+1); // save jump target location
      ++indent;
    }
    else if (op==IFL || op==IFNOTL) {  // long if
      if (op==IFL) comp.put(JT);
      if (op==IFNOTL) comp.put(JF);
      comp.put(3);
      op=LJ;
      operand=operand2=0;
      if_stack.push(comp.len()+1);
      if (verbose)
        printf("(%s 3 (%d 3) lj 0 0)",
          opcodelist[comp(comp.len()-2)], comp(comp.len()-2));
      ++indent;
    }
    else if (op==ELSE || op==ELSEL) {
      if (op==ELSE) op=JMP, operand=0;
      if (op==ELSEL) op=LJ, operand=operand2=0;
      int a=if_stack.pop();  // conditional jump target location
      assert(a>comp_begin && a<comp.len());
      if (comp(a-1)!=LJ) {  // IF, IFNOT
        assert(comp(a-1)==JT || comp(a-1)==JF || comp(a-1)==JMP);
        int j=comp.len()-a+1+(op==LJ); // offset at IF
        assert(j>=0);
        if (j>127) error("IF too big, try IFL, IFNOTL");
        comp[a]=j;
        if (verbose) printf("((%d) %s %d (to %d)) ",
          a-comp_begin-1, opcodelist[comp(a-1)], j, comp.len()-comp_begin+2);
      }
      else {  // IFL, IFNOTL
        int j=comp.len()-comp_begin+2+(op==LJ);
        assert(j>=0);
        comp[a]=j&255;
        comp[a+1]=(j>>8)&255;
        if (verbose) printf("((%d) lj %d) ", a-comp_begin-1, j);
      }
      if_stack.push(comp.len()+1);  // save JMP target location
    }
    else if (op==ENDIF) {
      int a=if_stack.pop();  // jump target address
      assert(a>comp_begin && a<comp.len());
      int j=comp.len()-a-1;  // jump offset
      assert(j>=0);
      if (comp(a-1)!=LJ) {
        assert(comp(a-1)==JT || comp(a-1)==JF || comp(a-1)==JMP);
        if (j>127) error("IF too big, try IFL, IFNOTL, ELSEL\n");
        comp[a]=j;
        if (verbose) printf("((%d) %s %d (to %d))\n",
          a-comp_begin-1, opcodelist[comp(a-1)], j, comp.len()-comp_begin);
      }
      else {
        assert(a+1<comp.len());
        j=comp.len()-comp_begin;
        comp[a]=j&255;
        comp[a+1]=(j>>8)&255;
        if (verbose) printf("((%d) lj %d)\n", a-1, j);
      }
      --indent;
    }
    else if (op==DO) {
      do_stack.push(comp.len());
      if (verbose) printf("\n");
      ++indent;
    }
    else if (op==WHILE || op==UNTIL || op==FOREVER) {
      int a=do_stack.pop();
      assert(a>=comp_begin && a<comp.len());
      int j=a-comp.len()-2;
      assert(j<=-2);
      if (j>=-127) {  // backward short jump
        if (op==WHILE) op=JT;
        if (op==UNTIL) op=JF;
        if (op==FOREVER) op=JMP;
        operand=j&255;
        if (verbose)
          printf("(%s %d (to %d)) ", opcodelist[op], j,
                 comp.len()-comp_begin+2+j);
      }
      else {  // backward long jump
        j=a-comp_begin;
        assert(j>=0 && j<comp.len()-comp_begin);
        if (op==WHILE) {
          comp.put(JF);
          comp.put(3);
          if (verbose) printf("(jf 3) ");


        }
        if (op==UNTIL) {
          comp.put(JT);
          comp.put(3);
          if (verbose) printf("(jt 3) ");
        }
        op=LJ;
        operand=j&255;
        operand2=j>>8;
        if (verbose) printf("(lj %d) ", j);
      }
      --indent;
    }
    else if ((op&7)==7) { // 2 byte operand, read N
      if (op==LJ) {
        operand=rtoken(in, 0, 65535);
        operand2=operand>>8;
        operand&=255;
        if (verbose) printf("(to %d) ", operand+256*operand2);
      }
      else if (op==JT || op==JF || op==JMP) {
        operand=rtoken(in, -128, 127);
        if (verbose) printf("(to %d) ", comp.len()-comp_begin+2+operand);
        operand&=255;
      }
      else
        operand=rtoken(in, 0, 255);
    }
    if (verbose) {
      if (operand2>=0)
        printf("(%d %d %d)\n", op, operand, operand2);
      else if (operand>=0)
        printf("(%d %d)\n", op, operand);
      else if (op>=0 && op<=255)
        printf("(%d)\n", op);
    }
    if (op>=0 && op<=255)
      comp.put(op);
    if (operand>=0)
      comp.put(operand);
    if (operand2>=0)
      comp.put(operand2);
    if (comp.len()>=0x10000)
      error("program too big");
  }
  comp.put(0); // END
  return CompType(op);
}

// Compile a configuration file. Store COMP/HCOMP section in hcomp.
// If there is a PCOMP section, store it in pcomp and store the PCOMP
// command in pcomp_cmd. Replace "$1..$9+n" with args[0..8]+n
void compile(FILE* in, String& hcomp, String& pcomp, String& pcomp_cmd) {

  // Allocate header
  hcomp="";
  pcomp="";
  pcomp_cmd="";
 
  // Compile the COMP section of header
  rtoken(in, "comp");
  hcomp.put(0);  // size low byte to fill in later
  hcomp.put(0);  // size high byte
  hcomp.put(rtoken(in, 0, 255)); // hh
  hcomp.put(rtoken(in, 0, 255)); // hm
  hcomp.put(rtoken(in, 0, 255)); // ph
  hcomp.put(rtoken(in, 0, 255)); // pm
  int n=rtoken(in, 0, 255); // number of components
  hcomp.put(n);
  if (verbose) printf("\n");
  for (int i=0; i<n; ++i) {
    if (verbose) printf("  ");
    rtoken(in, i, i);
    CompType type=CompType(rtoken(in, compname));
    hcomp.put(type);
    int clen=libzpaq::compsize[type];
    assert(clen>0 && clen<10);
    for (int j=1; j<clen; ++j)
      hcomp.put(rtoken(in, 0, 255));
    if (verbose) printf("\n");
  }
  hcomp.put(0); // END

  // Compile HCOMP
  rtoken(in, "hcomp");
  CompType op=compile_comp(in, hcomp);
  if (verbose) printf("\n");

  // Compute header size
  int hsize=hcomp.len()-2;
  hcomp[0]=hsize&255;
  hcomp[1]=hsize>>8;

  // Compile POST 0 END
  if (op==POST) {
    rtoken(in, 0, 0);
    rtoken(in, "end");
  }

  // Compile PCOMP pcomp_cmd ; program... END
  else if (op==PCOMP) {
    pcomp.put(0);  // fill in size later
    pcomp.put(0);

    // get pcomp_cmd ending with ";" (case sensitive)
    const char *tok;
    while ((tok=token(in, false))!=0 && strcmp(tok, ";")) {
      if (pcomp_cmd.len()>0) pcomp_cmd+=" ";
      pcomp_cmd+=tok;
    }
    op=compile_comp(in, pcomp);
    if (op!=END)
      error("Expected END in configuation file");

    // Compute header size
    int hsize=pcomp.len()-2;
    assert(hsize>=0);
    pcomp[0]=hsize&255;
    pcomp[1]=hsize>>8;
  }
}

// Compile cmd with or without .cfg extension. Point global
// hcomp, pcomp, and pcomp_cmd to the compiled strings or 0 if absent.
// Put comma-separated numeric arguments after cmd in args[9].
void compile_cmd(const char* cmd) {

  // parse args
  int argnum=0;
  String filename;
  for (const char* p=cmd; *p && argnum<9; ++p) {
    if (*p==',')
      args[argnum++]=atoi(p+1);
    else if (argnum==0)
      filename.put(*p);
  }

  // Add .cfg extension
  if (filename.sub(filename.len()-4)!=".cfg")
    filename+=".cfg";

  // Compile F or F.cfg
  FILE* in=fopen(filename.c_str(), "r");
  if (!in) perror(filename.c_str()), exit(1);
  fprintf(stderr, "Using model %s", filename.c_str());
  for (int i=0; i<argnum; ++i)
    fprintf(stderr, ",%d", args[i]);
  fprintf(stderr, "\n");
  static String hcomp_s, pcomp_s, pcomp_cmd_s;
  compile(in, hcomp_s, pcomp_s, pcomp_cmd_s);
  fclose(in);

  // Store the result
  hcomp=hcomp_s.c_str();
  pcomp=0;
  pcomp_cmd=0;
  if (pcomp_s!="") {
    fix_pcomp(hcomp_s, pcomp_s);
    pcomp=pcomp_s.c_str();
    pcomp_cmd=pcomp_cmd_s.c_str();
  }
}

//////////////////////////// step ///////////////////////////

// Execute program input and show progress
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
  printf("\n\nR (size %d) = (rows of all 0 omitted)\n", r.size());
  for (int i=0; i<r.isize(); i+=4) {
    if (r(i) || r(i+1) || r(i+2) || r(i+3))
      printf(ishex ? "%8X: %08X %08X %08X %08X\n"
                   : "%10u: %10u %10u %10u %10u\n",
        i, r(i), r(i+1), r(i+2), r(i+3));
  }

  // Print H, skipping rows of 4 zeros
  printf("\nH (size %d) = (rows of all 0 omitted)\n", h.size());
  for (int i=0; i<h.isize(); i+=4) {
    if (h(i) || h(i+1) || h(i+2) || h(i+3))
      printf(ishex ? "%8X: %08X %08X %08X %08X\n"
                   : "%10u: %10u %10u %10u %10u\n",
        i, h(i), h(i+1), h(i+2), h(i+3));
  }

  // Print M, skipping rows of 16 zeros
  printf("\nM (size %d) = (rows of all 0 omitted)\n", m.size());
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
}  // end namespace libzpaq

////////////////// List /////////////////

// Decompile ZPAQL starting at s[i]
void printCode(const StringWriter& s, int i) {
  int start=i;
  for (; i<s.len()-1; ++i) {
    int op=s(i);
    assert(op>=0 && op<256);
    printf("  (%d) %s", i-start, opcodelist[op]);
    if (op==LJ) printf(" %d", s(i+1)+256*s(i+2)), i+=2;
    else if (op%8==7) {
      int n=s(++i);
      if ((op==JT || op==JF || op==JMP) && n>=128) n-=256;
      printf(" %d", n);
      if (op==JT || op==JF || op==JMP) printf(" (to %d)", i-start+n+1);
    }
    printf("\n");
  }
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
      printf("\nBlock %d model %d needs %1.3f MB\n", i, d.getModel(),
          memory*0.000001);
      bool firstSegment=true;
      while (d.findFilename(&name)) {
        d.readComment(&comment);
        if (firstSegment && verbose) {
          StringWriter hcomp;
          d.hcomp(&hcomp);
          if (hcomp.len()<7) error("hcomp too small");

          // Print COMP section
          printf("comp %d %d %d %d %d (hh hm ph pm n)\n",
            hcomp(2), hcomp(3), hcomp(4), hcomp(5), hcomp(6));
          int op=7;  // pointer to hcomp
          for (int i=0; i<hcomp(6); ++i) {
            if (!compname[hcomp(op)]) error("bad component");
            printf("  %d %s", i, compname[hcomp(op)]);
            int len=libzpaq::compsize[hcomp(op)];
            if (len<1) error("bad component");
            for (int j=1; j<len; ++j) {
              if (op+j>=hcomp.len()) error("end of hcomp");
              printf(" %d", hcomp(op+j));
            }
            printf("\n");
            op+=len;
          }
          if (hcomp(op)!=0) error("missing 0 at end of hcomp");

          // Print HCOMP and PCOMP/POST sections
          printf("hcomp\n");
          printCode(hcomp, op+1);
          d.decompress(0);
          StringWriter pcomp;
          if (!d.pcomp(&pcomp))
            printf("post\n  0\nend\n");
          else {
            printf("pcomp (model %d) ;\n", d.getPostModel());
            printCode(pcomp, 2);
            printf("end\n");
          }
        }
        firstSegment=false;
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
  in.f=0;
  printf("\n");
}

/////////////////////////// block_append //////////////////


// Extract blocks: cmd = b archive output [N-[N]]...
// Append blocks in list from archive to output
void block_append() {

  // Open archive
  FileCount in(fopen(archive.c_str(), "rb"));
  if (!in.f) {
    perror(archive.c_str());
    exit(1);
  }
  libzpaq::Decompresser d;
  d.setInput(&in);

  // Make a list of blocks and their offsets
  std::vector<int64_t> bl;  // block list
  bl.push_back(0);  // start of first block
  while (d.findBlock()) {
    bl.push_back(0);
    while (d.findFilename()) {
      d.readComment();
      d.readSegmentEnd();
      bl.back()=in.count+1;
    }
  }
  if (verbose) {
    for (int i=1; i<size(bl); ++i)
      fprintf(stderr, "[%d] %1.0f to %1.0f\n",
          i, double(bl[i-1]), double(bl[i]));
  }

  // Open output
  std::string output=cmd[2];
  if (size(output)<=5 || output.substr(size(output)-5)!=".zpaq")
    output+=".zpaq";
  FILE* out=fopen(output.c_str(), "ab");
  if (!out) {
    perror(output.c_str());
    exit(1);
  }
  fprintf(stderr, "Appending blocks from %s[1-%d] to %s\n",
      archive.c_str(), size(bl)-1, output.c_str());

  // Append blocks
  in.count=0;
  for (int i=3; i<ncmd; ++i) {
    int first=atoi(cmd[i]);
    int last=first;
    for (int j=0; cmd[i][j]; ++j)
      if (cmd[i][j]=='-')
        last=atoi(cmd[i]+j+1);
    if (last<=0 || last>=size(bl)) last=size(bl)-1;
    if (first<1) first=1;
    if (last>=first) {
      fprintf(stderr, "Appending blocks %d-%d (offset %1.0f-%1.0f)\n",
        first, last, double(bl[first-1]), double(bl[last]));
      fseek64(in.f, bl[first-1]);
      for (int64_t j=bl[first-1]; j<bl[last]; ++j)
        putc(in.get(), out);
    }
  }
  fprintf(stderr, "%1.0f bytes appended\n", double(in.count));
  fclose(out);
}

/////////////////////////// optimize ///////////////////////

// This code is to convert ZPAQL to C++.

// Read little-endian 2 byte number at models[p..p+1]
int get2(const std::string& models, int p) {
  assert(p+1<size(models));
  return (models[p]&255)+256*(models[p+1]&255);
}

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
// jopt=0..3: don't optimize, optimize, keep .cpp, keep .exe
void optimize(const std::string& models, int argc, char** argv) {
  if (jopt<1) return;

  // Get file name
  std::string basename=tempname(0);
  std::string sourcefile=basename+".cpp";
  std::string exefile=basename+".exe";

  // Open output file
  FILE* out=fopen(sourcefile.c_str(), "w");
  if (!out) perror(sourcefile.c_str()), exit(1);
  
  // Print models[]
  fprintf(out,
  "// Generated by zpaq\n"
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

  // Print pcomp_cmd
  if (pcomp_cmd)
    fprintf(out,
    "const char* pcomp_cmd=\"%s\";\n", pcomp_cmd);
  else
    fprintf(out,
    "const char* pcomp_cmd=0;\n");

  // Close output and make sure it exists
  fclose(out);
  if (verbose)
    fprintf(stderr, "Created %s\n", sourcefile.c_str());

  // Compile
  std::string command="zpaqopt "+basename;
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
  if (jopt<3) delete_file((basename+".obj").c_str());
  if (jopt<3) delete_file(exefile.c_str());
  if (jopt<2) delete_file(sourcefile.c_str());
  exit(0);
}

/////////////////////////// Optimized models ////////////////////////

// Optimized code generated by zpaq for fast.cfg and -m1...-m4
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
        if (int(comp[6].c)!=y) comp[6].a=0;
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
        if (int(comp[8].c)!=y) comp[8].a=0;
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

#endif // ifndef OPT
