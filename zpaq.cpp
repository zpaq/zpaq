/* zpaq.cpp v4.04 - Archiver and compression development tool.

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

  g++ -O3 -s -msse2 -DNDEBUG zpaq.cpp libzpaq.cpp divsufsort.c -o zpaq

With Visual C++ (wildcard expansion won't work):

  cl /O2 /EHsc /DNDEBUG zpaq.cpp libzpaq.cpp divsufsort.c

In Linux, also use options -fopenmp -Dunix
In Windows you can use -fopenmp if you have pthreadGC2.dll in your PATH.
Other optimization options may be appropriate.

-DNDEBUG turns off run time checks in divsufsort.c. They are off
by default in zpaq.cpp and libzpaq.cpp. To turn them on use -DDEBUG.

To turn off JIT for non x86-32/64 machines or old processors without
SSE2 instructions, compile libzpaq with -DNOJIT

*/

#define _FILE_OFFSET_BITS 64  // In Linux make sizeof(off_t) == 8
#include "divsufsort.h"
#include "libzpaq.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <string>
#include <vector>
#include <map>
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
#include <dirent.h>
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
void compile_cmd(const char* cmd);
int numberOfProcessors();  // Default for -t
char slash();  // '/' in Linux, '\' in Windows
void run();  // do r and t commands

