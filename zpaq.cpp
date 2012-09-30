/* zpaq.cpp v6.02 - Journaling incremental deduplicating archiving compressor

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

zpaq is for creating compressed archives for incremental
user-level backups of files and directory trees. It is:

Journaling - When you change or delete files, both the old and new
versions are saved in the archive. You can restore the old versions
of your files and directories by reverting to an earlier version of
the archive.

Incremental - Successive backups of the same files or directories
save only the changes, which is faster and takes less space.

Deduplicating - Multiple copies of files are saved only once. When
a file is added, it is divided into fragments of about 64 KB along
boundaries that depend on the local file contents. The SHA-1 hashes
of the fragments are compared to those already in the archive, and
only those with no match are compressed and added. If a file is
edited, only the fragments that have changed need to be added.

zpaq is not intended for administrator level backup of the operating
system or for multiple users. It does not save permissions and
ownership information, and restores using default permissions.
It saves only the last modification date, rounded to seconds.

zpaq compresses using the ZPAQ level 2 self describing format
described in http://mattmahoney.net/zpaq/
This allows new compression algorithms to be introduced in a way
that does not break compatibility with older decompressers.
zpaq also has tools to help compression algorithm developers
create new and better compression methods. It allows methods
to be described in external configuration files and preprocessors,
and includes commands for testing and debugging them.

Usage: zpaq -options [arguments] ...

All options can take a variable number of arguments, which will
be filled in with default values as needed. With no options, the
program displays a help message. Options can be abbreviated like
-e or -ex for -extract, as long as the abbreviation is not ambiguous.
It is recommended that if zpaq is run from a script that abbreviations
not be used because future versions might add options that would make
existing abbreviations ambiguous.

  -list archive files ...

Show contents of archive and compare with the listed files
and directory trees, if any. A listing shows for each file version the
version number, last-modified date, size, filename, comparison result,
and list of fragment identifiers. Files containing identical fragments
will share identifiers. The comparison character has the following
meanings:

  = internal and external file dates are identical.
  # internal and external file dates differ.
  > external file does not exist.
  < internal file does not exist.

If no file or directory arguments are given, then all internal
files are compared. Otherwise, only the listed files and directory
trees are displayed and compared. Directories and their subdirectories
are scanned recursively and all ordinary files are compared.
In Linux, symbolic links are not followed.

If the archive does not exist, then archive.zpaq is tried.

A listing also shows a transaction history. For each transaction,
the version number, date, and the number of files, fragments, uncompressed
bytes, and compressed bytes is shown.

  -to files ...

Rename the external files and directories for the purpose of comparison.
For example, "-list archive dir1 -to dir2" would compare internal
dir1/file to external dir2/file, and so on.

  -not files ...

Exclude the listed files from the argument list (before renaming). For
example, "-list archive dir -not dir/dir2 dir/foo" would exclude dir/foo
and all files in dir/dir2 from comparison.

  -add archive files ...

Update the archive as follows:

  = no action.
  # update in archive.
  > delete from archive.
  < add to archive.

Older versions of updated or deleted files can be recovered using the
-date option to roll back the archive to an earlier state. Options
-to and -not apply. -to really means "from". For example,
"-add archive dir1 -to dir2" means save dir2/file as dir1/file.
-add with no arguments will update or delete all the files in the
archive depending on the comparison result.

  -extract archive files ...

Create files and directories as follows:

  =,# error (file exists) or overwrite with -force option.
  > create file.
  < no action.

Thus, with no arguments, the entire archive is extracted. Options
-to and -not apply as before. -to really means what it says, as in
"-extract archive dir1 dir2 dir3 -to dir4 dir5" would extract
dir1/file to dir4/file, and dir2/file to dir5/file, but not rename any
files in dir3.

  -force

Do not compare files. -list will show contents as if the external
files do not exist. -extract will clobber existing files. -add will
update files even if the dates match. If the files really are identical
then only the index is updated.

  -version N

Use version N of the archive, ignoring any versions of any files that
were added later. Versions are shown by -list. Each -add operation
increments the current version, beginning at 1.

  -method 0..4

Choose compression method for -add, from 0 (fastest) to 4 (best).
The default is "-method 1". Methods are briefly described:

  0 = Not compressed.
  1 = Byte aligned LZ77.
  2 = LZ77 + context model + arithmetic coding.
  3 = BWT (Burrows-Wheeler transform) + order 0-1 indirect model.
  4 = CM (context mixing) dependent on file type.

  -method config arg1 ... arg9

Specify a custom compression algorithm described in ZPAQL in the
file config.cfg with up to 9 numeric arguments. See libzpaq.h
for a description of the ZPAQL language. If the config file specifies
an external preprocessor program, then the program is called once
for each block to be compressed with two additional commmand line
arguments consisting of temporary input and output file names.
zpaq will report an error at compress time if the postprocessor
would not regenerate the original data correctly. Temporary file
names have the form zpaqtmp{PID}_{thread}.{in|out} where {PID}
is the process ID and {thread} is a number 0 or higher. Temporaries
are placed in %TEMP% if defined, or else in $TMPDIR, or else in /tmp,
or else in the current directory.

  -streaming
  -solid
  -tiny

Compress in a format that is readable by
older ZPAQ versions but that does not support journaling, incremental
updates, or deduplication. Dates and attributes are not saved.
Files are added unconditionally, without checking if the files are
already in the archive.

In -streaming format, each file is stored in a separately compressed
block and large files are split into 16 MB blocks. Each block has a
header locator tag and one segment. The segment filename field of
the first block of each file contains the file name and is empty in
subsequent segments. The comment field contains the uncompressed
segment size as a decimal string. The checksum field contains the
SHA-1 hash of the segment.

In -solid format, all of the files are compressed into a single block
with the fields as in -streaming mode. The block can only be compressed
or extracted in a single thread. Compression is usually better.
-method 1, 2, and 3 are not supported. The default is -method 4 unless
a config file is specified. If the config file specifies an external
preprocessor, then it is called once for each input file with output
to a temporary file, and the postprocessor state is maintained between
calls. Compression statistics are displayed showing the fraction of
memory used by each component.

-tiny format is like -solid except that no header locator tags, comments,
or checksums are saved. Archives containing any -tiny blocks can only
be extracted in a single thread even if multiple blocks are present.
Pre/post processing is not verified at compress time, which means that
decompression errors can only be caught by comparing to the original file.

The default format is JIDAC. Files are fragmented along content-dependent
boundaries and those fragments whose SHA-1 hashes do not match those
already in the archive are grouped into blocks up to 16 MB and compressed.
Each update is an append-only operation that is considered a dated
transaction that is undone if interrupted before finished. A transaction
consists of a header block, a set of data blocks containing fragments,
a set of fragment table blocks, and a set of index update blocks. Each
block has a locator tag and a single segment with all fields used.
The comment field contains the size as a decimal string followed by
the 5 byte string " jDC\x01".

The filename fields are in lexicographical order, beginning with "jDC"
followed by the 14 digit transaction date in the form YYYYMMDDHHMMSS,
a letter indicating its type (c,d,h,i), and a 10 digit number N,
for example, "jDC20121231235959d0000000001". The blocks are as follows:

  Type        Contents
  --------     --------
  c header    csize[8]
  d data      frag[fsize[N..N+F-1]][F] fsize[F][4] N[4] F[4]
  h hashes    bsize[4] (sha1[20] fsize[4])[N...]
  i index     (0[8] filename 0 | 
               date[8] filename 0 na[4] attr[na] ni[4] ptr[4][ni])[]

The header block (type c) contains the 8 byte compressed size of the sequence
of data blocks (type d) to follow. All numbers are in little-endian (LSB
first) format. Skipping over these blocks speeds up reading the index.
N is the first fragment ID of the first data block of the transaction.

The data blocks (type d) contain a sequence of fragments. At the
end is a list containing the size of the fragments, followed by the
ID of the first fragment (same as N in the filename) and then the
number of fragments. The first fragment ID in the archive is 1, after
which they are ordered consecutively.

There is one hash table (type h) block for each data block, with the
corresponding N in the filename. It contains the compressed size
of the corresponding block, followed by a list of SHA-1 hashes
and sizes of the corresponding segments. The sizes must be consistent
with those at the end of the data block. Fragments with identical
hashes are assumed to be identical.

There may be any number of index (type i) blocks which contain updates
to the archive index when the transactions are processed in order.
An update is either a new/changed or a deleted file. A deletion is indicated
by a date of 0. Otherwise the filename is followed by the number of
attribute bytes (always 0), then those bytes (reserved for future use),
then the number of fragments, and finally a list of fragment indexes
which point to either the current or earlier transactions.
A file is extracted by concatenating those data fragments. The filename is
UTF-8 encoded with "/" as the path separator, and NUL terminated.
The number N in the filename is arbitrary but different for each index
block.

  -threads N

Specify the number of parallel threads for compression or decompression.
Each thread needs about 100-150 MB memory depending on the compression
method. (Custom methods could be arbitrarily high). The default is
the number of cores. Cores are detected by %NUMBER_OF_PROCESSORS%
in Windows or from /proc/cpuinfo in Linux. N does not include threads
for reading or writing.

  -run hcomp in out
  -run pcomp in out

Run the HCOMP or PCOMP section of config.cfg specified by -method,
taking input from in (default stdin) and writing to out (default stdout).
Specifically, the program is run once for each input byte. For PCOMP,
the program is run once more with input -1 to simulate post-processing.
Built-in methods 1 through 4 are not supported.
This command is useful for debugging config files, as is -trace.

  -trace hcomp args...
  -trace pcomp args...

Single step config.cfg HCOMP or PCOMP with numeric args as input. args may
be decimal or hexadecimal (like 255 or xff), and the output will be
displayed in the same format. The output shows the ZPAQL instruction
and registers after each step, and a memory dump (omitting zeros) at
the end, once per argument.

  -detailed

With -list, give a techinal listing that includes the decompiled
ZPAQL block headers, segment header contents, and JIDAC fragment
tables and indexes. With -add and -extract, show detailed steps.

  -quiet

With -add and -extract, display only errors and warnings.


TO COMPILE:

This program needs libzpaq from http://mattmahoney.net/zpaq/ and
libdivsufsort-lite from above or http://code.google.com/p/libdivsufsort/
Recommended compile for Windows with MinGW:

  g++ -O3 -s -msse2 zpaq.cpp libzpaq.cpp divsufsort.c -o zpaq

With Visual C++:

  cl /O2 /EHsc /arch:SSE2 /DNDEBUG zpaq.cpp libzpaq.cpp divsufsort.c

For Linux:

  g++ -O3 -s -msse2 -Dunix -fopenmp -DNDEBUG \
  zpaq.cpp libzpaq.cpp divsufsort.c -o zpaq

Possible options:

  -o         Name of output executable.
  -O3 or /O2 Optimize (faster).
  /EHsc      Enable exception handing in VC++ (required).
  -s         Strip debugging symbols. Smaller executable.
  -msse2 /arch:SSE2  Assume x86 processor with SSE2. Otherwise use -DNOJIT.
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
    cv=PTHREAD_COND_INITIALIZER;
    mutex=PTHREAD_MUTEX_INITIALIZER;
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
void run(ThreadID& tid, ThreadReturn(*f)(void*), void* arg)
  {tid=CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)f, arg, 0, NULL);}
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

#ifdef _MSC_VER
#define fseeko64(a,b,c) _fseeki64(a,b,c)
#define ftello64(a) _ftelli64(a)
#endif

// For testing -Dunix in Windows
#ifdef unixtest
#define lstat(a,b) stat(a,b)
#define mkdir(a,b) mkdir(a)
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

// In Windows, convert UTF-8 string to wide string and / to \ ignoring
// invalid UTF-8 or >64K
std::wstring utow(const char* ss) {
  assert(sizeof(wchar_t)==2);
  assert((wchar_t)(-1)==65535);
  std::wstring r;
  if (!ss) return r;
  const unsigned char* s=(const unsigned char*)ss;
  for (; s && *s; ++s) {
    if (s[0]=='/') r+='\\';
    else if (s[0]<128) r+=s[0];
    else if (s[0]>=192 && s[0]<224 && s[1]>=128 && s[1]<192)
      r+=(s[0]-192)*64+s[1]-128, ++s;
    else if (s[0]>=224 && s[0]<240 && s[1]>=128 && s[1]<192
             && s[2]>=128 && s[2]<192)
      r+=(s[0]-224)*4096+(s[1]-128)*64+s[2]-128, s+=2;
    else
      printf("unicode error s=%d %d %d %d\n", s[0], s[1], s[2], s[3]);
  }
  return r;
}
#endif

// Convert 64 bit decimal YYYYMMDDHHMMSS to "YYYY-MM-DD HH:MM:SS"
// where -1 = unknown date, 0 = deleted.
string datetostring(int64_t date) {
  if (date<0)       return "unknown date       ";
  else if (date==0) return "                   ";
  string s="0000-00-00 00:00:00";
  static const int t[]={18,17,15,14,12,11,9,8,6,5,3,2,1,0};
  for (int i=0; i<14; ++i) s[t[i]]+=int(date%10), date/=10;
  return s;
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

// File types accepting UTF-8 filenames
#ifdef unix

class InputFile: public libzpaq::Reader {
  FILE* in;
public:
  InputFile(): in(0) {}

  // Open file for reading. Return true if successful
  bool open(const char* filename) {
    in=fopen(filename, "rb");
    if (!in) perror(filename);
    return in!=0;
  }

  // True if open
  bool isopen() {return in!=0;}

  // Read and return 1 byte (0..255) or EOF
  int get() {assert(in); return getc(in);}

  // Return file position
  int64_t tell() {return ftello64(in);}

  // Set file position
  void seek(int64_t pos, int whence) {
    fseeko64(in, pos, whence);
  }

  // Close file if open
  void close() {if (in) fclose(in), in=0;}
  ~InputFile() {close();}
};

#else  // Windows

class InputFile: public libzpaq::Reader {
  enum {BUFSIZE=4096};      // input buffer max size
  HANDLE in;                // input file handle
  libzpaq::Array<char> buf; // input buffer
  DWORD n;                  // buffer size
  DWORD ptr;                // number of bytes read from buffer
public:
  InputFile():
    in(INVALID_HANDLE_VALUE), buf(BUFSIZE), n(0), ptr(0) {}

  // Open for reading. Return true if successful
  bool open(const char* filename) {
    assert(in==INVALID_HANDLE_VALUE);
    n=ptr=0;
    std::wstring w=utow(filename);
    in=CreateFile(w.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL,
                  OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (in==INVALID_HANDLE_VALUE) {  // print error message
      int err=GetLastError();
      if (err==ERROR_FILE_NOT_FOUND)
        fwprintf(stderr, L"%s: file not found\n", w.c_str());
      else if (err==ERROR_PATH_NOT_FOUND)
        fwprintf(stderr, L"%s: path not found\n", w.c_str());
      else if (err==ERROR_ACCESS_DENIED)
        fwprintf(stderr, L"%s: access denied\n", w.c_str());
      else if (err==ERROR_SHARING_VIOLATION)
        fwprintf(stderr, L"%s: in use by another process\n", w.c_str());
      else
        fwprintf(stderr, L"%s: Windows error %d\n", w.c_str(), err);
    }
    return in!=INVALID_HANDLE_VALUE;
  }

  bool isopen() {return in!=INVALID_HANDLE_VALUE;}

  // Read 1 byte
  int get() {
    if (ptr>=n) {
      assert(ptr==n);
      ptr=0;
      ReadFile(in, &buf[0], BUFSIZE, &n, NULL);
      if (n==0) return EOF;
    }
    assert(ptr<n);
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

#endif

// Output file accepting UTF-8 filenames
#ifdef unix

class OutputFile: public libzpaq::Writer {
  FILE* out;
  string filename;
public:
  OutputFile(): out(0) {}

  // Return true if file is open
  bool isopen() {return out!=0;}

  // Open for append/update or create if needed
  bool open(const char* filename) {
    this->filename=filename;
    out=fopen(filename, "rb+");
    if (out) fseeko64(out, 0, SEEK_END);
    else out=fopen(filename, "wb+");
    if (!out) perror(filename);
    return isopen();
  }

  // Write 1 byte
  void put(int c) {assert(out); putc(c, out);}

  // Write size bytes at offset
  void write(const char* buf, int64_t offset, int size) {
    fseeko64(out, offset, SEEK_SET);
    int n=fwrite(buf, 1, size, out);
    if (n!=size) perror("fwrite");
  }

  // Seek to pos. whence is SEEK_SET, SEEK_CUR, or SEEK_END
  void seek(int64_t pos, int whence) {
    fflush(out);
    fseeko64(out, pos, whence);
  }

  // return position
  int64_t tell() {
    return ftello64(out);
  }

  // Truncate file and move file pointer to end
  void truncate(int64_t newsize=0) {
    seek(newsize, SEEK_CUR);
    (void)ftruncate(fileno(out), newsize);
  }

  // Close file and set date if not 0
  void close(int64_t date=0) {
    if (out) fclose(out);
    out=0;
    if (date>0) {
      struct utimbuf ub;
      ub.actime=time(NULL);
      ub.modtime=unix_time(date);
      utime(filename.c_str(), &ub);
    }
  }

  ~OutputFile() {close();}
};

#else  // Windows

class OutputFile: public libzpaq::Writer {
  enum {BUFSIZE=4096};      // output buffer max size
  HANDLE out;               // output file handle
  libzpaq::Array<char> buf; // output buffer
  DWORD ptr;                // number of pending output bytes
public:
  OutputFile(): out(INVALID_HANDLE_VALUE), buf(BUFSIZE), ptr(0) {}

  // Return true if file is open
  bool isopen() {
    return out!=INVALID_HANDLE_VALUE;
  }

  // Open file ready to update or append, create if needed
  bool open(const char* filename) {
    assert(!isopen());
    ptr=0;
    std::wstring w=utow(filename);
    out=CreateFile(w.c_str(), GENERIC_WRITE, 0, NULL, OPEN_ALWAYS,
                   FILE_ATTRIBUTE_NORMAL, NULL);
    if (out==INVALID_HANDLE_VALUE) {  // print error message
      int err=GetLastError();
      if (err==ERROR_FILE_NOT_FOUND)
        fwprintf(stderr, L"%s: file not found\n", w.c_str());
      else if (err==ERROR_PATH_NOT_FOUND)
        fwprintf(stderr, L"%s: path not found\n", w.c_str());
      else if (err==ERROR_ACCESS_DENIED)
        fwprintf(stderr, L"%s: access denied\n", w.c_str());
      else if (err==ERROR_SHARING_VIOLATION)
        fwprintf(stderr, L"%s: in use by another process\n", w.c_str());
      else
        fwprintf(stderr, L"%s: Windows error %d\n", w.c_str(), err);
    }
    else
      SetFilePointer(out, 0, NULL, FILE_END);
    return isopen();
  }

  // Write pending output
  void flush() {
    assert(isopen());
    if (ptr) {
      DWORD n=0;
      WriteFile(out, &buf[0], ptr, &n, NULL);
      if (ptr!=n)
        fprintf(stderr, "write failed: wrote %d of %d bytes\n",
                int(n), int(ptr));
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

  // Write size bytes at offset
  void write(const char* bufp, int64_t offset, int size) {
    assert(isopen());
    flush();
    DWORD n=0;
    if (offset!=tell()) {
      LONG offhigh=offset>>32;
      SetFilePointer(out, offset, &offhigh, FILE_BEGIN);
    }
    WriteFile(out, bufp, size, &n, NULL);
    if (size!=int(n))
      fprintf(stderr, "write failed: wrote %d of %d bytes\n", int(n), size);
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

  // Close file and set date if not 0
  void close(int64_t date=0) {
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
    }
  }
  ~OutputFile() {close();}
};

#endif

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

// Handle errors
void libzpaq::error(const char* msg) {
  fprintf(stderr, "zpaq error: %s\n", msg);
  throw std::runtime_error(msg);
}
using libzpaq::error;

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

bool detailed=false;  // global: set by -detailed, -quiet
bool quiet=false;

/////////////////////////////// Jidac /////////////////////////////////

// A Jidac object represents an archive contents: a list of file
// fragments with hash, size, and archive offset, and a list of
// files with date, attributes, and list of fragment pointers.
// Methods add to, extract from, compare, and list the archive.

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
  vector<unsigned> ptr;  // list of fragment indexes to HT
  int version;           // which transaction was it added?
  bool streaming;        // true if any fragment sizes are unknown or too big
  DTV(): date(0), size(0), version(0), streaming(false) {}
};

// filename entry
struct DT {
  int64_t edate;         // date of external file, 0=not found
  vector<DTV> dtv;       // list of versions
  int written;           // 0..ptr.size() = fragments output. -1=ignore
  DT(): edate(0), written(-1) {}
};

class CompressJob;

// Do everything
class Jidac {
public:
  void doCommand(int argc, char** argv);
  friend ThreadReturn decompressThread(void* arg);
  friend struct ExtractJob;
  enum {BAD, SOLID, STREAMING, TINY, JIDAC} mode; // default archive is JIDAC
private:

  // Command line arguments
  string command;           // "-add", "-extract", "-list"
  enum {blocksize=1<<24};   // maximum block size
  string archive;           // archive name
  vector<string> files;     // list of files and directories to add
  vector<string> notfiles;  // list of prefixes to exclude
  vector<string> tofiles;   // files renamed with -to
  int64_t date;             // now as decimal YYYYMMDDHHMMSS (UT)
  bool force;               // true if -force selected
  int version;              // Set by -ersion
  int threads;              // default is number of cores + 1
  string method;            // "1".."4" or ZPAQL source code
  int args[9];              // arguments to method

  // Archive state
  vector<HT> ht;            // list of fragments
  map<string, DT> dt;       // set of files

  // Commands
  void add();
  void extract();
  void list();
  void runtrace(int argc, char** argv);  // run|trace hcomp|pcomp args...
  void usage();

  // Support functions
  string rename(const string& name);    // replace files prefix with tofiles
  string unrename(const string& name);  // undo rename
  int64_t read_archive();  // read index block chain into ht, dt
  void read_args(bool force);  // read command line args, scan directories
  void scandir(const char* filename);   // scan dirs and add args to dt
  void addfile(const char* filename, int64_t date);  // add to dt
  void write_fragments(CompressJob& job, StringBuffer& b,
                       unsigned& block_start);  // compress fragments in b
};

void Jidac::usage() {
  printf(
  "zpaq 6.02 - Journaling incremental deduplicating archiving compressor\n"
  "(C) " __DATE__ ", Dell Inc. This is free software under GPL v3.\n"
  "\n"
  "Usage: zpaq -options ...\n"
  "-list archive files ...         Show =equal #different <external >internal\n"
  "  -detailed                     Detailed listing of archive.zpaq\n"
  "-add archive files ...          Append changes but keep old versions\n"
  "  -method 0...4                 Compress faster...better (default 1)\n"
  "  -method config arg1 ... arg9  or use ZPAQL program config.cfg\n"
  "  -streaming -solid -tiny       Append unconditionally in old formats\n"
  "-extract archive files ...      Decompress >new files and directories\n"
  "-quiet                          Display only errors and warnings\n"
  "-force                          Overwrite even if identical\n"
  "-to files ...                   Rename external files and directories\n"
  "-not files ...                  Exclude\n"
  "-version N                      Roll back archive (default: latest)\n"
  "-threads %-4d                   Default shown is cores detected\n"
  "-run hcomp|pcomp input output   Run config.cfg (default: stdin, stdout)\n"
  "-trace hcomp|pcomp args ...     Single step with decimal/hex inputs\n",
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

// Expand an abbreviated option, or report error if not exactly 1 match
string expandOption(const char* opt) {
  const char* opts[]={"-list","-detailed",
    "-add","-method","-streaming","-solid","-tiny",
    "-extract","-force","-quiet",
    "-to","-not","-version","-threads","-run","-trace",
    "hcomp","pcomp",0};
  const int n=strlen(opt);
  string result;
  for (unsigned i=0; opts[i]; ++i) {
    if (!strncmp(opt, opts[i], n)) {
      if (result!="")
        fprintf(stderr, "Ambiguous: %s\n", opt), exit(1);
      result=opts[i];
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
  detailed=force=false;
  date=0;  // only needed by -add in JIDAC mode
  version=0x7fffffff;
  threads=numberOfProcessors();
  mode=JIDAC;
  method="1";  // 1..4 or config.cfg -> ZPAQL
  for (int i=0; i<9; ++i) args[i]=0;

  // Get optional options
  for (int i=1; i<argc; ++i) {
    const string opt=expandOption(argv[i]);
    if ((opt=="-add" || opt=="-extract" || opt=="-list")
        && i<argc-1 && command=="") {
      command=opt;
      archive=argv[++i];
      while (++i<argc && argv[i][0]!='-')
        files.push_back(atou(argv[i]));
      --i;
    }
    else if (opt=="-detailed") detailed=true;
    else if (opt=="-quiet") quiet=true;
    else if (opt=="-force") force=true;
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
      if (method!="0" && method!="1" && method!="2"
          && method!="3" && method!="4") {
        method+=".cfg";
        FILE* in=fopen(method.c_str(), "r");
        if (!in) {
          perror(method.c_str());
          exit(1);
        }
        int c;
        method="";
        while ((c=getc(in))!=EOF) method+=char(c);
        fclose(in);
        ++i;
        for (int j=0;  // read up to 9 args starting with 0..9 or -0..-9
             j<9 && i<argc && (isdigit(argv[i][0]) 
                 || (argv[i][0]=='-' && isdigit(argv[i][1]))); ++i, ++j)
          args[j]=atoi(argv[i]);
        --i;
      }
    }
    else if (opt=="-streaming") mode=STREAMING;
    else if (opt=="-tiny") mode=TINY;
    else if (opt=="-solid") mode=SOLID;
    else if (i<argc-1 && (opt=="-run" || opt=="-trace")) {
      string arg1=expandOption(argv[i+1]);
      if (arg1=="hcomp" || arg1=="pcomp") {
        command=opt;
        runtrace(argc-i, argv+i);
        return;
      }
    }
    else usage();
  }

  // Add .zpaq extension to archive
  if (size(archive)<5 || archive.substr(archive.size()-5)!=".zpaq")
    archive+=".zpaq";

  // Execute command
  if (command=="-add") add();
  else if (command=="-list") list();
  else if (command=="-extract") extract();
  else usage();
}

// Print list of numbers. Shorten like "1 2 3 4 5" to "1-5".
void printList(const vector<unsigned>& ptr) {
  bool hyphen=false;
  for (int i=0; i<size(ptr); ++i) {
    if (i==0 || i==size(ptr)-1 || ptr[i]!=ptr[i-1]+1 || ptr[i]!=ptr[i+1]-1) {
      if (!hyphen) printf(" ");
      hyphen=false;
      printf("%d", ptr[i]);
    }
    else {
      if (!hyphen) printf("-");
      hyphen=true;
    }
  }
  printf("\n");
}

// Read archive up to -date into ht, dt. Return place to append.
int64_t Jidac::read_archive() {
  ht.resize(1);  // element 0 not used

  // Open archive or archive.zpaq
  InputFile in;
  if (!in.open(archive.c_str())) {
    if (command=="-add" && !quiet)
      printf("Creating new archive %s\n", archive.c_str());
    return 0;
  }
  if (!quiet) printf("Reading archive %s\n", archive.c_str());

  // Scan archive contents
  libzpaq::Decompresser d;
  d.setInput(&in);
  string lastfile=archive; // last named file in streaming format
  if (size(lastfile)>5)
    lastfile=lastfile.substr(0, size(lastfile)-5); // drop .zpaq
  bool isbad=true;         // no ZPAQ blocks found?
  int64_t block_offset=0;  // start of last block of any type
  int64_t data_offset=0;   // start of last block of fragments
  double memory;           // findBlock() output
  int fversion=0;          // which transaction?

  // Detect archive format and read the filenames, fragment sizes,
  // and hashes. In JIDAC format, these are in the index blocks, allowing
  // data to be skipped. Otherwise the whole archive is scanned to get
  // this information from the segment headers and trailers.
  while (d.findBlock(&memory)) {
    try {
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
            if (++fversion>version)  // roll back archive to here
              isbreak=true;
            else if (os.size()==8) {  // jump
              const char* s=os.c_str();
              int64_t jmp=btol(s);
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
              if (!quiet)
                printf("Archive truncated to %1.0f\n", double(block_offset));
              in.close();
              return block_offset;
            }
            if (!quiet)
              printf("%4d %s\n", fversion, datetostring(fdate).c_str());
          }

          // Fragment table. Filename is jDCYYYYMMDDHHMMSSdNNNNNNNNNN
          // Contents is csize[4] (sha1[20] usize[4])... for fragment N...
          // where csize is the compressed block size.
          // Store in ht[].{sha1,usize}. Set ht[].csize to block offset
          // assuming N in ascending order.
          else if (filename.s[17]=='h' && num>0) {
            const char* s=os.c_str();
            const unsigned bsize=btoi(s);
            const unsigned n=(os.size()-4)/24;
            if (ht.size()!=num)
              fprintf(stderr,
                "Unordered fragment tables: expected %d found %1.0f\n",
                size(ht), double(num));
            for (unsigned i=0; i<n; ++i) {
              while (ht.size()<=num+i) ht.push_back(HT());
              memcpy(ht[num+i].sha1, s, 20);
              s+=20;
              ht[num+i].usize=btoi(s);
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
              dtv.version=fversion;
              dtv.date=btol(s);
              s+=strlen(fp)+1;  // skip filename
              if (dtv.date && s<=end-8) {
                const unsigned na=btoi(s);
                s+=na;  // skip attr
                if (s<=end-4) {
                  const unsigned ni=btoi(s);
                  dtv.ptr.resize(ni);
                  for (unsigned i=0; i<ni && s<=end-4; ++i) {
                    dtv.ptr[i]=btoi(s);
                    if (dtv.ptr[i]>0 && dtv.ptr[i]<ht.size() && dtv.size>=0)
                      dtv.size+=ht[dtv.ptr[i]].usize;
                    else
                      dtv.size=-1;
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
          if (filename.s.size()>0) {
            if (!fversion) fversion=1;
            if (size(dtr.dtv)>0 && dtr.dtv.back().version>=fversion) {
              fversion=dtr.dtv.back().version+1;
              if (fversion>version) {
                if (!quiet)
                  printf("Archive truncated to %1.0f\n",double(block_offset));
                in.close();
                return block_offset;
              }
            }
            dtr.dtv.push_back(DTV());
            dtr.dtv.back().date=fdate;
            dtr.dtv.back().version=fversion;
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
      }
      block_offset=in.tell();
      isbad=false;
    }
    catch (std::exception& e) {
      fprintf(stderr, "Skipping block: %s\n", e.what());
    }
  }
  in.close();
  if (isbad) error("archive is empty");
  return block_offset;
}

// Mark each file in dt according to args in -add, -extract, -list, -not
// using written=0 for each match, or all if no args.
// If force is false then scan external directories and add to dt.
void Jidac::read_args(bool force) {

  // Match to files[] except notfiles[] or match all if files[] is empty
  for (map<string, DT>::iterator p=dt.begin(); p!=dt.end(); ++p) {
    if (size(files)==0) {
      if (p->second.dtv.size() && p->second.dtv.back().date) {
        if (force) p->second.written=0;
        else scandir(rename(p->first).c_str());
      }
    }
    else {
      bool matched=false;
      for (int i=0; !matched && i<size(files); ++i)
        if (ispath(files[i].c_str(), p->first.c_str()))
          matched=true;
      for (int i=0; matched && i<size(notfiles); ++i)
        if (ispath(notfiles[i].c_str(), p->first.c_str()))
          matched=false;
      if (matched && p->second.dtv.size() && p->second.dtv.back().date)
        p->second.written=0;
    }
  }

  // Scan external files and directories
  if (!force)
    for (int i=0; i<size(files); ++i)
      scandir(rename(files[i]).c_str());
  if (!quiet) printf("\n");
}

// Insert filename into dt unless in notfiles. recurse=insert subdirectories.
void Jidac::scandir(const char* filename) {

  // Omit if in notfiles
  for (int i=0; i<size(notfiles); ++i)
    if (ispath(notfiles[i].c_str(), unrename(filename).c_str())) return;

#ifdef unix

  // Add regular file
  struct stat sb;
  if (!lstat(filename, &sb)) {
    if (S_ISREG(sb.st_mode))
      addfile(filename, decimal_time(sb.st_mtime));

    // Traverse directory
    else if (S_ISDIR(sb.st_mode)) {
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
  else
    addfile(filename, 0);

#else  // Windows

  // Add regular file
  WIN32_FILE_ATTRIBUTE_DATA sb;
  std::wstring w=utow(filename);
  if (GetFileAttributesEx(w.c_str(), GetFileExInfoStandard, &sb)) {
    if (!(sb.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
      DT d;
      SYSTEMTIME st;
      if (FileTimeToSystemTime(&sb.ftLastWriteTime, &st)) {
        addfile(filename,
                st.wYear*10000000000LL+st.wMonth*100000000LL+st.wDay*1000000
                +st.wHour*10000+st.wMinute*100+st.wSecond);
      }
    }

    // Traverse directory
    else {
      string s=filename;
      int len=s.size()-1;  // path length
      if (len>=0 && s[len]!='/' && s[len]!='\\') s+="/", ++len;
      s+="*";
      WIN32_FIND_DATA ffd;
      HANDLE h=FindFirstFile(utow(s.c_str()).c_str(), &ffd);
      while (h!=INVALID_HANDLE_VALUE) {
        string t=wtou(ffd.cFileName);
        if (len>=0 && t!="." && t!="..")
          scandir((s.substr(0, len+1)+t).c_str());
        if (!FindNextFile(h, &ffd)) break;
      }
      FindClose(h);
    }
  }
  else
    addfile(filename, 0);
#endif
}

// Add external file and its date to dt
void Jidac::addfile(const char* filename, int64_t edate) {
  DT& d=dt[unrename(filename)];
  d.edate=edate;
  d.written=0;
}

/////////////////////////////// add ///////////////////////////////////

// Construct a temporary file name
string tempname(int id) {

  // Get temporary directory
  string result;
  const char* env=getenv("TMPDIR");
  if (!env) env=getenv("TEMP");
  if (!env) env="/tmp";
  result=env;
  const int n=result.size();
  if (n<1 || (result[n-1]!='/' && result[n-1]!='\\'))
#ifdef unix
     result+='/';
#else
     result+='\\';
#endif

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

// BWT Preprocessor
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
  int idx=divbwt(&buf[0], &buf[0], &w[0], n);
  assert(idx>=0 && idx<=int(n));
  memmove(&buf[idx+1], &buf[idx], n-idx);
  buf[idx]=255;
  for (int i=0; i<4; ++i) buf[n+1+i]=idx>>(i*8);
  n+=5;
}

// LZ preprocessor for level 1 or 2 compression. Leve 1 output format
// is byte oriented LZ77. Lengths and offsets are MSB first:
//
// 00xxxxxx                            x+1 (1..64) literals follow
// 01xxxyyy yyyyyyyy                   copy x+4 (4..11), offset y+1 (1..2048)
// 10xxxxxx yyyyyyyy yyyyyyyy          copy x+1 (1..64), offset y+1 (1..65536)
// 11xxxxxx yyyyyyyy yyyyyyyy yyyyyyyy copy x+1 (1..64), offset y+1 (1..2^24)
//
// Level 2 differs in that 2 byte code lengths are x+5 (5..12) instead of x+4.

class LZBuffer: public libzpaq::Reader {
  const unsigned char* in;    // input
  const unsigned n;           // input length
  StringBuffer buf;           // compressed output
  int level;                  // compression level 1 or 2
  void write_literal(unsigned i, unsigned& lit);
  void write_match(unsigned len, unsigned off);
public:
  LZBuffer(StringBuffer& inbuf, int level);  // input to compress
  int get() {return buf.get();}   // return 1 byte of compressed output
  size_t size() const {return buf.size();}  
};

// Write literal sequence buf[i-lit..i-1], set lit=0
void LZBuffer::write_literal(unsigned i, unsigned& lit) {
  while (lit>0) {
    unsigned lit1=lit;
    if (lit1>64) lit1=64;
    buf.put(lit1-1);
    for (unsigned j=i-lit; j<i-lit+lit1; ++j) buf.put(in[j]);
    lit-=lit1;
  }
}

// Write match sequence of given length and offset (off in 1..2^24)
void LZBuffer::write_match(unsigned len, unsigned off) {
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
    buf(inbuf.size()*9/8), level(level_) {
  assert(level==1 || level==2);

  // Tunable LZ77 parameters for levels 1 and 2
  const int HASHES=2;  // number of hashes computed per byte
  const unsigned HASHORDER[2][HASHES]={{7,4},{10,5}};  // context orders
  const unsigned HASHMUL[2][HASHES]={{88,96},{44,48}}; // hash multipliers

  // Allocate hashtable ht
  // ht[h] low 24 bits points to in[i..i+HASHORDER-1], high 8 bits is in[i]
  libzpaq::Array<unsigned> ht(1<<22);
  int h[HASHES]={0};  // context hashes of in[i..]

  // Scan the input
  unsigned lit=0;  // number of output literals pending
  for (unsigned i=0; i<n;) {

    // Search for longest match, or pick closest in case of tie
    // Try the longest context orders first. If a match is found, then
    // skip the lower orders as a speed optimization.
    unsigned blen=0, bp=0, len=0;
    for (int j=0; j<HASHES; ++j) {
      for (int k=0; k<4; ++k) {
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
      if (blen>=HASHORDER[level-1][j]) break;
    }

    // If match is long enough, then output any pending literals first,
    // and then the match. blen is the length of the match.
    assert(i>=bp);
    const unsigned off=i-bp;  // offset
    if (off>0 && off<(1<<24)
        && blen>=1u+level*2+(off>=2048)+(off>=65536)+(level==1 && lit>0)) {
      write_literal(i, lit);
      write_match(blen, off);
    }

    // Otherwise add to literal length
    else {
      blen=1;
      ++lit;
    }

    // Update index, advance blen bytes
    while (blen--) {
      for (int j=0; j<HASHES; ++j) {
        ht[h[j]+(i&3)]=(i&0xffffff)|(in[i]<<24);
      }
      ++i;
      for (int j=0; j<HASHES; ++j) {
        if (i+HASHORDER[level-1][j]<=n) {
          h[j]>>=2;
          h[j]*=HASHMUL[level-1][j];
          h[j]+=in[i+HASHORDER[level-1][j]-1]+1;
          h[j]<<=2;
          h[j]&=ht.size()-1;
        }
      }
    }
  }

  // Write pending literals at end of block
  write_literal(n, lit);
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
  if (detailed)
    printf("Job %d compressing %d bytes to %s %s\n", jobNumber,
           size(*in), filename?filename:"", comment?comment:"");

  // Built in ZPAQL code for -method 0..4
  const char* cfg[5]={
    "(method 0 - store uncompressed) "
    "comp 0 0 0 0 0 hcomp post 0 end ",

    "(method 1 - LZ77 byte coded) "
    "comp 0 0 0 24 0 "
    "hcomp halt "
    "pcomp lzpre c ; (code below is equivalent to \"lzpre d\") "
    "  (LZ77 decoder: b=i, c=c d=state r1=len r2=off "
    "    state = d = 0 = expect literal or match code "
    "                1 = decoding a literal with len bytes left "
    "                2 = expecting last offset byte of a match "
    "                3,4 = expecting 2,3 match offset bytes "
    "    i = b = position in 16M output buffer "
    "    c = c = input byte "
    "    len = r1 = length of match or literal "
    "    off = r2 = offset of match back from i "
    "  Input format: "
    "    00llllll: literal of length lllllll=1..64 to follow "
    "    01lllooo oooooooo: length lll=4..11, offset o=1..2048 "
    "    10llllll oooooooo oooooooo: l=1..64 offset=1..65536 "
    "    11llllll oooooooo oooooooo oooooooo: 1..64, 1..2^24) "
    "  c=a a=d a== 0 if "
    "    a=c a>>= 6 a++ d=a "
    "    a== 1 if (state?) "
    "      a+=c r=a 1 a=0 r=a 2 (literal len=c+1 off=0) "
    "    else "
    "      a== 2 if a=c a&= 7 r=a 2 (short match: off=c&7) "
    "        a=c a>>= 3 a-= 4 r=a 1 (len=(c>>3)-4) "
    "      else (3 or 4 byte match) "
    "        a=c a&= 63 a++ r=a 1 a=0 r=a 2 (off=0, len=(c&63)-1) "
    "      endif "
    "    endif "
    "  else "
    "    a== 1 if (writing literal) "
    "      a=c *b=a b++ out "
    "      a=r 1 a-- a== 0 if d=0 endif r=a 1 (if (--len==0) state=0) "
    "    else "
    "      a> 2 if (reading offset) "
    "        a=r 2 a<<= 8 a|=c r=a 2 d-- (off=off<<8|c, --state) "
    "      else (state==2, write match) "
    "        a=r 2 a<<= 8 a|=c c=a a=b a-=c a-- c=a (c=i-off-1) "
    "        d=r 1 (d=len) "
    "        do (copy and output d=len bytes) "
    "          a=*c *b=a out c++ b++ "
    "        d-- a=d a> 0 while "
    "        (d=state=0. off, len don\'t matter) "
    "      endif "
    "    endif "
    "  endif "
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
    "pcomp lzpre c ; (like level 1 except 2 byte codes are 5..12, not 4..11) "
    "  c=a a=d a== 0 if "
    "    a=c a>>= 6 a++ d=a "
    "    a== 1 if (state?) "
    "      a+=c r=a 1 a=0 r=a 2 (literal len=c+1 off=0) "
    "    else "
    "      a== 2 if a=c a&= 7 r=a 2 (short match: off=c&7) "
    "        a=c a>>= 3 a-= 3 r=a 1 (len=(c>>3)-3) "
    "      else (3 or 4 byte match) "
    "        a=c a&= 63 a++ r=a 1 a=0 r=a 2 (off=0, len=(c&63)-1) "
    "      endif "
    "    endif "
    "  else "
    "    a== 1 if (writing literal) "
    "      a=c *b=a b++ out "
    "      a=r 1 a-- a== 0 if d=0 endif r=a 1 (if (--len==0) state=0) "
    "    else "
    "      a> 2 if (reading offset) "
    "        a=r 2 a<<= 8 a|=c r=a 2 d-- (off=off<<8|c, --state) "
    "      else (state==2, write match) "
    "        a=r 2 a<<= 8 a|=c c=a a=b a-=c a-- c=a (c=i-off-1) "
    "        d=r 1 (d=len) "
    "        do (copy and output d=len bytes) "
    "          a=*c *b=a out c++ b++ "
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
    "  else "
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
    "    (traverse list and output) "
    "    d=r 1 do "
    "      a=d a== 0 ifnot "
    "        a=*d a>>= 8 d=a "
    "        a=*d out "
    "      forever "
    "    endif "
    " "
    "  endif "
    "  halt "
    "end "
    " ",

    "(method 4 mid.cfg) "
    "comp 3 3 0 0 8 (hh hm ph pm n) "
    "  0 icm 5        (order 0...5 chain) "
    "  1 isse 13 0 "
    "  2 isse $1+17 1 "
    "  3 isse $1+18 2 "
    "  4 isse $1+18 3 "
    "  5 isse $1+19 4 "
    "  6 match $1+22 $1+24  (order 7) "
    "  7 mix 16 0 7 24 255  (order 1) "
    "hcomp "
    "  c++ *c=a b=c a=0 (save in rotating buffer M) "
    "  d= 1 hash *d=a   (orders 1...5 for isse) "
    "  b-- d++ hash *d=a "
    "  b-- d++ hash *d=a "
    "  b-- d++ hash *d=a "
    "  b-- d++ hash *d=a "
    "  b-- d++ hash b-- hash *d=a (order 7 for match) "
    "  d++ a=*c a<<= 8 *d=a       (order 1 for mix) "
    "  halt "
    "post "
    "  0 "
    "end "};

  int level=-1;
  if (*method>='0' && *method<='4' && method[1]==0) {
    level=*method-'0';
    method=cfg[level];
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
  else if (pcomp_cmd.size()) {  // input from preprocessor temp output
    assert(level==-1);

    // Write in to temporary file tmp_in
    string tmp_in=tempname(jobNumber);
    string tmp_out=tmp_in+".out";
    tmp_in+=".in";
    FILE* tmp=fopen(tmp_in.c_str(), "wb");
    if (!tmp) {
      perror(tmp_in.c_str());
      error("Cannot create preprocessor temporary file");
    }
    int c;
    while ((c=in->get())!=EOF)
      putc(c, tmp);
    fclose(tmp);

    // Run the external preprocessor passing tmp_in, tmp_out as arguments
    string cmd=pcomp_cmd.c_str();
    cmd+=" "+tmp_in+" "+tmp_out;
    if (!quiet) printf("%s\n", cmd.c_str());
    (void)system(cmd.c_str());
    remove(tmp_in.c_str());

    // Open preprocessor output to compress
    InputFile tmpIn;
    if (!tmpIn.open(tmp_out.c_str())) error("Preprocessor failed");
    assert(tmp_out!="");
    co.setInput(&tmpIn);
    co.setVerify(true);
    co.compress();
    co.endSegment(sha1ptr);
    tmpIn.close();
    remove(tmp_out.c_str());
    if (memcmp(co.getChecksum(), sha1ptr, 20))
      error("Pre/post processor test failed\n");
  }
  else {  // compress without preprocessing
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
  Semaphore full;        // 1 if in is FULL of data ready to compress
  Semaphore compressed;  // 1 if out contains COMPRESSED data
  bool end;              // mark end of data
  CJ(): state(EMPTY), in(1<<24), out(1<<24), end(false) {}
};

// Instructions to a compression job
class CompressJob {
  Mutex mutex;           // protects state changes
  int job;               // number of jobs
  vector<CJ> q;          // buffer queue
  int front;             // next to remove from queue
  libzpaq::Writer* out;  // archive
  const char* method;    // custom compression in ZPAQL
  int* args;             // arguments to method
  Semaphore empty;       // number of empty buffers ready to fill
public:
  friend ThreadReturn compressThread(void* arg);
  friend ThreadReturn writeThread(void* arg);
  CompressJob(int t, libzpaq::Writer* f, const char* m, int* a):
      job(0), q(t), front(0), out(f), method(m), args(a) {
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
             const char* comment=0, bool end=false);
  vector<int> csize;  // compressed block sizes
};

// Write s at the back of the queue. Signal end of input with end=true
void CompressJob::write(const StringBuffer& s, const char* fn,
                        const char* cm, bool end) {
  for (unsigned k=end?q.size():1; k>0; --k) {
    empty.wait();
    lock(mutex);
    unsigned i, j;
    for (i=0; i<q.size(); ++i) {
      if (q[j=(i+front)%q.size()].state==CJ::EMPTY) {
        q[j].state=CJ::FILLING;
        q[j].end=end;
        q[j].filename=fn?fn:"";
        q[j].comment=cm?cm:"";
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
  {

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
      compressBlock(&cj.in, &cj.out, job.method, job.args, jobNumber+1,
                    cj.filename.c_str(), cj.comment.c_str());
      lock(job.mutex);
      cj.in.reset();
      cj.state=CJ::COMPRESSED;
      cj.compressed.signal();
      release(job.mutex);
    }
  }
  return 0;
}

// Write compressed data to the archive in the background
ThreadReturn writeThread(void* arg) {
  CompressJob& job=*(CompressJob*)arg;
  {

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
void Jidac::write_fragments(CompressJob& job, StringBuffer& b,
                            unsigned& block_start) {
  if (block_start>=ht.size()) return;
  for (unsigned i=block_start; i<ht.size(); ++i)
    b+=itob(ht[i].usize);
  b+=itob(block_start);
  b+=itob(ht.size()-block_start);
  job.write(b,
    ("jDC"+itos(date, 14)+"d"+itos(block_start,10)).c_str(), " jDC\x01");
  b.reset();
  ht[block_start].csize=-1;  // to fill in later
  block_start=ht.size();
}

// Add files to archive
void Jidac::add() {

  // Get transaction date
  if (mode==JIDAC) {
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
             datetostring(date).c_str());
    if (now==-1 || date<20120000000000LL || date>30000000000000LL)
      error("date is incorrect");
  }

  // Compress in SOLID or TINY mode to 1 block in a single thread
  if (mode==TINY || mode==SOLID) {

    if (method=="4") method=
    "(method 4 mid.cfg) "
    "comp 3 3 0 0 8 (hh hm ph pm n) "
    "  0 icm 5        (order 0...5 chain) "
    "  1 isse 13 0 "
    "  2 isse $1+17 1 "
    "  3 isse $1+18 2 "
    "  4 isse $1+18 3 "
    "  5 isse $1+19 4 "
    "  6 match $1+22 $1+24  (order 7) "
    "  7 mix 16 0 7 24 255  (order 1) "
    "hcomp "
    "  c++ *c=a b=c a=0 (save in rotating buffer M) "
    "  d= 1 hash *d=a   (orders 1...5 for isse) "
    "  b-- d++ hash *d=a "
    "  b-- d++ hash *d=a "
    "  b-- d++ hash *d=a "
    "  b-- d++ hash *d=a "
    "  b-- d++ hash b-- hash *d=a (order 7 for match) "
    "  d++ a=*c a<<= 8 *d=a       (order 1 for mix) "
    "  halt "
    "post "
    "  0 "
    "end ";
    else if (method=="0")
      method="comp 0 0 0 0 0 hcomp post 0 end ";
    else if (method=="1" || method=="2" || method=="3")
      error("-method 1, 2, 3 not supported in -solid or -tiny mode");

    // Start block
    read_args(false);
    libzpaq::Compressor co;
    StringWriter pcomp_cmd;
    OutputFile out;
    if (!out.open(archive.c_str())) return;
    co.setOutput(&out);
    if (mode==SOLID) co.writeTag();
    co.startBlock(method.c_str(), args, &pcomp_cmd);
    co.setVerify(mode==SOLID);
    int64_t offset=out.tell();

    // Compress files
    for (map<string, DT>::iterator p=dt.begin(); p!=dt.end(); ++p) {
      InputFile in;
      if (!in.open(rename(p->first.c_str()).c_str())) continue;

      // Get input size and checksum
      int64_t sz=0;
      char sha1result[20];
      if (mode==SOLID) {
        libzpaq::SHA1 sha1;
        for (int c; (c=in.get())!=EOF; sha1.put(c));
        sz=sha1.usize();  // input size
        memcpy(sha1result, sha1.result(), 20);  // checksum
        in.seek(0, SEEK_SET);
      }

      // Preprocess
      string pre;
      if (pcomp_cmd.s!="") {
        in.close();
        pre=tempname(0);
        string pcmd=pcomp_cmd.s+" "+p->first+" "+pre;
        if (!quiet) printf("%s\n", pcmd.c_str());
        (void)system(pcmd.c_str());
        if (!in.open(pre.c_str())) continue;
      }

      // Compress
      co.startSegment(p->first.c_str(), mode==SOLID ?
        (itos(sz)+" "+datetostring(p->second.edate)).c_str() : 0);
      co.setInput(&in);
      while (co.compress(100000)) {
        if (!quiet)
          printf("%s %1.0f -> %1.0f\r", p->first.c_str(), double(in.tell()),
               double(out.tell()-offset));
        fflush(stdout);
      }
      co.endSegment(mode==SOLID ? sha1result : 0);
      if (!quiet)
        printf("%s %1.0f -> %1.0f -> %1.0f  \n", p->first.c_str(), double(sz),
             double(in.tell()), double(out.tell()-offset));
      in.close();
      offset=out.tell();

      // Verify pre/post processor
      if (pcomp_cmd.s!="") {
        if (mode==SOLID) {
          int64_t postsz=co.getSize();
          if (memcmp(sha1result, co.getChecksum(), 20)) {
            fprintf(stderr,
                    "WARNING: pre/post test failed: restored size = %1.0f\n",
                    double(postsz));
          }
        }
        remove(pre.c_str());
      }
    }
    co.endBlock();
    co.stat(0);
    return;
  }

  // Compress in JIDAC or STREAMING mode. 
  // First Read archive index list into ht, dt.
  assert(mode==JIDAC || mode==STREAMING);
  int64_t header_pos=read_archive();
  if (mode==BAD)
    error("Archive not in ZPAQ format");
  read_args(false);

  // Build htinv for fast lookups of sha1 in ht
  map<string, unsigned> htinv;     // htinv[ht[i].sha1] == i
  for (int i=0; i<size(ht); ++i)
    htinv[string(ht[i].sha1, ht[i].sha1+20)]=i;

  // Open archive to append
  if (!quiet) printf("Appending to archive %s\n", archive.c_str());
  OutputFile out;
  if (!out.open(archive.c_str())) exit(1);
  int64_t archive_size=out.tell();
  if (mode!=JIDAC) header_pos=archive_size;
  if (archive_size!=header_pos) {
    if (!quiet)
      printf("Archive truncated from %1.0f to %1.0f bytes\n",
             double(archive_size), double(header_pos));
    out.truncate(header_pos);
  }

  // reserve space for the header block
  const unsigned htsize=ht.size();
  if (mode==JIDAC) writeJidacHeader(&out, date, -1, htsize);
  const int64_t header_end=out.tell();

  // Start compress and write jobs
  vector<ThreadID> tid(threads);
  ThreadID wid;
  CompressJob job(threads, &out, method.c_str(), args);
  for (int i=0; i<threads; ++i) run(tid[i], compressThread, &job);
  run(wid, writeThread, &job);

  // Split input files into fragments and group into blocks.
  unsigned block_start=ht.size();
  StringBuffer b(1<<24);  // current block
  int unmatchedFragments=0, totalFragments=0;  // counts
  for (map<string, DT>::iterator p=dt.begin(); p!=dt.end(); ++p) {
    if (p->second.edate && (force || p->second.dtv.size()==0
       || p->second.edate!=p->second.dtv.back().date)) {

      // Open input file
      string filename=rename(p->first);
      InputFile in;
      if (!in.open(filename.c_str())) {
        p->second.edate=0;
        continue;
      }
      else if (!quiet) {
        if (p->second.dtv.size()==0 || p->second.dtv.back().date==0)
          printf("Adding %s", p->first.c_str());
        else
          printf("Updating %s", p->first.c_str());
        if (p->first!=filename)
          printf(" from %s", filename.c_str());
        printf("\n");
      }

      // In STREAMING mode, split large files into 16 MB fragments. Store
      // each in a separate block. ht and dt are not used.
      if (mode!=JIDAC) {
        assert(b.size()==0);
        const char* name=p->first.c_str();  // name to store in archive
        string comment=" "+datetostring(p->second.edate);
        while (true) {
          int c=in.get();
          if (c==EOF || b.size()==16000000) {
            ++totalFragments;
            ++unmatchedFragments;
            job.write(b, name, name?comment.c_str():0);
            b.reset();
            name=0;
          }
          if (c==EOF) break;
          b.put(c);
        }
      }

      // In JIDAC mode, split the file into fragments on content-dependent
      // boundaries and group into blocks. For each fragment, look up its
      // hash in ht. If found, then omit it from the block, otherwise append
      // the hash to ht. Save fragment pointers in dt.
      else {
        libzpaq::SHA1 sha1;  // to compute SHA-1 hashes and sizes
        int c, c1=0;  // current, previous bytes
        const unsigned HM[2]={314159265, 271828182};  // must be odd and %4==2
        unsigned h=0;  // rolling hash for finding fragment boundaries
        unsigned char o1[256]={0};  // order-1 prediction
        int fragment=0;  // size
        p->second.dtv.push_back(DTV());
        do {
          c=in.get();
          if (c!=EOF) {
            sha1.put(c);
            b.put(c);
            ++fragment;
            h=(h+c+1)*HM[c==o1[c1]];  // depends on last 32 mispredicted bytes
            o1[c1]=c;
            c1=c;
          }
          const int MIN_FRAGMENT=4096;
          const int MAX_FRAGMENT=520192;
          if (c==EOF || (h<65536 && fragment>=MIN_FRAGMENT)
             || fragment>=MAX_FRAGMENT) {
            ++totalFragments;
            const char* shp=sha1.result();  // resets
            string sh(shp, shp+20);
            unsigned &j=htinv[sh];
            if (j==0) {  // new hash
              j=ht.size();
              ht.push_back(HT(shp, fragment, 0));
              ++unmatchedFragments;
            }
            else
              b.resize(b.size()-fragment);
            p->second.dtv.back().ptr.push_back(j);
            fragment=0;
            if (size(b)>=blocksize-257-MAX_FRAGMENT
                || (c==EOF && size(b)>blocksize*7/8))
              write_fragments(job, b, block_start);
          }
        } while (c!=EOF);
      }
      in.close();
    }
  }
  if (!quiet)
    printf("Matched %d of %d fragments\n",
           totalFragments-unmatchedFragments, totalFragments);

  // compress last partial block
  write_fragments(job, b, block_start);

  // Wait for jobs to finish
  job.write(b, 0, 0, true);  // signal end of input
  for (int i=0; i<threads; ++i)
    join(tid[i]);
  join(wid);
  if (mode!=JIDAC) {
    out.close();
    return;
  }

  // JIDAC: Fill in compressed sizes in ht
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
  for (map<string, DT>::iterator p=dt.begin(); p!=dt.end();) {
    if (p->second.dtv.size()==0) {
      ++p;
      continue;
    }
    if (p->second.written>=0 && p->second.dtv.back().date
        && !p->second.edate) {
      is+=ltob(0)+p->first+'\0';
      if (!quiet) printf("Removing %s\n", p->first.c_str());
    }
    else if (p->second.edate
             && (force || p->second.edate!=p->second.dtv.back().date)) {
      is+=ltob(p->second.edate)+p->first+'\0';
      is+=itob(0)+itob(size(p->second.dtv.back().ptr));
      for (int i=0; i<size(p->second.dtv.back().ptr); ++i)
        is+=itob(p->second.dtv.back().ptr[i]);
    }
    ++p;
    if (is.size()>8000 || (is.size()>0 && p==dt.end())) {
      compressBlock(&is, &out, 
        // 2-D indirect context model with 4 columns
        "comp 0 2 0 0 1 "
        "  0 icm 11 "
        "hcomp "
        "  *b=a hash "
        "  b++ hash *d=a a=b a&= 3 hashd "
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
void makepath(string& path) {
  for (int i=0; i<size(path); ++i) {
    if (path[i]=='\\' || path[i]=='/') {
      path[i]=0;
#ifdef unix
      int ok=!mkdir(path.c_str(), 0777);
#else
      int ok=CreateDirectory(utow(path.c_str()).c_str(), 0);
#endif
      if (ok && !quiet) printf("Created directory %s\n", path.c_str());
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
  map<string, DT>::iterator lastdt;  // currently open output file
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
        if (detailed)
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
    for (map<string,DT>::iterator p=job.jd.dt.begin();p!=job.jd.dt.end();++p){
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
            if (!quiet)
              printf("Job %d: extracting %s\n", jobNumber, filename.c_str());
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
          job.outf.close(dtr.dtv.back().date);
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

// Extract files from archive
void Jidac::extract() {

  // Read HT, DT
  read_archive();
  read_args(force);

  // Map fragments to blocks
  ExtractJob job(*this);
  vector<unsigned> hti(ht.size());  // fragment index -> block index
  for (unsigned i=1; i<ht.size(); ++i) {
    if (ht[i].csize>=0) job.block.push_back(Block(i, ht[i].csize));
    hti[i]=job.block.size()-1;
  }

  // Don't clobber
  for (map<string, DT>::iterator p=dt.begin(); p!=dt.end(); ++p) {
    if (p->second.dtv.size() && p->second.dtv.back().date
        && p->second.written==0) {
      if (p->second.edate) {
        fprintf(stderr, "File exists: %s\n", rename(p->first).c_str());
        error("won't clobber existing files without -force");
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
  lock(job.write_mutex);
  libzpaq::Decompresser d;
  libzpaq::SHA1 sha1;
  InputFile in;
  if (!in.open(archive.c_str())) return;
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
        lastfile=filename.s;
        if (out.isopen()) out.close();
        first=false;
        string newfile;
        map<string,DT>::iterator p=dt.find(lastfile);
        if (p!=dt.end() && p->second.written==0) {  // todo
          newfile=rename(lastfile);
          makepath(newfile);
          if (out.open(newfile.c_str())) {
            if (!quiet) printf("main: extracting %s\n", newfile.c_str());
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
}

/////////////////////////////// list //////////////////////////////////

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
void Jidac::list() {

  // detailed mode
  if (detailed) {
    InputFile in;
    if (!in.open(archive.c_str())) return;
    printf("Archive %s\n", archive.c_str());
    libzpaq::Decompresser d;
    d.setInput(&in);
    double mem;
    StringWriter filename, comment, buf;
    char sha1result[21];
    map<string, int> m;
    int block=0;
    int64_t offset=0;
    while (d.findBlock(&mem)) {
      try {
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
          printf(" %s %s -> %1.0f\n", filename.s.c_str(), comment.s.c_str(),
                 double(in.tell()-offset));
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
                printf("  %14.0f ", double(fdate));
                while (p<end && *p) putchar(*p++);  // filename
                ++p;
                if (fdate) {
                  putchar(' ');
                  if (p>end-4) break;
                  int n=btoi(p);  // na
                  while (n>0 && p<end)
                    printf("%02x", *p++&255); // attr
                  if (p>end-4) break;
                  n=btoi(p);  // ni
                  vector<unsigned> ptr;
                  for (; n>0 && p<=end-4; --n)
                    ptr.push_back(btoi(p));
                  printList(ptr);
                }
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
      catch (std::exception& e) {
        printf("Skipping block: %s\n", e.what());
      }
    }
    in.close();
    return;
  }

  // Non-detailed list
  int64_t csize=read_archive();  // read into ht, dt
  int64_t usize=0;
  int nfiles=0, versions=0, deletions=0;
  read_args(force);
  for (map<string, DT>::const_iterator p=dt.begin(); p!=dt.end(); ++p) {
    if (!p->second.dtv.size()) continue;
    if (p->second.dtv.back().date) ++nfiles;
    if (p->second.dtv.back().size>=0) usize+=p->second.dtv.back().size;
    for (unsigned i=0; i<p->second.dtv.size(); ++i) {
      if (p->second.dtv[i].date) ++versions;
      else ++deletions;
    }
    if (files.size() && p->second.written==-1) continue;
    for (unsigned i=0; i<p->second.dtv.size(); ++i) {
      printf("%4d ", p->second.dtv[i].version);
      const int64_t t=p->second.dtv[i].date;
      printf("%s ", datetostring(t).c_str());

      // Compute comparison result symbol
      char cmp=' ';
      if (p->second.written==0 && i+1==p->second.dtv.size()) {
        if (p->second.dtv[i].date) {
          if (p->second.edate==0) cmp='>';
          else if (p->second.edate==p->second.dtv[i].date) cmp='=';
          else cmp='#';
        }
        else if (p->second.edate) cmp='<';
      }
      if (p->second.dtv[i].size>=0 && p->second.dtv[i].date)
        printf("%12.0f ", double(p->second.dtv[i].size));
      else printf("             ");
      printf("%c %s ", cmp, p->first.c_str());
      if (p->second.dtv[i].streaming) printf("*");
      else printf(":");
      printList(p->second.dtv[i].ptr);
    }
  }
  printf("%u versions of %u files and %u deletions in %u fragments\n"
         "%1.0f -> %1.0f\n",
         versions, nfiles, deletions, ht.size()-1,
         double(usize), double(csize));
}

////////////////////////// step, stat ///////////////////////////

// -trace: Execute ZPAQL input and show virtual register contents after
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
  printf("Memory utilization for job [%d]:\n", id);
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

// Writer to stdout
class Stdout: public libzpaq::Writer {
  void put(int c) {putchar(c);}
};

// Run or trace config.cfg specified by -method config
// -run hcomp|pcomp input output (default stdin, stdout)
// -trace hcomp|pcomp args... (single-step with decimal or hex (xNN) args)
void Jidac::runtrace(int argc, char** argv) {
  assert(argc>=2);
  assert(argv[0][1]=='r' || argv[0][1]=='t');  // run or trace
  assert(argv[1][0]=='h' || argv[1][0]=='p');  // hcomp or pcomp

  // Compile config.cfg, args[] to hz, pz, pcomp_cmd
  libzpaq::ZPAQL hz, pz, *z;
  StringBuffer pcomp_cmd;
  libzpaq::Compiler(method.c_str(), args, hz, pz, &pcomp_cmd);

  // Initialize either hz or pz to execute instructions
  if (argv[1][0]=='h') {  // hcomp
    z=&hz;
    z->inith();
  }
  else {  // pcomp
    if (pcomp_cmd.size()==0) error("no PCOMP section");
    z=&pz;
    z->initp();
  }

  // Trace from decimal or hex command line arguments
  if (argv[0][1]=='t') {
    for (int i=2; i<argc; ++i)
      z->step(ntoi(argv[i]), tolower(argv[i][0])=='x');
  }

  // Run from input to output
  else {

    // open input and output (default stdin, stdout)
    FILE* in=stdin;
    OutputFile out;
    Stdout outc;
    if (argc>2) {
      in=fopen(argv[2], "rb");
      if (!in) perror(argv[2]), exit(1);
    }
    if (argc>3) {
      if (!out.open(argv[3])) exit(1);
      z->output=&out;
    }
    else
      z->output=&outc;

    // run once per input byte, plus EOF if pcomp
    int c;
    while ((c=getc(in))!=EOF) z->run(c);
    if (argv[1][0]=='p') z->run(-1);
    z->flush();
    out.close();
    if (in!=stdin) fclose(in);
  }    
}

int main(int argc, char** argv) {
  clock_t start=clock();  // get start time
  try {
    Jidac jidac;
    jidac.doCommand(argc, argv);
  }
  catch (std::exception& e) {
    printf("Error: %s\n", e.what());
  }
  if (!quiet)
    printf("%1.2f seconds\n", double(clock()-start)/CLOCKS_PER_SEC);
  return 0;
}

