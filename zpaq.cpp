/* zpaq.cpp v6.19 - Journaling incremental deduplicating archiver

  Copyright (C) 2013, Dell Inc. Written by Matt Mahoney.

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

zpaq is for creating journaling compressed archives for incremental
backups of files and directory trees. Incremental update of an entire
disk is fast because only those files whose last-modified date has
changed are added. zpaq is journaling, which means that the archive
is (mostly) append-only, and both the old and new versions are saved.
You can extract the old versions by rolling back the archive to an
earlier version. zpaq deduplicates: it saves identical files or file
fragments only once by comparing SHA-1 hashes to those stored in the
archive.

zpaq compresses in the open-standard ZPAQ level 2 (v2.01) format
specified at http://mattmahoney.net/zpaq/
The format is self describing, meaning that new versions of the
archiver that improve compression will still produce archives that
older decompressers can read, because the decompression instructions
are stored in the archive.

Usage: command archive.zpaq [file|dir]... -options...
Commands:
  a    Add changed files to archive.zpaq
  x    Extract latest versions of files
  l    List contents
Options (may be abbreviated):
  -not <file|dir>...   Do not add/extract/list
  -to <file|dir>...    Rename external files
  -version N           Revert to N'th update
  -force               a: Add even if unchanged. x: output clobbers
  -quiet               Display only errors and warnings
  -threads N           Use N threads (default: cores detected)
  -method 0...4        Compress faster...better (default: 1)
list options:
  -summary [N]         Show top N files and types (default: 20)
  -since N             List from N'th update or last -N updates
  -above N             List files N bytes or larger

The archive file name must end with ".zpaq" or the extension will be
assumed. Directory names should be written without a trailing slash.
Commands a, x, l can be written as -add, -extract, or -list respectively
for backward compatibility. Options can be abbreviated as long as it is
not ambiguous, like -th or -thr (but not -t) for -threads.
It is recommended that if zpaq is run from a script that abbreviations
not be used because future versions might add options that would make
existing abbreviations ambiguous.

  a or -add

Add files and directory trees. Directories are scanned recursively.
Only ordinary files and directories are added, not special types
(devices, symbolic links, etc). Symbolic links are not followed. The file
names are saved as given on the command line. The last-modified
date is saved, rounded to the nearest second. Windows attributes
or Linux permissions are saved. However, additional metadata such
as owner, group, extended attributes (xattrs, ACLs, alternate
streams) are not saved.

Existing files and directories are updated, but preserving the
old version as well. This means that if an external file or the
directory containing it is named but no longer exists then the internal
file is marked as deleted in the current version but still available
in the previous version.

Before adding files, the last-modifed date is compared with the
date stored in the archive. Files are added only if the dates
differ or if the file is not already in the archive.

If a file is added, then it is deduplicated by computing the SHA-1
hashes of file fragments (average size 64 KB) along content-dependent
boundaries, and compared with the hashes already in the archive.
Only those hashes that don't match are added. Matching fragments
are stored as pointers to the match, which may be in the current
or earlier versions.

Files are sorted by filename extension (like ".exe", ".html", ".jpg")
and then lexicographically by name. Non-matching fragments are packed into
blocks of about 16 MB and compressed in parallel by separate threads
and appended to the archive. Blocks are packaged into a transacted update
consisting of a temporary header, compressed blocks, and compressed
index listing fragment hashes and sizes and a list of added and deleted
files. For each added file, the date, attributes, and list of fragment
indexes is stored.

The header stores the date of the update and compressed data size.
The size is temporarily set to an invalid value (-1) and updated as the
last step. If -add encounters an error or is interrupted by the
user with Ctrl-C, resulting in an invalid header followed by corrupted
data, then zpaq will interpret the invalid value as the end of the
archive. It will ignore anything that follows, and the next -add command
will overwrite it.

  -method 0..9

Compress faster...better. The default is -method 1. The algorithms are:

  0 = Not compressed.
  1 = Byte aligned LZ77.
  2 = LZ77 + context model + arithmetic coding.
  3 = BWT (Burrows-Wheeler transform) + order 0-1 indirect model.
  4 = CM (mid.cfg context mixing) with 8 components.
  5..9 = CM (max.cfg) with 22 components using increasing memory.

Method 0 deduplicates file fragments but does not otherwise compress.
Deduplication is accomplished by a rolling hash that depends on the
last 32 bytes that are not predicted by an order-1 context and
selected with probability 2^-16, with a minimum size of 4096 or EOF,
whichever is first, and a maximum size of 520192 bytes. The hash
update function for each byte c is:

  hash := (hash + c + 1) * 314159265 (mod 2^32)  if c is predicted,
  hash := (hash + c + 1) * 271828182 (mod 2^32)  if c is not predicted.

and a boundary occurs after byte c if hash < 65536. c is predicted
if the previous occurrence of the byte before c is also followed by c.

For methods 1 through 4, if less than 1/16, 1/32, 1/64, or 1/128
respectively of the bytes in the block are predicted, then the
block is stored without compression. Otherwise it is compressed
as follows:

Method 1 is LZ77 using variable length codes to represent the lengths
of literal byte strings or the length and offset of matches to
earlier occurrences of the same string in a 16 MB output block.
Matches are found by indexing a hash of the next 4 bytes in the
input buffer into a table of size 4M which is grouped into 512K
buckets of 8 pointers each. The longest match is coded, provided
the length is at least 4, or 5 if the offset > 64K and the last
output was a literal. Ties are broken by favoring the smaller offset.
Bucket elements are selected for replacement using the low 3 bits
of the output count.

Literal lengths are coded using "marked binary", where the leading
1 bit of the number is dropped and a 1 bit is inserted in front
of the remaining bits and a 0 marks the end. For example, 1100
is coded as 1,1,1,0,1,0,0. Matches are coded as a length and an
offset. The length is at least 4. All but the last 2 bits are
coded as a marked binary. The number of match bits is given in
the first 5 bits of the code. If the code starts with 00, then
a literal length and string of literal follow. Otherwise the 5
bits code a number from 0 to 23, and that number of bits, with
an implied leading 1 give the offset.

Method 2 is also LZ77, but the codes are byte aligned and context
modeled rather than coded directly. It also searches 4 order-7
context hashes and 4 order-4 hashes, rather than 8 order-4 hashes
like method 1. Method 2 first codes as follows, according to the
high 2 bits of the first byte:

  00 = literal of length 1..64, followed by uncompressed bytes.
  01 = match of length 4..11 and offset 1..2048.
  10 = match of length 1..64 and offset of 1..65536.
  11 = match of length 1..64 and offset of 1..16777216.

These codes are arithmetic coded using an indirect context model. The
context depends on the parse state and in the case of literals, on the
previous byte. An indirect context model maps a context into
a bit history (represented as an 8 bit state) and then to a bit
prediction. The model is updated by adjusting the prediction
to reduce the error by 0.1%. A bit history represents a bounded
pair of bit counts (n0,n1) and the value of the most recent bit.
The bounds for (n0,n1) and (n1,n0) are (20,0), (48,1), (15,2),
(8,3), (6,4), (5,5).

Method 3 uses a Burrows-Wheeler transform (BWT). The input bytes
are sorted by their right contexts and compressed using an
order 0-1 ICM-ISSE chain. The order 0 ICM (indirect context
model) works as in method 2, taking only the previous bits of
the current byte (MSB first) as context. The prediction is
adjusted by an order-1 indirect secondary symbol estimator (ISSE).
An ISSE maps its context (the previous byte and the leading
bits of the current byte) to a bit history, and the history
selects a pair of mixing weights to compute the weighted average
of the constant 1 and the ICM output in the logistic domain,
log(p/(1-p)). The output is converted back to linear, and the
two weights are updated to reduce the prediction error in favor
of the better model. In other words, the output is:

  p' := 1/(1 + exp(-w1*1 - w2*log(p/(1-p))))

and after the bit is arithmetic coded, the weights w1 and w2 are updated:

  w1 := w1 + 1            * 0.001 * (bit - p')
  w2 := w2 + log(p/(1-p)) * 0.001 * (bit - p')

Method 4 directly models the data using an order 0-5 ICM-ISSE chain,
an order 7 match model, and an order 1 mixer which produces the bit
prediction by mixing the predictions of all other components. The 6
components in the chain each mix the next lower order prediction using a
hash of the next higher order context to select a bit history for that
context, which selects the mixing weights. A match model has a 16 MB
history buffer and a 4M hash table of the previous occurrence of the
current context. If a match is found, it predicts the bit that followed
the match with probability 1 - 1/(length in bits). The outputs of
all 7 models are then mixed as with an ISSE except with a vector of
7 weights selected by an order 1 (16 bit) context, and with a faster
weight update rate of about 0.01.

With method 4 you can give an argument like "-method 4 1" to double
the memory allocated to the components to improve compression. The
same extra memory is needed to decompress. The default is 111 MB
per thread. An argument n multiplies memory usage by 2^n. n can be negative.

In all cases, the context modeling and postprocessing (inverse BWT
or LZ77 decoding) is executed by JIT optimized ZPAQL code stored in
the archive. JIT optimization translates ZPAQL to 32 or 64 bit x86
code which is executed directly for better speed. On other processors,
the ZPAQL code is interpreted. The ZPAQL language is described
in libzpaq.h. The precise semantics are described in the ZPAQ
specification.

  -force

Add files even if the dates match. If a file really is identical,
then it will not take significant space because all of its fragments
will be deduplicated. With -extract, output files will be overwritten.

  l or -list

List the archive contents. If any file names are directories are
specified, then only those are listed. For each file, all versions
are shown along with version numbers. The listing shows the attributes,
uncompressed size, and file name. Windows attributes have the form
"DASHRI" where the letters are replaced with "." unless the directory,
archive, system, hidden, read-only, and index attributes are respectively
set. If any other attributes are set, then the value is shown as a
hexadecimal number. In Linux, the attributes are shown as a 6 digit octal
number as would normally be passed to chmod(), e.g. "100644" for a normal
file with user read-write permissions and read-only for group and others.
If no attributes are shown then none are stored and the file
will be restored with default attributes.

  -since N

List only versions N and later. If N is negative then list only the
last -N updates. Default is 0 (all).

  -above N

List only files at least N bytes in size. Default is -1 (all including
unknown sizes).

  -summary N

Modifies -list to summarize statistics about the archive. There are
3 tables. The first table shows the top N (default 20) files,
directories, and file types (extensions) ranked by total size,
giving the rank, size, and number of files in each category.
A second table shows deduplication statistics: the number of
fragments and MB (deduplicated and expanded) that are shared by 0, 1, 2,...
files. A third table summarizes updates, showing for each one the version
number, date, number of files added and deleted, and MB added
before and after compression. In addition the following is
reported: number of fragments, number of fragments with unknown
size (compressed with -tiny), number of blocks, number of blocks
not referenced by any file, and total size before and after compression.

  x or -extract

Extract files and directores (default: all) from archive.
Files will be extracted by creating filenames as saved in the archive.
If any of the external files already exist then zpaq will exit with
an error and not extract any files unless -force is given to allow
files to be overwritten. Last-modified dates and attributes will be
restored as saved. Attributes saved in Windows will not be restored
in Linux and vice versa.

  -to

Rename external files corresponding to the arguments to -add,
-extract, or -list. The most common use is to rename extracted
files and directories, for example:

    zpaq a arc calgary
    zpaq x arc calgary -to out

will extract calgary/book1 (from arc.zpaq) to out/book1 and so on.

  -not

Exclude files and directories (before renaming) from -add, -extract,
and -list. For example "zpaq a arc calgary -not calgary/book1"
will add all the files in calgary except book1.

  -version N

Roll back the archive to an earlier version. With -list and -extract,
versions later than N will be ignored. With -add, the archive
will be truncated at N, discarding any subsquently added contents
before updating.

  -quiet

With -add and -extract, don't display anything except errors and
warnings. Normally, each filename is displayed as it is added or
extracted.

  -threads N

Set the number of parallel threads for -add and -extract to N. The
default is the number of cores detected by %NUMBER_OF_PROCESSORS%
in Windows or /proc/cpuinfo in Linux. This number is displayed
by the help message when zpaq is run with no options.
Using fewer threads can reduce memory usage, but using more is
not any faster.


TO COMPILE:

This program needs libzpaq from http://mattmahoney.net/zpaq/ and
libdivsufsort-lite from above or http://code.google.com/p/libdivsufsort/
Recommended compile for Windows with MinGW:

  g++ -O3 -s zpaq.cpp libzpaq.cpp divsufsort.c -o zpaq -DNDEBUG

With Visual C++:

  cl /O2 /EHsc /arch:SSE2 /DNDEBUG zpaq.cpp libzpaq.cpp divsufsort.c

For Linux:

  g++ -O3 -s -Dunix -DNDEBUG zpaq.cpp libzpaq.cpp divsufsort.c \
  -fopenmp -o zpaq

Possible options:

  -o         Name of output executable.
  -O3 or /O2 Optimize (faster).
  /EHsc      Enable exception handing in VC++ (required).
  -s         Strip debugging symbols. Smaller executable.
  /arch:SSE2 Assume x86 processor with SSE2. Otherwise use -DNOJIT.
  -DNOJIT    Don't assume x86 with SSE2 for libzpaq. Slower (disables JIT).
  -static    Don't assume C++ runtime on target. Bigger executable but safer.
  -Dunix     Not Windows. Sometimes automatic in Linux. Needed for Mac OS/X.
  -fopenmp   Parallel divsufsort (faster, implies -pthread, broken in MinGW).
  -pthread   Required in Linux, implied by -fopenmp.
  -DNDEBUG   Turn off debugging checks in divsufsort (faster).
  -DDEBUG    Turn on debugging checks in libzpaq, zpaq (slower).
  -DPTHREAD  Use Pthreads instead of Windows threads. Requires pthreadGC2.dll
             or pthreadVC2.dll from http://sourceware.org/pthreads-win32/

*/
#define _FILE_OFFSET_BITS 64  // In Linux make sizeof(off_t) == 8
#include "libzpaq.h"
#include "divsufsort.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <stdint.h>
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <stdexcept>

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
#include <utime.h>

#else  // Assume Windows
#define UNICODE
#include <windows.h>
#include <io.h>
#endif

using std::string;
using std::vector;
using std::map;

// Handle errors in libzpaq and elsewhere
void libzpaq::error(const char* msg) {
  fprintf(stderr, "zpaq error: %s\n", msg);
  throw std::runtime_error(msg);
}
using libzpaq::error;

// Portable thread types and functions for Windows and Linux. Use like this:
//
// // Create mutex for locking thread-unsafe code
// Mutex mutex;            // shared by all threads
// init_mutex(mutex);      // initialize in unlocked state
// Semaphore sem(n);       // n >= 0 is initial state
//
// // Declare a thread function
// ThreadReturn thread(void *arg) {  // arg points to in/out parameters
//   lock(mutex);          // wait if another thread has it first
//   release(mutex);       // allow another waiting thread to continue
//   sem.wait();           // wait until n>0, then --n
//   sem.signal();         // ++n to allow waiting threads to continue
//   return 0;             // must return 0 to exit thread
// }
//
// // Start a thread
// ThreadID tid;
// run(tid, thread, &arg); // runs in parallel
// join(tid);              // wait for thread to return
// destroy_mutex(mutex);   // deallocate resources used by mutex

#ifdef PTHREAD
#include <pthread.h>
typedef void* ThreadReturn;                                // job return type
typedef pthread_t ThreadID;                                // job ID type
void run(ThreadID& tid, ThreadReturn(*f)(void*), void* arg)// start job
  {pthread_create(&tid, NULL, f, arg);}
void join(ThreadID tid) {pthread_join(tid, NULL);}         // wait for job
typedef pthread_mutex_t Mutex;                             // mutex type
void init_mutex(Mutex& m) {pthread_mutex_init(&m, 0);}     // init mutex
void lock(Mutex& m) {pthread_mutex_lock(&m);}              // wait for mutex
void release(Mutex& m) {pthread_mutex_unlock(&m);}         // release mutex
void destroy_mutex(Mutex& m) {pthread_mutex_destroy(&m);}  // destroy mutex