void usage() {
  fprintf(stderr, 
  "zpaq v4.04 - ZPAQ archiver and compression algorithm development tool.\n"
  "(C) 2011, Dell Inc. Written by Matt Mahoney. Compiled " __DATE__ ".\n"
  "This is free software under GPL v3. http://www.gnu.org/copyleft/gpl.html\n"
  "\n"
  "Usage: zpaq [-options] command     Commands [optional arguments...]\n"
  "  l arc                            List archive arc.zpaq contents\n"
  "  c arc [files...]                 Compress files or arc to new arc.zpaq\n"
  "  a arc  files...                  Add files\n"
  "  u arc [files...]                 Update and add files\n"
  "  d arc  files...                  Delete from archive\n"
  "  x arc [dir%c | output [file]]     Extract\n"
  "Notes: a and u are incremental. Archive is updated only if files are new\n"
  "or changed. u also updates or deletes internal files to match external\n"
  "files. x (extract) to dir or to saved paths compares without clobbering.\n"
  "Extracting file or concatenated contents to output overwrites if different.\n"
  "Options:\n"
  "  -f             Force extract to overwrite existing files that differ\n"
  "  -r             Recursively compress subdirectories\n"
  "  -m1...-m4      Compress faster...smaller (default -m1)\n"
  "  -mF[,N...]     Compress using F.cfg with optional numeric arguments\n"
  "  -bN            Compress in N MB blocks (default -b16 for -m1,-m2)\n"
  "  -b0            Compress 1 block per file (default for -m3,-m4,-mF)\n"
  "  -bs            Compress all files to 1 solid block (cannot be updated)\n"
  "  -n             Don't save tags, comments, or checksums (cannot update)\n"
  "  -tN            Work on N blocks at once (default -t%d cores detected)\n"
  "  -q             Don't test F.cfg postprocessor during compression\n"
  "Configuration file debugging (requires -mF):\n"
  "  l              Translate F.cfg to byte string\n"
  "  r [in [out]]   Run F.cfg as stand-alone program (default stdin, stdout)\n"
  "  t [N...]       Trace F.cfg with decimal/hex inputs\n"
  "  -h             Run/trace HCOMP (default PCOMP)\n",
  slash(), numberOfProcessors());
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
int topt=1;            // -t number of threads
bool verbose=false;    // -v verbose option
bool fopt=false;       // -f force overwrite with x
bool ropt=false;       // -r recurse subdirectories option
int mopt=1;            // -m compression method 1..4, 0 = config file
bool nopt=false;       // -n don't save tags, comments, checksums
bool hopt=false;       // -h run HCOMP
const char* config=0;  // config file name from -m
int args[9]={0};       // config file arguments
int64_t bopt=-2;       // -b in bytes, -1 = -bs (solid), -2 = default
bool qopt=false;       // -q don't test postprocessor
std::string archive;   // archive file name
const char* hcomp=0;   // COMP+HCOMP selected by -m, length in first 2 bytes
const char* pcomp=0;   // PCOMP with empty COMP header, selected by -m
const char* pcomp_cmd=0;//preprocessor command from config file from -m
bool iserror=false;    // return code, set true by error()

// Seek f to 64 bit pos, return true if successful
int fseek64(FILE* f, int64_t pos) {
#ifdef unix
  return fseeko(f, pos, SEEK_SET)==0;
#else
  rewind(f);
  HANDLE h=(HANDLE)_get_osfhandle(_fileno(f));
  LONG low=pos, high=pos>>32;
  SetFilePointer(h, low, &high, FILE_BEGIN);
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
  DWORD high=0;
  DWORD low=GetFileSize(h, &high);
  return int64_t(high)<<32|low;
#endif
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

// Print and run a command
int run_cmd(const std::string& cmd) {
  if (verbose)
    fprintf(stderr, "%s\n", cmd.c_str());
  return system(cmd.c_str());
}

// File for libzpaq (de)compression
struct File: public libzpaq::Reader, public libzpaq::Writer {
  FILE* f;
  int get() {return getc(f);}
  void put(int c) {putc(c, f);}
  File(FILE* f_=0): f(f_) {}
  ~File() {if (f && f!=stdin && f!=stdout) fclose(f);}
};

// To count bytes read or written
struct FileCount: public libzpaq::Reader, public libzpaq::Writer {
  FILE* f;
  int64_t count;
  FileCount(FILE* f_): f(f_), count(0) {}
  ~FileCount() {if (f && f!=stdin && f!=stdout) fclose(f);}
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

// Return '/' in Linux or '\' in Windows
char slash() {
#ifdef unix
  return '/';
#else
  return '\\';
#endif
}

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

// For appending files
class Appender {
  FILE* out;
  std::string outname;
  libzpaq::Array<unsigned char> buf;
public:
  Appender(): out(stdout) {buf.resize(1<<16);}
  ~Appender() {buf.resize(0); if (out && out!=stdout) fclose(out);}
  int64_t append(const std::string& file1, const std::string& file2);
};

// Append file2 to file1 and delete file2. Return number of bytes appended.
// "" means stdout, stdin. If file1 is different than previous call
// then close and reopen out, otherwise leave open.
int64_t Appender::append(const std::string& file1, const std::string& file2) {
  if (verbose)
    fprintf(stderr, "Appending to %s from %s", file1.c_str(), file2.c_str());
  FILE* in=stdin;
  if (file2!="") in=fopen(file2.c_str(), "rb");
  if (!in) {
    perror(file2.c_str());
    return 0;
  }
  if (outname!=file1) {
    outname=file1;
    if (out && out!=stdout) fclose(out);
    out=stdout;
    if (file1!="") out=fopen(file1.c_str(), "ab");
    if (!out) {
      perror(file1.c_str());
      if (in!=stdin) fclose(in);
      return 0;
    }
  }
  assert(in);
  assert(out);
  const int BUFSIZE=size(buf);
  int n;
  int64_t sum=0;
  while ((n=fread(&buf[0], 1, BUFSIZE, in))>0) {
    fwrite(&buf[0], 1, n, out);
    sum+=n;
    if (verbose) fprintf(stderr, ".");
  }
  if (in!=stdin) {
    fclose(in);
    remove(file2.c_str());
  }
  if (verbose) fprintf(stderr, "\n");
  return sum;
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
  if (in && in!=stdin) fclose(in);
  if (tmp_out!="") delete_file(tmp_out.c_str());
}

//////////////////////// Job ////////////////////////

// Information read from one archive segment or to be written to it.
//
// Compress: if memory>=0 then start new block.
// Open filename for reading, seek to offset, save filename (unless -n),
// size (unless -i), compress size bytes or to EOF if -1,
// recompute and write sha1 (unless -s). Ignore other fields.
//
// Decompress: if filename then close, open filename. Decompress
// to end of seg. Compute SHA1 and warn of mismatch. Ignore other fields.
//
// Archive map: offset=compressed size, size=comment or -1,
// memory=MB or 0 if not first seg in block, filename as read,
// sha1result[0]=1 if present else 0. cmp: =equal, #different, >not found,
// ?might differ, or (space) not tested.

struct Segment {
  int64_t csize;        // compressed size
  int64_t size;         // uncompressed size or -1 if unknown
  int memory;           // if first seg in block then c:mopt or x:mem, else -1
  StringWriter filename;// internal file name
  char sha1result[21];  // 1+checksum or 0 if absent from archive
  char cmp;             // =equal, #diff, >not found, ' 'not tested
  Segment();
  void print(FILE* f=stdout); // print contents
};

Segment::Segment(): csize(0), size(-1), memory(-1), cmp(' ') {
  sha1result[0]=0;
}

void Segment::print(FILE* f) {
  if (memory>=0) fprintf(f, "%6d", memory); else fprintf(f, "      ");
  fprintf(f, "%12.0f%12.0f ", double(size), double(csize));
  if (sha1result[0]==1)
    for (int i=1; i<5; ++i) fprintf(f, "%02x", sha1result[i]&255);
  else fprintf(f, "        ");
  fprintf(f, " %c%s\n", cmp, filename.s.c_str());
}

// State: Possible job states. A thread is initialized as READY. When main()
// is ready to start the thread it is set to RUNNING and runs  it. When
// the thread finishes, it sets its state to FINISHED or FINISHED_ERR
// if there is an error, signals main (using cv, protected by mutex),
// and exits. main then waits on the thread, receives the return status, and
// updates the state to OK or ERR.

typedef enum {READY, RUNNING, FINISHED_ERR, FINISHED, ERR, OK} State;

// A Job is a thread to compress or decompress one or more ZPAQ blocks.
// Compress: (**cmd is 'a' or 'c'): if id==0 then append to archive
// else write to tempfile(id), close out.
//
// Decompress (**cmd is 'x') always 1 block: seek to start.
// If begin->filename then open for write, else open tempfile(id)
// for write. Compress each seg, close output.

struct Job {
  State state;        // job state, protected by mutex, initially READY
  pthread_t tid;      // thread ID (for scheduler)
  int id;             // unique id>0 for each thread
  std::vector<Segment>::iterator begin, end;  // work list
  int64_t start;      // offset to start reading archive
  double size;        // estimated time to complete
  void print(FILE* f=stdout);
  Job(): state(READY), id(0), start(0), size(0) {}
  Job(int i, int64_t start,
      std::vector<Segment>::iterator b, std::vector<Segment>::iterator e);
};

Job::Job(int i, int64_t s, std::vector<Segment>::iterator b,
                std::vector<Segment>::iterator e):
  state(READY), id(i), begin(b), end(e), start(s), size(0) {
  while (b<e) size+=b->size, ++b;
}

void Job::print(FILE* f) {
  const char* states[]={
    "READY", "RUNNING", "FINISHED_ERR", "FINISHED", "ERR", "OK"};
  fprintf(f, "Job %d: %1.0f", id, size);
  if (start) fprintf(f, " +%1.0f", double(start));
  fprintf(f, " %s\n", states[state]);
  for (std::vector<Segment>::iterator p=begin; p!=end; ++p)
    p->print(f);
}

// segment update indicator?
bool isdel(char c) {return c=='#' || c=='>' || c=='?';}

// Return true if cmp is a type to be extracted, either > (does not exist)
// or # or ? (different and overwrite is allowed).
bool isextract(char cmp) {
  assert(**cmd=='x');
  if (cmp=='>') return true;
  if (fopt && isdel(cmp)) return true;
  int len;
  return ((cmp=='#' || cmp=='?') && ncmd>2 
      && (len=strlen(cmd[2]))>0 && strchr("/\\", cmd[2][len-1])==0);
}

/////////////////// compress ////////////////////////

// Compress 1 block
void compress(Job& job) {
  assert(job.end>job.begin);  // at least 1 segment?
  assert(job.begin->memory>=0);  // start of block?

  // Open output file
  libzpaq::Compressor c;
  std::string output=tempname(job.id);
  FileCount out(fopen(output.c_str(), "wb"));
  if (!out.f) {
    perror(output.c_str());
    error("file creation failed");
  }
  c.setOutput(&out);
  double outsize=-1;

  // Compress segments
  bool first=false;  // first segment in block?
  for (std::vector<Segment>::iterator p=job.begin; p!=job.end; ++p) {

    // Write block header
    if (p->memory>=0) {
      if (p!=job.begin) c.endBlock();  // end previous block
      if (!nopt) c.writeTag();

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
      first=true;
    }

    // Compress segment
    FileToCompress in(p->filename.s.c_str(), p->csize, p->size, job.id);
    int64_t insize=in.filesize();
    c.setInput(&in);
    const bool isname=p->csize==0 && (ncmd>2 || **cmd!='c');
    c.startSegment(isname ? p->filename.s.c_str() : 0, // name
                   nopt ? 0 : itos(p->size).c_str());  // comment (size)
    if (first) {
      if (pcomp) c.postProcess(pcomp+8, get2(pcomp)-6);
      else c.postProcess(0);
      first=false;
    }
    c.compress();
    c.endSegment(nopt ? 0 : in.sha1());
    fprintf(stderr, "Compressed: %s", p->filename.s.c_str());
    if (job.start>0) fprintf(stderr, "+%1.0f", double(job.start));
    fprintf(stderr, " %1.0f -> %1.0f (%1.4f bpc)\n",
        double(insize), out.count-outsize,
        (out.count-outsize)*8.0/(insize+1e-6));
    outsize=out.count;
  }
  c.endBlock();
  fclose(out.f);
  out.f=0;
  if (job.id==0 && mopt==0)
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

// Decompress a list of segments. If memory>=0 then expect a block.
// Discard output for segments not marked # or >, or if marked #
// and overwrite is not allowed (ncmd==2). Otherwise output
// to the last named segment filename or to tempname(id) if none.
// Assume all segments of a file are marked the same way.

void decompress(Job& job) {
  assert(job.end>job.begin);

  // Open input and seek to start of block
  File in(fopen(archive.c_str(), "rb"));
  if (!in.f) {
    perror(archive.c_str());
    error("cannot read archive");
  }
  if (job.start>0 && !fseek64(in.f, job.start))
    error("fseek64");

  // Decompress segments
  libzpaq::Decompresser d;
  d.setInput(&in);
  File out(0);
  std::string filename=tempname(job.id);
  
  for (std::vector<Segment>::iterator p=job.begin; p!=job.end; ++p) {
    if (p->memory>=0 && !d.findBlock()) error("block expected");
    if (!d.findFilename()) error ("segment expected");
    d.readComment();

    // Open new output file
    if (p==job.begin || p->filename.s!="") {
      if (p->filename.s!="") filename=p->filename.s;
      if (out.f) fclose(out.f), out.f=0;
      if (isextract(p->cmp)) {
        if (p->filename.s!="")
          fprintf(stderr, "Extracting: %s\n", filename.c_str());
        makepath(filename);
        out.f=fopen(filename.c_str(), "wb");
        if (!out.f) {
          perror(filename.c_str());
          error("cannot create file");
        }
      }
    }

    // Decompress segment and verify checksum
    d.setOutput(isextract(p->cmp) ? &out : 0);
    libzpaq::SHA1 sha1;
    d.setSHA1(&sha1);
    d.decompress();
    char sha1string[21];
    d.readSegmentEnd(sha1string);
    if (sha1string[0] && memcmp(sha1string+1, sha1.result(), 20)) {
      fprintf(stderr, "CHECKSUM ERROR: %s\n", filename.c_str());
    }
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
    if (strchr("cau", **cmd)) compress(*job);
    else if (**cmd=='x') decompress(*job);
  }
  catch (const char* msg) {
    result=msg;
  }

  // Let controlling thread know we're done and the result
#ifdef PTHREAD
  pthread_mutex_lock(&mutex);
  job->state=result?FINISHED_ERR:FINISHED;
  pthread_cond_signal(&cv);
  pthread_mutex_unlock(&mutex);
#else
  WaitForSingleObject(mutex, INFINITE);
  job->state=result?FINISHED_ERR:FINISHED;
  ReleaseMutex(mutex);
#endif
  return 0;
}


/////////////////////////// User interface //////////////////////////////

// Put n'th model into p and return its length (including length code).
// If there is no n'th model, set p=0 and return 0.
int getmodel(int n, const char* &p) {

  static const char builtin_models[]={

  // Model 1 fast
  26,0,1,2,0,0,2,3,16,8,19,0,0,96,4,28,
  59,10,59,112,25,10,59,10,59,112,56,0,

  // Model 2 bwtrle1 -m1
  21,0,1,0,27,27,1,3,7,0,-38,80,47,3,9,63,
  1,12,65,52,60,56,0,

  // Model 3 bwtrle1 post -m1
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

  // Model 4 bwt2 -m2
  17,0,1,0,27,27,2,3,5,8,12,0,0,95,1,52,
  60,56,0,

  // Model 5 bwt2 post -m2
  111,0,1,0,27,27,0,0,-17,-1,39,4,96,9,63,95,
  10,68,10,-49,8,-124,10,-49,8,-124,10,-49,8,-124,80,55,
  1,65,55,2,65,-17,0,47,10,10,68,1,-81,-1,88,27,
  49,63,-15,28,27,119,1,4,-122,112,26,24,3,-17,-1,3,
  24,47,-11,12,66,-23,47,9,92,27,49,94,26,113,9,63,
  -13,74,9,23,2,66,-23,47,9,92,27,49,94,26,113,9,
  63,-13,31,1,67,-33,0,39,6,94,75,68,57,63,-11,56,
  0,

  // Model 6 mid -m3
  69,0,3,3,0,0,8,3,5,8,13,0,8,17,1,8,
  18,2,8,18,3,8,19,4,4,22,24,7,16,0,7,24,
  -1,0,17,104,74,4,95,1,59,112,10,25,59,112,10,25,
  59,112,10,25,59,112,10,25,59,112,10,25,59,10,59,112,
  25,69,-49,8,112,56,0,

  // Model 7 max -m4
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

// Compare file begin->filename.s with the range of segments in [begin,end)
// representing that file. Set all of the file's segments to
// '=' if equal, '#' if different, '?' if exists but can't be compared,
// '>' if not found, or ' ' if filename is "." and should be ignored.
// If result is not 0 then don't read files and just set result.

void compare(std::vector<Segment>::iterator begin,
             std::vector<Segment>::iterator end, char result=0) {
  assert(end>=begin);
  if (begin==end) return;

  // If result is given then set it
  std::vector<Segment>::iterator p;
  if (result) {
    for (p=begin; p!=end && (p==begin || p->filename.s==""); ++p)
      p->cmp=result;
    return;
  }

  // If filename is "." then leave blank
  if (begin->filename.s==".") return;

  // Read file. If not found then >
  FILE *f=fopen(begin->filename.s.c_str(), "rb");
  if (!f) {
    for (p=begin; p!=end && (p==begin || p->filename.s==""); ++p)
      p->cmp='>';  // other segments not found
    return;
  }

  // If any missing checksums or prior sizes then assume possibly different
  int diff=0;  // 0,1,2 -> =,?,#
  for (p=begin; p!=end && !diff && (p==begin || p->filename.s==""); ++p) {
    if (p>begin && p[-1].size<0) diff=1;
    if (p->sha1result[0]!=1) diff=1;
  }

  // Find first mismatched segment
  for (p=begin; p!=end && !diff && (p==begin || p->filename.s==""); ++p) {
    libzpaq::SHA1 sha1;
    int c;
    for (int64_t l=0; l!=p->size && (c=getc(f))!=EOF; ++l)
      sha1.put(c);
    if (memcmp(sha1.result(), p->sha1result+1, 20))
      diff=2;
  }

  // Check that files are the same size
  if (!diff && getc(f)!=EOF) diff=2;
  fclose(f);

  // Set all segments of the file to the same result
  for (p=begin; p!=end && (p==begin || p->filename.s==""); ++p)
    p->cmp="=?#"[diff];
}

// A set of strings. You can add, remove, test membership, or iterate
class StringSet {
public:
  StringSet(): cur(m.begin()) {}
  void add(const std::string& s) {
    m[s]=true;
    cur=m.begin();
  }
  void remove(const std::string& s) {
    p=m.find(s); if (p!=m.end()) p->second=false;
  }
  bool in(const std::string& s) {  // is s in the set?
    p=m.find(s); return p!=m.end() && p->second;
  }
  bool next(std::string& s) {  // put next string in s, return false at end
    while (cur!=m.end() && !cur->second) ++cur;
    if (cur==m.end()) return false;
    s=cur->first;
    ++cur;
    return true;
  }
private:
  std::map<std::string, bool> m;
  std::map<std::string, bool>::iterator p, cur;
};

// Return the path of filename fn minus the bare name after the last slash
std::string path(std::string fn) {
  for (int i=size(fn)-1; i>=0; --i)
    if (fn[i]=='\\' || fn[i]=='/' || (i==1 && fn[i]==':'))
      return fn.substr(0, i+1);
  return "";
}

// Insert filename into ss. In Windows, expand wildcards. If -r then
// recursively insert directory contents. Return number of insertions.
int insert(const std::string& filename, StringSet& ss) {
  int result=0;
#ifdef unix

  // File or directory?
  struct stat sb;
  if (lstat(filename.c_str(), &sb)) {
    perror(filename.c_str());
    return 0;
  }
  if (!ropt || S_ISREG(sb.st_mode)) { // add regular file
    if (verbose) fprintf(stderr, "%s\n", filename.c_str());
    ss.add(filename);
    return 1;
  }

  // Scan directory and add contents except . and ..
  if (ropt && S_ISDIR(sb.st_mode)) {
    DIR* dirp=opendir(filename.c_str());
    if (!dirp) {
      perror(filename.c_str());
      return 0;
    }
    for (dirent* dp=readdir(dirp); dp; dp=readdir(dirp)) {
      if (strcmp(".", dp->d_name) && strcmp("..", dp->d_name)) {
        std::string s=filename;
        if (s=="" || s[size(s)-1]!='/') s+="/";
        s+=dp->d_name;
        result+=insert(s, ss);
      }
    }
    closedir(dirp);
  }

#else  // Windows

  WIN32_FIND_DATA ffd;
  HANDLE h=FindFirstFile(filename.c_str(), &ffd);
  while (h!=INVALID_HANDLE_VALUE) {
    if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
      if (ropt && strcmp(".", ffd.cFileName) && strcmp("..", ffd.cFileName))
        result+=insert(path(filename)+ffd.cFileName+"\\*", ss);
    }
    else {
      std::string s=path(filename)+ffd.cFileName;
      if (verbose) fprintf(stderr, "%s\n", s.c_str());
      ss.add(s);
      ++result;
    }
    if (!FindNextFile(h, &ffd)) break;
  }
#endif
  return result;
}

int main(int argc, char** argv) {

  // Parse command line arguments (see usage()).
  // Compile config file if any.
  // r,t: Run or trace config file and stop.
  // l,a,u,x,d: Build segment map arc[]. If listing, then list and stop.
  // x: Rename files in arc[] for extraction.
  // c,a,u,d: Collect argument list.
  // a,u,d: Test whether files can be deleted from solid archive.
  // c,a,u,x: Compare internal and external files.
  // c,a,u,x: Construct list of compression/decompression jobs, 1 per block.
  // c,a,u,x: Run jobs in parallel to temporary output.
  // c,a,u: If any errors, then don't update archive (skip next 2 steps).
  // c,a,u,d: Delete old files (c=all) from archive in place.
  // c,a,u,x: Concatenate temporary files to output files.
  // c,a,u,x: Delete any leftover temporary files.

  // Start timer
  time_t start_time=time(0);

  // Process options and set various Xopt variables.
  cmd=argv+1;
  ncmd=argc-1;
  topt=numberOfProcessors();
  while (ncmd>0 && cmd[0][0]=='-') {
    switch(cmd[0][1]) {
      case 'm':
        if (isdigit(cmd[0][2])) mopt=atoi(cmd[0]+2);
        else config=cmd[0]+2, mopt=0;
        break;
      case 'b':
        if (isdigit(cmd[0][2])) bopt=atof(cmd[0]+2)*1000000+0.25;
        else if (cmd[0][2]=='s') bopt=-1;
        else usage();
        break;
      case 't': topt=atoi(cmd[0]+2); break;
      case 'f': fopt=true; break;
      case 'r': ropt=true; break;
      case 'n': nopt=true; break;
      case 'h': hopt=true; break;
      case 'q': qopt=true; break;
      case 'v': verbose=true; break;  // undocumented
      default: usage(); break;
    }
    ++cmd;
    --ncmd;
  }

  // Process command to cmd[ncmd] = {command,archive,files...}
  if (ncmd<1 || !cmd || !*cmd) usage();
  switch(cmd[0][0]) {
    case 'a':
    case 'd':
      if (ncmd<3) usage();  // needs at least 1 file
    case 'u':
    case 'c':
    case 'x':
      if (ncmd<2) usage();  // needs at least archive.zpaq
    case 'l':
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

  // Check and set -m, -t, -b to valid ranges.
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

  // Initialize hcomp, pcomp, pcomp_cmd for commands l, c, a, u, t, r
  if (strchr("lcautr", **cmd)) {

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

  // list hcomp, pcomp with no archive
  if (**cmd=='l' && ncmd==1) {
    int len=0;
    if (hcomp && (len=get2(hcomp))>0) {
      printf("char hcomp[%d]={\n  ", len+2);
      for (int i=0; i<len+1; ++i) {
        printf("%d,", hcomp[i]);
        if (i%16==15) printf("\n  ");
      }
      printf("%d};\n", hcomp[len+1]);
    }
    if (pcomp && (len=get2(pcomp))>0) {
      printf("char pcomp[%d]={\n  ", len+2);
      for (int i=0; i<len+1; ++i) {
        printf("%d,", pcomp[i]);
        if (i%16==15) printf("\n  ");
      }
      printf("%d};\n", pcomp[len+1]);
    }
    return 0;
  }

  // Run or trace config file
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

  // Get archive name. Append .zpaq if not already.
  if (ncmd>1) {
    archive=cmd[1];
    if (size(archive)<5 || archive.substr(size(archive)-5)!=".zpaq")
      archive+=".zpaq";
  }
  fprintf(stderr, "Archive: %s\n", archive.c_str());

  // Read archive into arc[].{csize, size, memory, filename, sha1result}
  assert(strchr("lcauxd", **cmd));
  std::vector<Segment> arc;  // archive map
  if (strchr("lauxd", **cmd)) {
    FileCount in(fopen(archive.c_str(), "rb"));
    if (in.f) {
      if (**cmd=='l')
          printf("Block MB      Size  Compressed Checksum  %s\n"
                 "--------  --------  ---------- --------  -----\n", 
                 archive.c_str());
      int64_t last_offset=0;
      bool done=false;
      while (!done) {  // catch errors and loop to EOF
        try {
          libzpaq::Decompresser d;
          d.setInput(&in);
          double memory;

          // loop once per segment
          while (d.findBlock(&memory)) {
            Segment seg;
            seg.memory=int(memory/1000000.0+1);
            while (d.findFilename(&seg.filename)) {
              if (**cmd=='l' && size(arc)>0) arc.back().print();
              StringWriter comment;
              d.readComment(&comment);
              seg.size=0; // read decimal size from comment, or else -1
              if (size(comment.s)<1 || comment.s[0]<'0' || comment.s[0]>'9')
                seg.size=-1;
              else {
                for (int i=0; i<size(comment.s)
                     && comment.s[i]>='0' && comment.s[i]<='9'; ++i)
                  seg.size=seg.size*10+comment.s[i]-'0';
              }
              d.readSegmentEnd(seg.sha1result);
              seg.csize=in.count-last_offset;
              last_offset=in.count;
              arc.push_back(seg);
              seg.filename.s="";
              seg.memory=-1;
            }
            if (size(arc)>0) {
              ++last_offset;
              ++arc.back().csize;
            }
          }
          done=true;
          if (**cmd=='l' && size(arc)>0) arc.back().print();
        }
        catch(const char* msg) {
          fprintf(stderr, "%s: attempting to recover\n", msg);
        }
      }
    }
    else if (strchr("xdl", **cmd)) {  // not found (a,u: OK)
      perror(archive.c_str());
      return 1;
    }
  }
  if (**cmd=='l') return 0;

  // c,a,u,d: Collect set of filename arguments in args
  StringSet args;
  if (strchr("caud", **cmd)) {
    int filecount=0;
    for (int i=2-(ncmd==2 && **cmd=='c'); i<ncmd; ++i) {
      filecount+=insert(cmd[i], args);
    }
    fprintf(stderr, "%d files\n", filecount);
  }

  // x: Fix filenames in arc[] to the names used for extraction
  for (int i=0; i<size(arc); ++i) {
    if (i==0 && arc[i].filename.s=="" && size(archive)>5) // default name
      arc[0].filename.s=archive.substr(0, size(archive)-5);
    if (**cmd=='x' && ncmd==3) { // x arc out/ : replace path with out/
      int len=strlen(cmd[2]);
      if (len>0 && strchr("/\\", cmd[2][len-1])) {   // "out/" ?
        if (arc[i].filename.s!="")
          arc[i].filename.s=cmd[2]+strip(arc[i].filename.s);
      }
      else  // rename whole archive to out
        arc[i].filename.s=i?"":cmd[2];
    }
    if (**cmd=='x' && ncmd>3 && arc[i].filename.s!="") // x out file
      arc[i].filename.s=arc[i].filename.s==cmd[3]?cmd[2]:".";
    for (int j=0; j<size(arc[i].filename.s); ++j)  // fix slashes per OS
      if (strchr("/\\", arc[i].filename.s[j]))
        arc[i].filename.s[j]=slash();
  }

  // Compare internal and external files.
  // x,u: compare, mark as =,?,#,>
  // c,a: if in args then compare, mark =,?,#,>.
  // d: if in args then mark >.

  for (int i=0; i<size(arc); ++i) {
    if (arc[i].filename.s!="") {  // first segment of file?
      const char* fn=arc[i].filename.s.c_str();  // compare, report results
      if (strchr("ux", **cmd) 
         || (strchr("ca", **cmd) && args.in(arc[i].filename.s)))
        compare(arc.begin()+i, arc.end());
      if (**cmd=='d' && args.in(arc[i].filename.s))
        compare(arc.begin()+i, arc.end(), '>');

      // report results that won't result in compression or decompression
      if (**cmd=='x') {
        if (!isextract(arc[i].cmp)) {
          if (arc[i].cmp=='=')
            fprintf(stderr, "Identical: %s\n", fn);
          else if (!fopt && arc[i].cmp=='?')
            fprintf(stderr, "Cannot compare, NOT extracted: %s\n", fn);
          else if (!fopt && arc[i].cmp=='#')
            fprintf(stderr, "Differs, NOT extracted: %s\n", fn);
        }
      }
      else if (arc[i].cmp=='=')
        fprintf(stderr, "Identical, not updated: %s\n", fn);
      else if (arc[i].cmp=='>')
        fprintf(stderr, "Deleted from archive: %s\n", fn);
    }
  }

  if (verbose) for (int i=0; i<size(arc); ++i) arc[i].print(stderr);
        
  // a,u,d: fail in case of partial block deletion, i.e. #, ? or > followed
  // by not #, ?, or > in the same block.
  if (strchr("aud", **cmd)) {
    int del=-1;  // index of last deleted file in block or -1
    for (int i=0; i<size(arc); ++i) {
      if (arc[i].memory>=0) del=-1;  // start of block
      if (isdel(arc[i].cmp)) {
        if (del==-1) del=i;
      }
      else if (del>=0) {
        fprintf(stderr, 
          "Error: cannot delete %s in segment %d"
          " and keep %s in segment %d in solid archive\n",
          arc[del].filename.s.c_str(), del+1,
          arc[i].filename.s.c_str(), i+1);
        return 1;
      }
    }
  }

  // Schedule jobs, one per block:
  // u: for each seg marked # append block marked < per -b
  // u: for each arg with no matching seg, append block < per -b
  // c,a: for each arg, if no seg is = then append block marked < per -b
  // x: Schedule each block up to the last seg marked >
  // x out, x -f: Schedule up to last seg marked >, #, ?

  // c,u,a: update args to a list of files to be added
  std::vector<Job> jobs;
  const int arcsize=size(arc);  // size not including jobs appended
  if (strchr("cua", **cmd)) {
    for (int i=0; i<arcsize; ++i) {
      if (arc[i].filename.s!="") {
        if (arc[i].cmp=='=') args.remove(arc[i].filename.s);
        if (arc[i].cmp=='#' || arc[i].cmp=='?') args.add(arc[i].filename.s);
      }
    }

    // c,u,a: add args
    std::string filename;
    while (args.next(filename)) {
      FILE* f=fopen(filename.c_str(), "rb");  // to get size
      if (!f) perror(filename.c_str());
      else {
        int64_t sz=filesize(f);
        fclose(f);
        int64_t blk=sz;
        if (bopt>0) blk=bopt;
        for (int64_t j=0; j==0 || j<sz; j+=blk) {  // split into blocks
          Segment seg;
          seg.cmp='<';  // add
          seg.csize=j;  // offset
          seg.size=blk;
          if (seg.csize+seg.size>sz) seg.size=sz-seg.csize;
          seg.memory=(size(arc)==arcsize || bopt>=0)-1;  // 0=block else -1
          seg.filename.s=filename;
          arc.push_back(seg);
          if (sz<=0) break;
        }
      }
    }

    // c,u,a: schedule jobs on block boundaries
    if (topt==1) { // One single-threaded job
      if (size(arc)>arcsize)
        jobs.push_back(Job(0, 0, arc.begin()+arcsize, arc.end()));
    }
    else { // One job per block
      int start=arcsize, j=0;
      for (int i=arcsize; i<=size(arc); ++i) {
        if (i>start && (i==size(arc) || arc[i].memory>=0)) {
          jobs.push_back(Job(j++, 0, arc.begin()+start, arc.begin()+i));
          start=i;
        }
      }
    }
  }

  // x: schedule extraction jobs marked > from the start of block
  // x out, x -f: schedule up to last > or # or ? in block
  if (**cmd=='x') {
    int64_t start=0;  // start of segment
    for (int i=0; i<size(arc); ++i) {
      if (arc[i].memory>=0) {  // start of block
        int end=i;  // 1 past last seg to extract in block
        for (int j=i; j<size(arc) && (j==i || arc[j].memory<0); ++j)
          if (isextract(arc[j].cmp)) end=j+1;
        if (end>i)
          jobs.push_back(
              Job(size(jobs), start, arc.begin()+i, arc.begin()+end));
      }
      start+=arc[i].csize;
    }
  }

  if (verbose) for (int i=0; i<size(jobs); ++i) jobs[i].print(stderr);
  
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
  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
  pthread_mutex_lock(&mutex);  // locked
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
      pthread_create(&jobs[bi].tid, &attr, thread, &jobs[bi]);
#else
      jobs[bi].tid=CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)thread,
          &jobs[bi], 0, NULL);
#endif
    }

    // If no jobs can start then wait for one to finish
    else {
#ifdef PTHREAD
      pthread_cond_wait(&cv, &mutex);  // wait on cv

      // Join any finished threads. Usually that is the one
      // that signaled it, but there may be others.
      for (int i=0; i<size(jobs); ++i) {
        if (jobs[i].state==FINISHED || jobs[i].state==FINISHED_ERR) {
          void* status=0;
          pthread_join(jobs[i].tid, &status);
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
  pthread_mutex_unlock(&mutex);
#endif

  // Report unfinished jobs
  for (int i=0; i<size(jobs); ++i) {
    if (jobs[i].state!=OK) {
      fprintf(stderr, "failed: ");
      jobs[i].print(stderr);
    }
  }
  if (iserror && strchr("aud", **cmd))
    fprintf(stderr, "Archive %s not updated\n", archive.c_str());

  // c: delete archive
  if (!iserror && **cmd=='c')
    delete_file(archive.c_str());

  // a,u,d: delete segs in place in archive marked with > or # or ?.
  // In case of error, do not modify archive
  if (!iserror && strchr("aud", **cmd)) {
    FILE* f=fopen(archive.c_str(), "rb+");
    if (f) {
      if (verbose) fprintf(stderr, "Moving segments in %s\n", archive.c_str());
      libzpaq::Array<char> buf(1<<16);
      int64_t rbegin=0, rend=0, wbegin=0;
      for (int i=0; i<arcsize;) {

        // find segment to copy
        if (isdel(arc[i].cmp))
          rbegin+=arc[i++].csize;
        else {
          rend=rbegin;
          for (; i<arcsize && !isdel(arc[i].cmp); ++i)
            rend+=arc[i].csize;
          bool eob=(i==arcsize || arc[i].memory>=0);  // end of block?

          // copy rbegin..rend-1 to wbegin
          if (verbose)
            fprintf(stderr, "%s moved %1.0f..%1.0f -> %1.0f..%1.0f eob=%d\n",
                archive.c_str(), double(rbegin), double(rend),
                double(wbegin), double(wbegin+rend-rbegin), eob);
          assert(rbegin>=wbegin);
          if (rbegin>wbegin) {
            while (rbegin<rend) {
              int n=buf.size();  // copy n bytes
              if (rend-rbegin<n) n=rend-rbegin;
              fseek64(f, rbegin);
              int nr=fread(&buf[0], 1, n, f);
              if (nr!=n)
                fprintf(stderr, "Error reading %d of %d bytes at %1.0f in %s\n",
                    nr, n, double(rbegin), archive.c_str()), exit(1);
              fseek64(f, wbegin);
              fwrite(&buf[0], 1, n, f);
              rbegin+=n;
              wbegin+=n;
            }
          }
          else
            wbegin=rbegin=rend;

          // If the copied area ends in the middle of a block then replace
          // the EOB that was lost from the end of the last deleted segment.
          if (!eob) {
            fseek64(f, wbegin);
            putc(255, f);
            ++wbegin;
          }
        }
      }

      // Truncate
      assert(wbegin<=rbegin);
      if (wbegin<rbegin) {
        if (verbose)
          fprintf(stderr, "%s truncated %1.0f -> %1.0f\n",
              archive.c_str(), double(rbegin), double(wbegin));
        fseek64(f, wbegin);
#ifdef unix
        if (ftruncate(fileno(f), wbegin)) perror(archive.c_str()), exit(1);
#else
        SetEndOfFile((HANDLE)_get_osfhandle(_fileno(f)));
#endif
      }
      fclose(f);
    }
  }

  // Append temporary job output.
  // c,a,u: append to archive if no errors.
  // x: append to last named file in a job marked >

  if (!iserror && strchr("cau", **cmd)) {
    Appender a;
    int64_t sum=0;
    for (int i=0; i<size(jobs); ++i)
      sum+=a.append(archive, tempname(i));
    fprintf(stderr, "-> %1.0f, ", double(sum));
  }
  else if (**cmd=='x') {
    Appender a;
    std::vector<Segment>::iterator p;
    std::string lastfile="";
    for (int i=0; i<size(jobs); ++i) {

      // Append temporary output to last named file
      for (p=jobs[i].begin; p!=jobs[i].end; ++p) {
        if (isextract(p->cmp)) {
          if (p->filename.s=="") {
            assert(i>0);
            assert(lastfile!="");
            a.append(lastfile, tempname(i));
            break;
          }
          else
            break;
        }
      }

      // Update last named file
      for (p=jobs[i].begin; p!=jobs[i].end; ++p)
        if (isextract(p->cmp) && p->filename.s!="") lastfile=p->filename.s;
    }
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
  int get() {return n>0 ? (n--, *ptr++&255) : -1;}
};

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
    z.flush();
  }
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
  if (verbose) fprintf(stderr, "%s ", s);

  // Substitute parameters $1..$9 with args[0..8], $i+n with args[i-1]+n
  if (s[0]=='$' && s[1]>='1' && s[1]<='9') {
    int i=s[1]-'1';
    assert(i>=0 && i<9);
    int val=args[i];
    if (s[2]=='+')
      val+=atoi(s+3);
    sprintf(s, "%d", val);
    if (verbose) fprintf(stderr, "(%s) ", s);
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
  if (verbose) fprintf(stderr, "\n");
  int indent=0;  // program listing indentation
  while (comp.len()<0x10000) {
    if (verbose) {
      fprintf(stderr, "(%4d) ", comp.len()-comp_begin);
      for (int i=0; i<indent; ++i) fprintf(stderr, "  ");
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
        fprintf(stderr, "(%s 3 (%d 3) lj 0 0)",
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
        if (verbose) fprintf(stderr, "((%d) %s %d (to %d)) ",
          a-comp_begin-1, opcodelist[comp(a-1)], j, comp.len()-comp_begin+2);
      }
      else {  // IFL, IFNOTL
        int j=comp.len()-comp_begin+2+(op==LJ);
        assert(j>=0);
        comp[a]=j&255;
        comp[a+1]=(j>>8)&255;
        if (verbose) fprintf(stderr, "((%d) lj %d) ", a-comp_begin-1, j);
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
        if (verbose) fprintf(stderr, "((%d) %s %d (to %d))\n",
          a-comp_begin-1, opcodelist[comp(a-1)], j, comp.len()-comp_begin);
      }
      else {
        assert(a+1<comp.len());
        j=comp.len()-comp_begin;
        comp[a]=j&255;
        comp[a+1]=(j>>8)&255;
        if (verbose) fprintf(stderr, "((%d) lj %d)\n", a-1, j);
      }
      --indent;
    }
    else if (op==DO) {
      do_stack.push(comp.len());
      if (verbose) fprintf(stderr, "\n");
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
          fprintf(stderr, "(%s %d (to %d)) ", opcodelist[op], j,
                 comp.len()-comp_begin+2+j);
      }
      else {  // backward long jump
        j=a-comp_begin;
        assert(j>=0 && j<comp.len()-comp_begin);
        if (op==WHILE) {
          comp.put(JF);
          comp.put(3);
          if (verbose) fprintf(stderr, "(jf 3) ");


        }
        if (op==UNTIL) {
          comp.put(JT);
          comp.put(3);
          if (verbose) fprintf(stderr, "(jt 3) ");
        }
        op=LJ;
        operand=j&255;
        operand2=j>>8;
        if (verbose) fprintf(stderr, "(lj %d) ", j);
      }
      --indent;
    }
    else if ((op&7)==7) { // 2 byte operand, read N
      if (op==LJ) {
        operand=rtoken(in, 0, 65535);
        operand2=operand>>8;
        operand&=255;
        if (verbose) fprintf(stderr, "(to %d) ", operand+256*operand2);
      }
      else if (op==JT || op==JF || op==JMP) {
        operand=rtoken(in, -128, 127);
        if (verbose) fprintf(stderr, "(to %d) ", comp.len()-comp_begin+2+operand);
        operand&=255;
      }
      else
        operand=rtoken(in, 0, 255);
    }
    if (verbose) {
      if (operand2>=0)
        fprintf(stderr, "(%d %d %d)\n", op, operand, operand2);
      else if (operand>=0)
        fprintf(stderr, "(%d %d)\n", op, operand);
      else if (op>=0 && op<=255)
        fprintf(stderr, "(%d)\n", op);
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
  if (verbose) fprintf(stderr, "\n");
  for (int i=0; i<n; ++i) {
    if (verbose) fprintf(stderr, "  ");
    rtoken(in, i, i);
    CompType type=CompType(rtoken(in, compname));
    hcomp.put(type);
    int clen=libzpaq::compsize[type];
    assert(clen>0 && clen<10);
    for (int j=1; j<clen; ++j)
      hcomp.put(rtoken(in, 0, 255));
    if (verbose) fprintf(stderr, "\n");
  }
  hcomp.put(0); // END

  // Compile HCOMP
  rtoken(in, "hcomp");
  CompType op=compile_comp(in, hcomp);
  if (verbose) fprintf(stderr, "\n");

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

// Pad pcomp string with an empty COMP header with ph,pm from hcomp
void fix_pcomp(const std::string& hcomp, std::string& pcomp) {
  if (size(hcomp)>=8 && size(pcomp)>=2) {
    pcomp=hcomp.substr(0, 8)+pcomp.substr(2);
    pcomp[0]=(size(pcomp)-2)&255;  // new length of PCOMP
    pcomp[1]=(size(pcomp)-2)>>8;
    pcomp[6]=pcomp[7]=0;  // n=0 components
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
  if (verbose) {
    fprintf(stderr, "Using model %s", filename.c_str());
    for (int i=0; i<argnum; ++i)
      fprintf(stderr, ",%d", args[i]);
    fprintf(stderr, "\n");
  }
  static String hcomp_s, pcomp_s, pcomp_cmd_s;
  compile(in, hcomp_s, pcomp_s, pcomp_cmd_s);
  if (verbose) fprintf(stderr, "\n\n");
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

////////////////////////// step, stat ///////////////////////////

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

// Print compression component statistics
int Predictor::stat(int id) {
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

}  // end namespace libzpaq
