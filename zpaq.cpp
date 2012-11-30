/* zpaq.cpp v4 - Archiver and compression development tool.

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

See http://mattmahoney.net/dc/zpaq.html for the latest version
of this program and for online documentation. This program
creates, lists, and extracts compressed archives in the
ZPAQ level 1 format described in the above specification.
It has 4 built in compression levels and also accepts algorithms
described in configuration files and optional external
preprocessors. It uses multithreaded compression and decompression
for archives that have more than one ZPAQ block. It uses just-in-time
(JIT) speed optimization on x86-32 and x86-64 processors.
It can run or trace ZPAQL code in configuration files as a
tool for debugging them.

This program needs libzpaq from the above website and
libdivsufsort-lite from http://code.google.com/p/libdivsufsort/
To compile for Windows with MinGW:

  g++ -O3 zpaq.cpp libzpaq.cpp divsufsort.c -o zpaq

With Visual C++

  cl /O2 zpaq.cpp libzpaq.cpp divsufsort.c

In Linux, also use option -fopenmp
In Windows you can use this too if you have pthreadGC2.dll in your PATH.
Other optimization options may be appropriate.

To enable run time checks, compile with -DDEBUG

*/

#define _FILE_OFFSET_BITS 64
#include "libzpaq.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <string>
#include <vector>
#include <fcntl.h>

#ifndef DEBUG
#define NDEBUG 1
#endif
#include <assert.h>

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
#include "divsufsort.h"
void compile_cmd(const char* cmd);
void list(const char* filename);
void block_append();
int numberOfProcessors();  // Default for -t