class Semaphore {
public:
  Semaphore() {sem=-1;}
  void init(int n) {
    assert(n>=0);
    assert(sem==-1);
    pthread_cond_init(&cv, 0);
    pthread_mutex_init(&mutex, 0);
    sem=n;
  }
  void destroy() {
    assert(sem>=0);
    pthread_mutex_destroy(&mutex);
    pthread_cond_destroy(&cv);
  }
  int wait() {
    assert(sem>=0);
    pthread_mutex_lock(&mutex);
    int r=0;
    if (sem==0) r=pthread_cond_wait(&cv, &mutex);
    assert(sem>0);
    --sem;
    pthread_mutex_unlock(&mutex);
    return r;
  }
  void signal() {
    assert(sem>=0);
    pthread_mutex_lock(&mutex);
    ++sem;
    pthread_cond_signal(&cv);
    pthread_mutex_unlock(&mutex);
  }
private:
  pthread_cond_t cv;  // to signal FINISHED
  pthread_mutex_t mutex; // protects cv
  int sem;  // semaphore count
};

#else  // Windows
typedef DWORD ThreadReturn;
typedef HANDLE ThreadID;
void run(ThreadID& tid, ThreadReturn(*f)(void*), void* arg) {
  tid=CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)f, arg, 0, NULL);
  if (tid==NULL) error("CreateThread failed");
}
void join(ThreadID& tid) {WaitForSingleObject(tid, INFINITE);}
typedef HANDLE Mutex;
void init_mutex(Mutex& m) {m=CreateMutex(NULL, FALSE, NULL);}
void lock(Mutex& m) {WaitForSingleObject(m, INFINITE);}
void release(Mutex& m) {ReleaseMutex(m);}
void destroy_mutex(Mutex& m) {CloseHandle(m);}

class Semaphore {
public:
  enum {MAXCOUNT=2000000000};
  Semaphore(): h(NULL) {}
  void init(int n) {assert(!h); h=CreateSemaphore(NULL, n, MAXCOUNT, NULL);}
  void destroy() {assert(h); CloseHandle(h);}
  int wait() {assert(h); return WaitForSingleObject(h, INFINITE);}
  void signal() {assert(h); ReleaseSemaphore(h, 1, NULL);}
private:
  HANDLE h;  // Windows semaphore
};

#endif

#ifdef _MSC_VER  // Microsoft C++
#define fseeko(a,b,c) _fseeki64(a,b,c)
#define ftello(a) _ftelli64(a)
#else
#ifndef unix
#define fseeko(a,b,c) fseeko64(a,b,c)
#define ftello(a) ftello64(a)
#endif
#endif

// For testing -Dunix in Windows
#ifdef unixtest
#define lstat(a,b) stat(a,b)
#define mkdir(a,b) mkdir(a)
#define fseeko(a,b,c) fseeko64(a,b,c)
#define ftello(a) ftello64(a)
#endif

// signed size of a string or vector
template <typename T> int size(const T& x) {
  return x.size();
}

// In Windows convert 8 bit ASCII to UTF-8 and \ to /
string atou(const char* s) {
  assert(s);
#ifdef unix
  return s;
#else
  string r;
  for (; *s; ++s) {
    int c=*s&255;
    if (c=='\\') r+='/';
    else if (c<128) r+=c;
    else r+=192+c/64, r+=128+c%64;
  }
  return r;
#endif
}

// In Windows, convert 16-bit wide string to UTF-8 and \ to /
#ifndef unix
string wtou(const wchar_t* s) {
  assert(sizeof(wchar_t)==2);  // Not true in Linux
  assert((wchar_t)(-1)==65535);
  string r;
  if (!s) return r;
  for (; *s; ++s) {
    if (*s=='\\') r+='/';
    else if (*s<128) r+=*s;
    else if (*s<2048) r+=192+*s/64, r+=128+*s%64;
    else r+=224+*s/4096, r+=128+*s/64%64, r+=128+*s%64;
  }
  return r;
}

// In Windows, convert UTF-8 string to wide string ignoring
// invalid UTF-8 or >64K. If doslash then convert "/" to "\".
std::wstring utow(const char* ss, bool doslash=false) {
  assert(sizeof(wchar_t)==2);
  assert((wchar_t)(-1)==65535);
  std::wstring r;
  if (!ss) return r;
  const unsigned char* s=(const unsigned char*)ss;
  for (; s && *s; ++s) {
    if (s[0]=='/' && doslash) r+='\\';
    else if (s[0]<128) r+=s[0];
    else if (s[0]>=192 && s[0]<224 && s[1]>=128 && s[1]<192)
      r+=(s[0]-192)*64+s[1]-128, ++s;
    else if (s[0]>=224 && s[0]<240 && s[1]>=128 && s[1]<192
             && s[2]>=128 && s[2]<192)
      r+=(s[0]-224)*4096+(s[1]-128)*64+s[2]-128, s+=2;
  }
  return r;
}
#endif

// Print a UTF-8 string so it displays properly
void printUTF8(const char* s, FILE* f=stdout) {
  assert(f);
  assert(s);
#ifdef unix
  fprintf(f, "%s", s);
#else
  const HANDLE h=(HANDLE)_get_osfhandle(_fileno(f));
  if (h!=INVALID_HANDLE_VALUE && GetFileType(h)==FILE_TYPE_CHAR) {
    std::wstring w=utow(s);  // Windows console: convert to UTF-16
    DWORD n;
    WriteConsole(h, w.c_str(), w.size(), &n, 0);
  }
  else  // stdout redirected to file
    fprintf(f, "%s", s);
#endif
}

// Convert 64 bit decimal YYYYMMDDHHMMSS to "YYYY-MM-DD HH:MM:SS"
// where -1 = unknown date, 0 = deleted.
string dateToString(int64_t date) {
  if (date<=0) return "                   ";
  string s="0000-00-00 00:00:00";
  static const int t[]={18,17,15,14,12,11,9,8,6,5,3,2,1,0};
  for (int i=0; i<14; ++i) s[t[i]]+=int(date%10), date/=10;
  return s;
}

// Convert 'u'+(N*256) to octal N or 'w'+(N*256) to hex N or "DRASHI"
string attrToString(int64_t attrib) {
  string r="      ";
  if ((attrib&255)=='u') {
    for (int i=0; i<6; ++i)
      r[5-i]=(attrib>>(8+3*i))%8+'0';
  }
  else if ((attrib&255)=='w') {
    attrib>>=8;
    if (attrib&~0x20b7) {  // non-standard flags set?
      r="0x    ";
      for (int i=0; i<4; ++i)
        r[5-i]="0123456789abcdef"[attrib>>(4*i)&15];
    }
    else {
      r="......";
      if (attrib&0x10) r[0]='D';  // directory
      if (attrib&0x20) r[1]='A';  // archive
      if (attrib&0x04) r[2]='S';  // system
      if (attrib&0x02) r[3]='H';  // hidden
      if (attrib&0x01) r[4]='R';  // read only
      if (attrib&0x2000) r[5]='I';  // index
    }
  }
  return r;
}

// Convert seconds since 0000 1/1/1970 to 64 bit decimal YYYYMMDDHHMMSS
// Valid from 1970 to 2099.
int64_t decimal_time(time_t t) {
  if (t<=0) return -1;
  const int second=t%60;
  const int minute=t/60%60;
  const int hour=t/3600%24;
  t/=86400;  // days since Jan 1 1970
  const int term=t/1461;  // 4 year terms since 1970
  t%=1461;
  t+=(t>=59);  // insert Feb 29 on non leap years
  t+=(t>=425);
  t+=(t>=1157);
  const int year=term*4+t/366+1970;  // actual year
  t%=366;
  t+=(t>=60)*2;  // make Feb. 31 days
  t+=(t>=123);   // insert Apr 31
  t+=(t>=185);   // insert June 31
  t+=(t>=278);   // insert Sept 31
  t+=(t>=340);   // insert Nov 31
  const int month=t/31+1;
  const int day=t%31+1;
  return year*10000000000LL+month*100000000+day*1000000
         +hour*10000+minute*100+second;
}

// Convert decimal date to time_t - inverse of decimal_time()
time_t unix_time(int64_t date) {
  if (date<=0) return -1;
  static const int days[12]={0,31,59,90,120,151,181,212,243,273,304,334};
  const int year=date/10000000000LL%10000;
  const int month=(date/100000000%100-1)%12;
  const int day=date/1000000%100;
  const int hour=date/10000%100;
  const int min=date/100%100;
  const int sec=date%100;
  return (day-1+days[month]+(year%4==0 && month>1)+((year-1970)*1461+1)/4)
    *86400+hour*3600+min*60+sec;
}

/////////////////////////////// File //////////////////////////////////

// Base class of InputFile and OutputFile (OS independent)
class File {
protected:
  enum {BUFSIZE=1<<18};  // buffer size
  int ptr;  // next byte to read or write in buf
  libzpaq::Array<char> buf;  // I/O buffer
  File(): ptr(0), buf(BUFSIZE) {}
};

// File types accepting UTF-8 filenames
#ifdef unix

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
  string filename;
