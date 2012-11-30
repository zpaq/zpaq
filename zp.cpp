/* zp.cpp v1.01 - Parallel ZPAQ compressor

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

zp is a ZPAQ file compressor and archiver. To decompress, use unzp
or another ZPAQ compatible decoder. Usage:

  zp [-options] files...

Options:

  -c = compress to standard output. Default: file -> file.zpaq
  -bN = block size of about N MB. Default -b32
  -mN = compress with method N (1=fastest...4=best). Default -m1
  -tN = use N threads. Default = number of processors.
  -r = remove input files after compression.
  -v = verbose. Default: no output except for errors.

By default, zp compresses each file to a single file archive and
adds a .zpaq extension. The file name is saved as specified on
the command line. The size is saved as a comment as a decimal string.
Checksums are saved. Archives begin with a locator tag so that it
can be found by ZPAQ decompressers even if embedded in other data.

-c causes all output to be concatenated to standard output in the
same format to create a multi-file archive. To create a new archive:

  zp -c files... > archive.zpaq

Or to append to an existing archive:

  zp -c files... >> archive.zpaq

zp -c will fail if you don't redirect the output to a file or pipe.

Each file is compressed in a separate thread in parallel if possible.
Also, large input files are split into blocks that are compressed in
parallel by separate threads, and can be later decompressed in parallel
by unzp. Option -bN specifies a block size of N*1048576 - 256 bytes
(about N MB). Larger blocks compress better but reduce opportunities
for parallelism. Using a block size smaller than the input size
divided by the number of processors will compress worse with no
increase in speed, but will reduce the memory required to compress
using the two faster models (-m1, -m2) and later to decompress.
Compression levels are as follows:

  Level Model    Memory required per thread
  ----- -----    --------------------------
  -m1   bwtrle1  5x block size (default 160 MB)
  -m2   bwt2     5x block size (default 160 MB)
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
CPU power and ease CPU load for other programs.

The 4 models are also available as configuration files for zpaq from
http://mattmahoney.net/dc/zpaq.html
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

To compile

  g++ -O3 -DNDEBUG zp.cpp libzpaq.cpp divsufsort.c

You need libzpaq from http://mattmahoney.net/dc/zpaq
You need divsufsort from http://code.google.com/p/libdivsufsort/

Other compiler optimizations that might be appropriate:

  -march=native -fomit-frame-pointer -s

but -march=pentiumpro will preserve compatibility with older machines
without much performance penalty. -DNDEBUG turns off run time
debugging checks. -s strips debugging info.

History:

zp 1.00 is derived from pzpaq. zp and unzp together replace it.

*/

#include "libzpaq.h"
#include "divsufsort.h"
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

int numberOfProcessors();  // Default for -t

void usage() {
  fprintf(stderr,
  "zp v1.01 - Parallel ZPAQ compressor\n"
  "(C) 2011, Dell Inc. Written by Matt Mahoney\n"
  "This is free software under GPL v3. http://www.gnu.org/copyleft/gpl.html\n"
  "\n"
  "Usage: zp [-options]... files...\n"
  "Options\n"
  "  -bN = Block size of about N MB. Default -b32\n"
  "  -c  = Compress to standard output. Default: file -> file.zpaq\n"
  "  -mN = compress with Method N (1=fastest...4=best). Default -m1\n"
  "  -r  = Remove input files after compression\n"
  "  -tN = use N Threads. Default = -t%d\n"
  "  -v  = Verbose. Default: no output except error messages\n",
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
unsigned int bopt=32; // -b block size
bool copt=false;      // -c output to stdout
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

// To count bytes read or written
struct FileCount: public libzpaq::Reader, public libzpaq::Writer {
  FILE* f;
  int64_t count;
  FileCount(FILE* f_): f(f_), count(0) {}
  int get() {int c=getc(f); count+=(c!=EOF); return c;}
  void put(int c) {putc(c, f); count+=1;}
};

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
  result+="pzpaqtmp";

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


/////////////////// compress ////////////////////////

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
    compress(*job);
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
        case 'm': mopt=atoi(argv[i]+2); break;
        case 'b': bopt=atoi(argv[i]+2); break;
        case 'c': copt=true; break;
        case 'r': ropt=true; break;
        case 't': topt=atoi(argv[i]+1); break;
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
  if (copt && filesize(stdout)<0) {
    fprintf(stderr, "Won't compress to a terminal\n");
    exit(1);
  }
#endif

  // List of jobs
  std::vector<Job> jobs;

  // Schedule compression
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