void usage() {
  fprintf(stderr,
  "zpaq v4.00 - ZPAQ archiver and compression algorithm development tool.\n"
  "(C) 2011, Dell Inc. Written by Matt Mahoney. Compiled " __DATE__ ".\n"
  "This is free software under GPL v3. http://www.gnu.org/copyleft/gpl.html\n"
  "\n"
  "Usage: zpaq [-options] command [arguments...]\n"
  "Commands:\n"
  "  c|a archive files...     Compress|append to archive.zpaq\n"
  "  x archive [files...]     Extract as saved or rename to files...\n"
  "  l archive                List contents\n"
  "  r [input [output]]       Run config file F.cfg (specified by -m)\n"
  "  t [N...]                 Trace F.cfg with decimal/hex inputs\n"
  "Options:\n"
  "  -f   Force overwrite of output files\n"
  "  -m1 ... -m4  Compress faster...smaller (default -m1)\n"
  "  -mF[,N...]   Compress using F.cfg with up to 9 numeric arguments\n"
  "  -bN  Compress in N MB blocks, -b0 = file, -bs = solid\n"
  "  -v   Verbose\n"
  "  -tN  Use N threads (default -t%d)\n"
  "  -p   Ignore/don't save paths\n"
  "  -n   Ignore/don't save filenames\n"
  "  -s   Ignore/don't save checksums\n"
  "  -i   Don't save comments\n"
  "  -h   Save locator tag. With r or t run HCOMP (default PCOMP)\n"
  "  -q   Don't test F.cfg postprocessor during compression\n",
  numberOfProcessors());
#ifdef unix
  if (sizeof(off_t)!=8)
    fprintf(stderr, "Does not work with files larger than 2 GB\n");
#endif
#ifdef NOJIT
  fprintf(stderr, "x86 JIT disabled (compiled with -DNOJIT)\n");
#endif
#ifndef NDEBUG
  fprintf(stderr, "Debug (slow) version (compiled with -DDEBUG)\n");
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
int64_t bopt=-2;       // -b in bytes, -1 = -bs (solid), -2 = default
bool nopt=false;       // -n no names
bool popt=false;       // -p no paths or run/trace PCOMP
bool iopt=false;       // -i no comments
bool sopt=false;       // -s no checksums
bool hopt=false;       // -h no header locator tags, or rh, th commands
bool qopt=false;       // -q don't test postprocessor
int topt=1;            // -t number of threads
const char* config=0;  // config file name from -m, r, t
int args[9]={0};       // config file arguments
std::string archive;   // archive file name
const char* hcomp=0;   // COMP+HCOMP selected by -m, length in first 2 bytes
const char* pcomp=0;   // PCOMP with empty COMP header, selected by -m
bool iserror=false;    // return code, set true by error()
const char* pcomp_cmd=0;  // preprocessor command from config file from -m

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
      size_t count=0;
      for (size_t j=0; j<cr.cm.size(); ++j)
        if (cr.cm[j]) ++count;
      fprintf(stderr, ": buffer=%1.0f/%1.0f index=%1.0f/%1.0f (%1.2f%%)",
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
      fprintf(stderr, ": %1.0f/%1.0f (%1.2f%%)", double(count),
        double(cr.cm.size()), count*100.0/cr.cm.size());
    }
    else if (type==CM) {
      assert(cr.cm.size()>0);
      size_t count=0;
      for (size_t j=0; j<cr.cm.size(); ++j)
        if (cr.cm[j]!=0x80000000) ++count;
      fprintf(stderr, ": %1.0f/%1.0f (%1.2f%%)", double(count),
        double(cr.cm.size()), count*100.0/cr.cm.size());
    }
    else if (type==MIX) {
      size_t count=0;
      int m=z.header[cp+3];
      assert(m>0);
      for (size_t j=0; j<cr.cm.size(); ++j)
        if (int(cr.cm[j])!=65536/m) ++count;
      fprintf(stderr, ": %1.0f/%1.0f (%1.2f%%)", double(count),
        double(cr.cm.size()), count*100.0/cr.cm.size());
    }
    else if (type==MIX2) {
      size_t count=0;
      for (size_t j=0; j<cr.a16.size(); ++j)
        if (int(cr.a16[j])!=32768) ++count;
      fprintf(stderr, ": %1.0f/%1.0f (%1.2f%%)", double(count),
        double(cr.a16.size()), count*100.0/cr.a16.size());
    }
    else if (cr.ht.size()>0) {
      double hcount=0;
      for (size_t j=0; j<cr.ht.size(); ++j)
        if (cr.ht[j]>0) ++hcount;
      fprintf(stderr, ": %1.0f/%1.0f (%1.2f%%)",
          double(hcount), double(cr.ht.size()), hcount*100.0/cr.ht.size());
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
// read from that instead. If mopt is 0 and pcomp_cmd
// is not empty then preprocess the input to a temporary file and read
// from that file instead. This may require a second temporary if the
// input is not a complete file (start is 0 and either bopt is 0 or
// n < bopt). If qopt is false then test the preprocessor by running it
// through the postprocessor desribed in pcomp or the second model
// in models and comparing the output checksum.
FileToCompress::FileToCompress(const char* filename, int64_t start,
                               int64_t n, int id) {

  // Initialize BWT buffer
  if (mopt==1 || mopt==2) {
    assert(bopt>0);
    assert(n>=0);
    int len=n;
    assert(int64_t(len)==n);
    pos=0;
    rle=0;
    buf.resize(len+5);
  }

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
    if (mopt==1 || mopt==2) {
      assert(i>=0 && i<int64_t(buf.size()-5));
      buf[i]=c;
    }
  }
  inputsize=sha1.size();
  memcpy(sha1result, sha1.result(), 20);
  if (!fseek64(in, start))
    error("fseek64 failed");

  // For modes -m1 and -m2, close input and compute BWT in buf
  if (mopt==1 || mopt==2) {
    fclose(in);
    in=0;
    int len=n;
    libzpaq::Array<int> w(len+(len==0));
    int idx=divbwt(&buf[0], &buf[0], &w[0], len);
    if (len>idx) memmove(&buf[idx+1], &buf[idx], len-idx);
    buf[idx]=255;
    for (int j=0; j<4; ++j) buf[len+j+1]=idx>>(j*8);
  }

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
    fprintf(stderr, " %1.0f -> %1.0f (%1.4f bpc)\n",
        double(insize), out.count-outsize,
        (out.count-outsize)*8.0/(insize+1e-6));
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
    for (int i=1; i<ncmd; ++i) 
      z.step(ntoi(cmd[i]), tolower(cmd[i][0])=='x');
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

  static const char builtin_models[]={

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

  if (n<1) return p=0,0;
  int len=0;
  for (p=builtin_models; (len=get2(p)) && n>1; --n, p+=len+2);
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
    case 'c':
    case 'a':
      if (ncmd<3) usage();
    case 'x':
    case 'l':
      if (ncmd<2) usage();
    case 'r':
    case 't':
      if (cmd[0][1]) usage();
      break;
    default: usage(); break;
  }

  // Set default block size to -b16 for -m1, -m2, else -b0
  if (bopt<-1) {
    if (mopt==1 || mopt==2) bopt=16000000;
    else bopt=0;
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
  if (**cmd=='c') fopt=true;  // force overwrite
  if (**cmd=='x' && ncmd>2) fopt=true;

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

    // Compile config file. Put result in hcomp, pcomp, pcomp_cmd
    if (config) {
      assert(mopt==0);
      try {
        compile_cmd(config);
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
  }

  // Run
  if (**cmd=='r' || **cmd=='t') {
    try {
      run();
    }
    catch(const char* msg) {
      fprintf(stderr, "Run error: %s\n", msg);
      exit(1);
    }
    return 0;
  }

  // List
  if (**cmd=='l') {
    list(archive.c_str());
    return 0;
  }

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
  if (**cmd=='x') {
    fprintf(stderr, "Extracting from %s\n", archive.c_str());
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
    in.f=fopen(filename, "rb");
    if (!in.f) {
      perror(filename);
      return;
    }
  }
  try {
    libzpaq::Decompresser d;
    in.count=1;
    double insize=0, outsize=0;  // total uncompressed, compressed sizes
    d.setInput(&in);
    double memory=0, max_memory=0;  // per block and largest
    StringWriter name, comment;
    char s[21];  // checksum
    printf("Block Checksum File Comment -> Compressed size for %s\n",
         filename);
    for (int i=1; d.findBlock(&memory); ++i) {
      if (memory>max_memory) max_memory=memory;

      // Verbose listing showing ZPAQL block header code
      if (verbose)
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

        // Display block number, checksum, file, size, compressed size
        printf("[%3d]", i);
        if (s[0])
          printf(" %02x%02x%02x%02x ",
              s[1]&255, s[2]&255, s[3]&255, s[4]&255);
        else
          printf("          ");
        printf("%s %s -> %1.0f\n",
            name.s.c_str(), comment.s.c_str(), double(in.count));
        insize+=atof(comment.s.c_str());
        outsize+=in.count;
        name.s="";
        comment.s="";
        in.count=0;
      }
    }
    printf("Total %1.0f -> %1.0f. %1.3f MB memory per thread needed.\n",
      insize, outsize, max_memory*1e-6);
  }
  catch (const char* msg) {}
}