public:
  OutputFile(): out(0) {}

  // Return true if file is open
  bool isopen() {return out!=0;}

  // Open for append/update or create if needed.
  bool open(const char* filename) {
    assert(!isopen());
    ptr=0;
    this->filename=filename;
    out=fopen(filename, "rb+");
    if (out) fseeko(out, 0, SEEK_END);
    else out=fopen(filename, "wb+");
    if (!out) perror(filename);
    return isopen();
  }

  // Flush pending output
  void flush() {
    if (ptr) {
      assert(isopen());
      assert(ptr>0 && ptr<=BUFSIZE);
      int n=fwrite(&buf[0], 1, ptr, out);
      if (n!=ptr) {
        perror(filename.c_str());
        error("write failed");
      }
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

  // Write size bytes at offset
  void write(const char* bufp, int64_t pos, int size) {
    assert(isopen());
    flush();
    fseeko(out, pos, SEEK_SET);
    write(bufp, size);
  }

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

  // Truncate file and move file pointer to end
  void truncate(int64_t newsize=0) {
    assert(isopen());
    seek(newsize, SEEK_SET);
    if (ftruncate(fileno(out), newsize)) perror("ftruncate");
  }

  // Close file and set date if not 0. Set permissions if attr low byte is 'u'
  void close(int64_t date=0, int64_t attr=0) {
    if (out) {
      flush();
      fclose(out);
    }
    out=0;
    if (date>0) {
      struct utimbuf ub;
      ub.actime=time(NULL);
      ub.modtime=unix_time(date);
      utime(filename.c_str(), &ub);
    }
    if ((attr&255)=='u')
      chmod(filename.c_str(), attr>>8);
  }

  ~OutputFile() {close();}
};

#else  // Windows

// Print error message
void winError(const char* filename) {
  int err=GetLastError();
  printUTF8(filename, stderr);
  if (err==ERROR_FILE_NOT_FOUND)
    fprintf(stderr, ": file not found\n");
  else if (err==ERROR_PATH_NOT_FOUND)
    fprintf(stderr, ": path not found\n");
  else if (err==ERROR_ACCESS_DENIED)
    fprintf(stderr, ": access denied\n");
  else if (err==ERROR_SHARING_VIOLATION)
    fprintf(stderr, ": sharing violation\n");
  else
    fprintf(stderr, ": Windows error %d\n", err);
}

class InputFile: public File, public libzpaq::Reader {
  HANDLE in;  // input file handle
  DWORD n;    // buffer size
public:
  InputFile():
    in(INVALID_HANDLE_VALUE), n(0) {}

  // Open for reading. Return true if successful
  bool open(const char* filename) {
    assert(in==INVALID_HANDLE_VALUE);
    n=ptr=0;
    std::wstring w=utow(filename, true);
    in=CreateFile(w.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL,
                  OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (in==INVALID_HANDLE_VALUE) winError(filename);
    return in!=INVALID_HANDLE_VALUE;
  }

  bool isopen() {return in!=INVALID_HANDLE_VALUE;}

  // Read 1 byte
  int get() {
    if (ptr>=int(n)) {
      assert(ptr==int(n));
      ptr=0;
      ReadFile(in, &buf[0], BUFSIZE, &n, NULL);
      if (n==0) return EOF;
    }
    assert(ptr<int(n));
    return buf[ptr++]&255;
  }

  // set file pointer
  void seek(int64_t pos, int whence) {
    if (whence==SEEK_SET) whence=FILE_BEGIN;
    else if (whence==SEEK_END) whence=FILE_END;
    else if (whence==SEEK_CUR) {
      whence=FILE_BEGIN;
      pos+=tell();
    }
    LONG offhigh=pos>>32;
    SetFilePointer(in, pos, &offhigh, whence);
    n=ptr=0;
  }

  // get file pointer
  int64_t tell() {
    LONG offhigh=0;
    DWORD r=SetFilePointer(in, 0, &offhigh, FILE_CURRENT);
    return (int64_t(offhigh)<<32)+r+ptr-n;
  }

  // Close handle if open
  void close() {
    if (in!=INVALID_HANDLE_VALUE) {
      CloseHandle(in);
      in=INVALID_HANDLE_VALUE;
    }
  }
  ~InputFile() {close();}
};

class OutputFile: public File, public libzpaq::Writer {
  HANDLE out;               // output file handle
  std::wstring filename;    // filename as wide string
public:
  OutputFile(): out(INVALID_HANDLE_VALUE) {}

  // Return true if file is open
  bool isopen() {
    return out!=INVALID_HANDLE_VALUE;
  }

  // Open file ready to update or append, create if needed.
  bool open(const char* filename_) {
    assert(!isopen());
    ptr=0;
    filename=utow(filename_, true);
    out=CreateFile(filename.c_str(), GENERIC_WRITE, 0, NULL, OPEN_ALWAYS,
                   FILE_ATTRIBUTE_NORMAL, NULL);
    if (out==INVALID_HANDLE_VALUE) winError(filename_);
    else {
      LONG hi=0;
      SetFilePointer(out, 0, &hi, FILE_END);
    }
    return isopen();
  }

  // Write pending output
  void flush() {
    assert(isopen());
    if (ptr) {
      DWORD n=0;
      WriteFile(out, &buf[0], ptr, &n, NULL);
      if (ptr!=int(n)) {
        fprintf(stderr, "%s: error %d: wrote %d of %d bytes\n",
                wtou(filename.c_str()).c_str(), int(GetLastError()),
                int(n), ptr);
        error("write failed");
      }
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
    buf[ptr++]=c;
  }

  // Write bufp[0..size-1]
  void write(const char* bufp, int size);

  // Write size bytes at offset
  void write(const char* bufp, int64_t pos, int size) {
    assert(isopen());
    flush();
    if (pos!=tell()) seek(pos, SEEK_SET);
    write(bufp, size);
  }

  // set file pointer
  void seek(int64_t pos, int whence) {
    if (whence==SEEK_SET) whence=FILE_BEGIN;
    else if (whence==SEEK_CUR) whence=FILE_CURRENT;
    else if (whence==SEEK_END) whence=FILE_END;
    flush();
    LONG offhigh=pos>>32;
    SetFilePointer(out, pos, &offhigh, whence);
  }

  // get file pointer
  int64_t tell() {
    LONG offhigh=0;
    DWORD r=SetFilePointer(out, 0, &offhigh, FILE_CURRENT);
    return (int64_t(offhigh)<<32)+r+ptr;
  }

  // Truncate file and move file pointer to end
  void truncate(int64_t newsize=0) {
    seek(newsize, SEEK_SET);
    SetEndOfFile(out);
  }

  // Close file and set date if not 0. Set attr if low byte is 'w'.
  void close(int64_t date=0, int64_t attr=0) {
    if (isopen()) {
      flush();
      if (date>0) {
        SYSTEMTIME st;
        FILETIME ft;
        st.wYear=date/10000000000LL%10000;
        st.wMonth=date/100000000%100;
        st.wDayOfWeek=0;  // ignored
        st.wDay=date/1000000%100;
        st.wHour=date/10000%100;
        st.wMinute=date/100%100;
        st.wSecond=date%100;
        st.wMilliseconds=0;
        SystemTimeToFileTime(&st, &ft);
        if (!SetFileTime(out, NULL, NULL, &ft))
          fprintf(stderr, "SetFileTime error %d\n", int(GetLastError()));
      }
      CloseHandle(out);
      out=INVALID_HANDLE_VALUE;
      if ((attr&255)=='w')
        SetFileAttributes(filename.c_str(), attr>>8);
      filename=L"";
    }
  }
  ~OutputFile() {close();}
};

#endif

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

///////////////////////// NumberOfProcessors ///////////////////////////

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

////////////////////////////// StringBuffer //////////////////////////


// For libzpaq output to a string
struct StringWriter: public libzpaq::Writer {
  string s;
  void put(int c) {s+=char(c);}
};

// For (de)compressing to/from a string. Writing appends bytes
// which can be later read.
class StringBuffer: public libzpaq::Reader, public libzpaq::Writer {
  string s;  // buffer: rpos <= wpos <= s.size()
  unsigned rpos, wpos;  // number of bytes read, written
public:

  // Direct access to data
  unsigned char* data() {
    return (unsigned char*)(char*)s.data();
  }

  // Allocate n bytes initially and make the size 0. More memory will
  // be allocated later if needed.
  StringBuffer(int n=0): s(n,'\0'), rpos(0), wpos(0) {}

  // Return number of bytes written.
  size_t size() const {return wpos;}

  // Reset size to 0.
  void reset() {rpos=wpos=0;}  // make size=0

  // Write a single byte.
  void put(int c) {  // write 1 byte
    if (wpos>=s.size()) s.resize(wpos*2+64);
    s[wpos++]=c;
  }

  // Write buf[0..n-1]
  void write(const char* buf, int n) {  // 
    if (wpos+n>=s.size()) s.resize((wpos+n)*2+64);
    memcpy((char*)s.c_str()+wpos, buf, n);
    wpos+=n;
  }

  // Read a single byte. Return EOF (-1) and reset at end of string.
  int get() {return rpos<wpos ? s[rpos++]&255 : (reset(),-1);}

  // Read up to n bytes into buf[0..] or fewer if EOF is first.
  // Return the number of bytes actually read.
  int read(char* buf, int n) {
    if (n>int(wpos-rpos)) n=wpos-rpos;
    if (n>0) memcpy(buf, s.c_str()+rpos, n);
    rpos+=n;
    return n;
  }

  // Return the entire string as a read-only array.
  const char* c_str() const {return s.c_str();}

  // Truncate the string to size i.
  void resize(int i) {wpos=i;}

  // Write a string.
  void operator+=(const string& t) {
    for (unsigned i=0; i<t.size(); ++i) put(t[i]);
  }
};

// convert \ to /. In Windows also convert upper case to lower case.
int tolowerslash(int c) {
  if (c=='\\') return '/';
#ifndef unix
  else if (c>='A' && c<='Z') return c-'A'+'a';
#endif
  else return c;
}

// Return true if path a is a prefix of path b.
// In Windows, not case sensitive.
bool ispath(const char* a, const char* b) {
  if (!*a) return !*b;
  for (;*a && *b; ++a, ++b)
    if (tolowerslash(*a)!=tolowerslash(*b)) return false;
  return *a==0 && (*b==0 || *b=='/' || *b=='\\');
}

// Convert string to lower case
string lowercase(string s) {
  for (unsigned i=0; i<s.size(); ++i)
    if (s[i]>='A' && s[i]<='Z') s[i]+='a'-'A';
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

// Convert x to 4 byte little-endian string
string itob(unsigned x) {
  string s(4, '\0');
  s[0]=x, s[1]=x>>8, s[2]=x>>16, s[3]=x>>24;
  return s;
}

// convert to 8 byte little-endian string
string ltob(int64_t x) {
  string s(8, '\0');
  s[0]=x,     s[1]=x>>8,  s[2]=x>>16, s[3]=x>>24;
  s[4]=x>>32, s[5]=x>>40, s[6]=x>>48, s[7]=x>>56;
  return s;
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

// Convert non-negative decimal number x to string of at least n digits
string itos(int64_t x, int n=1) {
  assert(x>=0);
  assert(n>=0);
  string r;
  for (; x || n>0; x/=10, --n) r=char('0'+x%10)+r;
  return r;
}

/////////////////////////////// Jidac /////////////////////////////////

// A Jidac object represents an archive contents: a list of file
// fragments with hash, size, and archive offset, and a list of
// files with date, attributes, and list of fragment pointers.
// Methods add to, extract from, compare, and list the archive.

bool quiet=false;  // -quiet option

// Run cmd and print it unless quiet
void syscmd(const char* cmd) {
  if (!quiet) printf("`%s`", cmd);
  int r=system(cmd);
  if (!quiet) printf(" returns %d\n", r);
}

// fragment hash table entry
struct HT {
  unsigned char sha1[20];  // fragment hash
  int usize;      // uncompressed size, -1 if unknown
  int64_t csize;  // if >=0 then block offset else -fragment number
  HT(const char* s=0, int u=0, int c=0) {
    if (s) memcpy(sha1, s, 20);
    else memset(sha1, 0, 20);
    usize=u; csize=c;
  }
};

// filename version entry
struct DTV {
  int64_t date;          // decimal YYYYMMDDHHMMSS (UT) or 0 if deleted
  int64_t size;          // size or -1 if unknown
  int64_t attr;          // first 8 attribute bytes
  vector<unsigned> ptr;  // list of fragment indexes to HT
  int version;           // which transaction was it added?
  bool streaming;        // true if any fragment sizes are unknown or too big
  DTV(): date(0), size(0), attr(0), version(0), streaming(false) {}
};

// filename entry
struct DT {
  int64_t edate;         // date of external file, 0=not found
  int64_t esize;         // size of external file
  int64_t eattr;         // external file attributes ('u' or 'w' in low byte)
  vector<unsigned> eptr; // fragment list of external file to add
  vector<DTV> dtv;       // list of versions
  int written;           // 0..ptr.size() = fragments output. -1=ignore
  DT(): edate(0), esize(0), eattr(0), written(-1) {}
};

// Version info
struct VER {
  int64_t date;          // 0 if not JIDAC
  int64_t usize;         // uncompressed size of files
  int64_t fsize;         // uncompressed size of fragments
  int64_t csize;         // compressed size of data only
  int64_t offset;        // start of transaction
  int updates;           // file updates
  int deletes;           // file deletions
  int fragments;         // number of fragments
  int firstFragment;     // first fragment ID
  VER() {memset(this, 0, sizeof(*this));}
};

// Compare by filename extension, then by full path
struct Compare {
  bool operator()(const string& a, const string& b) const {
    const char* as=a.c_str();
    const char* bs=b.c_str();
    const char* ae=strrchr(as, '.');
    if (!ae) ae="";
    const char* be=strrchr(bs, '.');
    if (!be) be="";
    int cmp=strcmp(ae, be);
    if (cmp) return cmp<0;
    return strcmp(as, bs)<0;
  }
};

typedef map<string, DT, Compare> DTMap;

class CompressJob;

// Do everything
class Jidac {
public:
  void doCommand(int argc, char** argv);
  friend ThreadReturn decompressThread(void* arg);
  friend struct ExtractJob;
private:

  // Command line arguments
  string command;           // "-add", "-extract", "-list"
  int blocksize;            // depends on method
  string archive;           // archive name
  vector<string> files;     // list of files and directories to add
  vector<string> notfiles;  // list of prefixes to exclude
  vector<string> tofiles;   // files renamed with -to
  int64_t date;             // now as decimal YYYYMMDDHHMMSS (UT)
  int64_t above;            // minimum file size to list
  int version;              // Set by -version
  int threads;              // default is number of cores
  int since;                // First version to -list
  int summary;              // Arg to -summary
  string method;            // "0".."4" or ZPAQL source code
  bool force;               // true if -force selected
  bool compare;             // true if -compare selected

  // Archive state
  vector<HT> ht;            // list of fragments
  DTMap dt;                 // set of files
  vector<VER> ver;          // version info

  // Commands
  void add();
  void extract();
  void list();
  void runtrace(int argc, char** argv);  // run|trace hcomp|pcomp args...
  void usage();

  // Support functions
  string rename(const string& name);    // replace files prefix with tofiles
  string unrename(const string& name);  // undo rename
  int64_t read_archive();               // read index block chain
  void read_args(bool scan, bool all=false);  // read args, scan dirs
  void scandir(const char* filename);   // scan dirs and add args to dt
  void addfile(const char* filename, int64_t edate, int64_t esize,
               int64_t eattr);
  void write_fragments(CompressJob& job, StringBuffer& b,
                       const char* method, const int* args,
                       unsigned& block_start, int& misses);
  void list_versions(int64_t csize);
};

// Print help message
void Jidac::usage() {
  printf(
  "zpaq 6.19 - Journaling incremental deduplicating archiving compressor\n"
  "(C) " __DATE__ ", Dell Inc. This is free software under GPL v3.\n"
  "\n"
  "Usage: command archive.zpaq [file|dir]... -options...\n"
  "Commands:\n"
  "  a    Add changed files to archive.zpaq\n"
  "  x    Extract latest versions of files\n"
  "  l    List contents\n"
  "Options (may be abbreviated):\n"
  "  -not <file|dir>...   Do not add/extract/list\n"
  "  -to <file|dir>...    Rename external files\n"
  "  -version N           Revert to N'th update\n"
  "  -force               a: Add even if unchanged. x: output clobbers\n"
  "  -quiet               Display only errors and warnings\n"
  "  -threads N           Use N threads (default: %d detected)\n"
  "  -method 0...9        Compress faster...better (default: 1)\n"
  "list options:\n"
  "  -summary [N]         Show top N files and types (default: 20)\n"
  "  -since N             List from N'th update or last -N updates\n"
  "  -above N             List files N bytes or larger\n",
  threads);
  exit(1);
}

// Rename name by matching it to a prefix of files[i] and replacing
// the prefix with tofiles[i]
string Jidac::rename(const string& name) {
  for (unsigned i=0; i<files.size() && i<tofiles.size(); ++i) {
    const unsigned len=files[i].size();
    if (name.size()>=len && name.substr(0, len)==files[i])
      return tofiles[i]+name.substr(files[i].size());
  }
  return name;
}

// Rename name by matching it to a prefix of tofiles[i] and replacing
// the prefix with files[i]
string Jidac::unrename(const string& name) {
  for (unsigned i=0; i<files.size() && i<tofiles.size(); ++i) {
    const unsigned len=tofiles[i].size();
    if (name.size()>=len && name.substr(0, len)==tofiles[i])
      return files[i]+name.substr(tofiles[i].size());
  }
  return name;
}

// Expand an abbreviated option (with or without a leading "-")
// or report error if not exactly 1 match. Always expand commands.
string expandOption(const char* opt) {
  const char* opts[]={"list","add","extract","method",
    "force","quiet", "summary","since","above","compare",
    "to","not","version","threads",0};
  assert(opt);
  if (opt[0]=='-') ++opt;
  const int n=strlen(opt);
  if (n==1 && opt[0]=='x') return "-extract";
  string result;
  for (unsigned i=0; opts[i]; ++i) {
    if (!strncmp(opt, opts[i], n)) {
      if (result!="")
        fprintf(stderr, "Ambiguous: %s\n", opt), exit(1);
      result=string("-")+opts[i];
      if (i<3 && result!="") return result;
    }
  }
  if (result=="")
    fprintf(stderr, "No such option: %s\n", opt), exit(1);
  return result;
}

// Parse the command line
void Jidac::doCommand(int argc, char** argv) {

  // initialize to default values
  command="";
  force=compare=false;
  since=0;
  above=-1;
  summary=0;
  version=0x7fffffff;
  threads=numberOfProcessors();
  method="1";  // 0..4
  blocksize=(1<<24)-257;

  // Get optional options
  for (int i=1; i<argc; ++i) {
    const string opt=expandOption(argv[i]);
    if ((opt=="-add" || opt=="-extract" || opt=="-list")
        && i<argc-1 && argv[i+1][0]!='-' && command=="") {
      command=opt;
      archive=argv[++i];
      while (++i<argc && argv[i][0]!='-')
        files.push_back(atou(argv[i]));
      --i;
    }
    else if (opt=="-quiet") quiet=true;
    else if (opt=="-force") force=true;
    else if (opt=="-compare") compare=true;
    else if (opt=="-since" && i<argc-1)
      since=atoi(argv[++i]);
    else if (opt=="-above" && i<argc-1 && isdigit(argv[i+1][0]))
      above=atof(argv[++i]);
    else if (opt=="-summary") {
      summary=20;
      if (i<argc-1 && isdigit(argv[i+1][0])) summary=atoi(argv[++i]);
    }
    else if (opt=="-threads" && i<argc-1) {
      threads=atoi(argv[++i]);
      if (threads<1) threads=1;
    }
    else if (opt=="-to") {
      while (++i<argc && argv[i][0]!='-')
        tofiles.push_back(atou(argv[i]));
      --i;
    }
    else if (opt=="-not") {
      while (++i<argc && argv[i][0]!='-')
        notfiles.push_back(atou(argv[i]));
      --i;
    }
    else if (opt=="-version" && i<argc-1)
      version=atoi(argv[++i]);
    else if (opt=="-method" && i<argc-1) {  // read config file
      method=atou(argv[++i]);
      if (method.size()!=1 || !isdigit(method[0]))
        usage();
      int b=24;
      if (method[0]>'5') b=24+method[0]-'5';
      blocksize=(1<<b)-257;
    }
    else usage();
  }

  // Add .zpaq extension to archive
  if (size(archive)<5 || archive.substr(archive.size()-5)!=".zpaq")
    archive+=".zpaq";

  // Execute command
  if (size(files)<size(tofiles)) usage();
  if (command=="-add") {
    if (size(files)<1) usage();
    add();
  }
  else if (command=="-list") list();
  else if (command=="-extract") extract();
  else usage();
}

// Read archive up to -date into ht, dt, ver. Return place to append.
int64_t Jidac::read_archive() {
  ht.resize(1);  // element 0 not used
  ver.resize(1); // version 0

  // Open archive or archive.zpaq
  InputFile in;
  if (!in.open(archive.c_str())) {
    if (command=="-add") {
      if (!quiet) {
        printf("Creating new archive ");
        printUTF8(archive.c_str());
        printf("\n");
      }
      return 0;
    }
    else
      exit(1);
  }
  if (!quiet) {
    printf("Reading archive ");
    printUTF8(archive.c_str());
    printf("\n");
  }

  // Scan archive contents
  libzpaq::Decompresser d;
  d.setInput(&in);
  string lastfile=archive; // last named file in streaming format
  if (size(lastfile)>5)
    lastfile=lastfile.substr(0, size(lastfile)-5); // drop .zpaq
  int64_t block_offset=0;  // start of last block of any type
  int64_t data_offset=0;   // start of last block of fragments
  bool found_data=false;   // exit if nothing found
  bool first=true;         // first segment in archive?

  // Detect archive format and read the filenames, fragment sizes,
  // and hashes. In JIDAC format, these are in the index blocks, allowing
  // data to be skipped. Otherwise the whole archive is scanned to get
  // this information from the segment headers and trailers.
  while (d.findBlock()) {
    try {
      found_data=true;
      StringWriter filename, comment;
      int segs=0;
      while (d.findFilename(&filename)) {
        if (filename.s.size()) {
          for (unsigned i=0; i<filename.s.size(); ++i)
            if (filename.s[i]=='\\') filename.s[i]='/';
          lastfile=filename.s.c_str();
        }
        comment.s="";
        d.readComment(&comment);
        int64_t usize=0;  // read uncompressed size from comment or -1
        int64_t fdate=0;  // read date from filename or -1
        unsigned num=0;   // read fragment ID from filename
        const char* p=comment.s.c_str();
        for (; isdigit(*p); ++p)  // read size
          usize=usize*10+*p-'0';
        if (p==comment.s.c_str()) usize=-1;  // size not found
        for (; *p && fdate<19000000000000LL; ++p)  // read date
          if (isdigit(*p)) fdate=fdate*10+*p-'0';
        if (fdate<19000000000000LL || fdate>=30000000000000LL) fdate=-1;

        // Test for JIDAC format. Filename is jDC<fdate>[cdhi]<num>[f]
        // and comment ends with " jDC\x01"
        if (comment.s.size()>=4
            && usize>=0
            && comment.s.substr(comment.s.size()-4)=="jDC\x01"
            && filename.s.size()==28
            && filename.s.substr(0, 3)=="jDC"
            && strchr("cdhi", filename.s[17])) {

          // Read the date and number in the filename
          num=0;
          fdate=0;
          for (unsigned i=3; i<17 && isdigit(filename.s[i]); ++i)
            fdate=fdate*10+filename.s[i]-'0';
          for (unsigned i=18; i<filename.s.size() && isdigit(filename.s[i]);++i)
            num=num*10+filename.s[i]-'0';

          // Decompress the block
          StringBuffer os;
          d.setOutput(&os);
          libzpaq::SHA1 sha1;
          d.setSHA1(&sha1);
          d.decompress();
          char sha1result[21]={0};
          d.readSegmentEnd(sha1result);
          if (usize!=int64_t(sha1.usize()))
            fprintf(stderr, "%s size should be %1.0f, is %1.0f\n",
                    filename.s.c_str(), double(usize), double(sha1.usize()));
          if (memcmp(sha1result+1, sha1.result(), 20))
            fprintf(stderr, "%s checksum error\n", filename.s.c_str());

          // Transaction header. Filename is jDCYYYYMMDDHHMMSSc
          // If in the future then stop here, else read 8 byte data size
          // from input and jump over it.
          if (filename.s[17]=='c' && fdate>=19000000000000LL
              && fdate<30000000000000LL) {
            data_offset=in.tell();
            bool isbreak=false;
            int64_t jmp=0;
            if (size(ver)>version) // roll back archive to here
              isbreak=true;
            else if (os.size()==8) {  // jump
              const char* s=os.c_str();
              jmp=btol(s);
              if (jmp<0) {
                fprintf(stderr, "Incomplete transaction ignored\n");
                isbreak=true;
              }
              else if (jmp>0)
                in.seek(jmp, SEEK_CUR);
            }
            else {
              fprintf(stderr, "Bad JIDAC header size: %d\n", size(os));
              isbreak=true;
            }
            if (isbreak) {
              in.close();
              return block_offset;
            }
            ver.push_back(VER());
            ver.back().offset=block_offset;
            ver.back().csize=jmp;
            ver.back().date=fdate;
            ver.back().firstFragment=num;
          }

          // Fragment table. Filename is jDCYYYYMMDDHHMMSSdNNNNNNNNNN
          // Contents is bsize[4] (sha1[20] usize[4])... for fragment N...
          // where bsize is the compressed block size.
          // Store in ht[].{sha1,usize}. Set ht[].csize to block offset
          // assuming N in ascending order.
          else if (filename.s[17]=='h' && num>0) {
            const char* s=os.c_str();
            const unsigned bsize=btoi(s);
            assert(size(ver)>0);
            const unsigned n=(os.size()-4)/24;
            ver.back().fragments+=n;
            if (ht.size()!=num)
              fprintf(stderr,
                "Unordered fragment tables: expected %d found %1.0f\n",
                size(ht), double(num));
            for (unsigned i=0; i<n; ++i) {
              while (ht.size()<=num+i) ht.push_back(HT());
              memcpy(ht[num+i].sha1, s, 20);
              s+=20;
              ver.back().fsize+=ht[num+i].usize=btoi(s);
              if (ht[num+i].csize==0)
                ht[num+i].csize=i?-int(i):data_offset;
            }
            data_offset+=bsize;
          }

          // Index (type i)
          // Contents is: 0[8] filename 0 (deletion)
          // or:       date[8] filename 0 na[4] attr[na] ni[4] ptr[ni][4]
          // Read into DT
          else if (filename.s[17]=='i') {
            const char* s=os.c_str();
            const char* const end=s+os.size();
            while (s<=end-9) {
              const char* fp=s+8;  // filename
              DT& dtr=dt[fp];
              dtr.dtv.push_back(DTV());
              DTV& dtv=dtr.dtv.back();
              dtv.version=size(ver)-1;
              dtv.date=btol(s);
              assert(size(ver)>0);
              if (dtv.date) ++ver.back().updates;
              else ++ver.back().deletes;
              s+=strlen(fp)+1;  // skip filename
              if (dtv.date && s<=end-8) {
                const unsigned na=btoi(s);
                for (unsigned i=0; i<na && s<end; ++i, ++s)  // read attr
                  if (i<8) dtv.attr+=int64_t(*s&255)<<(i*8);
                if (s<=end-4) {
                  const unsigned ni=btoi(s);
                  dtv.ptr.resize(ni);
                  for (unsigned i=0; i<ni && s<=end-4; ++i) {  // read ptr
                    dtv.ptr[i]=btoi(s);
                    if (dtv.ptr[i]<1 || dtv.ptr[i]>=ht.size())
                      error("bad fragment ID");
                    dtv.size+=ht[dtv.ptr[i]].usize;
                    ver.back().usize+=ht[dtv.ptr[i]].usize;
                  }
                }
              }
            }
          }

          // Bad JIDAC block
          else {
            fprintf(stderr, "Bad JIDAC block ignored: %s %s\n",
                    filename.s.c_str(), comment.s.c_str());
          }
        }

        // Streaming format
        else {
          char sha1result[21]={0};
          d.readSegmentEnd(sha1result);
          DT& dtr=dt[lastfile];
          if (filename.s.size()>0 || first) {
            if (size(dtr.dtv)>0 && dtr.dtv.back().version>=size(ver)-1) {
              if (size(ver)>version) {
                in.close();
                return block_offset;
              }
              ver.push_back(VER());
              ver.back().offset=block_offset;
              ver.back().date=-1;
            }
            dtr.dtv.push_back(DTV());
            dtr.dtv.back().date=fdate;
            dtr.dtv.back().version=size(ver)-1;
            ver.back().csize=in.tell()-ver.back().offset;
            if (usize>=0 && ver.back().fsize>=0) {
              ver.back().usize+=usize;
              ver.back().fsize+=usize;
            }
            else {
              ver.back().usize=-1;
              ver.back().fsize=-1;
            }
            ++ver.back().updates;
          }
          assert(dtr.dtv.size()>0);
          dtr.dtv.back().ptr.push_back(size(ht));
          dtr.dtv.back().streaming|=usize<0 || usize>blocksize;
          if (usize>=0 && dtr.dtv.back().size>=0) dtr.dtv.back().size+=usize;
          else dtr.dtv.back().size=-1;
          ht.push_back(HT(sha1result+1, usize, segs ? -segs : block_offset));
        }
        ++segs;
        filename.s="";
        first=false;
      }
      block_offset=in.tell();
    }
    catch (std::exception& e) {
      fprintf(stderr, "Skipping block: %s\n", e.what());
    }
  }
  in.close();
  if (!found_data) error("archive contains no data");
  return block_offset;
}

// Mark each file in dt that matches the args to -add, -extract, -list
// (in files[]) and not matched to -not (in notfiles[])
// using written=0 for each match. Match all files in dt if no args
// (files[] is empty). If all is true, then mark deleted files too.
// If scan is true then recursively scan external directories in args,
// add to dt, and mark them.
void Jidac::read_args(bool scan, bool all) {

  // Match to files[] except notfiles[] or match all if files[] is empty
  if (!quiet && scan && size(files)) printf("Scanning files\n");
  for (DTMap::iterator p=dt.begin(); p!=dt.end(); ++p) {
    if (p->second.dtv.size()<1) {
      fprintf(stderr, "Invalid index entry: %s\n", p->first.c_str());
      error("corrupted index");
    }
    bool matched=size(files)==0;
    for (int i=0; !matched && i<size(files); ++i)
      if (ispath(files[i].c_str(), p->first.c_str()))
        matched=true;
    for (int i=0; matched && i<size(notfiles); ++i)
      if (ispath(notfiles[i].c_str(), p->first.c_str()))
        matched=false;
    if (matched &&
        (all || (p->second.dtv.size() && p->second.dtv.back().date)))
      p->second.written=0;
  }

  // Scan external files and directories, insert into dt and mark written=0
  if (scan)
    for (int i=0; i<size(files); ++i)
      scandir(rename(files[i]).c_str());
}

// Return the part of fn up to the last slash
string path(const string& fn) {
  int n=0;
  for (int i=0; fn[i]; ++i)
    if (fn[i]=='/' || fn[i]=='\\') n=i+1;
  return fn.substr(0, n);
}

// Insert filename (UTF-8 with "/") into dt unless in notfiles.
// If filename is a directory then also insert its contents.
// In Windows, filename might have wildcards like "file.*" or "dir/*"
void Jidac::scandir(const char* filename) {

#ifdef unix

  // Omit if in notfiles
  for (int i=0; i<size(notfiles); ++i)
    if (ispath(notfiles[i].c_str(), unrename(filename).c_str())) return;

  // Add regular files and directories
  struct stat sb;
  if (!lstat(filename, &sb)) {
    if (S_ISREG(sb.st_mode))
      addfile(filename, decimal_time(sb.st_mtime), sb.st_size,
              'u'+(sb.st_mode<<8));

    // Traverse directory
    if (S_ISDIR(sb.st_mode)) {
      addfile((string(filename)+"/").c_str(),
              decimal_time(sb.st_mtime), sb.st_size,
              'u'+(sb.st_mode<<8));
      DIR* dirp=opendir(filename);
      if (dirp) {
        for (dirent* dp=readdir(dirp); dp; dp=readdir(dirp)) {
          if (strcmp(".", dp->d_name) && strcmp("..", dp->d_name)) {
            string s=filename;
            int len=s.size();
            if (len>0 && s[len-1]!='/' && s[len-1]!='\\') s+="/";
            s+=dp->d_name;
            scandir(s.c_str());
          }
        }
        closedir(dirp);
      }
    }
  }

#else  // Windows: expand wildcards in filename

  // Expand wildcards
  WIN32_FIND_DATA ffd;
  HANDLE h=FindFirstFile(utow(filename).c_str(), &ffd);
  while (h!=INVALID_HANDLE_VALUE) {

    // For each file, get name, date, size, attributes
    SYSTEMTIME st;
    int64_t edate=0;
    if (FileTimeToSystemTime(&ffd.ftLastWriteTime, &st))
      edate=st.wYear*10000000000LL+st.wMonth*100000000LL+st.wDay*1000000
            +st.wHour*10000+st.wMinute*100+st.wSecond;
    const int64_t esize=ffd.nFileSizeLow+(int64_t(ffd.nFileSizeHigh)<<32);
    const int64_t eattr='w'+(int64_t(ffd.dwFileAttributes)<<8);

    // Ignore the names "." and ".." or any path/name in notfiles
    string t=wtou(ffd.cFileName);
    if (t=="." || t=="..") edate=0;  // don't add
    string fn=path(filename)+t;
    for (int i=0; i<size(notfiles); ++i)
      if (ispath(notfiles[i].c_str(), unrename(fn).c_str()))
        edate=0;

    // Save directory names with a trailing / and scan their contents
    // Otherwise, save plain files
    if (edate) {
      if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        fn+="/";
      addfile(fn.c_str(), edate, esize, eattr);
      if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
        fn+="*";
        scandir(fn.c_str());
      }
    }
    if (!FindNextFile(h, &ffd)) break;
  }
  FindClose(h);
#endif
}

// Add external file and its date, size, and attributes to dt
void Jidac::addfile(const char* filename, int64_t edate,
                    int64_t esize, int64_t eattr) {
  DT& d=dt[unrename(filename)];
  d.edate=edate;
  d.esize=esize;
  d.eattr=eattr;
  d.written=0;
}

/////////////////////////////// add ///////////////////////////////////

// E8E9 transform of buf[0..n-1] to improve compression of .exe and .dll.
// Patterns (E8|E9 xx xx xx 00|FF) at offset i replace the 3 middle
// bytes with x+i mod 2^24, LSB first, reading backward.
void e8e9(unsigned char* buf, int n) {
  for (int i=n-5; i>=0; --i) {
    if (((buf[i]&254)==0xe8) && ((buf[i+4]+1)&254)==0) {
      unsigned a=(buf[i+1]|buf[i+2]<<8|buf[i+3]<<16)+i;
      buf[i+1]=a;
      buf[i+2]=a>>8;
      buf[i+3]=a>>16;
    }
  }
}

// BWT Preprocessor preprocessed with e8e9.
// Format is a Burrows-Wheeler transform with the terminal symbol -1
// included and encoded as 255. The location of this symbol (idx) is
// encoded by appending it as 4 bytes LSB first.

class BWTBuffer: public libzpaq::Reader {
  enum {N=1<<24};  // array size
  vector<unsigned char> buf;
  unsigned rpos, n;
public:
  int get() {return rpos<n ? buf[rpos++] : -1;}
  BWTBuffer(StringBuffer& in);
  size_t size() const {return n;}  
};

BWTBuffer::BWTBuffer(StringBuffer& in):
    buf(N), rpos(0), n(in.size()) {
  libzpaq::Array<int> w(N);
  assert(n+5<=N);
  memcpy(&buf[0], in.c_str(), n);
  e8e9(&buf[0], n);
  int idx=divbwt(&buf[0], &buf[0], &w[0], n);
  assert(idx>=0 && idx<=int(n));
  memmove(&buf[idx+1], &buf[idx], n-idx);
  buf[idx]=255;
  for (int i=0; i<4; ++i) buf[n+1+i]=idx>>(i*8);
  n+=5;
}

// LZ preprocessor for level 1 or 2 compression and e8e9 filter.
// Level 1 uses variable length LZ77 codes like in the lazy compressor:
//
//   00,n,L[n] = n literal bytes
//   mm,mmm,n,ll,q (mm > 00) = match of length 4n+ll, offset q
//
// where q is written in 8mm+mmm-8 (0..23) bits with an implied leading 1 bit
// and n is written using interleaved Elias Gamma coding, i.e. the leading
// 1 bit is implied, remaining bits are preceded by a 1 and terminated by
// a 0. e.g. abc is written 1,b,1,c,0. Codes are packed LSB first and
// padded with leading 0 bits in the last byte.
//
// Level 2 is byte oriented LZ77 as in the lzpre compressor.
// Lengths and offsets are MSB first:
// 00xxxxxx                            x+1 (1..64) literals follow
// 01xxxyyy yyyyyyyy                   copy x+5 (5..12), offset y+1 (1..2048)
// 10xxxxxx yyyyyyyy yyyyyyyy          copy x+1 (1..64), offset y+1 (1..65536)
// 11xxxxxx yyyyyyyy yyyyyyyy yyyyyyyy copy x+1 (1..64), offset y+1 (1..2^24)

// return 0..32 bits in x not counting leading 0 bits
int lg(unsigned x) {
  for (unsigned i=0; i<32; ++i)
    if ((1u<<i)>x) return i;
  return 32;
}

class LZBuffer: public libzpaq::Reader {
  const unsigned char* in;    // input
  const unsigned n;           // input length
  StringBuffer buf;           // compressed output
  int level;                  // method (1 or 2)
  void write_literal1(unsigned i, unsigned& lit);  // level 1
  void write_match1(unsigned len, unsigned off);
  void write_literal2(unsigned i, unsigned& lit);  // level 2
  void write_match2(unsigned len, unsigned off);
  unsigned bits;              // pending output bits
  unsigned nbits;             // number of bits in bits
  void putb(unsigned x, int k) {  // write k bits of x
    x&=(1<<k)-1;
    bits|=x<<nbits;
    nbits+=k;
    while (nbits>7) buf.put(bits&255), bits>>=8, nbits-=8;
  }
  void flush() {  // write last byte
    if (nbits>0) buf.put(bits);
    bits=nbits=0;
  }
public:
  LZBuffer(StringBuffer& inbuf, int level);  // input to compress
  int get() {return buf.get();}   // return 1 byte of compressed output
  size_t size() const {return buf.size();}  
};

// Write literal sequence in[i-lit..i-1], set lit=0
// 00,u,L[u*8] = L literals
void LZBuffer::write_literal1(unsigned i, unsigned& lit) {
  assert(lit>=0);
  assert(i>=0 && i<=n);
  assert(i>=lit);
  if (lit<1) return;
  int ll=lg(lit);
  assert(ll>=1 && ll<=24);
  putb(0, 2);
  --ll;
  while (--ll>=0) {
    putb(1, 1);
    putb((lit>>ll)&1, 1);
  }
  putb(0, 1);
  while (lit) putb(in[i-lit--], 8);
}

// Write match sequence of given length and offset (off in 1..2^24)
// jj,kkk,u,nn,m[jjkkk-8] = match length unn, offset 1m
void LZBuffer::write_match1(unsigned len, unsigned off) {
  assert(len>=4 && len<(1<<24));
  assert(off>=1 && off<(1<<24));
  int ll=lg(len);
  assert(ll>=3 && ll<=24);
  int lo=lg(off);
  assert(lo>=1 && lo<=24);
  --lo;
  putb((lo>>3)+1, 2);
  putb(lo&7, 3);
  --ll;
  while (--ll>=2) {
    putb(1, 1);
    putb((len>>ll)&1, 1);
  }
  putb(0, 1);
  putb(len&3, 2);
  putb(off, lo);
}

// Write literal sequence buf[i-lit..i-1], set lit=0
void LZBuffer::write_literal2(unsigned i, unsigned& lit) {
  while (lit>0) {
    unsigned lit1=lit;
    if (lit1>64) lit1=64;
    buf.put(lit1-1);
    for (unsigned j=i-lit; j<i-lit+lit1; ++j) buf.put(in[j]);
    lit-=lit1;
  }
}

// Write match sequence of given length and offset (off in 1..2^24)
void LZBuffer::write_match2(unsigned len, unsigned off) {
  assert(off>0);
  --off;
  while (len>0) {
    int len1=len;
    if (len1>64) len1=64;
    if (off<2048 && len1>=3+level && len1<=10+level) {
      buf.put(64+(len1-3-level)*8+(off>>8));
      buf.put(off);
    }
    else if (off<65536) {
      buf.put(128+len1-1);
      buf.put(off>>8);
      buf.put(off);
    }
    else {
      buf.put(192+len1-1);
      buf.put(off>>16);
      buf.put(off>>8);
      buf.put(off);
    }
    len-=len1;
  }
}

LZBuffer::LZBuffer(StringBuffer& inbuf, int level_):
    in((const unsigned char*)inbuf.c_str()), n(inbuf.size()),
    buf(inbuf.size()*9/8), level(level_), bits(0), nbits(0) {
  assert(level==1 || level==2);

  // Tunable LZ77 parameters for levels 1 and 2
  const int HASHES[3]={0,1,2};  // level -> number of hashes computed per byte
  const unsigned HASHORDER[3][2]={{0},{4},{10,5}};  // context orders
  const unsigned HASHMUL[3][2]={{0},{96},{44,48}};  // hash multipliers
  const unsigned BUCKETBITS[3]={0,3,2};             // log bucket size
  const int bb=BUCKETBITS[level];
  const int bucket=1<<bb;

  // Allocate hashtable ht
  // ht[h] low 24 bits points to in[i..i+HASHORDER-1], high 8 bits is in[i]
  libzpaq::Array<unsigned> ht(1<<22);
  int h[2]={0};  // context hashes of in[i..]

  // e8e9 transform
  e8e9(inbuf.data(), n);

  // Scan the input
  unsigned lit=0;  // number of output literals pending
  for (unsigned i=0; i<n;) {

    // Search for longest match, or pick closest in case of tie
    // Try the longest context orders first. If a match is found, then
    // skip the lower orders as a speed optimization.
    unsigned blen=0, bp=0, len=0;
    for (int j=0; j<HASHES[level]; ++j) {
      for (int k=0; k<bucket; ++k) {
        unsigned p=ht[h[j]+k];
        if ((p>>24)==in[i]) {  // compare in ht first
          p=(p&0xffffff)+(i&0xff000000);
          if (p>i) p-=0x1000000;
          if (p<i && p+(1<<24)>i) {
            for (len=0; i+len<n && in[p+len]==in[i+len]; ++len);
            if (len>blen || (len==blen && p>bp)) blen=len, bp=p;
          }
        }
        if (blen>=128) break;
      }
      if (blen>=HASHORDER[level][j]) break;
    }

    // If match is long enough, then output any pending literals first,
    // and then the match. blen is the length of the match.
    assert(i>=bp);
    const unsigned off=i-bp;  // offset
    if (level==2 && off>0 && off<(1<<24)
        && blen>=5u+(off>=2048)+(off>=65536)) {
      write_literal2(i, lit);
      write_match2(blen, off);
    }
    else if (level==1 && off>0 && off<(1<<24) 
             && blen>=4u+(lit!=0 && off>=65536)) {
      write_literal1(i, lit);
      write_match1(blen, off);
    }

    // Otherwise add to literal length
    else {
      blen=1;
      ++lit;
    }

    // Update index, advance blen bytes
    while (blen--) {
      for (int j=0; j<HASHES[level]; ++j)
        ht[h[j]+(i&(bucket-1))]=(i&0xffffff)|(in[i]<<24);
      ++i;
      for (int j=0; j<HASHES[level]; ++j) {
        if (i+HASHORDER[level][j]<=n) {
          h[j]>>=bb;
          h[j]*=HASHMUL[level][j];
          h[j]+=in[i+HASHORDER[level][j]-1]+1;
          h[j]<<=bb;
          h[j]&=ht.size()-1;
        }
      }
    }
  }

  // Write pending literals at end of block
  if (level==1) write_literal1(n, lit);
  if (level==2) write_literal2(n, lit);
  flush();
}

// Compress from in to out in 1 block. method is "1".."4" or ZPAQL source
// with args[9] or 0.  jobNumber is unique among threads in order to
// create unique temporary file names. filename is saved in the segment
// header. comment is appended to the decimal size
void compressBlock(StringBuffer* in, libzpaq::Writer* out,
                   const char* method, int* args=0, int jobNumber=0,
                   const char* filename=0, const char* comment=0) {

  assert(in);
  assert(out);
  assert(method && *method);
  if (!quiet)
    printf("Job %d compressing %d bytes to %s %s\n", jobNumber,
           size(*in), filename?filename:"", comment?comment:"");

  // Built in ZPAQL code for -method 0..4
  const char* cfg[6]={
    "(method 0 - store uncompressed) "
    "comp 0 0 0 0 0 hcomp post 0 end ",

    "(method 1 - lazy2 = e8e9 + LZ77) "
    "comp 0 0 0 24 0 (for files up to 2^24 bytes) "
    "hcomp "
    "pcomp lazy2 3 ; (can be 1..5 for fastest..best compression) "
    " (r1 = state "
    "  r2 = len - match or literal length "
    "  r3 = m - number of offset bits expected "
    "  r4 = ptr to buf "
    "  c = bits - input buffer "
    "  d = n - number of bits in c) "
    " "
    "  (At EOF, do e8e9 transform of (e8|e9 xx xx xx 00|ff) and output) "
    "  a> 255 if "
    "    b=0 d=r 4 do (for b=0..d-1, d = end of buf) "
    "      a=b a==d ifnot "
    "        a+= 4 a<d if "
    "          a=*b a&= 254 a== 232 if (e8 or e9?) "
    "            c=b b++ b++ b++ b++ a=*b a++ a&= 254 a== 0 if (00 or ff) "
    "              b-- a=*b "
    "              b-- a<<= 8 a+=*b "
    "              b-- a<<= 8 a+=*b "
    "              a-=b a++ "
    "              *b=a a>>= 8 b++ "
    "              *b=a a>>= 8 b++ "
    "              *b=a b++ "
    "            endif "
    "            b=c "
    "          endif "
    "        endif "
    "        a=*b out b++ "
    "      forever "
    "    endif "
    " "
    "    (reset state) "
    "    a=0 b=0 c=0 d=0 r=a 1 r=a 2 r=a 3 r=a 4 "
    "    halt "
    "  endif "
    " "
    "  a<<=d a+=c c=a               (bits+=a<<n) "
    "  a= 8 a+=d d=a                (n+=8) "
    " "
    "  (if state==0 (expect new code)) "
    "  a=r 1 a== 0 if (match code jj,kkk) "
    "    a= 1 r=a 2                 (len=1) "
    "    a=c a&= 3 a> 0 if          (if (bits&3)) "
    "      a-- a<<= 3 r=a 3           (m=((bits&3)-1)*8) "
    "      a=c a>>= 2 c=a             (bits>>=2) "
    "      b=r 3 a&= 7 a+=b r=a 3     (m+=bits&7) "
    "      a=c a>>= 3 c=a             (bits>>=3) "
    "      a=d a-= 5 d=a              (n-=5) "
    "      a= 1 r=a 1                 (state=1) "
    "    else (literal, discard 00) "
    "      a=c a>>= 2 c=a             (bits>>=2) "
    "      d-- d--                    (n-=2) "
    "      a= 3 r=a 1                 (state=3) "
    "    endif "
    "  endif "
    " "
    "  (while state==1 && n>=3 (expect match length u,nn)) "
    "  do a=r 1 a== 1 if a=d a> 2 if "
    "    a=c a&= 1 a== 1 if         (if bits&1) "
    "      a=c a>>= 1 c=a             (bits>>=1) "
    "      b=r 2 a=c a&= 1 a+=b a+=b r=a 2 (len+=len+(bits&1)) "
    "      a=c a>>= 1 c=a             (bits>>=1) "
    "      d-- d--                    (n-=2) "
    "    else "
    "      a=c a>>= 1 c=a             (bits>>=1) "
    "      a=r 2 a<<= 2 b=a           (len<<=2) "
    "      a=c a&= 3 a+=b r=a 2       (len+=bits&3) "
    "      a=c a>>= 2 c=a             (bits>>=2) "
    "      d-- d-- d--                (n-=3) "
    "      a= 2 r=a 1                 (state=2) "
    "    endif "
    "  forever endif endif "
    " "
    "  (if state==2 && n>=m) (expect m offset bits) "
    "  a=r 1 a== 2 if a=r 3 a>d ifnot "
    "    a=c r=a 6 a=d r=a 7          (save c=bits, d=n in r6,r7) "
    "    b=r 3 a= 1 a<<=b d=a         (d=1<<m) "
    "    a-- a&=c a+=d d=a            (d=offset=bits&((1<<m)-1)|(1<<m)) "
    "    b=r 4 a=b a-=d c=a           (c=p=(b=ptr)-offset) "
    "     "
    "    (while len-- (copy and output match)) "
    "    d=r 2 do a=d a> 0 if d-- "
    "      a=*c *b=a c++ b++          (buf[ptr++]-buf[p++]) "
    "    forever endif "
    "    a=b r=a 4 "
    " "
    "    a=r 6 b=r 3 a>>=b c=a        (bits>>=m) "
    "    a=r 7 a-=b d=a               (n-=m) "
    "    a=0 r=a 1                    (state=0) "
    "  endif endif "
    " "
    "  (while state==3 && n>=2 (expect literal length)) "
    "  do a=r 1 a== 3 if a=d a> 1 if "
    "    a=c a&= 1 a== 1 if         (if bits&1) "
    "      a=c a>>= 1 c=a              (bits>>=1) "
    "      b=r 2 a&= 1 a+=b a+=b r=a 2 (len+=len+(bits&1)) "
    "      a=c a>>= 1 c=a              (bits>>=1) "
    "      d-- d--                     (n-=2) "
    "    else "
    "      a=c a>>= 1 c=a              (bits>>=1) "
    "      d--                         (--n) "
    "      a= 4 r=a 1                  (state=4) "
    "    endif "
    "  forever endif endif "
    " "
    "  (if state==4 && n>=8 (expect len literals)) "
    "  a=r 1 a== 4 if a=d a> 7 if "
    "    b=r 4 a=c *b=a b++ a=b r=a 4  (buf[ptr++]=bits) "
    "    a=c a>>= 8 c=a               (bits>>=8) "
    "    a=d a-= 8 d=a                (n-=8) "
    "    a=r 2 a-- r=a 2 a== 0 if     (if --len<1) "
    "      a=0 r=a 1                    (state=0) "
    "    endif "
    "  endif endif "
    "  halt "
    "end ",

    "(method 2 - LZ77 + ICM) "
    "comp 0 0 0 24 1 "
    "  0 icm 12 (sometimes \"0 cm 20 48\" will compress better) "
    "hcomp "
    "  (c=state: 0=init, 1=expect LZ77 literal or match code, "
    "   2..4=expect n-1 offset bytes, "
    "   5..68=expect n-4 literals) "
    "  b=a (save input) "
    "  a=c a== 1 if (expect code ccxxxxxx as input) "
    "    (cc is number of offset bytes following) "
    "    (00xxxxxx means x+1 literal bytes follow) "
    "    a=b a>>= 6 a&= 3 a> 0 if "
    "      a++ c=a (high 2 bits is code length) "
    "      *d=0 a=b a>>= 3 hashd "
    "    else "
    "      a=b a&= 63 a+= 5 c=a (literal length) "
    "      *d=0 a=b hashd "
    "    endif "
    "  else "
    "    a== 5 if (end of literal) "
    "      c= 1 *d=0 "
    "    else "
    "      a== 0 if (init) "
    "        c= 124 *d=0 (5+length of postprocessor) "
    "      else (literal or offset) "
    "        c-- "
    "        (model literals in order 1 context, offset order 0) "
    "        a> 5 if *d=0 a=b hashd endif "
    "      endif "
    "    endif "
    "  endif "
    " "
    "  (model parse state as context) "
    "  a=c a> 5 if a= 5 endif hashd "
    "  halt "
    "pcomp lzpre c ; "
    "  (Decode LZ77: d=state, M=output buffer, b=size) "
    "  a> 255 if (at EOF decode e8e9 and output) "
    "    d=b b=0 do (for b=0..d-1, d = end of buf) "
    "      a=b a==d ifnot "
    "        a+= 4 a<d if "
    "          a=*b a&= 254 a== 232 if (e8 or e9?) "
    "            c=b b++ b++ b++ b++ a=*b a++ a&= 254 a== 0 if (00 or ff) "
    "              b-- a=*b "
    "              b-- a<<= 8 a+=*b "
    "              b-- a<<= 8 a+=*b "
    "              a-=b a++ "
    "              *b=a a>>= 8 b++ "
    "              *b=a a>>= 8 b++ "
    "              *b=a b++ "
    "            endif "
    "            b=c "
    "          endif "
    "        endif "
    "        a=*b out b++ "
    "      forever "
    "    endif "
    "    b=0 c=0 d=0 a=0 r=a 1 r=a 2 (reset state) "
    "  halt "
    "  endif "
    " "
    "  (in state d==0, expect a new code) "
    "  c=a a=d a== 0 if "
    "    a=c a>>= 6 a++ d=a "
    "    a== 1 if (state?) "
    "      a+=c r=a 1 a=0 r=a 2 (literal r1=len=c+1 r2=off=0) "
    "    else "
    "      a== 2 if a=c a&= 7 r=a 2 (short match: off=c&7) "
    "        a=c a>>= 3 a-= 3 r=a 1 (len=(c>>3)-3) "
    "      else (3 or 4 byte match) "
    "        a=c a&= 63 a++ r=a 1 a=0 r=a 2 (off=0, len=(c&63)-1) "
    "      endif "
    "    endif "
    "  else "
    "    a== 1 if (writing literal) "
    "      a=c *b=a b++ "
    "      a=r 1 a-- a== 0 if d=0 endif r=a 1 (if (--len==0) state=0) "
    "    else "
    "      a> 2 if (reading offset) "
    "        a=r 2 a<<= 8 a|=c r=a 2 d-- (off=off<<8|c, --state) "
    "      else (state==2, write match) "
    "        a=r 2 a<<= 8 a|=c c=a a=b a-=c a-- c=a (c=i-off-1) "
    "        d=r 1 (d=len) "
    "        do (copy and output d=len bytes) "
    "          a=*c *b=a c++ b++ "
    "        d-- a=d a> 0 while "
    "        (d=state=0. off, len don\'t matter) "
    "      endif "
    "    endif "
    "  endif "
    "  halt "
    "end ",

    "(method 3 - BWT) "
    "comp 1 0 24 24 2 "
    "  0 icm 5 "
    "  1 isse 12 0 "
    "hcomp "
    "  d= 1 *d=0 hashd halt "
    "pcomp bwtrle c ; "
    " "
    "  (read BWT, index into M, size in b) "
    "  a> 255 ifnot "
    "    *b=a b++ "
    " "
    "  (inverse BWT) "
    "  elsel "
    " "
    "    (index in last 4 bytes, put in c and R1) "
    "    b-- a=*b "
    "    b-- a<<= 8 a+=*b "
    "    b-- a<<= 8 a+=*b "
    "    b-- a<<= 8 a+=*b c=a r=a 1 "
    " "
    "    (save size in R2) "
    "    a=b r=a 2 "
    " "
    "    (count bytes in H[~1..~255, ~0]) "
    "    do "
    "      a=b a> 0 if "
    "        b-- a=*b a++ a&= 255 d=a d! *d++ "
    "      forever "
    "    endif "
    " "
    "    (cumulative counts: H[~i=0..255] = count of bytes before i) "
    "    d=0 d! *d= 1 a=0 "
    "    do "
    "      a+=*d *d=a d-- "
    "    d<>a a! a> 255 a! d<>a until "
    " "
    "    (build first part of linked list in H[0..idx-1]) "
    "    b=0 do "
    "      a=c a>b if "
    "        d=*b d! *d++ d=*d d-- *d=b "
    "      b++ forever "
    "    endif "
    " "
    "    (rest of list in H[idx+1..n-1]) "
    "    b=c b++ c=r 2 do "
    "      a=c a>b if "
    "        d=*b d! *d++ d=*d d-- *d=b "
    "      b++ forever "
    "    endif "
    " "
    "    (copy M to low 8 bits of H to reduce cache misses in next loop) "
    "    b=0 do "
    "      a=c a>b if "
    "        d=b a=*d a<<= 8 a+=*b *d=a "
    "      b++ forever "
    "    endif "
    " "
    "    (traverse list and copy to M) "
    "    d=r 1 b=0 do "
    "      a=d a== 0 ifnot "
    "        a=*d a>>= 8 d=a "
    "        *b=*d b++ "
    "      forever "
    "    endif "
    " "
    "    (e8e9 transform to out) "
    "    d=b b=0 do (for b=0..d-1, d = end of buf) "
    "      a=b a==d ifnot "
    "        a+= 4 a<d if "
    "          a=*b a&= 254 a== 232 if (e8 or e9?) "
    "            c=b b++ b++ b++ b++ a=*b a++ a&= 254 a== 0 if (00 or ff) "
    "              b-- a=*b "
    "              b-- a<<= 8 a+=*b "
    "              b-- a<<= 8 a+=*b "
    "              a-=b a++ "
    "              *b=a a>>= 8 b++ "
    "              *b=a a>>= 8 b++ "
    "              *b=a b++ "
    "            endif "
    "            b=c "
    "          endif "
    "        endif "
    "        a=*b out b++ "
    "      forever "
    "    endif "
    " "
    "  endif "
    "  halt "
    "end "
    " ",

    "(method 4 mid.cfg with e8e9 postprocessing) "
    "comp 3 3 0 24 8 (hh hm ph pm n) "
    "  0 icm 5        (order 0...5 chain) "
    "  1 isse 13 0 "
    "  2 isse $1+17 1 "
    "  3 isse $1+18 2 "
    "  4 isse $1+18 3 "
    "  5 isse $1+19 4 "
    "  6 match $1+22 24  (order 7) "
    "  7 mix 16 0 7 24 255  (order 1) "
    "hcomp "
    "  c++ *c=a b=c a=0 (save in rotating buffer M) "
    "  d= 1 hash *d=a   (orders 1...5 for isse) "
    "  b-- d++ hash *d=a "
    "  b-- d++ hash *d=a "
    "  b-- d++ hash *d=a "
    "  b-- d++ hash *d=a "
    "  b-- d++ hash b-- hash *d=a (order 7 for match) "
    "  d++ a=*c a<<= 8 *d=a (order 1 for mix) "
    "  halt "
    "pcomp e8e9 d ; "
    "  (Decode LZ77: M=output buffer, b=size) "
    "  a> 255 if (at EOF decode e8e9 and output) "
    "    d=b b=0 do (for b=0..d-1, d = end of buf) "
    "      a=b a==d ifnot "
    "        a+= 4 a<d if "
    "          a=*b a&= 254 a== 232 if (e8 or e9?) "
    "            c=b b++ b++ b++ b++ a=*b a++ a&= 254 a== 0 if (00 or ff) "
    "              b-- a=*b "
    "              b-- a<<= 8 a+=*b "
    "              b-- a<<= 8 a+=*b "
    "              a-=b a++ "
    "              *b=a a>>= 8 b++ "
    "              *b=a a>>= 8 b++ "
    "              *b=a b++ "
    "            endif "
    "            b=c "
    "          endif "
    "        endif "
    "        a=*b out b++ "
    "      forever "
    "    endif "
    "    b=0 (reset state) "
    "  else "
    "    *b=a b++ "
    "  endif "
    "  halt "
    "end ",

    // method 5 - max.cfg + E8E9
    "comp 5 9 0 $1+24 22 (hh hm ph pm n) "
    "  0 const 160 "
    "  1 icm 5  (orders 0-6) "
    "  2 isse 13 1 (sizebits j) "
    "  3 isse $1+16 2 "
    "  4 isse $1+18 3 "
    "  5 isse $1+19 4 "
    "  6 isse $1+19 5 "
    "  7 isse $1+19 6 "
    "  8 match $1+21 $1+24 "
    "  9 icm $1+17 (order 0 word) "
    "  10 isse $1+19 9 (order 1 word) "
    "  11 icm 13 (sparse with gaps 1-3) "
    "  12 icm 13 "
    "  13 icm 13 "
    "  14 icm 14 (pic) "
    "  15 mix 16 0 15 24 255 (mix orders 1 and 0) "
    "  16 mix 8 0 16 10 255 (including last mixer) "
    "  17 mix2 0 15 16 24 0 "
    "  18 sse 8 17 32 255 (order 0) "
    "  19 mix2 8 17 18 16 255 "
    "  20 sse 16 19 32 255 (order 1) "
    "  21 mix2 0 19 20 16 0 "
    "hcomp "
    "  c++ *c=a b=c a=0 (save in rotating buffer) "
    "  d= 2 hash *d=a b-- (orders 1,2,3,4,5,7) "
    "  d++ hash *d=a b-- "
    "  d++ hash *d=a b-- "
    "  d++ hash *d=a b-- "
    "  d++ hash *d=a b-- "
    "  d++ hash b-- hash *d=a b-- "
    "  d++ hash *d=a b-- (match, order 8) "
    "  d++ a=*c a&~ 32 (lowercase words) "
    "  a> 64 if "
    "    a< 91 if (if a-z) "
    "      d++ hashd d-- (update order 1 word hash) "
    "      *d<>a a+=*d a*= 20 *d=a (order 0 word hash) "
    "      jmp 9 "
    "    endif "
    "  endif "
    "  (else not a letter) "
    "    a=*d a== 0 ifnot (move word order 0 to 1) "
    "      d++ *d=a d-- "
    "    endif "
    "    *d=0  (clear order 0 word hash) "
    "  (end else) "
    "  d++ "
    "  d++ b=c b-- a=0 hash *d=a (sparse 2) "
    "  d++ b-- a=0 hash *d=a (sparse 3) "
    "  d++ b-- a=0 hash *d=a (sparse 4) "
    "  d++ a=b a-= 212 b=a a=0 hash "
    "    *d=a b<>a a-= 216 b<>a a=*b a&= 60 hashd (pic) "
    "  d++ a=*c a<<= 9 *d=a (mix) "
    "  d++ "
    "  d++ "
    "  d++ d++ "
    "  d++ *d=a (sse) "
    "  halt "
    "pcomp e8e9 d ; "
    "  (Decode LZ77: M=output buffer, b=size) "
    "  a> 255 if (at EOF decode e8e9 and output) "
    "    d=b b=0 do (for b=0..d-1, d = end of buf) "
    "      a=b a==d ifnot "
    "        a+= 4 a<d if "
    "          a=*b a&= 254 a== 232 if (e8 or e9?) "
    "            c=b b++ b++ b++ b++ a=*b a++ a&= 254 a== 0 if (00 or ff) "
    "              b-- a=*b "
    "              b-- a<<= 8 a+=*b "
    "              b-- a<<= 8 a+=*b "
    "              a-=b a++ "
    "              *b=a a>>= 8 b++ "
    "              *b=a a>>= 8 b++ "
    "              *b=a b++ "
    "            endif "
    "            b=c "
    "          endif "
    "        endif "
    "        a=*b out b++ "
    "      forever "
    "    endif "
    "    b=0 (reset state) "
    "  else "
    "    *b=a b++ "
    "  endif "
    "  halt "
    "end "
  };

  int level=-1;
  if (*method>='0' && *method<='9' && method[1]==0) {
    level=*method-'0';
    if (level>5) method=cfg[5];
    else method=cfg[level];
    assert(level!=1 || size(*in)<(1<<24));
    assert(level!=2 || size(*in)<(1<<24));
    assert(level!=3 || size(*in)<=(1<<24)-257);
    assert(level!=4 || size(*in)<(1<<24));
    assert(level<5  || size(*in)<(1<<(24+level-5)));
  }

  // Get hash of input
  libzpaq::SHA1 sha1;
  const char* sha1ptr=0;
  const int n=in->size();  // input size
  for (const char* p=in->c_str(), *end=p+n; p<end; ++p)
    sha1.put(*p);
  sha1ptr=sha1.result();

  // Compress in to out using method
  libzpaq::Compressor co;
  co.setOutput(out);
  StringBuffer pcomp_cmd;
  co.writeTag();
  if (level>=5 && level<=9) {
    int a[9]={0};
    a[0]=level-5;
    co.startBlock(method, a, &pcomp_cmd);
  }
  else
    co.startBlock(method, args, &pcomp_cmd);
  string cs=itos(n);
  if (comment) cs+=comment;
  co.startSegment(filename, cs.c_str());
  if (level==1 || level==2) {  // preprocess with built-in LZ77
    LZBuffer lz(*in, level);
    co.setInput(&lz);
    co.compress();
    co.endSegment(sha1ptr);
  }
  else if (level==3) {  // preprocess with built-in BWT
    BWTBuffer bwt(*in);
    co.setInput(&bwt);
    co.compress();
    co.endSegment(sha1ptr);
  }
  else {  // compress method 4 with e8e9 else without preprocessing
    if (level>=4 && level<=9)
      e8e9(in->data(), in->size());
    co.setInput(in);
    co.compress();
    co.endSegment(sha1ptr);
  }
  co.endBlock();
}

// A CompressJob is a queue of blocks to compress and write to the archive.
// Each block cycles through states EMPTY, FILLING, FULL, COMPRESSING,
// COMPRESSED, WRITING. The main thread waits for EMPTY buffers and
// fills them. A set of compressThreads waits for FULL threads and compresses
// them. A writeThread waits for COMPRESSED buffers at the front
// of the queue and writes and removes them.

// Buffer queue element
struct CJ {
  enum {EMPTY, FILLING, FULL, COMPRESSING, COMPRESSED, WRITING} state;
  StringBuffer in, out;  // uncompressed and compressed data
  string filename;       // to write in filename field
  string comment;        // to append to size in comment field
  string method;         // compression algorithm
  int args[9];           // arguments to method
  Semaphore full;        // 1 if in is FULL of data ready to compress
  Semaphore compressed;  // 1 if out contains COMPRESSED data
  bool end;              // mark end of data
  CJ(): state(EMPTY), in(1<<24), out(1<<24), end(false) {
    memset(args, 0, sizeof(args));
  }
};

// Instructions to a compression job
class CompressJob {
  Mutex mutex;           // protects state changes
  int job;               // number of jobs
  vector<CJ> q;          // buffer queue
  int front;             // next to remove from queue
  libzpaq::Writer* out;  // archive
  Semaphore empty;       // number of empty buffers ready to fill
public:
  friend ThreadReturn compressThread(void* arg);
  friend ThreadReturn writeThread(void* arg);
  CompressJob(int t, libzpaq::Writer* f):
      job(0), q(t), front(0), out(f) {
    init_mutex(mutex);
    empty.init(t);
    for (int i=0; i<t; ++i) {
      q[i].full.init(0);
      q[i].compressed.init(0);
    }
  }
  ~CompressJob() {
    for (int i=size(q)-1; i>=0; --i) {
      q[i].compressed.destroy();
      q[i].full.destroy();
    }
    destroy_mutex(mutex);
  }      
  void write(const StringBuffer& s, const char* filename,
             const char* comment, const char* method, const int* args);
  vector<int> csize;  // compressed block sizes
};

// Write s at the back of the queue. Signal end of input with method=0
void CompressJob::write(const StringBuffer& s, const char* fn,
                        const char* cm, const char* method, const int* args) {
  for (unsigned k=(method==0)?q.size():1; k>0; --k) {
    empty.wait();
    lock(mutex);
    unsigned i, j;
    for (i=0; i<q.size(); ++i) {
      if (q[j=(i+front)%q.size()].state==CJ::EMPTY) {
        q[j].state=CJ::FILLING;
        q[j].end=method==0;
        q[j].filename=fn?fn:"";
        q[j].comment=cm?cm:"";
        q[j].method=method?method:"";
        for (int k=0; k<9; ++k) q[j].args[k]=args?args[k]:0;
        release(mutex);
        q[j].in=s;
        lock(mutex);
        q[j].state=CJ::FULL;
        q[j].full.signal();
        break;
      }
    }
    release(mutex);
    assert(i<q.size());  // queue should not be full
  }
}  

// Compress data in the background, one per buffer
ThreadReturn compressThread(void* arg) {
  CompressJob& job=*(CompressJob*)arg;
  int jobNumber=0;
  try {

    // Get job number = assigned position in queue
    lock(job.mutex);
    jobNumber=job.job++;
    assert(jobNumber>=0 && jobNumber<size(job.q));
    CJ& cj=job.q[jobNumber];
    release(job.mutex);

    // Work until done
    while (true) {
      cj.full.wait();
      lock(job.mutex);

      // Check for end of input
      if (cj.end) {
        cj.compressed.signal();
        release(job.mutex);
        return 0;
      }

      // Compress
      assert(cj.state==CJ::FULL);
      cj.state=CJ::COMPRESSING;
      release(job.mutex);
      compressBlock(&cj.in, &cj.out, cj.method.c_str(), cj.args,
                    jobNumber+1, cj.filename.c_str(), cj.comment.c_str());
      lock(job.mutex);
      cj.in.reset();
      cj.state=CJ::COMPRESSED;
      cj.compressed.signal();
      release(job.mutex);
    }
  }
  catch (std::runtime_error& e) {
    fprintf(stderr, "zpaq exiting from job %d: %s\n", jobNumber+1, e.what());
    exit(1);
  }
  return 0;
}

// Write compressed data to the archive in the background
ThreadReturn writeThread(void* arg) {
  CompressJob& job=*(CompressJob*)arg;
  try {

    // work until done
    while (true) {

      // wait for something to write
      CJ& cj=job.q[job.front];  // no other threads move front
      cj.compressed.wait();

      // Quit if end of input
      lock(job.mutex);
      if (cj.end) {
        release(job.mutex);
        return 0;
      }

      // Write to archive
      assert(cj.state==CJ::COMPRESSED);
      cj.state=CJ::WRITING;
      job.csize.push_back(cj.out.size());
      int outsize=cj.out.size();
      if (outsize>0) {
        if (!quiet)
          printf("Job %d -> WriteThread writing %d bytes\n",
                 job.front+1, outsize);
        release(job.mutex);
        job.out->write(cj.out.c_str(), outsize);
        lock(job.mutex);
      }
      cj.state=CJ::EMPTY;
      cj.out.reset();
      job.front=(job.front+1)%job.q.size();
      job.empty.signal();
      release(job.mutex);
    }
  }
  catch (std::runtime_error& e) {
    fprintf(stderr, "zpaq exiting from writeThread: %s\n", e.what());
    exit(1);
  }
  return 0;
}

// Write a ZPAQ compressed JIDAC block header. Output size should not
// depend on input data.
void writeJidacHeader(libzpaq::Writer *out, int64_t date,
                      int64_t cdata, unsigned htsize) {
  assert(out);
  assert(date>=19700000000000LL && date<30000000000000LL);
  libzpaq::Compressor co;
  StringBuffer is;
  is+=ltob(cdata);
  compressBlock(&is, out, "0", 0, 0,
    ("jDC"+itos(date, 14)+"c"+itos(htsize, 10)).c_str(), " jDC\x01");
}

// Compress fragments in b indexed by ht[block_start...] to job
// using method and args with list of fragment sizes appended.
// Select method "0" if misses is near b.size() and reset misses=0.
void Jidac::write_fragments(CompressJob& job, StringBuffer& b,
                            const char* method, const int* args,
                            unsigned& block_start, int& misses) {
  bool iscompressed=true;
  if (strlen(method)==1) {
    switch(method[0]-'0') {
      case 1: iscompressed=b.size()-misses>b.size()/16; break;
      case 2: iscompressed=b.size()-misses>b.size()/32; break;
      case 3: iscompressed=b.size()-misses>b.size()/64; break;
      case 4: iscompressed=b.size()-misses>b.size()/128; break;
    }
  }
  if (!quiet) {
    printf("Fragments %d-%d misses=%d/%d %1.2f%% (%s)\n",
           block_start, size(ht), misses, size(b), misses*100.0/size(b),
           iscompressed ? "compressed" : "stored");
  }
  if (!iscompressed) method="0", args=0;
  if (block_start>=ht.size()) return;
  for (unsigned i=block_start; i<ht.size(); ++i)
    b+=itob(ht[i].usize);
  b+=itob(block_start);
  b+=itob(ht.size()-block_start);
  job.write(b, 
    ("jDC"+itos(date, 14)+"d"+itos(block_start,10)).c_str(), " jDC\x01",
     method, args);
  b.reset();
  ht[block_start].csize=-1;  // to fill in later
  block_start=ht.size();
  misses=0;
}

// Maps sha1 -> fragment ID in ht
class HTIndex {
  enum {N=1<<22};   // size of hash table t
  vector<HT>& htr;  // reference to ht
  vector<vector<unsigned> > t;  // sha1 prefix -> list of indexes
  unsigned htsize;  // number of IDs in t
public:
  HTIndex(vector<HT>& r): htr(r), t(N), htsize(0) {
    update();
  }

  // Compuate a hash index for sha1[20]
  unsigned hash(const unsigned char* sha1) {
    return (sha1[0]|(sha1[1]<<8)|(sha1[2]<<16))&(N-1);
  }

  // Find sha1 in ht. Return its index or 0 if not found.
  unsigned find(const char* sha1) {
    vector<unsigned>& v=t[hash((const unsigned char*)sha1)];
    for (unsigned i=0; i<v.size(); ++i)
      if (memcmp(sha1, htr[v[i]].sha1, 20)==0)
        return v[i];
    return 0;
  }

  // Update index of ht.
  void update() {
    for (; htsize<htr.size(); ++htsize)
      t[hash(htr[htsize].sha1)].push_back(htsize);
  }    
};

// Add files to archive
void Jidac::add() {

  // Get transaction date
  time_t now=time(NULL);
#ifdef unix
  date=decimal_time(now);
#else
  tm* t=gmtime(&now);
  date=(t->tm_year+1900)*10000000000LL+(t->tm_mon+1)*100000000LL
      +t->tm_mday*1000000+t->tm_hour*10000+t->tm_min*100+t->tm_sec;
#endif
  if (!quiet)
    printf("Adding transaction dated %s (UT)\n",
           dateToString(date).c_str());
  if (now==-1 || date<20120000000000LL || date>30000000000000LL)
    error("date is incorrect");

  // Read archive index list into ht, dt.
  const int64_t header_pos=read_archive();
  read_args(true);

  // Build htinv for fast lookups of sha1 in ht
  HTIndex htinv(ht);

  // Open archive to append
  if (!quiet) {
    printf("Appending to archive ");
    printUTF8(archive.c_str());
    printf("\n");
  }
  OutputFile out;
  if (!out.open(archive.c_str())) exit(1);
  int64_t archive_size=out.tell();
  if (archive_size!=header_pos) {
    if (!quiet)
      printf("Archive truncated from %1.0f to %1.0f bytes\n",
             double(archive_size), double(header_pos));
    out.truncate(header_pos);
  }

  // reserve space for the header block
  const unsigned htsize=ht.size();
  writeJidacHeader(&out, date, -1, htsize);
  const int64_t header_end=out.tell();

  // Start compress and write jobs
  vector<ThreadID> tid(threads);
  ThreadID wid;
  CompressJob job(threads, &out);
  for (int i=0; i<threads; ++i) run(tid[i], compressThread, &job);
  run(wid, writeThread, &job);

  // Split input files into fragments and group into blocks.
  unsigned block_start=ht.size();
  StringBuffer b(1<<24);  // current block
  int unmatchedFragments=0, totalFragments=0;  // counts
  int misses=0, fmisses=0;  // mispredictions in block and fragment
  for (DTMap::iterator p=dt.begin(); p!=dt.end(); ++p) {
    if (p->second.edate && (force || p->second.dtv.size()==0
       || p->second.edate!=p->second.dtv.back().date)) {

      // Add directory
      string filename=rename(p->first);
      if (filename!="" && filename[filename.size()-1]=='/') {
        if (!quiet) {
          printf("Adding directory ");
          printUTF8(p->first.c_str());
          printf("\n");
        }
        continue;
      }

      // Open input file
      InputFile in;
      if (!in.open(filename.c_str())) {
        p->second.edate=0;
        continue;
      }
      else if (!quiet) {
        if (p->second.dtv.size()==0 || p->second.dtv.back().date==0) {
          printf("Adding %1.0f ", double(p->second.esize));
          printUTF8(p->first.c_str());
        }
        else {
          printf("Updating %1.0f ", double(p->second.esize));
          printUTF8(p->first.c_str());
        }
        if (p->first!=filename) {
          printf(" from ");
          printUTF8(filename.c_str());
        }
        printf("\n");
      }

      // Split the file into fragments on content-dependent
      // boundaries and group into blocks. For each fragment, look up its
      // hash in ht. If found, then omit it from the block, otherwise append
      // the hash to ht. Save fragment pointers in dt.
      {
        libzpaq::SHA1 sha1;  // to compute SHA-1 hashes and sizes
        int c, c1=0;  // current, previous bytes
        const unsigned HM[2]={314159265, 271828182};  // must be odd and %4==2
        unsigned h=0;  // rolling hash for finding fragment boundaries
        unsigned char o1[256]={0};  // order-1 prediction
        int fragment=0;  // size
        if (size(b)>blocksize*3/4 && size(b)+p->second.esize>blocksize)
          write_fragments(job, b, method.c_str(), 0, block_start, misses);
        do {
          c=in.get();
          if (c!=EOF) {
            sha1.put(c);
            b.put(c);
            ++fragment;
            if (c!=o1[c1]) ++fmisses, h=(h+c+1)*HM[1];
            else h=(h+c+1)*HM[0];
            o1[c1]=c;
            c1=c;
          }
          const int MIN_FRAGMENT=4096;
          const int MAX_FRAGMENT=520192;
          if (c==EOF || (h<65536 && fragment>=MIN_FRAGMENT)
             || fragment>=MAX_FRAGMENT) {
            ++totalFragments;
            const char* sh=sha1.result();  // resets
            unsigned j=htinv.find(sh);
            if (j==0) {  // new hash
              j=ht.size();
              ht.push_back(HT(sh, fragment, 0));
              htinv.update();
              ++unmatchedFragments;
              misses+=fmisses;
            }
            else
              b.resize(b.size()-fragment);
            p->second.eptr.push_back(j);
            fragment=fmisses=0;
            if (b.size()>=blocksize-MAX_FRAGMENT-8-(ht.size()-block_start)*4)
              write_fragments(job, b, method.c_str(), 0,
                              block_start, misses);
            c1=h=0;
            memset(o1, 0, sizeof(o1));
          }
        } while (c!=EOF);
      }
      if (in.tell()!=p->second.esize)
        fprintf(stderr, "Warning: %s size changed from %1.0f to %1.0f\n",
                filename.c_str(), double(p->second.esize), double(in.tell()));
      in.close();
    }
  }
  if (!quiet)
    printf("Matched %d of %d fragments\n",
           totalFragments-unmatchedFragments, totalFragments);

  // compress last partial block
  write_fragments(job, b, method.c_str(), 0, block_start, misses);

  // Wait for jobs to finish
  job.write(b, 0, 0, 0, 0);  // signal end of input
  for (int i=0; i<threads; ++i)
    join(tid[i]);
  join(wid);

  // Fill in compressed sizes in ht
  unsigned j=0;
  for (unsigned i=htsize; i<ht.size() && j<job.csize.size(); ++i)
    if (ht[i].csize==-1)
      ht[i].csize=job.csize[j++];
  assert(j==job.csize.size());

  // Append compressed fragment tables to archive
  if (!quiet) printf("Updating index\n");
  int64_t cdatasize=out.tell()-header_end;  // compressed size for header
  StringBuffer is;
  block_start=0;
  for (unsigned i=htsize; i<=ht.size(); ++i) {
    if ((i==ht.size() || ht[i].csize>0) && is.size()>0) {  // write a block
      assert(block_start>=htsize && block_start<i);
      compressBlock(&is, &out, "0", 0, 0,
        ("jDC"+itos(date, 14)+"h"+itos(block_start, 10)).c_str(),
        " jDC\x01");
        assert(is.size()==0);
    }
    if (i<ht.size()) {
      if (ht[i].csize) is+=itob(ht[i].csize), block_start=i;
      is+=string(ht[i].sha1, ht[i].sha1+20)+itob(ht[i].usize);
    }
  }
  assert(is.size()==0);

  // Append compressed index to archive
  int dtcount=0;
  for (DTMap::const_iterator p=dt.begin(); p!=dt.end();) {
    const DT& dtr=p->second;

    // Remove file if external does not exist and is currently in archive
    if (dtr.written==0 && !dtr.edate && dtr.dtv.size() && dtr.dtv.back().date) {
      is+=ltob(0)+p->first+'\0';
      if (!quiet) {
        printf("Removing ");
        printUTF8(p->first.c_str());
        printf("\n");
      }
    }

    // Update file if compressed and anything differs
    if (p->second.edate && (force || p->second.dtv.size()==0
       || p->second.edate!=p->second.dtv.back().date)) {
      if (dtr.dtv.size()==0 // new file
         || dtr.edate!=dtr.dtv.back().date  // date change
         || dtr.eattr!=dtr.dtv.back().attr  // attr change
         || dtr.eptr!=dtr.dtv.back().ptr) { // content change
        is+=ltob(dtr.edate)+p->first+'\0';
        if ((dtr.eattr&255)=='u') {  // unix attributes
          is+=itob(3);
          is.put('u');
          is.put(dtr.eattr>>8&255);
          is.put(dtr.eattr>>16&255);
        }
        else if ((dtr.eattr&255)=='w') {  // windows attributes
          is+=itob(5);
          is.put('w');
          is+=itob(dtr.eattr>>8);
        }
        else is+=itob(0);
        is+=itob(size(dtr.eptr));  // list of frag pointers
        for (int i=0; i<size(dtr.eptr); ++i)
          is+=itob(dtr.eptr[i]);
      }
    }
    ++p;
    if (is.size()>16000 || (is.size()>0 && p==dt.end())) {
      compressBlock(&is, &out, 
        // 2-D indirect context model with 4 columns
        "comp 0 1 0 0 1 "
        "  0 icm 11 "
        "hcomp "
        "  *b=a hash "
        "  b-- hash *d=a "
        "  halt "
        "post 0 end ",
        0, 0, ("jDC"+itos(date)+"i"+itos(++dtcount, 10)).c_str(), " jDC\x01");
      assert(is.size()==0);
    }
    if (p==dt.end()) break;
  }

  // Back up and write the header
  const int64_t archive_end=out.tell();
  out.seek(header_pos, SEEK_SET);
  writeJidacHeader(&out, date, cdatasize, htsize);
  if (!quiet)
    printf("%1.0f + %1.0f + %1.0f -> %1.0f\n",
           double(header_pos),
           double(header_end-header_pos),
           double(archive_end-header_end),
           double(archive_end));
  assert(header_end==out.tell());
  out.close();
}

/////////////////////////////// extract ///////////////////////////////

// Create directories as needed. For example if path="/tmp/foo/bar"
// then create directories /, /tmp, and /tmp/foo unless they exist.
// Change \ to / in path.
void makepath(string& path) {
  for (int i=0; i<size(path); ++i) {
    if (path[i]=='\\' || path[i]=='/') {
      path[i]=0;
#ifdef unix
      int ok=!mkdir(path.c_str(), 0777);
#else
      int ok=CreateDirectory(utow(path.c_str()).c_str(), 0);
#endif
      if (ok && !quiet) {
        printf("Created directory ");
        printUTF8(path.c_str());
        printf("\n");
      }
      path[i]='/';
    }
  }
}

// An extract job is a set of blocks with at least one file pointing to them.
// Blocks are extracted in separate threads, set READY -> WORKING.
// A block is extracted to memory up to the last fragment that has a file
// pointing to it. Then the checksums are verified. Then for each file
// pointing to the block, each of the fragments that it points to within
// the block are written in order.

struct Block {  // list of fragments
  int64_t offset;       // location in archive
  unsigned start;       // index in ht of first fragment
  int size;             // number of fragments to decompress
  enum {READY, WORKING} state;
  bool streaming;       // extract in main thread?
  Block(unsigned s, int64_t o):
    offset(o), start(s), size(0), state(READY), streaming(false) {}
};

struct ExtractJob {         // list of jobs
  Mutex mutex;              // protects state
  Mutex write_mutex;        // protects writing to disk
  int job;                  // number of jobs started
  vector<Block> block;      // list of blocks to extract
  Jidac& jd;                // what to extract
  OutputFile outf;          // currently open output file
  DTMap::iterator lastdt;  // currently open output file
  ExtractJob(Jidac& j): job(0), jd(j), lastdt(j.dt.end()) {
    init_mutex(mutex);
    init_mutex(write_mutex);
  }
  ~ExtractJob() {
    destroy_mutex(mutex);
    destroy_mutex(write_mutex);
  }
};

// Decompress blocks in a job until none are READY
ThreadReturn decompressThread(void* arg) {
  ExtractJob& job=*(ExtractJob*)arg;
  int jobNumber=0;
  InputFile in;

  // Get job number
  lock(job.mutex);
  jobNumber=++job.job;
  release(job.mutex);

  // Open archive for reading
  if (!in.open(job.jd.archive.c_str())) return 0;
  StringBuffer out(job.jd.blocksize);

  // Look for next READY job
  for (unsigned i=0; i<job.block.size(); ++i) {
    Block& b=job.block[i];
    lock(job.mutex);
    if (b.state==Block::READY) {
      b.state=Block::WORKING;
      release(job.mutex);
    }
    else {
      release(job.mutex);
      continue;
    }
    if (b.size==0 || b.streaming) continue;  // nothing to decompress

    // Get uncompressed size of block
    int output_size=0;
    assert(b.start>0);
    for (int j=0; j<b.size; ++j) {
      assert(b.start+j<job.jd.ht.size());
      assert(job.jd.ht[b.start+j].usize>=0);
      output_size+=job.jd.ht[b.start+j].usize;
    }

    // Decompress
    try {
      assert(b.start>0);
      assert(b.start<job.jd.ht.size());
      assert(b.size>=0);
      assert(b.start+b.size<=job.jd.ht.size());
      in.seek(job.jd.ht[b.start].csize, SEEK_SET);
      libzpaq::Decompresser d;
      d.setInput(&in);
      out.reset();
      d.setOutput(&out);
      if (!d.findBlock()) error("archive block not found");
      while (d.findFilename()) {
        d.readComment();
        while (size(out)<output_size && d.decompress(1<<14));
        if (!quiet)
          printf("Job %d: decompressed %d bytes\n", jobNumber, size(out));
        d.readSegmentEnd();
      }
      if (size(out)<output_size)
        error("unexpected end of compressed data");

      // Verify checksums if present
      const char* q=out.c_str();
      for (unsigned j=b.start; j<b.start+b.size; ++j) {
        libzpaq::SHA1 sha1;
        for (unsigned k=job.jd.ht[j].usize; k>0; --k) sha1.put(*q++);
        if (memcmp(sha1.result(), job.jd.ht[j].sha1, 20)) {
          for (int k=0; k<20; ++k) {
            if (job.jd.ht[j].sha1[k]) {  // all zeros is OK
              fprintf(stderr, 
                     "Job %d: fragment %d size %d checksum failed\n",
                     jobNumber, j, job.jd.ht[j].usize);
              error("bad checksum");
            }
          }
        }
      }
    }
    catch (std::exception& e) {
      fprintf(stderr, "Job %d: skipping block: %s\n", jobNumber, e.what());
      continue;
    }

    // Write the files in dt that point to this block
    lock(job.write_mutex);
    for (DTMap::iterator p=job.jd.dt.begin();p!=job.jd.dt.end();++p){
      DT& dtr=p->second;
      if (dtr.written<0 || size(dtr.dtv)==0 
          || dtr.written>=size(dtr.dtv.back().ptr))
        continue;  // don't write

      // Look for pointers to this block
      vector<unsigned>& ptr=dtr.dtv.back().ptr;
      string filename="";
      int64_t offset=0;  // write offset
      for (unsigned j=0; j<ptr.size(); ++j) {
        if (ptr[j]<b.start || ptr[j]>=b.start+b.size) {
          offset+=job.jd.ht[ptr[j]].usize;
          continue;
        }

        // Close last opened file if different
        if (p!=job.lastdt) {
          if (job.outf.isopen()) {
            assert(job.lastdt!=job.jd.dt.end());
            assert(job.lastdt->second.dtv.size()>0);
            assert(job.lastdt->second.dtv.back().date);
            job.outf.close(job.lastdt->second.dtv.back().date);
          }
          job.lastdt=job.jd.dt.end();
        }

        // Open file for output
        if (!job.outf.isopen()) {
          filename=job.jd.rename(p->first);
          if (dtr.written==0) {
            makepath(filename);
            if (!quiet) {
              printf("Job %d: extracting ", jobNumber);
              printUTF8(filename.c_str());
              printf("\n");
            }
            if (job.outf.open(filename.c_str()))  // create new file
              job.outf.truncate();
          }
          else
            job.outf.open(filename.c_str());  // update existing file
          if (!job.outf.isopen()) break;  // skip file if error
          else job.lastdt=p;
        }
        assert(job.outf.isopen());
        assert(job.lastdt==p);

        // Find block offset of fragment
        const char* q=out.c_str();
        for (unsigned k=b.start; k<ptr[j]; ++k)
          q+=job.jd.ht[k].usize;
        assert(q>=out.c_str());
        assert(q<=out.c_str()+out.size()-job.jd.ht[ptr[j]].usize);

        // Write the fragment and any consecutive fragments that follow
        assert(offset>=0);
        ++dtr.written;
        int usize=job.jd.ht[ptr[j]].usize;
        while (j+1<ptr.size() && ptr[j+1]==ptr[j]+1
               && ptr[j+1]<b.start+b.size) {
          ++dtr.written;
          assert(dtr.written<=size(ptr));
          usize+=job.jd.ht[ptr[++j]].usize;
        }
        assert(q+usize<=out.c_str()+out.size());
        job.outf.write(q, offset, usize);
        offset+=usize;
        if (dtr.written==size(ptr)) {  // close file
          assert(dtr.dtv.size()>0);
          assert(dtr.dtv.back().date);
          assert(job.outf.isopen());
          assert(job.lastdt!=job.jd.dt.end());
          job.outf.close(dtr.dtv.back().date, dtr.dtv.back().attr);
          job.lastdt=job.jd.dt.end();
        }
      }
    }

    // Last file
    release(job.write_mutex);
  }

  // Last block
  in.close();
  return 0;
}

// Return true if a file or directory (UTF-8 without trailing /) exists
bool exists(string filename) {
  int len=filename.size();
  if (len<1) return false;
  if (filename[len-1]=='/') filename=filename.substr(0, len-1);
#ifdef unix
  struct stat sb;
  return !lstat(filename.c_str(), &sb);
#else
  return GetFileAttributes(utow(filename.c_str()).c_str())
         !=INVALID_FILE_ATTRIBUTES;
#endif
}

// Extract files from archive
void Jidac::extract() {

  // Read HT, DT
  read_archive();
  read_args(false);

  // Map fragments to blocks
  ExtractJob job(*this);
  vector<unsigned> hti(ht.size());  // fragment index -> block index
  for (unsigned i=1; i<ht.size(); ++i) {
    if (ht[i].csize>=0) job.block.push_back(Block(i, ht[i].csize));
    hti[i]=job.block.size()-1;
  }

  // Don't clobber
  for (DTMap::iterator p=dt.begin(); p!=dt.end(); ++p) {
    if (p->second.dtv.size() && p->second.dtv.back().date
        && p->second.written==0) {
      if (!force) {
        if (exists(rename(p->first))) {
          fprintf(stderr, "File exists: ");
          printUTF8(rename(p->first).c_str(), stderr);
          fprintf(stderr, "\n");
          error("won't clobber existing files without -force");
        }
      }

      // Make a list of the number of fragments to extract from each block
      assert(p->second.dtv.size()>0);
      for (unsigned i=0; i<p->second.dtv.back().ptr.size(); ++i) {
        unsigned j=p->second.dtv.back().ptr[i];
        assert(j>0 && j<ht.size());
        assert(ht.size()==hti.size());
        int64_t c=-ht[j].csize;
        if (c<0) c=0;  // position of fragment in block
        j=hti[j];  // block index
        assert(j>=0 && j<job.block.size());
        if (p->second.dtv.back().streaming) job.block[j].streaming=true;
        if (job.block[j].size<=c) job.block[j].size=c+1;
      }
    }
  }

  // Decompress archive in parallel
  if (!quiet) printf("Starting %d decompression jobs\n", threads);
  vector<ThreadID> tid(threads);
  for (int i=0; i<size(tid); ++i) run(tid[i], decompressThread, &job);

  // Decompress streaming files in a single thread
  InputFile in;
  if (!in.open(archive.c_str())) return;
  lock(job.write_mutex);
  libzpaq::Decompresser d;
  libzpaq::SHA1 sha1;
  d.setInput(&in);
  d.setSHA1(&sha1);
  OutputFile out;
  string lastfile=archive;  // default output file: drop .zpaq from archive
  if (lastfile.size()>5 && lastfile.substr(lastfile.size()-5)==".zpaq")
    lastfile=lastfile.substr(0, lastfile.size()-5);
  bool first=true;
  for (unsigned i=0; i<job.block.size(); ++i) {
    if (job.block[i].size==0 || !job.block[i].streaming) continue;
    in.seek(job.block[i].offset, SEEK_SET);
    if (!d.findBlock()) error("findBlock failed");
    StringWriter filename;
      
    // decompress segments
    for (int j=0; d.findFilename(&filename); ++j) {
      d.readComment();

      // Named segment starts new file
      if (filename.s.size()>0 || first) {
        for (unsigned i=0; i<filename.s.size(); ++i)
          if (filename.s[i]=='\\') filename.s[i]='/';
        if (filename.s.size()>0) lastfile=filename.s;
        if (out.isopen()) out.close();
        first=false;
        string newfile;
        DTMap::iterator p=dt.find(lastfile);
        if (p!=dt.end() && p->second.written==0) {  // todo
          newfile=rename(lastfile);
          makepath(newfile);
          if (out.open(newfile.c_str())) {
            if (!quiet) {
              printf("main: extracting ");
              printUTF8(newfile.c_str());
              printf("\n");
            }
            out.truncate(0);
          }
        }
        if (out.isopen()) d.setOutput(&out);
        else d.setOutput(0);
      }
      filename.s="";

      // Decompress, verify checksum
      if (j<job.block[i].size) {
        d.decompress();
        char sha1out[21];
        d.readSegmentEnd(sha1out);
        if (sha1out[0] && memcmp(sha1out+1, sha1.result(), 20))
          fprintf(stderr, "WARNING: %s checksum error\n", lastfile.c_str());
      }
      else
        d.readSegmentEnd();  // skip unused trailing segments
    }
  }
  out.close();
  release(job.write_mutex);

  // Wait for threads to finish
  for (int i=0; i<size(tid); ++i) join(tid[i]);

  // Create empty directories
  for (DTMap::iterator p=dt.begin(); p!=dt.end(); ++p) {
    if (p->second.written==0 && p->first!=""
        && p->first[p->first.size()-1]=='/') {
      string s=rename(p->first);
      makepath(s);
    }
  }
}

/////////////////////////////// list //////////////////////////////////

// For counting files and sizes by -list -summary
struct TOP {
  int64_t size;
  int count;
  TOP(): size(0), count(0) {}
  void inc(int64_t n) {size+=n; ++count;}
};

void Jidac::list_versions(int64_t csize) {
  printf("\n"
  "Version Date    Time (UT) +Files  -Deleted     Original MB   Compressed MB\n"
  "---- ---------- -------- -------- -------- --------------- ---------------\n");
  for (int i=0; i<size(ver); ++i) {
    printf("%4d %s %8d %8d %15.6f %15.6f\n",
      i, dateToString(ver[i].date).c_str(), ver[i].updates,
      ver[i].deletes, ver[i].usize/1000000.0,
      ((i<size(ver)-1 ? ver[i+1].offset : csize)-ver[i].offset)/1000000.0);
  }
}

// List contents
void Jidac::list() {

  // Summary. Show only the largest files and directories, sorted by size,
  // and block and fragment usage statistics.
  if (summary) {
    const int64_t csize=read_archive();
    read_args(false);

    // Report biggest files, directories, and extensions
    printf("\nRank      Size (MB)     Files File, Directory/, or .Type\n"
             "---- -------------- --------- --------------------------\n");
    map<string, TOP> top;  // filename or dir -> total size and count
    vector<int> frag(ht.size());  // frag ID -> reference count
    int unknown_ref=0;  // count fragments and references with unknown size
    int unknown_size=0;
    for (DTMap::const_iterator p=dt.begin(); p!=dt.end(); ++p) {
      if (p->second.dtv.size() && p->second.dtv.back().date
          && p->second.written==0) {
        top[""].inc(p->second.dtv.back().size);
        top[p->first].inc(p->second.dtv.back().size);
        int ext=0;  // location of . in filename
        for (unsigned i=0; i<p->first.size(); ++i) {
          if (p->first[i]=='/') {
            top[p->first.substr(0, i+1)].inc(p->second.dtv.back().size);
            ext=0;
          }
          else if (p->first[i]=='.') ext=i;
        }
        if (ext)
          top[lowercase(p->first.substr(ext))].inc(p->second.dtv.back().size);
        else
          top["."].inc(p->second.dtv.back().size);
        for (unsigned i=0; i<p->second.dtv.back().ptr.size(); ++i) {
          const unsigned j=p->second.dtv.back().ptr[i];
          if (j<frag.size()) {
            ++frag[j];
            if (ht[j].usize<0) ++unknown_ref;
          }
        }
      }
    }
    map<int64_t, vector<string> > st;
    for (map<string, TOP>::const_iterator p=top.begin();
         p!=top.end(); ++p)
      st[-p->second.size].push_back(p->first);
    int i=1;
    for (map<int64_t, vector<string> >::const_iterator p=st.begin();
         p!=st.end() && i<=summary; ++p) {
      for (unsigned j=0; i<=summary && j<p->second.size(); ++i, ++j) {
        printf("%4d %14.6f %9d ", i, (-p->first)/1000000.0,
               top[p->second[j].c_str()].count);
        printUTF8(p->second[j].c_str());
        printf("\n");
      }
    }

    // Report block and fragment usage statistics
    printf("\nShares Fragments Deduplicated MB    Extracted MB\n"
             "------ --------- --------------- ---------------\n");
    map<unsigned, TOP> fr, frc; // refs -> deduplicated, extracted count, size
    for (unsigned i=1; i<frag.size(); ++i) {
      assert(i<ht.size());
      int j=frag[i];
      if (j>10) j=10;
      fr[j].inc(ht[i].usize);
      fr[-1].inc(ht[i].usize);
      frc[j].inc(ht[i].usize*frag[i]);
      frc[-1].inc(ht[i].usize*frag[i]);
      if (ht[i].usize<0) ++unknown_size;
    }
    for (map<unsigned, TOP>::const_iterator p=fr.begin(); p!=fr.end(); ++p) {
      if (int(p->first)==-1) printf(" Total ");
      else if (p->first==10) printf("   10+ ");
      else printf("%6u ", p->first);
      printf("%9d %15.6f %15.6f\n", p->second.count,
        p->second.size/1000000.0, frc[p->first].size/1000000.0);
    }

    // Print versions
    list_versions(csize);

    // Report fragments with unknown size
    printf("\n%d references to %d of %d fragments have unknown size.\n",
           unknown_ref, unknown_size, size(ht)-1);

    // Count blocks and used blocks
    int blocks=0, used=0, isused=0;
    for (unsigned i=1; i<ht.size(); ++i) {
      if (ht[i].csize>=0) {
        ++blocks;
        used+=isused;
        isused=0;
      }
      isused|=frag[i]>0;
    }
    used+=isused;
    const double usize=top[""].size;
    printf("%d of %d blocks used.\nCompression %1.6f -> %1.6f MB",
           used, blocks, usize/1000000.0, csize/1000000.0);
    if (usize>0) printf(" (ratio %1.3f%%)", csize*100.0/usize);
    printf("\n");
    return;
  }

  // Ordinary list
  int64_t csize=read_archive();  // read into ht, dt
  int64_t usize=0;
  int nfiles=0;
  read_args(false, true);
  if (since<0) since+=ver.size();
  for (DTMap::const_iterator p=dt.begin(); p!=dt.end(); ++p) {
    if (p->second.written==0) {
      for (unsigned i=0; i<p->second.dtv.size(); ++i) {
        if (p->second.dtv[i].version>=since && p->second.dtv[i].size>=above) {
          printf("%4d%c", p->second.dtv[i].version,
                 p->second.dtv[i].streaming ? '*' : ' ');
          if (p->second.dtv[i].date) {
            ++nfiles;
            usize+=p->second.dtv[i].size;
            printf("%s %s %12.0f ",
                   dateToString(p->second.dtv[i].date).c_str(),
                   attrToString(p->second.dtv[i].attr).c_str(),
                   double(p->second.dtv[i].size));
          }
          else
            printf("%-40s", "Deleted");
          printUTF8(p->first.c_str());
          printf("\n");
        }
      }
    }
  }
  printf("%u files. %1.0f -> %1.0f\n",
         nfiles, double(usize), double(csize));
  list_versions(csize);
}

int main(int argc, char** argv) {
  clock_t start=clock();  // get start time
  int errorcode=0;
  try {
    Jidac jidac;
    jidac.doCommand(argc, argv);
  }
  catch (std::exception& e) {
    fprintf(stderr, "zpaq exiting from main: %s\n", e.what());
    errorcode=1;
  }
  if (!quiet)
    printf("%1.2f seconds\n", double(clock()-start)/CLOCKS_PER_SEC);
  return errorcode;
}
