// zpaq.cpp - Journaling incremental deduplicating archiver

#define ZPAQ_VERSION "6.59"

/*  Copyright (C) 2009-2014, Dell Inc. Written by Matt Mahoney.

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

    divsufsort.c from libdivsufsort-lite is (C) 2003-2008, Yuta Mori
    and is embedded in this file. It is licensed under the MIT license
    described below.

zpaq is a journaling (append-only) archiver for incremental backups.
Files are added only when the last-modified date has changed. Both the old
and new versions are saved. You can extract from old versions of the
archive by specifying a date or version number. zpaq supports 5
compression levels, deduplication, AES-256 encryption, and multi-threading
using an open, self-describing format for backward and forward
compatibility in Windows and Linux. See zpaq.pod for usage.

TO COMPILE:

This program needs libzpaq from http://mattmahoney.net/zpaq/ and
libdivsufsort-lite from above or http://code.google.com/p/libdivsufsort/
Recommended compile for Windows with MinGW:

  g++ -O3 zpaq.cpp libzpaq.cpp -o zpaq

With Visual C++:

  cl /O2 /EHsc zpaq.cpp libzpaq.cpp advapi32.lib

For Linux:

  g++ -O3 -Dunix zpaq.cpp libzpaq.cpp -pthread -o zpaq

Possible options:

  -o         Name of output executable.
  -O3 or /O2 Optimize (faster).
  /EHsc      Enable exception handing in VC++ (required).
  -s         Strip debugging symbols. Smaller executable.
  /arch:SSE2 Assume x86 processor with SSE2. Otherwise use -DNOJIT.
  -msse2     Same. Implied by -m64 for a x86-64 target.
  -DNOJIT    Don't assume x86 with SSE2 for libzpaq. Slower (disables JIT).
  -static    Don't assume C++ runtime on target. Bigger executable but safer.
  -Dunix     Not Windows. Sometimes automatic in Linux. Needed for Mac OS/X.
  -fopenmp   Parallel divsufsort (faster, implies -pthread, broken in MinGW).
  -pthread   Required in Linux, implied by -fopenmp.
  -DDEBUG    Turn on debugging checks.
  -DPTHREAD  Use Pthreads instead of Windows threads. Requires pthreadGC2.dll
             or pthreadVC2.dll from http://sourceware.org/pthreads-win32/
  -Dunixtest To make -Dunix work in Windows with MinGW.
  -Wl,--large-address-aware  To make 3 GB available in 32 bit Windows.

*/
#define _FILE_OFFSET_BITS 64  // In Linux make sizeof(off_t) == 8
#define UNICODE  // For Windows
#include "libzpaq.h"
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
#include <fcntl.h>

#ifndef DEBUG
#define NDEBUG 1
#endif
#include <assert.h>

#ifdef unix
#define PTHREAD 1
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>
#include <dirent.h>
#include <utime.h>
#include <errno.h>

#ifdef unixtest
struct termios {
  int c_lflag;
};
#define ECHO 1
#define ECHONL 2
#define TCSANOW 4
int tcgetattr(int, termios*) {return 0;}
int tcsetattr(int, int, termios*) {return 0;}
#else
#include <termios.h>
#endif

#else  // Assume Windows
#include <windows.h>
#include <wincrypt.h>
#include <io.h>
#endif

using std::string;
using std::vector;
using std::map;
using std::min;
using std::max;

// Handle errors in libzpaq and elsewhere
void libzpaq::error(const char* msg) {
  fprintf(stderr, "zpaq error: %s\n", msg);
  if (strstr(msg, "ut of memory")) throw std::bad_alloc();
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
// sem.destroy();          // deallocate resources used by semaphore

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
#ifndef fseeko
#define fseeko(a,b,c) fseeko64(a,b,c)
#endif
#ifndef ftello
#define ftello(a) ftello64(a)
#endif
#endif
#endif

// For testing -Dunix in Windows
#ifdef unixtest
#define lstat(a,b) stat(a,b)
#define mkdir(a,b) mkdir(a)
#ifndef fseeko
#define fseeko(a,b,c) fseeko64(a,b,c)
#endif
#ifndef ftello
#define ftello(a) ftello64(a)
#endif
#endif

// Global variables
FILE* con=stdout;    // log output, can be stderr
bool fragile=false;  // -fragile option
int64_t quiet=-1;    // -quiet option
static const int64_t MAX_QUIET=0x7FFFFFFFFFFFFFFFLL;  // no output but errors
int64_t global_start=0;  // set to mtime() at start of main()

// signed size of a string or vector
template <typename T> int size(const T& x) {
  return x.size();
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

// Print a UTF-8 string to f (stdout, stderr) so it displays properly
void printUTF8(const char* s, FILE* f) {
  assert(f);
  assert(s);
#ifdef unix
  fprintf(f, "%s", s);
#else
  const HANDLE h=(HANDLE)_get_osfhandle(_fileno(f));
  DWORD ft=GetFileType(h);
  if (ft==FILE_TYPE_CHAR) {
    fflush(f);
    std::wstring w=utow(s);  // Windows console: convert to UTF-16
    DWORD n=0;
    WriteConsole(h, w.c_str(), w.size(), &n, 0);
  }
  else  // stdout redirected to file
    fprintf(f, "%s", s);
#endif
}

// Return relative time in milliseconds
int64_t mtime() {
#ifdef unix
  timeval tv;
  gettimeofday(&tv, 0);
  return tv.tv_sec*1000LL+tv.tv_usec/1000;
#else
  int64_t t=GetTickCount();
  if (t<global_start) t+=0x100000000LL;
  return t;
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
      if (attrib>0x10000) {
        r="0x        ";
        for (int i=0; i<8; ++i)
          r[9-i]="0123456789abcdef"[attrib>>(4*i)&15];
      }
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
int64_t decimal_time(time_t tt) {
  if (tt==-1) tt=0;
  int64_t t=(sizeof(tt)==4) ? unsigned(tt) : tt;
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

// Put n cryptographic random bytes in buf[0..n-1].
// The first byte will not be 'z' or '7' (start of a ZPAQ archive).
// For a pure random number, discard the first byte.

void random(char* buf, int n) {
#ifdef unix
  FILE* in=fopen("/dev/urandom", "rb");
  if (in && fread(buf, 1, n, in)==n)
    fclose(in);
  else {
    perror("/dev/urandom");
    error("key generation failed");
  }
#else
  HCRYPTPROV h;
  if (CryptAcquireContext(&h, NULL, NULL, PROV_RSA_FULL,
      CRYPT_VERIFYCONTEXT) && CryptGenRandom(h, n, (BYTE*)buf))
    CryptReleaseContext(h, 0);
  else {
    fprintf(stderr, "CryptGenRandom: error %d\n", int(GetLastError()));
    error("key generation failed");
  }
#endif
  if (n>=1 && (buf[0]=='z' || buf[0]=='7'))
    buf[0]^=0x80;
}

/////////////////////////////// File //////////////////////////////////

// Convert non-negative decimal number x to string of at least n digits
string itos(int64_t x, int n=1) {
  assert(x>=0);
  assert(n>=0);
  string r;
  for (; x || n>0; x/=10, --n) r=string(1, '0'+x%10)+r;
  return r;
}

// Replace * and ? in fn with part or digits of part
string subpart(string fn, int part) {
  for (int j=fn.size()-1; j>=0; --j) {
    if (fn[j]=='?')
      fn[j]='0'+part%10, part/=10;
    else if (fn[j]=='*')
      fn=fn.substr(0, j)+itos(part)+fn.substr(j+1), part=0;
  }
  return fn;
}

// Return true if a file or directory (UTF-8 without trailing /) exists.
// If part>0 then replace * and ? in filename with part or its digits.
bool exists(string filename, int part=0) {
  if (part>0) filename=subpart(filename, part);
  int len=filename.size();
  if (len<1) return false;
  if (filename[len-1]=='/') filename=filename.substr(0, len-1);
#ifdef unix
  struct stat sb;
  return !lstat(filename.c_str(), &sb);
#else
  return GetFileAttributes(utow(filename.c_str(), true).c_str())
         !=INVALID_FILE_ATTRIBUTES;
#endif
}

// Delete a file, return true if successful
bool delete_file(const char* filename) {
#ifdef unix
  return remove(filename)==0;
#else
  return DeleteFile(utow(filename, true).c_str());
#endif
}

#ifndef unix

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
  else if (err==ERROR_BAD_PATHNAME)
    fprintf(stderr, ": bad pathname\n");
  else if (err==ERROR_INVALID_NAME)
    fprintf(stderr, ": invalid name\n");
  else
    fprintf(stderr, ": Windows error %d\n", err);
}

// Set the last-modified date of an open file handle
void setDate(HANDLE out, int64_t date) {
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
}
#endif

// Create directories as needed. For example if path="/tmp/foo/bar"
// then create directories /, /tmp, and /tmp/foo unless they exist.
// Set date and attributes if not 0.
void makepath(string path, int64_t date=0, int64_t attr=0) {
  for (int i=0; i<size(path); ++i) {
    if (path[i]=='\\' || path[i]=='/') {
      path[i]=0;
#ifdef unix
      int ok=!mkdir(path.c_str(), 0777);
#else
      int ok=CreateDirectory(utow(path.c_str(), true).c_str(), 0);
#endif
      if (ok && quiet<=0) {
        fprintf(con, "Created directory ");
        printUTF8(path.c_str(), con);
        fprintf(con, "\n");
      }
      path[i]='/';
    }
  }

  // Set date and attributes
  string filename=path;
  if (filename!="" && filename[filename.size()-1]=='/')
    filename=filename.substr(0, filename.size()-1);  // remove trailing slash
#ifdef unix
  if (date>0) {
    struct utimbuf ub;
    ub.actime=time(NULL);
    ub.modtime=unix_time(date);
    utime(filename.c_str(), &ub);
  }
  if ((attr&255)=='u')
    chmod(filename.c_str(), attr>>8);
#else
  for (int i=0; i<size(filename); ++i)  // change to backslashes
    if (filename[i]=='/') filename[i]='\\';
  if (date>0) {
    HANDLE out=CreateFile(utow(filename.c_str(), true).c_str(),
                          FILE_WRITE_ATTRIBUTES, 0, NULL, OPEN_EXISTING,
                          FILE_FLAG_BACKUP_SEMANTICS, NULL);
    if (out!=INVALID_HANDLE_VALUE) {
      setDate(out, date);
      CloseHandle(out);
    }
    else winError(filename.c_str());
  }
  if ((attr&255)=='w') {
    SetFileAttributes(utow(filename.c_str(), true).c_str(), attr>>8);
  }
#endif
}

// Base class of InputFile and OutputFile (OS independent)
class File {
protected:
  enum {BUFSIZE=1<<16};  // buffer size
  int ptr;  // next byte to read or write in buf
  libzpaq::Array<char> buf;  // I/O buffer
  libzpaq::AES_CTR *aes;  // if not NULL then encrypt
  int64_t eoff;  // extra offset for multi-file encryption
  File(): ptr(0), buf(BUFSIZE), aes(0), eoff(0) {}
};

// File types accepting UTF-8 filenames
#ifdef unix

class InputFile: public File, public libzpaq::Reader {
  FILE* in;
  int n;  // number of bytes in buf
public:
  InputFile(): in(0), n(0) {}

  // Open file for reading. Return true if successful.
  // If aes then encrypt with aes+eoff.
  bool open(const char* filename, libzpaq::AES_CTR* a=0, int64_t e=0) {
    in=fopen(filename, "rb");
    if (!in) perror(filename);
    aes=a;
    eoff=e;
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
      if (aes) {
        int64_t off=tell()+eoff;
        if (off<32) error("attempt to read salt");
        aes->encrypt(&buf[0], n, off);
      }
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
  // If aes then encrypt with aes+eoff.
  bool open(const char* filename, libzpaq::AES_CTR* a=0, int64_t e=0) {
    assert(!isopen());
    ptr=0;
    this->filename=filename;
    out=fopen(filename, "rb+");
    if (!out) out=fopen(filename, "wb+");
    if (!out) perror(filename);
    aes=a;
    eoff=e;
    if (out) fseeko(out, 0, SEEK_END);
    return isopen();
  }

  // Flush pending output
  void flush() {
    if (ptr) {
      assert(isopen());
      assert(ptr>0 && ptr<=BUFSIZE);
      if (aes) {
        int64_t off=ftello(out)+eoff;
        if (off<32) error("attempt to overwrite salt");
        aes->encrypt(&buf[0], ptr, off);
      }
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

class InputFile: public File, public libzpaq::Reader {
  HANDLE in;  // input file handle
  DWORD n;    // buffer size
public:
  InputFile():
    in(INVALID_HANDLE_VALUE), n(0) {}

  // Open for reading. Return true if successful.
  // Encrypt with aes+e if aes.
  bool open(const char* filename, libzpaq::AES_CTR* a=0, int64_t e=0) {
    assert(in==INVALID_HANDLE_VALUE);
    n=ptr=0;
    std::wstring w=utow(filename, true);
    in=CreateFile(w.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL,
                  OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (in==INVALID_HANDLE_VALUE) winError(filename);
    aes=a;
    eoff=e;
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
      if (aes) {
        int64_t off=tell()+eoff;
        if (off<32) error("attempt to read salt");
        aes->encrypt(&buf[0], n, off);
      }
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
  // If aes then encrypt with aes+e.
  bool open(const char* filename_, libzpaq::AES_CTR* a=0, int64_t e=0) {
    assert(!isopen());
    ptr=0;
    filename=utow(filename_, true);
    out=CreateFile(filename.c_str(), GENERIC_READ | GENERIC_WRITE,
                   0, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (out==INVALID_HANDLE_VALUE) winError(filename_);
    else {
      LONG hi=0;
      aes=a;
      eoff=e;
      SetFilePointer(out, 0, &hi, FILE_END);
    }
    return isopen();
  }

  // Write pending output
  void flush() {
    assert(isopen());
    if (ptr) {
      DWORD n=0;
      if (aes) {
        int64_t off=tell()-ptr+eoff;
        if (off<32) error("attempt to overwrite salt");
        aes->encrypt(&buf[0], ptr, off);
      }
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
      setDate(out, date);
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

// Count bytes written and discard them
struct Counter: public libzpaq::Writer {
  int64_t pos;  // count of written bytes
  Counter(): pos(0) {}
  void put(int c) {++pos;}
  void write(const char* bufp, int size) {pos+=size;}
};

/////////////////////////////// Archive ///////////////////////////////

// An Archive is a multi-part file that supports encrypted input
class Archive: public libzpaq::Reader, public libzpaq::Writer {
  libzpaq::AES_CTR* aes;  // NULL if not encrypted
  struct FE {  // File element for multi-part archives
    string fn;    // file name
    int64_t end;  // size of previous and current files
    FE(): end(0) {}
    FE(const string& s, int64_t e): fn(s), end(e) {}
  };
  vector<FE> files;  // list of parts. only last part is writable.
  int fi;  // current file in files
  int64_t off;  // total offset over all files
  int mode;     // 'r' or 'w' for reading or writing or 0 if closed
  InputFile in; // currently open input file
  OutputFile out;  // currently open output file
public:

  // Constructor
  Archive(): aes(0), fi(0), off(0), mode(0) {}

  // Destructor
  ~Archive() {close();}

  // Open filename for read and write. If filename contains wildards * or ?
  // then replace * with part number 1, 2, 3... or ? with single digits
  // up to the last existing file. Return true if at least one file is found.
  // If password is not NULL then assume the concatenation of the files
  // is in encrypted format. mode_ is 'r' for reading or 'w' for writing.
  // If the filename contains wildcards then output is to the first
  // non-existing file, else to filename. If newsize>=0 then truncate
  // the output to newsize bytes. If password and offset>0 then encrypt
  // output as if previous parts had size offset and salt salt.
  bool open(const char* filename, const char* password=0, int mode_='r',
            int64_t newsize=-1, int64_t offset=0, const char* salt=0);

  // True if archive is open
  bool isopen() const {return files.size()>0;}

  // Position the next read or write offset to p.
  void seek(int64_t p, int whence);

  // Return current file offset.
  int64_t tell() const {return off;}

  // Read up to n bytes into buf at current offset. Return 0..n bytes
  // actually read. 0 indicates EOF.
  int read(char* buf, int n) {
    assert(mode=='r');
    if (fi>=size(files)) return 0;
    if (!in.isopen()) return 0;
    n=in.read(buf, n);
    seek(n, SEEK_CUR);
    return n;
  }

  // Read and return 1 byte or -1 (EOF)
  int get() {
    assert(mode=='r');
    if (fi>=size(files)) return -1;
    while (off==files[fi].end) {
      in.close();
      if (++fi>=size(files)) return -1;
      if (!in.open(files[fi].fn.c_str(), aes, fi>0 ? files[fi-1].end : 0))
        error("cannot read next archive part");
    }
    ++off;
    return in.get();
  }

  // Write one byte
  void put(int c) {
    assert(fi==size(files)-1);
    assert(fi>0 || out.tell()==off);
    assert(fi==0 || out.tell()+files[fi-1].end==off);
    assert(mode=='w');
    out.put(c);
    ++off;
  }

  // Write buf[0..n-1]
  void write(const char* buf, int n) {
    assert(fi==size(files)-1);
    assert(fi>0 || out.tell()==off);
    assert(fi==0 || out.tell()+files[fi-1].end==off);
    assert(mode=='w');
    out.write(buf, n);
    off+=n;
  }

  // Close any open part
  void close() {
    if (out.isopen()) out.close();
    if (in.isopen()) in.close();
    if (aes) {
      delete aes;
      aes=0;
    }
    files.clear();
    fi=0;
    off=0;
    mode=0;
  }
};

bool Archive::open(const char* filename, const char* password, int mode_,
                   int64_t newsize, int64_t offset, const char* salt) {
  assert(filename);
  assert(mode_=='r' || mode_=='w');
  mode=mode_;

  // Read part files and get sizes. Get salt from the first part.
  string next;
  for (int i=1; !offset; ++i) {
    next=subpart(filename, i);
    if (!exists(next)) break;
    if (files.size()>0 && files[0].fn==next) break; // part overflow

    // set up key from salt in first file
    if (!in.open(next.c_str())) error("cannot read archive");
    if (i==1 && password && newsize!=0) {
      char slt[32], key[32];
      if (in.read(slt, 32)!=32) error("no salt");
      libzpaq::stretchKey(key, password, slt);
      aes=new libzpaq::AES_CTR(key, 32, slt);
    }

    // Get file size
    in.seek(0, SEEK_END);
    files.push_back(FE(next,
        in.tell()+(files.size() ? files[files.size()-1].end : 0)));
    in.close();
    if (next==filename) break;  // no wildcards
  }

  // If offset is not 0 then use it for the part sizes and use
  // salt as the salt of the first part.
  if (offset>0) {
    files.push_back(FE("", offset));
    files.push_back(FE(filename, offset));
    if (password) {
      assert(salt);
      char key[32]={0};
      libzpaq::stretchKey(key, password, salt);
      aes=new libzpaq::AES_CTR(key, 32, salt);
    }
  }

  // Open file for reading
  fi=files.size();
  if (mode=='r') {
    seek(32*(password!=0), SEEK_SET);  // open first input file
    return files.size()>0;
  }

  // Truncate, deleting extra parts
  if (newsize>=0) {
    while (files.size()>0 && files.back().end>newsize) {
      if (newsize==0 || (files.size()>1 &&
          files[files.size()-2].end>=newsize)) {
        printUTF8(files.back().fn.c_str(), con);
        fprintf(con, " deleted.\n");
        next=files.back().fn.c_str();
        delete_file(files.back().fn.c_str());
        files.pop_back();
      }
      else if (files.size()>0) {
        if (!out.open(files.back().fn.c_str()))
          error("cannot open archive part to truncate");
        int64_t newlen=newsize;
        if (files.size()>=2) newlen-=files[files.size()-2].end;
        printUTF8(files.back().fn.c_str(), con);
        fprintf(con, " truncated from %1.0f to %1.0f bytes.\n",
          out.tell()+0.0, newlen+0.0);
        assert(newlen>=0);
        out.truncate(newlen);
        out.close();
        files.back().end=newsize;
      }
    }
  }
         
  // Get name of part to write. If filename has wildcards then use
  // the next part number, else just filename.
  if (files.size()==0 || (next!=filename && next!=files[0].fn))
    files.push_back(FE(next, files.size() ? files.back().end : 0));

  // Write salt for a new encrypted archive
  fi=files.size()-1;
  assert(fi>=0);
  if (password && !aes) {
    assert(fi==0);
    assert(files.size()==1);
    if (!out.open(files[fi].fn.c_str()))
      error("cannot write salt to archive");
    out.seek(0, SEEK_SET);
    char key[32]={0};
    char slt[32]={0};
    if (salt) memcpy(slt, salt, 32);
    else random(slt, 32);
    libzpaq::stretchKey(key, password, slt);
    aes=new libzpaq::AES_CTR(key, 32, slt);
    out.write(slt, 32);
    files[fi].end=out.tell();  // 32
    out.close();
  }

  // Open for output
  assert(fi+1==size(files));
  assert(fi>=0);
  makepath(files[fi].fn.c_str());
  if (!out.open(files[fi].fn.c_str(), aes, fi>0 ? files[fi-1].end : 0))
    error("cannot open archive for output");
  off=files.back().end;
  assert(fi>0 || files[fi].end==out.tell());
  assert(fi==0 || files[fi].end==out.tell()+files[fi-1].end);
  fprintf(con, "Appending to ");
  printUTF8(files[fi].fn.c_str(), con);
  fprintf(con, " at offset %1.0f\n", out.tell()+0.0);
  return true;
}

void Archive::seek(int64_t p, int whence) {
  if (whence==SEEK_SET) off=p;
  else if (whence==SEEK_CUR) off+=p;
  else if (whence==SEEK_END) off=(files.size() ? files.back().end : 0)+p;
  else assert(false);
  if (mode=='r') {
    int oldfi=fi;
    for (fi=0; fi<size(files) && off>=files[fi].end; ++fi);
    if (fi!=oldfi) {
      in.close();
      if (fi<size(files) && !in.open(files[fi].fn.c_str(), aes,
          fi>0 ? files[fi-1].end : 0))
        error("cannot reopen archive after seek");
    }
    if (fi<size(files)) in.seek(off-files[fi].end, SEEK_END);
  }
  else if (mode=='w') {
    assert(files.size()>0);
    assert(out.isopen());
    assert(fi+1==size(files));
    p=off;
    if (files.size()>=2) p-=files[files.size()-2].end;
    if (p<0) error("seek before start of output");
    out.seek(p, SEEK_SET);
  }
}

///////////////////////// NumberOfProcessors ///////////////////////////

// Guess number of cores. In 32 bit mode, max is 2.
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
  if (sizeof(char*)==4 && rc>2) rc=2;
  return rc;
}

////////////////////////////// StringBuffer //////////////////////////

// For libzpaq output to a string
struct StringWriter: public libzpaq::Writer {
  string s;
  void put(int c) {s+=char(c);}
};

// WriteBuffer for memory efficient output buffering
class WriteBuffer: public libzpaq::Writer {
  enum {BUFSIZE=(1<<19)-80};  // buffer size
  int wptr;  // number of bytes in last buffer
  int limit;  // max buffers
  vector<char*> v;  // array of buffers
  WriteBuffer& operator=(const WriteBuffer&);  // no assignment
  WriteBuffer(const WriteBuffer&);  // no copy
  void grow();  // append a buffer

public:
  WriteBuffer(): wptr(BUFSIZE), limit(0x7fffffff) {}

  // Number of bytes put
  int64_t size() const {return int64_t(v.size())*BUFSIZE+wptr-BUFSIZE;}

  // Set allocation limit
  void setLimit(size_t lim) {limit=lim/BUFSIZE+1;}

  // store n bytes from buf[0..n-1]
  void write(const char* buf, int n);

  // store 1 byte
  void put(int c) {
    if (wptr==BUFSIZE) grow();
    assert(v.size()>0);
    assert(wptr>=0 && wptr<BUFSIZE);
    v.back()[wptr++]=c;
  }

  // write to out
  void save(libzpaq::Writer* out);

  // Write n bytes at begin..begin+n-1 to out at offset off
  void save(OutputFile& out, int64_t off, int64_t begin, int64_t n);

  // Return the SHA-1 of n bytes at begin..begin+n-1 to result[0..19]
  void sha1(char* result, int64_t begin, int64_t n);

  // Free memory
  void reset();
  ~WriteBuffer() {reset();}
};

// Append a buffer
void WriteBuffer::grow() {
  assert(wptr==BUFSIZE);
  if (int(v.size())>=limit) error("WriteBuffer overflow");
  v.push_back((char*)malloc(BUFSIZE));
  if (!v.back()) error("WriteBuffer: out of memory");
  wptr=0;
}

// store n bytes from buf[0..n-1]
void WriteBuffer::write(const char* buf, int n) {
  while (n>0) {
    assert(wptr>=0 && wptr<=BUFSIZE);
    if (wptr==BUFSIZE) grow();
    int n1=n;
    if (n1>BUFSIZE-wptr) n1=BUFSIZE-wptr;
    assert(n1>0 && n1<=BUFSIZE);
    memcpy(v.back()+wptr, buf, n1);
    wptr+=n1;
    n-=n1;
    buf+=n1;
  }
}

// write to out
void WriteBuffer::save(libzpaq::Writer* out) {
  if (!out) return;
  for (int i=0; i<int(v.size())-1; ++i)
    out->write(v[i], BUFSIZE);
  if (v.size())
    out->write(v.back(), wptr);
}

// Write n bytes at begin..begin+n-1 to out at offset off..off+n-1
void WriteBuffer::save(OutputFile& out, int64_t off, int64_t begin,
                       int64_t n) {
  assert(out.isopen());
  assert(off>=0);
  assert(begin>=0);
  assert(n>=0);
  assert(begin+n<=size());

  // Trim leading and trailing zeros before writing
  for (int i=begin/BUFSIZE; i<int(v.size()); ++i) {
    assert(i>=0 && i<int(v.size()));
    int64_t b=begin-int64_t(i)*BUFSIZE;
    int64_t e=b+n;
    if (b<0) b=0;
    if (e>BUFSIZE) e=BUFSIZE;
    if (e<=0) break;
    int b1=b, e1=e;
    while (b1<e1 && v[i][b1]==0) ++b1;
    while (e1>b1 && v[i][e1-1]==0) --e1;
    if (b1-b<4096) b1=b;
    if (e-e1<4096) e1=e;
    if (e1>b1) out.write(v[i]+b1, off-begin+i*BUFSIZE+b1, e1-b1);
  }
}

// Return the SHA-1 of n bytes at begin..begin+n-1 to result[0..19]
void WriteBuffer::sha1(char* result, int64_t begin, int64_t n) {
  if (!result) return;
  assert(begin>=0);
  assert(n>=0);
  assert(begin+n<=size());
  libzpaq::SHA1 s;
  for (int i=begin/BUFSIZE; i<int(v.size()); ++i) {
    assert(i>=0 && i<int(v.size()));
    int64_t b=begin-int64_t(i)*BUFSIZE;
    int64_t e=b+n;
    if (b<0) b=0;
    if (e>BUFSIZE) e=BUFSIZE;
    if (e<=0) break;
    while (b<e) s.put(v[i][b++]);
  }
  assert(uint64_t(n)==s.usize());
  memcpy(result, s.result(), 20);
}

// Free memory
void WriteBuffer::reset() {
  while (v.size()>0) {
    if (v.back()) free(v.back());
    v.pop_back();
  }
  wptr=BUFSIZE;
}

// For (de)compressing to/from a string. Writing appends bytes
// which can be later read.
class StringBuffer: public libzpaq::Reader, public libzpaq::Writer {
  unsigned char* p;  // allocated memory, not NUL terminated, may be NULL
  size_t al;         // number of bytes allocated, 0 iff p is NULL
  size_t wpos;       // index of next byte to write, wpos <= al
  size_t rpos;       // index of next byte to read, rpos < wpos or return EOF.
  size_t limit;      // max size, default = -1
  const size_t init; // initial size on first use after reset

  // Increase capacity to a without changing size
  void reserve(size_t a) {
    assert(!al==!p);
    if (a<=al) return;
    unsigned char* q=0;
    if (a>0) q=(unsigned char*)(p ? realloc(p, a) : malloc(a));
    if (a>0 && !q) {
      fprintf(stderr, "StringBuffer realloc %1.0f to %1.0f at %p failed\n",
          double(al), double(a), p);
      error("Out of memory");
    }
    p=q;
    al=a;
  }

  // Enlarge al to make room to write at least n bytes.
  void lengthen(unsigned n) {
    assert(wpos<=al);
    if (wpos+n>limit) error("StringBuffer overflow");
    if (wpos+n<=al) return;
    size_t a=al;
    while (wpos+n>=a) a=a*2+init;
    reserve(a);
  }

  // No assignment or copy
  void operator=(const StringBuffer&);
  StringBuffer(const StringBuffer&);

public:

  // Direct access to data
  unsigned char* data() {assert(p || wpos==0); return p;}

  // Allocate no memory initially
  StringBuffer(size_t n=0):
      p(0), al(0), wpos(0), rpos(0), limit(size_t(-1)), init(n>128?n:128) {}

  // Set output limit
  void setLimit(size_t n) {limit=n;}

  // Free memory
  ~StringBuffer() {if (p) free(p);}

  // Return number of bytes written.
  size_t size() const {return wpos;}

  // Return number of bytes left to read
  size_t remaining() const {return wpos-rpos;}

  // Reset size to 0.
  void reset() {
    if (p) free(p);
    p=0;
    al=rpos=wpos=0;
  }

  // Write a single byte.
  void put(int c) {  // write 1 byte
    lengthen(1);
    assert(p);
    assert(wpos<al);
    p[wpos++]=c;
    assert(wpos<=al);
  }

  // Write buf[0..n-1]
  void write(const char* buf, int n) {
    assert(buf);
    if (n<1) return;
    lengthen(n);
    assert(p);
    assert(wpos+n<=al);
    memcpy(p+wpos, buf, n);
    wpos+=n;
  }

  // Read a single byte. Return EOF (-1) and reset at end of string.
  int get() {
    assert(rpos<=wpos);
    assert(rpos==wpos || p);
    return rpos<wpos ? p[rpos++] : (reset(),-1);
  }

  // Read up to n bytes into buf[0..] or fewer if EOF is first.
  // Return the number of bytes actually read.
  int read(char* buf, int n) {
    assert(rpos<=wpos);
    assert(wpos<=al);
    assert(!al==!p);
    if (rpos+n>wpos) n=wpos-rpos;
    if (n>0) memcpy(buf, p+rpos, n);
    rpos+=n;
    return n;
  }

  // Return the entire string as a read-only array.
  const char* c_str() const {return (const char*)p;}

  // Truncate the string to size i.
  void resize(size_t i) {wpos=i;}

  // Write a string.
  void operator+=(const string& t) {write(t.data(), t.size());}

  // Swap efficiently (init is not swapped)
  void swap(StringBuffer& s) {
    std::swap(p, s.p);
    std::swap(al, s.al);
    std::swap(wpos, s.wpos);
    std::swap(rpos, s.rpos);
    std::swap(limit, s.limit);
  }
};

////////////////////////////// misc ///////////////////////////////////

// In Windows convert upper case to lower case.
inline int tolowerW(int c) {
#ifndef unix
  if (c>='A' && c<='Z') return c-'A'+'a';
#endif
  return c;
}

// Return true if strings a == b or a+"/" is a prefix of b
// or a ends in "/" and is a prefix of b.
// Match ? in a to any char in b.
// Match * in a to any string in b.
// In Windows, not case sensitive.
bool ispath(const char* a, const char* b) {
  for (; *a; ++a, ++b) {
    const int ca=tolowerW(*a);
    const int cb=tolowerW(*b);
    if (ca=='*') {
      while (true) {
        if (ispath(a+1, b)) return true;
        if (!*b) return false;
        ++b;
      }
    }
    else if (ca=='?') {
      if (*b==0) return false;
    }
    else if (ca==cb && ca=='/' && a[1]==0)
      return true;
    else if (ca!=cb)
      return false;
  }
  return *b==0 || *b=='/';
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

// Convert decimal, octal (leading o) or hex (leading x) string to int
int ntoi(const char* s) {
  int n=0, base=10, sign=1;
  for (; *s; ++s) {
    int c=*s;
    if (isupper(c)) c=tolower(c);
    if (!n && c=='x') base=16;
    else if (!n && c=='o') base=8;
    else if (!n && c=='-') sign=-1;
    else if (c>='0' && c<='9') n=n*base+c-'0';
    else if (base==16 && c>='a' && c<='f') n=n*base+c-'a'+10;
    else break;
  }
  return n*sign;
}

/////////////////////////// read_password ////////////////////////////

// Read a password from argv[i+1..argc-1] or from the console without
// echo (repeats times) if this sequence is empty. repeats can be 1 or 2.
// If 2, require the same password to be entered twice in a row.
// Advance i by the number of words in the password on the command
// line, which will be 0 if the user is prompted.
// Write the SHA-256 hash of the password in hash[0..31].
// Return the length of the original password.

int read_password(char* hash, int repeats,
                 int argc, const char** argv, int& i) {
  assert(repeats==1 || repeats==2);
  libzpaq::SHA256 sha256;
  int result=0;

  // Read password from argv[i+1..argc-1]
  if (i<argc-1 && argv[i+1][0]!='-') {
    while (true) {  // read multi-word password with spaces between args
      ++i;
      for (const char* p=argv[i]; p && *p; ++p) sha256.put(*p);
      if (i<argc-1 && argv[i+1][0]!='-') sha256.put(' ');
      else break;
    }
    result=sha256.usize();
    memcpy(hash, sha256.result(), 32);
    return result;
  }

  // Otherwise prompt user
  char oldhash[32]={0};
  if (repeats==2)
    fprintf(stderr, "Enter new password twice:\n");
  else {
    fprintf(stderr, "Password: ");
    fflush(stderr);
  }
  do {

  // Read password without echo to end of line
#if unix
    struct termios term, oldterm;
    FILE* in=fopen("/dev/tty", "r");
    if (!in) in=stdin;
    tcgetattr(fileno(in), &oldterm);
    memcpy(&term, &oldterm, sizeof(term));
    term.c_lflag&=~ECHO;
    term.c_lflag|=ECHONL;
    tcsetattr(fileno(in), TCSANOW, &term);
    char buf[256];
    if (!fgets(buf, 250, in)) return 0;
    tcsetattr(fileno(in), TCSANOW, &oldterm);
    if (in!=stdin) fclose(in);
    for (unsigned i=0; i<250 && buf[i]!=10 && buf[i]!=13 && buf[i]!=0; ++i)
      sha256.put(buf[i]);
#else
    HANDLE h=GetStdHandle(STD_INPUT_HANDLE);
    DWORD mode=0, n=0;
    wchar_t buf[256];
    if (h!=INVALID_HANDLE_VALUE
        && GetConsoleMode(h, &mode)
        && SetConsoleMode(h, mode&~ENABLE_ECHO_INPUT)
        && ReadConsole(h, buf, 250, &n, NULL)) {
      SetConsoleMode(h, mode);
      fprintf(stderr, "\n");
      for (unsigned i=0; i<n && i<250 && buf[i]!=10 && buf[i]!=13; ++i)
        sha256.put(buf[i]);
    }
    else {
      fprintf(stderr, "Windows error %d\n", int(GetLastError()));
      error("Read password failed");
    }
#endif
    result=sha256.usize();
    memcpy(oldhash, hash, 32);
    memcpy(hash, sha256.result(), 32);
    memset(buf, 0, sizeof(buf));  // clear sensitive data
  }
  while (repeats==2 && memcmp(oldhash, hash, 32));
  return result;
}

/////////////////////////////// Jidac /////////////////////////////////

// A Jidac object represents an archive contents: a list of file
// fragments with hash, size, and archive offset, and a list of
// files with date, attributes, and list of fragment pointers.
// Methods add to, extract from, compare, and list the archive.

// enum for HT::csize and version
static const int64_t EXTRACTED= 0x7FFFFFFFFFFFFFFELL;  // decompressed?
static const int64_t HT_BAD=   -0x7FFFFFFFFFFFFFFALL;  // no such frag
static const int64_t DEFAULT_VERSION=99999999999999LL; // unless -until

// fragment hash table entry
struct HT {
  unsigned char sha1[20];  // fragment hash
  int usize;      // uncompressed size, -1 if unknown
  int64_t csize;  // if >=0 then block offset else -fragment number
  HT(const char* s=0, int u=-1, int64_t c=HT_BAD) {
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
  double csize;          // approximate compressed size
  vector<unsigned> ptr;  // list of fragment indexes to HT
  int version;           // which transaction was it added?
  DTV(): date(0), size(0), attr(0), csize(0), version(0) {}
};

// filename entry
struct DT {
  int64_t edate;         // date of external file, 0=not found
  int64_t esize;         // size of external file
  int64_t eattr;         // external file attributes ('u' or 'w' in low byte)
  uint64_t sortkey;      // determines sort order for compression
  vector<unsigned> eptr; // fragment list of external file to add
  vector<DTV> dtv;       // list of versions
  int written;           // 0..ptr.size() = fragments output. -1=ignore
  DT(): edate(0), esize(0), eattr(0), sortkey(0), written(-1) {}
};
typedef map<string, DT> DTMap;

// Version info
struct VER {
  int64_t date;          // 0 if not JIDAC
  int64_t usize;         // uncompressed size of files
  int64_t offset;        // start of transaction
  int64_t csize;         // size of compressed data, -1 = no index
  int updates;           // file updates
  int deletes;           // file deletions
  unsigned firstFragment;// first fragment ID
  VER() {memset(this, 0, sizeof(*this));}
};

class CompressJob;

// Do everything
class Jidac {
public:
  int doCommand(int argc, const char** argv);
  friend ThreadReturn decompressThread(void* arg);
  friend ThreadReturn testThread(void* arg);
  friend struct ExtractJob;
private:

  // Command line arguments
  string command;           // "-add", "-extract", "-list"
  string archive;           // archive name
  vector<string> files;     // filename args
  bool all;                 // -all option
  bool duplicates;          // -duplicates option
  bool force;               // -force option
  int fragment;             // Log average fragment size in KB, default 6
  char password_string[32]; // hash of -key argument
  const char* password;     // points to password_string or NULL
  string method;            // 0..9, default "1"
  char new_password_string[32];  // hash of encrypt -to arg
  const char* new_password; // points to new_password_string or NULL
  bool noattributes;        // -noattributes option
  bool nodelete;            // -nodelete option
  vector<string> notfiles;  // list of prefixes to exclude
  string compare;           // -not =...
  vector<string> onlyfiles; // list of prefixes to include
  int since;                // First version to -list
  int summary;              // Arg to -summary
  int threads;              // default is number of cores
  vector<string> tofiles;   // -to option
  string archive2;          // -to archive2.zpaq
  int64_t date;             // now as decimal YYYYMMDDHHMMSS (UT)
  int64_t version;          // version number or 14 digit date
  int64_t volume;           // -volume option

  // Archive state
  int64_t dhsize;           // total size of D blocks according to H blocks
  int64_t dcsize;           // total size of D blocks according to C blocks
  vector<HT> ht;            // list of fragments
  DTMap dt;                 // set of files
  vector<VER> ver;          // version info

  // Commands
  int add();                // add, return 1 if error else 0
  int extract();            // extract, return 1 if error else 0
  int list();               // list, return 1 if compare = finds a mismatch
  int test();               // test, return 1 if error else 0
  void purge();             // extract -to out.zpaq
  void usage();             // help

  // Support functions
  string rename(string name);           // rename from -to
  int64_t read_archive(int *errors=0, const char* arc=0);  // read arc
  bool isselected(const char* filename);// by files, -only, -not
  void read_args();                     // mark matched files
  void scandir(string filename, bool recurse=true);  // scan dirs to dt
  void addfile(string filename, int64_t edate, int64_t esize,
               int64_t eattr);          // add external file to dt
  void list_versions(int64_t csize);    // print ver. csize=archive size
  bool equal(DTMap::const_iterator p, const char* filename, int vi=-1);
             // compare file contents with vi'th update of p
};

// Print help message
void Jidac::usage() {
  fprintf(con, 
"zpaq archiver for incremental backups with rollback capability.\n"
"(C) 2009-2014, Dell Inc. Free under GPL v3. http://mattmahoney.net/zpaq\n"
#ifndef NDEBUG
"DEBUG version\n"
#endif
"\n"
"Usage: zpaq add|extract|list|test archive[.zpaq] [files]... -options...\n"
"Files... may be directory trees. Default is the whole archive.\n"
"* and ? in archive match numbers or digits in a multi-part archive.\n"
"Part 0 is the index. If present, no other parts are needed to add or list.\n"
"Commands (a,x,l,t) and options may be abbreviated if not ambiguous.\n"
"  -key [password] AES-256 encrypted archive [prompt without echo].\n"
"  -noattributes   Ignore/don't save file attributes or permissions.\n"
"  -not files...   Exclude. * and ? match any string or char.\n"
"  -only files...  Include only matches (default: *).\n"
"  -quiet [d|N[kmg]]  Hide output [d=detailed or hide files < N KB,MB,GB].\n"
"  -threads N      Use N threads (default: %d).\n"
"  -until N        Roll back archive to N'th update or -N from end.\n"
"  -until %s  Set date, roll back (UT, default time: 235959).\n"
"add options. archive can be \"\" to test compression with no output:\n"
"  -force          Add files even if the date is unchanged.\n"
"  -nodelete       Do not mark unmatched files as deleted.\n"
"  -fragile        Do not save checksums or recovery info.\n"
"  -fragment N     Set dedupe fragment size to 2^N KiB (default: 6).\n"
"  -method 0..5[B] Compress faster..better in 2^B MiB blocks (default: 14).\n"
"          {xsi}B[,N2]...[{ciawmst|fF}[N1[,N2]...]]...  Advanced:\n"
"  x=journaling (default). s=streaming (no dedupe). i=index (no data).\n"
"    N2: 0=no pre/post. 1,2=packed,byte LZ77. 3=BWT. 4..7=0..3 with E8E9.\n"
"    N3=LZ77 min match. N4=longer match to try first (0=none). 2^N5=search\n"
"    depth. 2^N6=hash table size (N6=B+21: suffix array). N7=lookahead.\n"
"    Context modeling defaults shown below:\n"
"  c0,0,0: context model. N1: 0=ICM, 1..256=CM max count. 1000..1256 halves\n"
"    memory. N2: 1..255=count mod N2, 1000..1255=count from N2-1000 byte.\n"
"    N3...: order 0... context masks (0..255). 256..511=mask+byte LZ77\n"
"    parse state, >1000: gap of N3-1000 zeros.\n"
"  i: ISSE chain. N1=context order. N2...=order increment.\n"
"  a24,0,0: MATCH: N1=hash multiplier. N2=halve buffer. N3=halve hash tab.\n"
"  w1,65,26,223,20,0: Order 0..N1-1 word ISSE chain. A word is bytes\n"
"    N2..N2+N3-1 ANDed with N4, hash mulitpiler N5, memory halved by N6.\n"
"  m8,24: MIX all previous models, N1 context bits, learning rate N2.\n"
"  s8,32,255: SSE last model. N1 context bits, count range N2..N3.\n"
"  t8,24: MIX2 last 2 models, N1 context bits, learning rate N2.\n"
"  fF: use ZPAQL model in file F.cfg (see docs).\n"
"extract options:\n"
"  -fragile        Skip fragment SHA-1 verification.\n"
"  -force          Overwrite existing files (default: skip).\n"
"  -to out...      Extract files... to out... or all to out/all.\n"
"      out.zpaq [out2...]  Extract to new archive [rename files to out2].\n"
"  -newkey [password]  Set out.zpaq password. (default: no encryption).\n"
"  -all            Copy all versions of all files to out.zpaq.\n"
"list options:\n"
"  -all            List all versions (default: latest only).\n"
"  -duplicates     List by size and label identical files with =\n"
"  -not =[=#/?]... Compare [omit =equal, #different, /not found, ?unknown].\n"
"  -to other.zpaq [names...]  Compare 2 archives [files with names].\n"
"  -since N        List from version N or -N from end (default: 1).\n"
"  -summary [N]    List top N (20) files and types and a version table.\n"
"test options (verifies whole archive):\n"
"  -fragile        Allow testing of fragile archives without errors.\n",
  threads, dateToString(date).c_str());
  exit(1);
}

// Rename name using tofiles[]
string Jidac::rename(string name) {
  if (tofiles.size()==0) return name;  // same name
  if (files.size()==0) {  // append prefix tofiles[0]
    int n=name.size();
    if (n>1 && name[1]==':') {  // remove : from drive letter
      if (n>2 && name[2]=='/') name=name.substr(0, 1)+name.substr(2), --n;
      else name[1]='/';
    }
    if (n>0 && name[0]!='/') name="/"+name;  // insert / if needed
    return tofiles[0]+name;
  }
  else {  // replace prefix files[i] with tofiles[i]
    const int n=name.size();
    for (int i=0; i<size(files) && i<size(tofiles); ++i) {
      const int fn=files[i].size();
      if (fn<=n && files[i]==name.substr(0, fn))
        return tofiles[i]+name.substr(fn);
    }
  }
  return name;
}

// Expand an abbreviated option (with or without a leading "-")
// or report error if not exactly 1 match. Always expand commands.
string expandOption(const char* opt) {
  const char* opts[]={
    "add","extract","list","test",
    "all","duplicates","force","fragile","fragment","key","method",
    "newkey","noattributes","nodelete","not","only","quiet","since",
    "summary","to","threads","until","volume",0};
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
      if (i<4 && result!="") return result;
    }
  }
  if (result=="")
    fprintf(stderr, "No such option: %s\n", opt), exit(1);
  return result;
}

// Parse the command line. Return 1 if error else 0.
int Jidac::doCommand(int argc, const char** argv) {

  // initialize options to default values
  command="";
  all=false;
  duplicates=false;
  force=false;
  fragile=false;  // global
  fragment=6;
  password=0;  // no password
  method="";  // 0..5
  new_password=0;  // no new password
  noattributes=false;
  nodelete=false;
  compare="";
  quiet=-1; // global
  since=0;
  summary=0;
  threads=0; // 0 = auto-detect
  version=DEFAULT_VERSION;
  date=0;
  volume=(1ULL<<63)-1;  // max int64_t

  // Init archive state
  ht.resize(1);  // element 0 not used
  ver.resize(1); // version 0
  dhsize=dcsize=0;

  // Get date
  time_t now=time(NULL);
  tm* t=gmtime(&now);
  date=(t->tm_year+1900)*10000000000LL+(t->tm_mon+1)*100000000LL
      +t->tm_mday*1000000+t->tm_hour*10000+t->tm_min*100+t->tm_sec;

  // Get optional options
  for (int i=1; i<argc; ++i) {
    const string opt=expandOption(argv[i]);  // read command
    if ((opt=="-add" || opt=="-extract" || opt=="-list" || opt=="-test")
        && i<argc-1 && argv[i+1][0]!='-' && command=="") {
      archive=argv[++i];
      if (archive!="" &&   // Add .zpaq extension
          (size(archive)<5 || archive.substr(archive.size()-5)!=".zpaq"))
         archive+=".zpaq";
      command=opt;
      while (++i<argc && argv[i][0]!='-')
        files.push_back(argv[i]);
      --i;
    }
    else if (opt=="-all") all=true;
    else if (opt=="-duplicates") duplicates=true;
    else if (opt=="-force") force=true;
    else if (opt=="-fragile") fragile=true;
    else if (opt=="-fragment" && i<argc-1) fragment=atoi(argv[++i]);
    else if (opt=="-key") {
      if (read_password(password_string, 2-exists(archive, 1),
          argc, argv, i))
        password=password_string;
    }
    else if (opt=="-method" && i<argc-1) method=argv[++i];
    else if (opt=="-newkey") {
      if (read_password(new_password_string, 2, argc, argv, i))
        new_password=new_password_string;
    }
    else if (opt=="-noattributes") noattributes=true;
    else if (opt=="-nodelete") nodelete=true;
    else if (opt=="-not") {  // read notfiles
      while (++i<argc && argv[i][0]!='-') {
        if (argv[i][0]=='=') compare=argv[i];
        notfiles.push_back(argv[i]);
      }
      --i;
    }
    else if (opt=="-only") {  // read onlyfiles
      while (++i<argc && argv[i][0]!='-')
        onlyfiles.push_back(argv[i]);
      --i;
    }
    else if (opt=="-quiet") {  // read number followed by k, m, g
      quiet=MAX_QUIET;
      if (i<argc-1 && argv[i+1][0]!='-') {
        quiet=0;
        for (const char* p=argv[++i]; *p; ++p) {
          int c=tolower(*p);
          if (isdigit(c)) quiet=quiet*10+c-'0';
          else if (c=='k') quiet*=1000;
          else if (c=='m') quiet*=1000000;
          else if (c=='g') quiet*=1000000000;
          else if (c=='d') quiet=-2;
          else break;
        }
      }
    }
    else if (opt=="-since" && i<argc-1) since=atoi(argv[++i]);
    else if (opt=="-summary") {
      summary=20;
      if (i<argc-1 && isdigit(argv[i+1][0])) summary=atoi(argv[++i]);
    }
    else if (opt=="-threads" && i<argc-1) {
      threads=atoi(argv[++i]);
      if (threads<1) threads=1;
    }
    else if (opt=="-to") {  // read tofiles
      while (++i<argc && argv[i][0]!='-') {
        if (archive2=="" && strlen(argv[i])>=5
            && strcmp(argv[i]+strlen(argv[i])-5, ".zpaq")==0)
          archive2=argv[i];
        else
          tofiles.push_back(argv[i]);
      }
      --i;
    }
    else if (opt=="-until" && i+1<argc) {  // read date

      // Read digits from multiple args and fill in leading zeros
      version=0;
      int digits=0;
      if (argv[i+1][0]=='-') {  // negative version
        version=atol(argv[i+1]);
        if (version>-1) usage();
        ++i;
      }
      else {  // positive version or date
        while (++i<argc && argv[i][0]!='-') {
          for (int j=0; ; ++j) {
            if (isdigit(argv[i][j])) {
              version=version*10+argv[i][j]-'0';
              ++digits;
            }
            else {
              if (digits==1) version=version/10*100+version%10;
              digits=0;
              if (argv[i][j]==0) break;
            }
          }
        }
        --i;
      }

      // Append default time
      if (version>=19000000LL     && version<=29991231LL)
        version=version*100+23;
      if (version>=1900000000LL   && version<=2999123123LL)
        version=version*100+59;
      if (version>=190000000000LL && version<=299912312359LL)
        version=version*100+59;
      if (version>9999999) {
        if (version<19000101000000LL || version>29991231235959LL) {
          fprintf(stderr,
            "Version date %1.0f must be 19000101000000 to 29991231235959\n",
             double(version));
          exit(1);
        }
        date=version;
      }
    }
    else if (opt=="-volume" && i<argc-1) {
      volume=0;
      for (const char* p=argv[++i]; *p; ++p) {
        int c=tolower(*p);
        if (isdigit(c)) volume=volume*10+c-'0';
        else if (c=='k') volume*=1000;
        else if (c=='m') volume*=1000000;
        else if (c=='g') volume*=1000000000;
        else break;
      }
      fprintf(con, "volume = %1.0f\n", volume+0.0);
      error("volume not implemented");
    }
    else
      usage();
  }

  // Set threads
  if (!threads)
    threads=numberOfProcessors();

  // Set verbosity level
  if ((command=="-add" || command=="-extract") && quiet==-1)
    quiet=MAX_QUIET-1;

  // Test date
  if (now==-1 || date<19000000000000LL || date>30000000000000LL)
    error("date is incorrect, use -until YYYY-MM-DD HH:MM:SS to set");

  // Adjust negative version
  if (version<0) {
    Jidac jidac(*this);
    jidac.version=DEFAULT_VERSION;
    if (!jidac.read_archive())  // not found?
      jidac.read_archive(0, subpart(archive, 0).c_str());  // try remote index
    version+=size(jidac.ver)-1;
  }

  // Suppress output
  if (quiet==MAX_QUIET) {
#ifdef unix
    con=fopen("/dev/null", "wb");
#else
    con=fopen("nul:", "wb");
#endif
    if (!con) error("console redirect failed");
  }

  // Execute command
  fprintf(con, "zpaq v" ZPAQ_VERSION " journaling archiver, compiled "
         __DATE__ "\n");
  if (command=="-add" && files.size()>0) return add();
  else if (command=="-extract") {
    if (archive2!="") purge();
    else return extract();
  }
  else if (command=="-list") return list();
  else if (command=="-test") return test();
  else usage();
  return 0;
}

// Read arc (default: archive) up to -date into ht, dt, ver. Return place to
// append. If errors is not NULL then set it to number of errors found.
int64_t Jidac::read_archive(int *errors, const char* arc) {
  if (errors) *errors=0;
  dcsize=dhsize=0;

  // Open archive or archive.zpaq. If not found then try the index of
  // a multi-part archive.
  if (!arc) arc=archive.c_str();
  Archive in;
  if (!in.open(arc, password)) {
    if (command!="-add") {
      printUTF8(arc, stderr);
      fprintf(stderr, " not found.\n");
      if (errors) ++*errors;
    }
    return 0;
  }
  printUTF8(arc, con);
  if (version==DEFAULT_VERSION) fprintf(con, ": ");
  else fprintf(con, " -until %1.0f: ", version+0.0);
  fflush(con);

  // Test password
  if (password) {
    char s[4]={0};
    const int nr=in.read(s, 4);
    if (nr>0 && memcmp(s, "7kSt", 4) && (memcmp(s, "zPQ", 3) || s[3]<1))
      error("password incorrect");
    in.seek(-nr, SEEK_CUR);
  }

  // Scan archive contents
  string lastfile=arc; // last named file in streaming format
  if (size(lastfile)>5)
    lastfile=lastfile.substr(0, size(lastfile)-5); // drop .zpaq
  int64_t block_offset=32*(password!=0);  // start of last block of any type
  int64_t data_offset=block_offset;    // start of last block of d fragments
  int64_t segment_offset=block_offset; // start of last segment
  bool found_data=false;   // exit if nothing found
  bool first=true;         // first segment in archive?
  enum {NORMAL, ERR, RECOVER} pass=NORMAL;  // recover ht from data blocks?
  StringBuffer os(32832);  // decompressed block
  map<int64_t, double> compressionRatio;  // block offset -> compression ratio

  // Detect archive format and read the filenames, fragment sizes,
  // and hashes. In JIDAC format, these are in the index blocks, allowing
  // data to be skipped. Otherwise the whole archive is scanned to get
  // this information from the segment headers and trailers.
  bool done=false;
  while (!done) {
    try {

      // If there is an error in the h blocks, scan a second time in RECOVER
      // mode to recover the redundant fragment data from the d blocks.
      libzpaq::Decompresser d;
      d.setInput(&in);
      if (d.findBlock())
        found_data=true;
      else if (pass==ERR) {
        segment_offset=block_offset=32*(password!=0);
        in.seek(block_offset, SEEK_SET);
        if (!d.findBlock()) break;
        pass=RECOVER;
        fprintf(con, "Attempting to recover fragment tables...\n");
      }
      else
        break;

      // Read the segments in the current block
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
        if (pass!=NORMAL)
          fprintf(con, "Reading %s %s at %1.0f\n", filename.s.c_str(),
                 comment.s.c_str(), double(block_offset));
        int64_t usize=0;  // read uncompressed size from comment or -1
        int64_t fdate=0;  // read date from filename or -1
        int64_t fattr=0;  // read attributes from comment as wN or uN
        unsigned num=0;   // read fragment ID from filename
        const char* p=comment.s.c_str();
        for (; isdigit(*p); ++p)  // read size
          usize=usize*10+*p-'0';
        if (p==comment.s.c_str()) usize=-1;  // size not found
        for (; *p && fdate<19000000000000LL; ++p)  // read date
          if (isdigit(*p)) fdate=fdate*10+*p-'0';
        if (fdate<19000000000000LL || fdate>=30000000000000LL) fdate=-1;

        // Read the comment attribute wN or uN where N is a number
        int attrchar=0;
        for (; true; ++p) {
          if (*p=='u' || *p=='w') {
            attrchar=*p;
            fattr=0;
          }
          else if (isdigit(*p) && (attrchar=='u' || attrchar=='w'))
            fattr=fattr*10+*p-'0';
          else if (attrchar) {
            fattr=fattr*256+attrchar;
            attrchar=0;
          }
          if (!*p) break;
        }

        // Test for JIDAC format. Filename is jDC<fdate>[cdhi]<num>
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
          for (unsigned i=18; i<filename.s.size() && isdigit(filename.s[i]);
               ++i)
            num=num*10+filename.s[i]-'0';

          // Decompress the block. In recovery mode, only decompress
          // data blocks containing missing HT data.
          os.reset();
          os.setLimit(usize);
          d.setOutput(&os);
          libzpaq::SHA1 sha1;
          d.setSHA1(&sha1);
          if (pass!=RECOVER || (filename.s[17]=='d' && num>0 &&
              num<ht.size() && ht[num].csize==HT_BAD)) {
            d.decompress();
            char sha1result[21]={0};
            d.readSegmentEnd(sha1result);
            if (usize!=int64_t(sha1.usize())) {
              fprintf(stderr, "%s size should be %1.0f, is %1.0f\n",
                      filename.s.c_str(), double(usize),
                      double(sha1.usize()));
              error("incorrect block size");
            }
            if (sha1result[0] && memcmp(sha1result+1, sha1.result(), 20)) {
              fprintf(stderr, "%s checksum error\n", filename.s.c_str());
              error("bad checksum");
            }
          }
          else
            d.readSegmentEnd();

          // Transaction header (type c).
          // If in the future then stop here, else read 8 byte data size
          // from input and jump over it.
          if (filename.s[17]=='c' && fdate>=19000000000000LL
              && fdate<30000000000000LL && pass!=RECOVER) {
            data_offset=in.tell()+1;
            bool isbreak=version<19000000000000LL ? size(ver)>version :
                         version<fdate;
            int64_t jmp=0;
            if (!isbreak && os.size()==8) {  // jump
              const char* s=os.c_str();
              jmp=btol(s);
              if (jmp<0) {
                fprintf(stderr, "Incomplete transaction ignored\n");
                isbreak=true;
              }
              else if (jmp>0) {
                dcsize+=jmp;
                in.seek(jmp, SEEK_CUR);
              }
            }
            if (os.size()!=8) {
              fprintf(stderr, "Bad JIDAC header size: %d\n", size(os));
              isbreak=true;
              if (*errors) ++*errors;
            }
            if (isbreak) {
              done=true;
              break;
            }
            ver.push_back(VER());
            ver.back().firstFragment=size(ht);
            ver.back().offset=block_offset;
            ver.back().date=fdate;
            ver.back().csize=jmp;
          }

          // Fragment table (type h).
          // Contents is bsize[4] (sha1[20] usize[4])... for fragment N...
          // where bsize is the compressed block size.
          // Store in ht[].{sha1,usize}. Set ht[].csize to block offset
          // assuming N in ascending order.
          else if (filename.s[17]=='h' && num>0 && os.size()>=4
                   && pass!=RECOVER) {
            const char* s=os.c_str();
            const unsigned bsize=btoi(s);
            dhsize+=bsize;
            assert(size(ver)>0);
            const unsigned n=(os.size()-4)/24;
            if (ht.size()>num) {
              fprintf(stderr,
                "Unordered fragment tables: expected >= %d found %1.0f\n",
                size(ht), double(num));
              pass=ERR;
            }
            double usum=0;  // total uncompressed size
            for (unsigned i=0; i<n; ++i) {
              while (ht.size()<=num+i) ht.push_back(HT());
              memcpy(ht[num+i].sha1, s, 20);
              s+=20;
              if (ht[num+i].csize!=HT_BAD) error("duplicate fragment ID");
              usum+=ht[num+i].usize=btoi(s);
              ht[num+i].csize=i?-int(i):data_offset;
            }
            if (usum>0) compressionRatio[data_offset]=bsize/usum;
            data_offset+=bsize;
          }

          // Index (type i)
          // Contents is: 0[8] filename 0 (deletion)
          // or:       date[8] filename 0 na[4] attr[na] ni[4] ptr[ni][4]
          // Read into DT
          else if (filename.s[17]=='i' && pass!=RECOVER) {
            const bool islist=command=="-list";
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
                if (noattributes) dtv.attr=0;
                if (s<=end-4) {
                  const unsigned ni=btoi(s);
                  dtv.ptr.resize(ni);
                  for (unsigned i=0; i<ni && s<=end-4; ++i) {  // read ptr
                    const unsigned j=dtv.ptr[i]=btoi(s);
                    if (j<1 || j>=ht.size()+(1<<24))
                      error("bad fragment ID");
                    while (j>=ht.size()) {
                      pass=ERR;
                      ht.push_back(HT());
                    }
                    dtv.size+=ht[j].usize;
                    ver.back().usize+=ht[j].usize;

                    // Estimate compressed size
                    if (islist) {
                      unsigned k=j;
                      if (ht[j].csize<0 && ht[j].csize!=HT_BAD)
                        k+=ht[j].csize;
                      if (k>0 && k<ht.size() && ht[k].csize!=HT_BAD
                          && ht[k].csize>=0)
                        dtv.csize+=compressionRatio[ht[k].csize]*ht[j].usize;
                    }
                  }
                }
              }
            }
          }

          // Recover fragment sizes and hashes from data block
          else if (pass==RECOVER && filename.s[17]=='d' && num>0
                   && num<ht.size()) {
            if (os.size()>=8 && ht[num].csize==HT_BAD) {
              const char* p=os.c_str()+os.size()-8;
              unsigned n=btoi(p);  // first fragment == num or 0
              if (n==0) n=num;
              unsigned f=btoi(p);  // number of fragments
              if (n!=num)
                fprintf(con, "fragments %u-%u were moved to %u-%u\n",
                    n, n+f-1, num, num+f-1);
              n=num;
              if (f && f*4+8<=os.size()) {
                fprintf(con, "Recovering fragments %u-%u at %1.0f\n",
                       n, n+f-1, double(block_offset));
                while (ht.size()<=n+f) ht.push_back(HT());
                p=os.c_str()+os.size()-8-4*f;

                // read fragment sizes into ht[n..n+f-1].usize
                unsigned sum=0;
                for (unsigned i=0; i<f; ++i) {
                  sum+=ht[n+i].usize=btoi(p);
                  ht[n+i].csize=i ? -int(i) : block_offset;
                }

                // Compute hashes
                if (sum+f*4+8==os.size()) {
                  fprintf(con, "Computing hashes for %d bytes\n", sum);
                  libzpaq::SHA1 sha1;
                  p=os.c_str();
                  for (unsigned i=0; i<f; ++i) {
                    for (int j=0; j<ht[n+i].usize; ++j) {
                      assert(p<os.c_str()+os.size());
                      sha1.put(*p++);
                    }
                    memcpy(ht[n+i].sha1, sha1.result(), 20);
                  }
                  assert(p==os.c_str()+sum);
                }
              }
            }

            // Correct bad offsets
            assert(num>0 && num<ht.size());
            if (ht[num].csize!=block_offset) {
              fprintf(con, "Changing block %d offset from %1.0f to %1.0f\n",
                     num, double(ht[num].csize), double(block_offset));
              ht[num].csize=block_offset;
            }
          }

          // Bad JIDAC block
          else if (pass!=RECOVER) {
            fprintf(stderr, "Bad JIDAC block ignored: %s %s\n",
                    filename.s.c_str(), comment.s.c_str());
            if (errors) ++*errors;
          }
        }

        // Streaming format
        else if (pass!=RECOVER) {

          // If previous version does not exist, start a new one
          if (size(ver)==1) {
            if (size(ver)>version) {
              done=true;
              break;
            }
            ver.push_back(VER());
            ver.back().firstFragment=size(ht);
            ver.back().offset=block_offset;
            ver.back().csize=-1;
          }

          char sha1result[21]={0};
          d.readSegmentEnd(sha1result);
          DT& dtr=dt[lastfile];
          if (filename.s.size()>0 || first) {
            dtr.dtv.push_back(DTV());
            dtr.dtv.back().date=fdate;
            dtr.dtv.back().attr=noattributes?0:fattr;
            dtr.dtv.back().version=size(ver)-1;
            ++ver.back().updates;
          }
          assert(dtr.dtv.size()>0);
          dtr.dtv.back().ptr.push_back(size(ht));
          if (usize>=0 && dtr.dtv.back().size>=0) dtr.dtv.back().size+=usize;
          else dtr.dtv.back().size=-1;
          dtr.dtv.back().csize+=in.tell()-segment_offset;
          if (usize>=0) ver.back().usize+=usize;
          ht.push_back(HT(sha1result+1, usize>0x7fffffff ? -1 : usize,
                          segs ? -segs : block_offset));
          assert(size(ver)>0);
        }
        ++segs;
        filename.s="";
        first=false;
        segment_offset=in.tell();
      }  // end while findFilename
      if (!done) segment_offset=block_offset=in.tell();
    }  // end try
    catch (std::exception& e) {
      block_offset=in.tell();
      fprintf(stderr, "Skipping block at %1.0f: %s\n", double(block_offset),
              e.what());
      if (errors) ++*errors;
    }
  }  // end while !done
  if (in.tell()>32*(password!=0) && !found_data)
    error("archive contains no data");
  in.close();

  // Recompute file sizes in recover mode
  if (pass==RECOVER) {
    fprintf(stderr, "Recomputing file sizes\n");
    for (DTMap::iterator p=dt.begin(); p!=dt.end(); ++p) {
      for (unsigned i=0; i<p->second.dtv.size(); ++i) {
        p->second.dtv[i].size=0;
        for (unsigned j=0; j<p->second.dtv[i].ptr.size(); ++j) {
          unsigned k=p->second.dtv[i].ptr[j];
          if (k>0 && k<ht.size())
            p->second.dtv[i].size+=ht[k].usize;
        }
      }
    }
  }
  fprintf(con, "%d versions, %d files, %d fragments, %1.6f MB\n", 
      size(ver)-1, size(dt), size(ht)-1, block_offset*0.000001);
  return block_offset;
}

// Test whether filename and attributes are selected by files, -only, and -not
bool Jidac::isselected(const char* filename) {
  bool matched=true;
  if (files.size()>0) {
    matched=false;
    for (int i=0; i<size(files) && !matched; ++i)
      if (ispath(files[i].c_str(), filename))
        matched=true;
  }
  if (matched && onlyfiles.size()>0) {
    matched=false;
    for (int i=0; i<size(onlyfiles) && !matched; ++i)
      if (ispath(onlyfiles[i].c_str(), filename))
        matched=true;
  }
  for (int i=0; matched && i<size(notfiles); ++i) {
    if (ispath(notfiles[i].c_str(), filename))
      matched=false;
  }
  return matched;
}

// Mark files in dt with written=0 selected by files, onlyfiles, and notfiles 
void Jidac::read_args() {
  for (DTMap::iterator p=dt.begin(); p!=dt.end(); ++p) {
    if (p->second.dtv.size()<1) {
      fprintf(stderr, "Invalid index entry: %s\n", p->first.c_str());
      error("corrupted index");
    }
    if (isselected(p->first.c_str())
        && p->second.dtv.size() 
        && (all || p->second.dtv.back().date))
      p->second.written=0;
  }
}

// Return the part of fn up to the last slash
string path(const string& fn) {
  int n=0;
  for (int i=0; fn[i]; ++i)
    if (fn[i]=='/' || fn[i]=='\\') n=i+1;
  return fn.substr(0, n);
}

// Insert external filename (UTF-8 with "/") into dt if selected
// by files, onlyfiles, and notfiles. If filename
// is a directory and recurse is true then also insert its contents.
// In Windows, filename might have wildcards like "file.*" or "dir/*"
void Jidac::scandir(string filename, bool recurse) {

  // Don't scan diretories excluded by -not
  for (int i=0; i<size(notfiles); ++i)
    if (ispath(notfiles[i].c_str(), filename.c_str()))
      return;

#ifdef unix

  // Add regular files and directories
  while (filename.size()>1 && filename[filename.size()-1]=='/')
    filename=filename.substr(0, filename.size()-1);  // remove trailing /
  struct stat sb;
  if (!lstat(filename.c_str(), &sb)) {
    if (S_ISREG(sb.st_mode))
      addfile(filename, decimal_time(sb.st_mtime), sb.st_size,
              'u'+(sb.st_mode<<8));

    // Traverse directory
    if (S_ISDIR(sb.st_mode)) {
      addfile(filename=="/" ? "/" : filename+"/", decimal_time(sb.st_mtime),
              0, 'u'+(sb.st_mode<<8));
      if (recurse) {
        DIR* dirp=opendir(filename.c_str());
        if (dirp) {
          for (dirent* dp=readdir(dirp); dp; dp=readdir(dirp)) {
            if (strcmp(".", dp->d_name) && strcmp("..", dp->d_name)) {
              string s=filename;
              if (s!="/") s+="/";
              s+=dp->d_name;
              scandir(s);
            }
          }
          closedir(dirp);
        }
        else
          perror(filename.c_str());
      }
    }
  }
  else if (recurse || errno!=ENOENT)
    perror(filename.c_str());

#else  // Windows: expand wildcards in filename

  // Expand wildcards
  WIN32_FIND_DATA ffd;
  string t=filename;
  if (t.size()>0 && t[t.size()-1]=='/') {
    if (recurse) t+="*";
    else filename=t=t.substr(0, t.size()-1);
  }
  HANDLE h=FindFirstFile(utow(t.c_str(), true).c_str(), &ffd);
  if (h==INVALID_HANDLE_VALUE && (recurse ||
      (GetLastError()!=ERROR_FILE_NOT_FOUND &&
       GetLastError()!=ERROR_PATH_NOT_FOUND)))
    winError(t.c_str());
  while (h!=INVALID_HANDLE_VALUE) {

    // For each file, get name, date, size, attributes
    SYSTEMTIME st;
    int64_t edate=0;
    if (FileTimeToSystemTime(&ffd.ftLastWriteTime, &st))
      edate=st.wYear*10000000000LL+st.wMonth*100000000LL+st.wDay*1000000
            +st.wHour*10000+st.wMinute*100+st.wSecond;
    const int64_t esize=ffd.nFileSizeLow+(int64_t(ffd.nFileSizeHigh)<<32);
    const int64_t eattr='w'+(int64_t(ffd.dwFileAttributes)<<8);

    // Ignore links, the names "." and ".." or any unselected file
    t=wtou(ffd.cFileName);
    if (ffd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT
        || t=="." || t=="..") edate=0;  // don't add
    string fn=path(filename)+t;

    // Save directory names with a trailing / and scan their contents
    // Otherwise, save plain files
    if (edate) {
      if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) fn+="/";
      addfile(fn, edate, esize, eattr);
      if (recurse && (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
        fn+="*";
        scandir(fn);
      }
    }
    if (!FindNextFile(h, &ffd)) {
      if (GetLastError()!=ERROR_NO_MORE_FILES) winError(fn.c_str());
      break;
    }
  }
  FindClose(h);
#endif
}

// Add external file and its date, size, and attributes to dt
void Jidac::addfile(string filename, int64_t edate,
                    int64_t esize, int64_t eattr) {
  if (!isselected(filename.c_str())) return;
  DT& d=dt[filename];
  d.edate=edate;
  d.esize=esize;
  d.eattr=noattributes?0:eattr;
  d.written=0;
}

////////////////////////// divsufsort ///////////////////////////////

/*
 * divsufsort.c for libdivsufsort-lite
 * Copyright (c) 2003-2008 Yuta Mori All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

/*- Constants -*/
#define INLINE __inline
#if defined(ALPHABET_SIZE) && (ALPHABET_SIZE < 1)
# undef ALPHABET_SIZE
#endif
#if !defined(ALPHABET_SIZE)
# define ALPHABET_SIZE (256)
#endif
#define BUCKET_A_SIZE (ALPHABET_SIZE)
#define BUCKET_B_SIZE (ALPHABET_SIZE * ALPHABET_SIZE)
#if defined(SS_INSERTIONSORT_THRESHOLD)
# if SS_INSERTIONSORT_THRESHOLD < 1
#  undef SS_INSERTIONSORT_THRESHOLD
#  define SS_INSERTIONSORT_THRESHOLD (1)
# endif
#else
# define SS_INSERTIONSORT_THRESHOLD (8)
#endif
#if defined(SS_BLOCKSIZE)
# if SS_BLOCKSIZE < 0
#  undef SS_BLOCKSIZE
#  define SS_BLOCKSIZE (0)
# elif 32768 <= SS_BLOCKSIZE
#  undef SS_BLOCKSIZE
#  define SS_BLOCKSIZE (32767)
# endif
#else
# define SS_BLOCKSIZE (1024)
#endif
/* minstacksize = log(SS_BLOCKSIZE) / log(3) * 2 */
#if SS_BLOCKSIZE == 0
# define SS_MISORT_STACKSIZE (96)
#elif SS_BLOCKSIZE <= 4096
# define SS_MISORT_STACKSIZE (16)
#else
# define SS_MISORT_STACKSIZE (24)
#endif
#define SS_SMERGE_STACKSIZE (32)
#define TR_INSERTIONSORT_THRESHOLD (8)
#define TR_STACKSIZE (64)


/*- Macros -*/
#ifndef SWAP
# define SWAP(_a, _b) do { t = (_a); (_a) = (_b); (_b) = t; } while(0)
#endif /* SWAP */
#ifndef MIN
# define MIN(_a, _b) (((_a) < (_b)) ? (_a) : (_b))
#endif /* MIN */
#ifndef MAX
# define MAX(_a, _b) (((_a) > (_b)) ? (_a) : (_b))
#endif /* MAX */
#define STACK_PUSH(_a, _b, _c, _d)\
  do {\
    assert(ssize < STACK_SIZE);\
    stack[ssize].a = (_a), stack[ssize].b = (_b),\
    stack[ssize].c = (_c), stack[ssize++].d = (_d);\
  } while(0)
#define STACK_PUSH5(_a, _b, _c, _d, _e)\
  do {\
    assert(ssize < STACK_SIZE);\
    stack[ssize].a = (_a), stack[ssize].b = (_b),\
    stack[ssize].c = (_c), stack[ssize].d = (_d), stack[ssize++].e = (_e);\
  } while(0)
#define STACK_POP(_a, _b, _c, _d)\
  do {\
    assert(0 <= ssize);\
    if(ssize == 0) { return; }\
    (_a) = stack[--ssize].a, (_b) = stack[ssize].b,\
    (_c) = stack[ssize].c, (_d) = stack[ssize].d;\
  } while(0)
#define STACK_POP5(_a, _b, _c, _d, _e)\
  do {\
    assert(0 <= ssize);\
    if(ssize == 0) { return; }\
    (_a) = stack[--ssize].a, (_b) = stack[ssize].b,\
    (_c) = stack[ssize].c, (_d) = stack[ssize].d, (_e) = stack[ssize].e;\
  } while(0)
#define BUCKET_A(_c0) bucket_A[(_c0)]
#if ALPHABET_SIZE == 256
#define BUCKET_B(_c0, _c1) (bucket_B[((_c1) << 8) | (_c0)])
#define BUCKET_BSTAR(_c0, _c1) (bucket_B[((_c0) << 8) | (_c1)])
#else
#define BUCKET_B(_c0, _c1) (bucket_B[(_c1) * ALPHABET_SIZE + (_c0)])
#define BUCKET_BSTAR(_c0, _c1) (bucket_B[(_c0) * ALPHABET_SIZE + (_c1)])
#endif


/*- Private Functions -*/

static const int lg_table[256]= {
 -1,0,1,1,2,2,2,2,3,3,3,3,3,3,3,3,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,
  5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,
  6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,
  6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,
  7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
  7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
  7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
  7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7
};

#if (SS_BLOCKSIZE == 0) || (SS_INSERTIONSORT_THRESHOLD < SS_BLOCKSIZE)

static INLINE
int
ss_ilg(int n) {
#if SS_BLOCKSIZE == 0
  return (n & 0xffff0000) ?
          ((n & 0xff000000) ?
            24 + lg_table[(n >> 24) & 0xff] :
            16 + lg_table[(n >> 16) & 0xff]) :
          ((n & 0x0000ff00) ?
             8 + lg_table[(n >>  8) & 0xff] :
             0 + lg_table[(n >>  0) & 0xff]);
#elif SS_BLOCKSIZE < 256
  return lg_table[n];
#else
  return (n & 0xff00) ?
          8 + lg_table[(n >> 8) & 0xff] :
          0 + lg_table[(n >> 0) & 0xff];
#endif
}

#endif /* (SS_BLOCKSIZE == 0) || (SS_INSERTIONSORT_THRESHOLD < SS_BLOCKSIZE) */

#if SS_BLOCKSIZE != 0

static const int sqq_table[256] = {
  0,  16,  22,  27,  32,  35,  39,  42,  45,  48,  50,  53,  55,  57,  59,  61,
 64,  65,  67,  69,  71,  73,  75,  76,  78,  80,  81,  83,  84,  86,  87,  89,
 90,  91,  93,  94,  96,  97,  98,  99, 101, 102, 103, 104, 106, 107, 108, 109,
110, 112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126,
128, 128, 129, 130, 131, 132, 133, 134, 135, 136, 137, 138, 139, 140, 141, 142,
143, 144, 144, 145, 146, 147, 148, 149, 150, 150, 151, 152, 153, 154, 155, 155,
156, 157, 158, 159, 160, 160, 161, 162, 163, 163, 164, 165, 166, 167, 167, 168,
169, 170, 170, 171, 172, 173, 173, 174, 175, 176, 176, 177, 178, 178, 179, 180,
181, 181, 182, 183, 183, 184, 185, 185, 186, 187, 187, 188, 189, 189, 190, 191,
192, 192, 193, 193, 194, 195, 195, 196, 197, 197, 198, 199, 199, 200, 201, 201,
202, 203, 203, 204, 204, 205, 206, 206, 207, 208, 208, 209, 209, 210, 211, 211,
212, 212, 213, 214, 214, 215, 215, 216, 217, 217, 218, 218, 219, 219, 220, 221,
221, 222, 222, 223, 224, 224, 225, 225, 226, 226, 227, 227, 228, 229, 229, 230,
230, 231, 231, 232, 232, 233, 234, 234, 235, 235, 236, 236, 237, 237, 238, 238,
239, 240, 240, 241, 241, 242, 242, 243, 243, 244, 244, 245, 245, 246, 246, 247,
247, 248, 248, 249, 249, 250, 250, 251, 251, 252, 252, 253, 253, 254, 254, 255
};

static INLINE
int
ss_isqrt(int x) {
  int y, e;

  if(x >= (SS_BLOCKSIZE * SS_BLOCKSIZE)) { return SS_BLOCKSIZE; }
  e = (x & 0xffff0000) ?
        ((x & 0xff000000) ?
          24 + lg_table[(x >> 24) & 0xff] :
          16 + lg_table[(x >> 16) & 0xff]) :
        ((x & 0x0000ff00) ?
           8 + lg_table[(x >>  8) & 0xff] :
           0 + lg_table[(x >>  0) & 0xff]);

  if(e >= 16) {
    y = sqq_table[x >> ((e - 6) - (e & 1))] << ((e >> 1) - 7);
    if(e >= 24) { y = (y + 1 + x / y) >> 1; }
    y = (y + 1 + x / y) >> 1;
  } else if(e >= 8) {
    y = (sqq_table[x >> ((e - 6) - (e & 1))] >> (7 - (e >> 1))) + 1;
  } else {
    return sqq_table[x] >> 4;
  }

  return (x < (y * y)) ? y - 1 : y;
}

#endif /* SS_BLOCKSIZE != 0 */


/*---------------------------------------------------------------------------*/

/* Compares two suffixes. */
static INLINE
int
ss_compare(const unsigned char *T,
           const int *p1, const int *p2,
           int depth) {
  const unsigned char *U1, *U2, *U1n, *U2n;

  for(U1 = T + depth + *p1,
      U2 = T + depth + *p2,
      U1n = T + *(p1 + 1) + 2,
      U2n = T + *(p2 + 1) + 2;
      (U1 < U1n) && (U2 < U2n) && (*U1 == *U2);
      ++U1, ++U2) {
  }

  return U1 < U1n ?
        (U2 < U2n ? *U1 - *U2 : 1) :
        (U2 < U2n ? -1 : 0);
}


/*---------------------------------------------------------------------------*/

#if (SS_BLOCKSIZE != 1) && (SS_INSERTIONSORT_THRESHOLD != 1)

/* Insertionsort for small size groups */
static
void
ss_insertionsort(const unsigned char *T, const int *PA,
                 int *first, int *last, int depth) {
  int *i, *j;
  int t;
  int r;

  for(i = last - 2; first <= i; --i) {
    for(t = *i, j = i + 1; 0 < (r = ss_compare(T, PA + t, PA + *j, depth));) {
      do { *(j - 1) = *j; } while((++j < last) && (*j < 0));
      if(last <= j) { break; }
    }
    if(r == 0) { *j = ~*j; }
    *(j - 1) = t;
  }
}

#endif /* (SS_BLOCKSIZE != 1) && (SS_INSERTIONSORT_THRESHOLD != 1) */


/*---------------------------------------------------------------------------*/

#if (SS_BLOCKSIZE == 0) || (SS_INSERTIONSORT_THRESHOLD < SS_BLOCKSIZE)

static INLINE
void
ss_fixdown(const unsigned char *Td, const int *PA,
           int *SA, int i, int size) {
  int j, k;
  int v;
  int c, d, e;

  for(v = SA[i], c = Td[PA[v]]; (j = 2 * i + 1) < size; SA[i] = SA[k], i = k) {
    d = Td[PA[SA[k = j++]]];
    if(d < (e = Td[PA[SA[j]]])) { k = j; d = e; }
    if(d <= c) { break; }
  }
  SA[i] = v;
}

/* Simple top-down heapsort. */
static
void
ss_heapsort(const unsigned char *Td, const int *PA, int *SA, int size) {
  int i, m;
  int t;

  m = size;
  if((size % 2) == 0) {
    m--;
    if(Td[PA[SA[m / 2]]] < Td[PA[SA[m]]]) { SWAP(SA[m], SA[m / 2]); }
  }

  for(i = m / 2 - 1; 0 <= i; --i) { ss_fixdown(Td, PA, SA, i, m); }
  if((size % 2) == 0) { SWAP(SA[0], SA[m]); ss_fixdown(Td, PA, SA, 0, m); }
  for(i = m - 1; 0 < i; --i) {
    t = SA[0], SA[0] = SA[i];
    ss_fixdown(Td, PA, SA, 0, i);
    SA[i] = t;
  }
}


/*---------------------------------------------------------------------------*/

/* Returns the median of three elements. */
static INLINE
int *
ss_median3(const unsigned char *Td, const int *PA,
           int *v1, int *v2, int *v3) {
  int *t;
  if(Td[PA[*v1]] > Td[PA[*v2]]) { SWAP(v1, v2); }
  if(Td[PA[*v2]] > Td[PA[*v3]]) {
    if(Td[PA[*v1]] > Td[PA[*v3]]) { return v1; }
    else { return v3; }
  }
  return v2;
}

/* Returns the median of five elements. */
static INLINE
int *
ss_median5(const unsigned char *Td, const int *PA,
           int *v1, int *v2, int *v3, int *v4, int *v5) {
  int *t;
  if(Td[PA[*v2]] > Td[PA[*v3]]) { SWAP(v2, v3); }
  if(Td[PA[*v4]] > Td[PA[*v5]]) { SWAP(v4, v5); }
  if(Td[PA[*v2]] > Td[PA[*v4]]) { SWAP(v2, v4); SWAP(v3, v5); }
  if(Td[PA[*v1]] > Td[PA[*v3]]) { SWAP(v1, v3); }
  if(Td[PA[*v1]] > Td[PA[*v4]]) { SWAP(v1, v4); SWAP(v3, v5); }
  if(Td[PA[*v3]] > Td[PA[*v4]]) { return v4; }
  return v3;
}

/* Returns the pivot element. */
static INLINE
int *
ss_pivot(const unsigned char *Td, const int *PA, int *first, int *last) {
  int *middle;
  int t;

  t = last - first;
  middle = first + t / 2;

  if(t <= 512) {
    if(t <= 32) {
      return ss_median3(Td, PA, first, middle, last - 1);
    } else {
      t >>= 2;
      return ss_median5(Td, PA, first, first + t, middle, last - 1 - t, last - 1);
    }
  }
  t >>= 3;
  first  = ss_median3(Td, PA, first, first + t, first + (t << 1));
  middle = ss_median3(Td, PA, middle - t, middle, middle + t);
  last   = ss_median3(Td, PA, last - 1 - (t << 1), last - 1 - t, last - 1);
  return ss_median3(Td, PA, first, middle, last);
}


/*---------------------------------------------------------------------------*/

/* Binary partition for substrings. */
static INLINE
int *
ss_partition(const int *PA,
                    int *first, int *last, int depth) {
  int *a, *b;
  int t;
  for(a = first - 1, b = last;;) {
    for(; (++a < b) && ((PA[*a] + depth) >= (PA[*a + 1] + 1));) { *a = ~*a; }
    for(; (a < --b) && ((PA[*b] + depth) <  (PA[*b + 1] + 1));) { }
    if(b <= a) { break; }
    t = ~*b;
    *b = *a;
    *a = t;
  }
  if(first < a) { *first = ~*first; }
  return a;
}

/* Multikey introsort for medium size groups. */
static
void
ss_mintrosort(const unsigned char *T, const int *PA,
              int *first, int *last,
              int depth) {
#define STACK_SIZE SS_MISORT_STACKSIZE
  struct { int *a, *b, c; int d; } stack[STACK_SIZE];
  const unsigned char *Td;
  int *a, *b, *c, *d, *e, *f;
  int s, t;
  int ssize;
  int limit;
  int v, x = 0;

  for(ssize = 0, limit = ss_ilg(last - first);;) {

    if((last - first) <= SS_INSERTIONSORT_THRESHOLD) {
#if 1 < SS_INSERTIONSORT_THRESHOLD
      if(1 < (last - first)) { ss_insertionsort(T, PA, first, last, depth); }
#endif
      STACK_POP(first, last, depth, limit);
      continue;
    }

    Td = T + depth;
    if(limit-- == 0) { ss_heapsort(Td, PA, first, last - first); }
    if(limit < 0) {
      for(a = first + 1, v = Td[PA[*first]]; a < last; ++a) {
        if((x = Td[PA[*a]]) != v) {
          if(1 < (a - first)) { break; }
          v = x;
          first = a;
        }
      }
      if(Td[PA[*first] - 1] < v) {
        first = ss_partition(PA, first, a, depth);
      }
      if((a - first) <= (last - a)) {
        if(1 < (a - first)) {
          STACK_PUSH(a, last, depth, -1);
          last = a, depth += 1, limit = ss_ilg(a - first);
        } else {
          first = a, limit = -1;
        }
      } else {
        if(1 < (last - a)) {
          STACK_PUSH(first, a, depth + 1, ss_ilg(a - first));
          first = a, limit = -1;
        } else {
          last = a, depth += 1, limit = ss_ilg(a - first);
        }
      }
      continue;
    }

    /* choose pivot */
    a = ss_pivot(Td, PA, first, last);
    v = Td[PA[*a]];
    SWAP(*first, *a);

    /* partition */
    for(b = first; (++b < last) && ((x = Td[PA[*b]]) == v);) { }
    if(((a = b) < last) && (x < v)) {
      for(; (++b < last) && ((x = Td[PA[*b]]) <= v);) {
        if(x == v) { SWAP(*b, *a); ++a; }
      }
    }
    for(c = last; (b < --c) && ((x = Td[PA[*c]]) == v);) { }
    if((b < (d = c)) && (x > v)) {
      for(; (b < --c) && ((x = Td[PA[*c]]) >= v);) {
        if(x == v) { SWAP(*c, *d); --d; }
      }
    }
    for(; b < c;) {
      SWAP(*b, *c);
      for(; (++b < c) && ((x = Td[PA[*b]]) <= v);) {
        if(x == v) { SWAP(*b, *a); ++a; }
      }
      for(; (b < --c) && ((x = Td[PA[*c]]) >= v);) {
        if(x == v) { SWAP(*c, *d); --d; }
      }
    }

    if(a <= d) {
      c = b - 1;

      if((s = a - first) > (t = b - a)) { s = t; }
      for(e = first, f = b - s; 0 < s; --s, ++e, ++f) { SWAP(*e, *f); }
      if((s = d - c) > (t = last - d - 1)) { s = t; }
      for(e = b, f = last - s; 0 < s; --s, ++e, ++f) { SWAP(*e, *f); }

      a = first + (b - a), c = last - (d - c);
      b = (v <= Td[PA[*a] - 1]) ? a : ss_partition(PA, a, c, depth);

      if((a - first) <= (last - c)) {
        if((last - c) <= (c - b)) {
          STACK_PUSH(b, c, depth + 1, ss_ilg(c - b));
          STACK_PUSH(c, last, depth, limit);
          last = a;
        } else if((a - first) <= (c - b)) {
          STACK_PUSH(c, last, depth, limit);
          STACK_PUSH(b, c, depth + 1, ss_ilg(c - b));
          last = a;
        } else {
          STACK_PUSH(c, last, depth, limit);
          STACK_PUSH(first, a, depth, limit);
          first = b, last = c, depth += 1, limit = ss_ilg(c - b);
        }
      } else {
        if((a - first) <= (c - b)) {
          STACK_PUSH(b, c, depth + 1, ss_ilg(c - b));
          STACK_PUSH(first, a, depth, limit);
          first = c;
        } else if((last - c) <= (c - b)) {
          STACK_PUSH(first, a, depth, limit);
          STACK_PUSH(b, c, depth + 1, ss_ilg(c - b));
          first = c;
        } else {
          STACK_PUSH(first, a, depth, limit);
          STACK_PUSH(c, last, depth, limit);
          first = b, last = c, depth += 1, limit = ss_ilg(c - b);
        }
      }
    } else {
      limit += 1;
      if(Td[PA[*first] - 1] < v) {
        first = ss_partition(PA, first, last, depth);
        limit = ss_ilg(last - first);
      }
      depth += 1;
    }
  }
#undef STACK_SIZE
}

#endif /* (SS_BLOCKSIZE == 0) || (SS_INSERTIONSORT_THRESHOLD < SS_BLOCKSIZE) */


/*---------------------------------------------------------------------------*/

#if SS_BLOCKSIZE != 0

static INLINE
void
ss_blockswap(int *a, int *b, int n) {
  int t;
  for(; 0 < n; --n, ++a, ++b) {
    t = *a, *a = *b, *b = t;
  }
}

static INLINE
void
ss_rotate(int *first, int *middle, int *last) {
  int *a, *b, t;
  int l, r;
  l = middle - first, r = last - middle;
  for(; (0 < l) && (0 < r);) {
    if(l == r) { ss_blockswap(first, middle, l); break; }
    if(l < r) {
      a = last - 1, b = middle - 1;
      t = *a;
      do {
        *a-- = *b, *b-- = *a;
        if(b < first) {
          *a = t;
          last = a;
          if((r -= l + 1) <= l) { break; }
          a -= 1, b = middle - 1;
          t = *a;
        }
      } while(1);
    } else {
      a = first, b = middle;
      t = *a;
      do {
        *a++ = *b, *b++ = *a;
        if(last <= b) {
          *a = t;
          first = a + 1;
          if((l -= r + 1) <= r) { break; }
          a += 1, b = middle;
          t = *a;
        }
      } while(1);
    }
  }
}


/*---------------------------------------------------------------------------*/

static
void
ss_inplacemerge(const unsigned char *T, const int *PA,
                int *first, int *middle, int *last,
                int depth) {
  const int *p;
  int *a, *b;
  int len, half;
  int q, r;
  int x;

  for(;;) {
    if(*(last - 1) < 0) { x = 1; p = PA + ~*(last - 1); }
    else                { x = 0; p = PA +  *(last - 1); }
    for(a = first, len = middle - first, half = len >> 1, r = -1;
        0 < len;
        len = half, half >>= 1) {
      b = a + half;
      q = ss_compare(T, PA + ((0 <= *b) ? *b : ~*b), p, depth);
      if(q < 0) {
        a = b + 1;
        half -= (len & 1) ^ 1;
      } else {
        r = q;
      }
    }
    if(a < middle) {
      if(r == 0) { *a = ~*a; }
      ss_rotate(a, middle, last);
      last -= middle - a;
      middle = a;
      if(first == middle) { break; }
    }
    --last;
    if(x != 0) { while(*--last < 0) { } }
    if(middle == last) { break; }
  }
}


/*---------------------------------------------------------------------------*/

/* Merge-forward with internal buffer. */
static
void
ss_mergeforward(const unsigned char *T, const int *PA,
                int *first, int *middle, int *last,
                int *buf, int depth) {
  int *a, *b, *c, *bufend;
  int t;
  int r;

  bufend = buf + (middle - first) - 1;
  ss_blockswap(buf, first, middle - first);

  for(t = *(a = first), b = buf, c = middle;;) {
    r = ss_compare(T, PA + *b, PA + *c, depth);
    if(r < 0) {
      do {
        *a++ = *b;
        if(bufend <= b) { *bufend = t; return; }
        *b++ = *a;
      } while(*b < 0);
    } else if(r > 0) {
      do {
        *a++ = *c, *c++ = *a;
        if(last <= c) {
          while(b < bufend) { *a++ = *b, *b++ = *a; }
          *a = *b, *b = t;
          return;
        }
      } while(*c < 0);
    } else {
      *c = ~*c;
      do {
        *a++ = *b;
        if(bufend <= b) { *bufend = t; return; }
        *b++ = *a;
      } while(*b < 0);

      do {
        *a++ = *c, *c++ = *a;
        if(last <= c) {
          while(b < bufend) { *a++ = *b, *b++ = *a; }
          *a = *b, *b = t;
          return;
        }
      } while(*c < 0);
    }
  }
}

/* Merge-backward with internal buffer. */
static
void
ss_mergebackward(const unsigned char *T, const int *PA,
                 int *first, int *middle, int *last,
                 int *buf, int depth) {
  const int *p1, *p2;
  int *a, *b, *c, *bufend;
  int t;
  int r;
  int x;

  bufend = buf + (last - middle) - 1;
  ss_blockswap(buf, middle, last - middle);

  x = 0;
  if(*bufend < 0)       { p1 = PA + ~*bufend; x |= 1; }
  else                  { p1 = PA +  *bufend; }
  if(*(middle - 1) < 0) { p2 = PA + ~*(middle - 1); x |= 2; }
  else                  { p2 = PA +  *(middle - 1); }
  for(t = *(a = last - 1), b = bufend, c = middle - 1;;) {
    r = ss_compare(T, p1, p2, depth);
    if(0 < r) {
      if(x & 1) { do { *a-- = *b, *b-- = *a; } while(*b < 0); x ^= 1; }
      *a-- = *b;
      if(b <= buf) { *buf = t; break; }
      *b-- = *a;
      if(*b < 0) { p1 = PA + ~*b; x |= 1; }
      else       { p1 = PA +  *b; }
    } else if(r < 0) {
      if(x & 2) { do { *a-- = *c, *c-- = *a; } while(*c < 0); x ^= 2; }
      *a-- = *c, *c-- = *a;
      if(c < first) {
        while(buf < b) { *a-- = *b, *b-- = *a; }
        *a = *b, *b = t;
        break;
      }
      if(*c < 0) { p2 = PA + ~*c; x |= 2; }
      else       { p2 = PA +  *c; }
    } else {
      if(x & 1) { do { *a-- = *b, *b-- = *a; } while(*b < 0); x ^= 1; }
      *a-- = ~*b;
      if(b <= buf) { *buf = t; break; }
      *b-- = *a;
      if(x & 2) { do { *a-- = *c, *c-- = *a; } while(*c < 0); x ^= 2; }
      *a-- = *c, *c-- = *a;
      if(c < first) {
        while(buf < b) { *a-- = *b, *b-- = *a; }
        *a = *b, *b = t;
        break;
      }
      if(*b < 0) { p1 = PA + ~*b; x |= 1; }
      else       { p1 = PA +  *b; }
      if(*c < 0) { p2 = PA + ~*c; x |= 2; }
      else       { p2 = PA +  *c; }
    }
  }
}

/* D&C based merge. */
static
void
ss_swapmerge(const unsigned char *T, const int *PA,
             int *first, int *middle, int *last,
             int *buf, int bufsize, int depth) {
#define STACK_SIZE SS_SMERGE_STACKSIZE
#define GETIDX(a) ((0 <= (a)) ? (a) : (~(a)))
#define MERGE_CHECK(a, b, c)\
  do {\
    if(((c) & 1) ||\
       (((c) & 2) && (ss_compare(T, PA + GETIDX(*((a) - 1)), PA + *(a), depth) == 0))) {\
      *(a) = ~*(a);\
    }\
    if(((c) & 4) && ((ss_compare(T, PA + GETIDX(*((b) - 1)), PA + *(b), depth) == 0))) {\
      *(b) = ~*(b);\
    }\
  } while(0)
  struct { int *a, *b, *c; int d; } stack[STACK_SIZE];
  int *l, *r, *lm, *rm;
  int m, len, half;
  int ssize;
  int check, next;

  for(check = 0, ssize = 0;;) {
    if((last - middle) <= bufsize) {
      if((first < middle) && (middle < last)) {
        ss_mergebackward(T, PA, first, middle, last, buf, depth);
      }
      MERGE_CHECK(first, last, check);
      STACK_POP(first, middle, last, check);
      continue;
    }

    if((middle - first) <= bufsize) {
      if(first < middle) {
        ss_mergeforward(T, PA, first, middle, last, buf, depth);
      }
      MERGE_CHECK(first, last, check);
      STACK_POP(first, middle, last, check);
      continue;
    }

    for(m = 0, len = MIN(middle - first, last - middle), half = len >> 1;
        0 < len;
        len = half, half >>= 1) {
      if(ss_compare(T, PA + GETIDX(*(middle + m + half)),
                       PA + GETIDX(*(middle - m - half - 1)), depth) < 0) {
        m += half + 1;
        half -= (len & 1) ^ 1;
      }
    }

    if(0 < m) {
      lm = middle - m, rm = middle + m;
      ss_blockswap(lm, middle, m);
      l = r = middle, next = 0;
      if(rm < last) {
        if(*rm < 0) {
          *rm = ~*rm;
          if(first < lm) { for(; *--l < 0;) { } next |= 4; }
          next |= 1;
        } else if(first < lm) {
          for(; *r < 0; ++r) { }
          next |= 2;
        }
      }

      if((l - first) <= (last - r)) {
        STACK_PUSH(r, rm, last, (next & 3) | (check & 4));
        middle = lm, last = l, check = (check & 3) | (next & 4);
      } else {
        if((next & 2) && (r == middle)) { next ^= 6; }
        STACK_PUSH(first, lm, l, (check & 3) | (next & 4));
        first = r, middle = rm, check = (next & 3) | (check & 4);
      }
    } else {
      if(ss_compare(T, PA + GETIDX(*(middle - 1)), PA + *middle, depth) == 0) {
        *middle = ~*middle;
      }
      MERGE_CHECK(first, last, check);
      STACK_POP(first, middle, last, check);
    }
  }
#undef STACK_SIZE
}

#endif /* SS_BLOCKSIZE != 0 */


/*---------------------------------------------------------------------------*/

/* Substring sort */
static
void
sssort(const unsigned char *T, const int *PA,
       int *first, int *last,
       int *buf, int bufsize,
       int depth, int n, int lastsuffix) {
  int *a;
#if SS_BLOCKSIZE != 0
  int *b, *middle, *curbuf;
  int j, k, curbufsize, limit;
#endif
  int i;

  if(lastsuffix != 0) { ++first; }

#if SS_BLOCKSIZE == 0
  ss_mintrosort(T, PA, first, last, depth);
#else
  if((bufsize < SS_BLOCKSIZE) &&
      (bufsize < (last - first)) &&
      (bufsize < (limit = ss_isqrt(last - first)))) {
    if(SS_BLOCKSIZE < limit) { limit = SS_BLOCKSIZE; }
    buf = middle = last - limit, bufsize = limit;
  } else {
    middle = last, limit = 0;
  }
  for(a = first, i = 0; SS_BLOCKSIZE < (middle - a); a += SS_BLOCKSIZE, ++i) {
#if SS_INSERTIONSORT_THRESHOLD < SS_BLOCKSIZE
    ss_mintrosort(T, PA, a, a + SS_BLOCKSIZE, depth);
#elif 1 < SS_BLOCKSIZE
    ss_insertionsort(T, PA, a, a + SS_BLOCKSIZE, depth);
#endif
    curbufsize = last - (a + SS_BLOCKSIZE);
    curbuf = a + SS_BLOCKSIZE;
    if(curbufsize <= bufsize) { curbufsize = bufsize, curbuf = buf; }
    for(b = a, k = SS_BLOCKSIZE, j = i; j & 1; b -= k, k <<= 1, j >>= 1) {
      ss_swapmerge(T, PA, b - k, b, b + k, curbuf, curbufsize, depth);
    }
  }
#if SS_INSERTIONSORT_THRESHOLD < SS_BLOCKSIZE
  ss_mintrosort(T, PA, a, middle, depth);
#elif 1 < SS_BLOCKSIZE
  ss_insertionsort(T, PA, a, middle, depth);
#endif
  for(k = SS_BLOCKSIZE; i != 0; k <<= 1, i >>= 1) {
    if(i & 1) {
      ss_swapmerge(T, PA, a - k, a, middle, buf, bufsize, depth);
      a -= k;
    }
  }
  if(limit != 0) {
#if SS_INSERTIONSORT_THRESHOLD < SS_BLOCKSIZE
    ss_mintrosort(T, PA, middle, last, depth);
#elif 1 < SS_BLOCKSIZE
    ss_insertionsort(T, PA, middle, last, depth);
#endif
    ss_inplacemerge(T, PA, first, middle, last, depth);
  }
#endif

  if(lastsuffix != 0) {
    /* Insert last type B* suffix. */
    int PAi[2]; PAi[0] = PA[*(first - 1)], PAi[1] = n - 2;
    for(a = first, i = *(first - 1);
        (a < last) && ((*a < 0) || (0 < ss_compare(T, &(PAi[0]), PA + *a, depth)));
        ++a) {
      *(a - 1) = *a;
    }
    *(a - 1) = i;
  }
}


/*---------------------------------------------------------------------------*/

static INLINE
int
tr_ilg(int n) {
  return (n & 0xffff0000) ?
          ((n & 0xff000000) ?
            24 + lg_table[(n >> 24) & 0xff] :
            16 + lg_table[(n >> 16) & 0xff]) :
          ((n & 0x0000ff00) ?
             8 + lg_table[(n >>  8) & 0xff] :
             0 + lg_table[(n >>  0) & 0xff]);
}


/*---------------------------------------------------------------------------*/

/* Simple insertionsort for small size groups. */
static
void
tr_insertionsort(const int *ISAd, int *first, int *last) {
  int *a, *b;
  int t, r;

  for(a = first + 1; a < last; ++a) {
    for(t = *a, b = a - 1; 0 > (r = ISAd[t] - ISAd[*b]);) {
      do { *(b + 1) = *b; } while((first <= --b) && (*b < 0));
      if(b < first) { break; }
    }
    if(r == 0) { *b = ~*b; }
    *(b + 1) = t;
  }
}


/*---------------------------------------------------------------------------*/

static INLINE
void
tr_fixdown(const int *ISAd, int *SA, int i, int size) {
  int j, k;
  int v;
  int c, d, e;

  for(v = SA[i], c = ISAd[v]; (j = 2 * i + 1) < size; SA[i] = SA[k], i = k) {
    d = ISAd[SA[k = j++]];
    if(d < (e = ISAd[SA[j]])) { k = j; d = e; }
    if(d <= c) { break; }
  }
  SA[i] = v;
}

/* Simple top-down heapsort. */
static
void
tr_heapsort(const int *ISAd, int *SA, int size) {
  int i, m;
  int t;

  m = size;
  if((size % 2) == 0) {
    m--;
    if(ISAd[SA[m / 2]] < ISAd[SA[m]]) { SWAP(SA[m], SA[m / 2]); }
  }

  for(i = m / 2 - 1; 0 <= i; --i) { tr_fixdown(ISAd, SA, i, m); }
  if((size % 2) == 0) { SWAP(SA[0], SA[m]); tr_fixdown(ISAd, SA, 0, m); }
  for(i = m - 1; 0 < i; --i) {
    t = SA[0], SA[0] = SA[i];
    tr_fixdown(ISAd, SA, 0, i);
    SA[i] = t;
  }
}


/*---------------------------------------------------------------------------*/

/* Returns the median of three elements. */
static INLINE
int *
tr_median3(const int *ISAd, int *v1, int *v2, int *v3) {
  int *t;
  if(ISAd[*v1] > ISAd[*v2]) { SWAP(v1, v2); }
  if(ISAd[*v2] > ISAd[*v3]) {
    if(ISAd[*v1] > ISAd[*v3]) { return v1; }
    else { return v3; }
  }
  return v2;
}

/* Returns the median of five elements. */
static INLINE
int *
tr_median5(const int *ISAd,
           int *v1, int *v2, int *v3, int *v4, int *v5) {
  int *t;
  if(ISAd[*v2] > ISAd[*v3]) { SWAP(v2, v3); }
  if(ISAd[*v4] > ISAd[*v5]) { SWAP(v4, v5); }
  if(ISAd[*v2] > ISAd[*v4]) { SWAP(v2, v4); SWAP(v3, v5); }
  if(ISAd[*v1] > ISAd[*v3]) { SWAP(v1, v3); }
  if(ISAd[*v1] > ISAd[*v4]) { SWAP(v1, v4); SWAP(v3, v5); }
  if(ISAd[*v3] > ISAd[*v4]) { return v4; }
  return v3;
}

/* Returns the pivot element. */
static INLINE
int *
tr_pivot(const int *ISAd, int *first, int *last) {
  int *middle;
  int t;

  t = last - first;
  middle = first + t / 2;

  if(t <= 512) {
    if(t <= 32) {
      return tr_median3(ISAd, first, middle, last - 1);
    } else {
      t >>= 2;
      return tr_median5(ISAd, first, first + t, middle, last - 1 - t, last - 1);
    }
  }
  t >>= 3;
  first  = tr_median3(ISAd, first, first + t, first + (t << 1));
  middle = tr_median3(ISAd, middle - t, middle, middle + t);
  last   = tr_median3(ISAd, last - 1 - (t << 1), last - 1 - t, last - 1);
  return tr_median3(ISAd, first, middle, last);
}


/*---------------------------------------------------------------------------*/

typedef struct _trbudget_t trbudget_t;
struct _trbudget_t {
  int chance;
  int remain;
  int incval;
  int count;
};

static INLINE
void
trbudget_init(trbudget_t *budget, int chance, int incval) {
  budget->chance = chance;
  budget->remain = budget->incval = incval;
}

static INLINE
int
trbudget_check(trbudget_t *budget, int size) {
  if(size <= budget->remain) { budget->remain -= size; return 1; }
  if(budget->chance == 0) { budget->count += size; return 0; }
  budget->remain += budget->incval - size;
  budget->chance -= 1;
  return 1;
}


/*---------------------------------------------------------------------------*/

static INLINE
void
tr_partition(const int *ISAd,
             int *first, int *middle, int *last,
             int **pa, int **pb, int v) {
  int *a, *b, *c, *d, *e, *f;
  int t, s;
  int x = 0;

  for(b = middle - 1; (++b < last) && ((x = ISAd[*b]) == v);) { }
  if(((a = b) < last) && (x < v)) {
    for(; (++b < last) && ((x = ISAd[*b]) <= v);) {
      if(x == v) { SWAP(*b, *a); ++a; }
    }
  }
  for(c = last; (b < --c) && ((x = ISAd[*c]) == v);) { }
  if((b < (d = c)) && (x > v)) {
    for(; (b < --c) && ((x = ISAd[*c]) >= v);) {
      if(x == v) { SWAP(*c, *d); --d; }
    }
  }
  for(; b < c;) {
    SWAP(*b, *c);
    for(; (++b < c) && ((x = ISAd[*b]) <= v);) {
      if(x == v) { SWAP(*b, *a); ++a; }
    }
    for(; (b < --c) && ((x = ISAd[*c]) >= v);) {
      if(x == v) { SWAP(*c, *d); --d; }
    }
  }

  if(a <= d) {
    c = b - 1;
    if((s = a - first) > (t = b - a)) { s = t; }
    for(e = first, f = b - s; 0 < s; --s, ++e, ++f) { SWAP(*e, *f); }
    if((s = d - c) > (t = last - d - 1)) { s = t; }
    for(e = b, f = last - s; 0 < s; --s, ++e, ++f) { SWAP(*e, *f); }
    first += (b - a), last -= (d - c);
  }
  *pa = first, *pb = last;
}

static
void
tr_copy(int *ISA, const int *SA,
        int *first, int *a, int *b, int *last,
        int depth) {
  /* sort suffixes of middle partition
     by using sorted order of suffixes of left and right partition. */
  int *c, *d, *e;
  int s, v;

  v = b - SA - 1;
  for(c = first, d = a - 1; c <= d; ++c) {
    if((0 <= (s = *c - depth)) && (ISA[s] == v)) {
      *++d = s;
      ISA[s] = d - SA;
    }
  }
  for(c = last - 1, e = d + 1, d = b; e < d; --c) {
    if((0 <= (s = *c - depth)) && (ISA[s] == v)) {
      *--d = s;
      ISA[s] = d - SA;
    }
  }
}

static
void
tr_partialcopy(int *ISA, const int *SA,
               int *first, int *a, int *b, int *last,
               int depth) {
  int *c, *d, *e;
  int s, v;
  int rank, lastrank, newrank = -1;

  v = b - SA - 1;
  lastrank = -1;
  for(c = first, d = a - 1; c <= d; ++c) {
    if((0 <= (s = *c - depth)) && (ISA[s] == v)) {
      *++d = s;
      rank = ISA[s + depth];
      if(lastrank != rank) { lastrank = rank; newrank = d - SA; }
      ISA[s] = newrank;
    }
  }

  lastrank = -1;
  for(e = d; first <= e; --e) {
    rank = ISA[*e];
    if(lastrank != rank) { lastrank = rank; newrank = e - SA; }
    if(newrank != rank) { ISA[*e] = newrank; }
  }

  lastrank = -1;
  for(c = last - 1, e = d + 1, d = b; e < d; --c) {
    if((0 <= (s = *c - depth)) && (ISA[s] == v)) {
      *--d = s;
      rank = ISA[s + depth];
      if(lastrank != rank) { lastrank = rank; newrank = d - SA; }
      ISA[s] = newrank;
    }
  }
}

static
void
tr_introsort(int *ISA, const int *ISAd,
             int *SA, int *first, int *last,
             trbudget_t *budget) {
#define STACK_SIZE TR_STACKSIZE
  struct { const int *a; int *b, *c; int d, e; }stack[STACK_SIZE];
  int *a, *b, *c;
  int t;
  int v, x = 0;
  int incr = ISAd - ISA;
  int limit, next;
  int ssize, trlink = -1;

  for(ssize = 0, limit = tr_ilg(last - first);;) {

    if(limit < 0) {
      if(limit == -1) {
        /* tandem repeat partition */
        tr_partition(ISAd - incr, first, first, last, &a, &b, last - SA - 1);

        /* update ranks */
        if(a < last) {
          for(c = first, v = a - SA - 1; c < a; ++c) { ISA[*c] = v; }
        }
        if(b < last) {
          for(c = a, v = b - SA - 1; c < b; ++c) { ISA[*c] = v; }
        }

        /* push */
        if(1 < (b - a)) {
          STACK_PUSH5(NULL, a, b, 0, 0);
          STACK_PUSH5(ISAd - incr, first, last, -2, trlink);
          trlink = ssize - 2;
        }
        if((a - first) <= (last - b)) {
          if(1 < (a - first)) {
            STACK_PUSH5(ISAd, b, last, tr_ilg(last - b), trlink);
            last = a, limit = tr_ilg(a - first);
          } else if(1 < (last - b)) {
            first = b, limit = tr_ilg(last - b);
          } else {
            STACK_POP5(ISAd, first, last, limit, trlink);
          }
        } else {
          if(1 < (last - b)) {
            STACK_PUSH5(ISAd, first, a, tr_ilg(a - first), trlink);
            first = b, limit = tr_ilg(last - b);
          } else if(1 < (a - first)) {
            last = a, limit = tr_ilg(a - first);
          } else {
            STACK_POP5(ISAd, first, last, limit, trlink);
          }
        }
      } else if(limit == -2) {
        /* tandem repeat copy */
        a = stack[--ssize].b, b = stack[ssize].c;
        if(stack[ssize].d == 0) {
          tr_copy(ISA, SA, first, a, b, last, ISAd - ISA);
        } else {
          if(0 <= trlink) { stack[trlink].d = -1; }
          tr_partialcopy(ISA, SA, first, a, b, last, ISAd - ISA);
        }
        STACK_POP5(ISAd, first, last, limit, trlink);
      } else {
        /* sorted partition */
        if(0 <= *first) {
          a = first;
          do { ISA[*a] = a - SA; } while((++a < last) && (0 <= *a));
          first = a;
        }
        if(first < last) {
          a = first; do { *a = ~*a; } while(*++a < 0);
          next = (ISA[*a] != ISAd[*a]) ? tr_ilg(a - first + 1) : -1;
          if(++a < last) { for(b = first, v = a - SA - 1; b < a; ++b) { ISA[*b] = v; } }

          /* push */
          if(trbudget_check(budget, a - first)) {
            if((a - first) <= (last - a)) {
              STACK_PUSH5(ISAd, a, last, -3, trlink);
              ISAd += incr, last = a, limit = next;
            } else {
              if(1 < (last - a)) {
                STACK_PUSH5(ISAd + incr, first, a, next, trlink);
                first = a, limit = -3;
              } else {
                ISAd += incr, last = a, limit = next;
              }
            }
          } else {
            if(0 <= trlink) { stack[trlink].d = -1; }
            if(1 < (last - a)) {
              first = a, limit = -3;
            } else {
              STACK_POP5(ISAd, first, last, limit, trlink);
            }
          }
        } else {
          STACK_POP5(ISAd, first, last, limit, trlink);
        }
      }
      continue;
    }

    if((last - first) <= TR_INSERTIONSORT_THRESHOLD) {
      tr_insertionsort(ISAd, first, last);
      limit = -3;
      continue;
    }

    if(limit-- == 0) {
      tr_heapsort(ISAd, first, last - first);
      for(a = last - 1; first < a; a = b) {
        for(x = ISAd[*a], b = a - 1; (first <= b) && (ISAd[*b] == x); --b) { *b = ~*b; }
      }
      limit = -3;
      continue;
    }

    /* choose pivot */
    a = tr_pivot(ISAd, first, last);
    SWAP(*first, *a);
    v = ISAd[*first];

    /* partition */
    tr_partition(ISAd, first, first + 1, last, &a, &b, v);
    if((last - first) != (b - a)) {
      next = (ISA[*a] != v) ? tr_ilg(b - a) : -1;

      /* update ranks */
      for(c = first, v = a - SA - 1; c < a; ++c) { ISA[*c] = v; }
      if(b < last) { for(c = a, v = b - SA - 1; c < b; ++c) { ISA[*c] = v; } }

      /* push */
      if((1 < (b - a)) && (trbudget_check(budget, b - a))) {
        if((a - first) <= (last - b)) {
          if((last - b) <= (b - a)) {
            if(1 < (a - first)) {
              STACK_PUSH5(ISAd + incr, a, b, next, trlink);
              STACK_PUSH5(ISAd, b, last, limit, trlink);
              last = a;
            } else if(1 < (last - b)) {
              STACK_PUSH5(ISAd + incr, a, b, next, trlink);
              first = b;
            } else {
              ISAd += incr, first = a, last = b, limit = next;
            }
          } else if((a - first) <= (b - a)) {
            if(1 < (a - first)) {
              STACK_PUSH5(ISAd, b, last, limit, trlink);
              STACK_PUSH5(ISAd + incr, a, b, next, trlink);
              last = a;
            } else {
              STACK_PUSH5(ISAd, b, last, limit, trlink);
              ISAd += incr, first = a, last = b, limit = next;
            }
          } else {
            STACK_PUSH5(ISAd, b, last, limit, trlink);
            STACK_PUSH5(ISAd, first, a, limit, trlink);
            ISAd += incr, first = a, last = b, limit = next;
          }
        } else {
          if((a - first) <= (b - a)) {
            if(1 < (last - b)) {
              STACK_PUSH5(ISAd + incr, a, b, next, trlink);
              STACK_PUSH5(ISAd, first, a, limit, trlink);
              first = b;
            } else if(1 < (a - first)) {
              STACK_PUSH5(ISAd + incr, a, b, next, trlink);
              last = a;
            } else {
              ISAd += incr, first = a, last = b, limit = next;
            }
          } else if((last - b) <= (b - a)) {
            if(1 < (last - b)) {
              STACK_PUSH5(ISAd, first, a, limit, trlink);
              STACK_PUSH5(ISAd + incr, a, b, next, trlink);
              first = b;
            } else {
              STACK_PUSH5(ISAd, first, a, limit, trlink);
              ISAd += incr, first = a, last = b, limit = next;
            }
          } else {
            STACK_PUSH5(ISAd, first, a, limit, trlink);
            STACK_PUSH5(ISAd, b, last, limit, trlink);
            ISAd += incr, first = a, last = b, limit = next;
          }
        }
      } else {
        if((1 < (b - a)) && (0 <= trlink)) { stack[trlink].d = -1; }
        if((a - first) <= (last - b)) {
          if(1 < (a - first)) {
            STACK_PUSH5(ISAd, b, last, limit, trlink);
            last = a;
          } else if(1 < (last - b)) {
            first = b;
          } else {
            STACK_POP5(ISAd, first, last, limit, trlink);
          }
        } else {
          if(1 < (last - b)) {
            STACK_PUSH5(ISAd, first, a, limit, trlink);
            first = b;
          } else if(1 < (a - first)) {
            last = a;
          } else {
            STACK_POP5(ISAd, first, last, limit, trlink);
          }
        }
      }
    } else {
      if(trbudget_check(budget, last - first)) {
        limit = tr_ilg(last - first), ISAd += incr;
      } else {
        if(0 <= trlink) { stack[trlink].d = -1; }
        STACK_POP5(ISAd, first, last, limit, trlink);
      }
    }
  }
#undef STACK_SIZE
}



/*---------------------------------------------------------------------------*/

/* Tandem repeat sort */
static
void
trsort(int *ISA, int *SA, int n, int depth) {
  int *ISAd;
  int *first, *last;
  trbudget_t budget;
  int t, skip, unsorted;

  trbudget_init(&budget, tr_ilg(n) * 2 / 3, n);
/*  trbudget_init(&budget, tr_ilg(n) * 3 / 4, n); */
  for(ISAd = ISA + depth; -n < *SA; ISAd += ISAd - ISA) {
    first = SA;
    skip = 0;
    unsorted = 0;
    do {
      if((t = *first) < 0) { first -= t; skip += t; }
      else {
        if(skip != 0) { *(first + skip) = skip; skip = 0; }
        last = SA + ISA[t] + 1;
        if(1 < (last - first)) {
          budget.count = 0;
          tr_introsort(ISA, ISAd, SA, first, last, &budget);
          if(budget.count != 0) { unsorted += budget.count; }
          else { skip = first - last; }
        } else if((last - first) == 1) {
          skip = -1;
        }
        first = last;
      }
    } while(first < (SA + n));
    if(skip != 0) { *(first + skip) = skip; }
    if(unsorted == 0) { break; }
  }
}


/*---------------------------------------------------------------------------*/

/* Sorts suffixes of type B*. */
static
int
sort_typeBstar(const unsigned char *T, int *SA,
               int *bucket_A, int *bucket_B,
               int n) {
  int *PAb, *ISAb, *buf;
#ifdef _OPENMP
  int *curbuf;
  int l;
#endif
  int i, j, k, t, m, bufsize;
  int c0, c1;
#ifdef _OPENMP
  int d0, d1;
  int tmp;
#endif

  /* Initialize bucket arrays. */
  for(i = 0; i < BUCKET_A_SIZE; ++i) { bucket_A[i] = 0; }
  for(i = 0; i < BUCKET_B_SIZE; ++i) { bucket_B[i] = 0; }

  /* Count the number of occurrences of the first one or two characters of each
     type A, B and B* suffix. Moreover, store the beginning position of all
     type B* suffixes into the array SA. */
  for(i = n - 1, m = n, c0 = T[n - 1]; 0 <= i;) {
    /* type A suffix. */
    do { ++BUCKET_A(c1 = c0); } while((0 <= --i) && ((c0 = T[i]) >= c1));
    if(0 <= i) {
      /* type B* suffix. */
      ++BUCKET_BSTAR(c0, c1);
      SA[--m] = i;
      /* type B suffix. */
      for(--i, c1 = c0; (0 <= i) && ((c0 = T[i]) <= c1); --i, c1 = c0) {
        ++BUCKET_B(c0, c1);
      }
    }
  }
  m = n - m;
/*
note:
  A type B* suffix is lexicographically smaller than a type B suffix that
  begins with the same first two characters.
*/

  /* Calculate the index of start/end point of each bucket. */
  for(c0 = 0, i = 0, j = 0; c0 < ALPHABET_SIZE; ++c0) {
    t = i + BUCKET_A(c0);
    BUCKET_A(c0) = i + j; /* start point */
    i = t + BUCKET_B(c0, c0);
    for(c1 = c0 + 1; c1 < ALPHABET_SIZE; ++c1) {
      j += BUCKET_BSTAR(c0, c1);
      BUCKET_BSTAR(c0, c1) = j; /* end point */
      i += BUCKET_B(c0, c1);
    }
  }

  if(0 < m) {
    /* Sort the type B* suffixes by their first two characters. */
    PAb = SA + n - m; ISAb = SA + m;
    for(i = m - 2; 0 <= i; --i) {
      t = PAb[i], c0 = T[t], c1 = T[t + 1];
      SA[--BUCKET_BSTAR(c0, c1)] = i;
    }
    t = PAb[m - 1], c0 = T[t], c1 = T[t + 1];
    SA[--BUCKET_BSTAR(c0, c1)] = m - 1;

    /* Sort the type B* substrings using sssort. */
#ifdef _OPENMP
    tmp = omp_get_max_threads();
    buf = SA + m, bufsize = (n - (2 * m)) / tmp;
    c0 = ALPHABET_SIZE - 2, c1 = ALPHABET_SIZE - 1, j = m;
#pragma omp parallel default(shared) private(curbuf, k, l, d0, d1, tmp)
    {
      tmp = omp_get_thread_num();
      curbuf = buf + tmp * bufsize;
      k = 0;
      for(;;) {
        #pragma omp critical(sssort_lock)
        {
          if(0 < (l = j)) {
            d0 = c0, d1 = c1;
            do {
              k = BUCKET_BSTAR(d0, d1);
              if(--d1 <= d0) {
                d1 = ALPHABET_SIZE - 1;
                if(--d0 < 0) { break; }
              }
            } while(((l - k) <= 1) && (0 < (l = k)));
            c0 = d0, c1 = d1, j = k;
          }
        }
        if(l == 0) { break; }
        sssort(T, PAb, SA + k, SA + l,
               curbuf, bufsize, 2, n, *(SA + k) == (m - 1));
      }
    }
#else
    buf = SA + m, bufsize = n - (2 * m);
    for(c0 = ALPHABET_SIZE - 2, j = m; 0 < j; --c0) {
      for(c1 = ALPHABET_SIZE - 1; c0 < c1; j = i, --c1) {
        i = BUCKET_BSTAR(c0, c1);
        if(1 < (j - i)) {
          sssort(T, PAb, SA + i, SA + j,
                 buf, bufsize, 2, n, *(SA + i) == (m - 1));
        }
      }
    }
#endif

    /* Compute ranks of type B* substrings. */
    for(i = m - 1; 0 <= i; --i) {
      if(0 <= SA[i]) {
        j = i;
        do { ISAb[SA[i]] = i; } while((0 <= --i) && (0 <= SA[i]));
        SA[i + 1] = i - j;
        if(i <= 0) { break; }
      }
      j = i;
      do { ISAb[SA[i] = ~SA[i]] = j; } while(SA[--i] < 0);
      ISAb[SA[i]] = j;
    }

    /* Construct the inverse suffix array of type B* suffixes using trsort. */
    trsort(ISAb, SA, m, 1);

    /* Set the sorted order of tyoe B* suffixes. */
    for(i = n - 1, j = m, c0 = T[n - 1]; 0 <= i;) {
      for(--i, c1 = c0; (0 <= i) && ((c0 = T[i]) >= c1); --i, c1 = c0) { }
      if(0 <= i) {
        t = i;
        for(--i, c1 = c0; (0 <= i) && ((c0 = T[i]) <= c1); --i, c1 = c0) { }
        SA[ISAb[--j]] = ((t == 0) || (1 < (t - i))) ? t : ~t;
      }
    }

    /* Calculate the index of start/end point of each bucket. */
    BUCKET_B(ALPHABET_SIZE - 1, ALPHABET_SIZE - 1) = n; /* end point */
    for(c0 = ALPHABET_SIZE - 2, k = m - 1; 0 <= c0; --c0) {
      i = BUCKET_A(c0 + 1) - 1;
      for(c1 = ALPHABET_SIZE - 1; c0 < c1; --c1) {
        t = i - BUCKET_B(c0, c1);
        BUCKET_B(c0, c1) = i; /* end point */

        /* Move all type B* suffixes to the correct position. */
        for(i = t, j = BUCKET_BSTAR(c0, c1);
            j <= k;
            --i, --k) { SA[i] = SA[k]; }
      }
      BUCKET_BSTAR(c0, c0 + 1) = i - BUCKET_B(c0, c0) + 1; /* start point */
      BUCKET_B(c0, c0) = i; /* end point */
    }
  }

  return m;
}

/* Constructs the suffix array by using the sorted order of type B* suffixes. */
static
void
construct_SA(const unsigned char *T, int *SA,
             int *bucket_A, int *bucket_B,
             int n, int m) {
  int *i, *j, *k;
  int s;
  int c0, c1, c2;

  if(0 < m) {
    /* Construct the sorted order of type B suffixes by using
       the sorted order of type B* suffixes. */
    for(c1 = ALPHABET_SIZE - 2; 0 <= c1; --c1) {
      /* Scan the suffix array from right to left. */
      for(i = SA + BUCKET_BSTAR(c1, c1 + 1),
          j = SA + BUCKET_A(c1 + 1) - 1, k = NULL, c2 = -1;
          i <= j;
          --j) {
        if(0 < (s = *j)) {
          assert(T[s] == c1);
          assert(((s + 1) < n) && (T[s] <= T[s + 1]));
          assert(T[s - 1] <= T[s]);
          *j = ~s;
          c0 = T[--s];
          if((0 < s) && (T[s - 1] > c0)) { s = ~s; }
          if(c0 != c2) {
            if(0 <= c2) { BUCKET_B(c2, c1) = k - SA; }
            k = SA + BUCKET_B(c2 = c0, c1);
          }
          assert(k < j);
          *k-- = s;
        } else {
          assert(((s == 0) && (T[s] == c1)) || (s < 0));
          *j = ~s;
        }
      }
    }
  }

  /* Construct the suffix array by using
     the sorted order of type B suffixes. */
  k = SA + BUCKET_A(c2 = T[n - 1]);
  *k++ = (T[n - 2] < c2) ? ~(n - 1) : (n - 1);
  /* Scan the suffix array from left to right. */
  for(i = SA, j = SA + n; i < j; ++i) {
    if(0 < (s = *i)) {
      assert(T[s - 1] >= T[s]);
      c0 = T[--s];
      if((s == 0) || (T[s - 1] < c0)) { s = ~s; }
      if(c0 != c2) {
        BUCKET_A(c2) = k - SA;
        k = SA + BUCKET_A(c2 = c0);
      }
      assert(i < k);
      *k++ = s;
    } else {
      assert(s < 0);
      *i = ~s;
    }
  }
}

/* Constructs the burrows-wheeler transformed string directly
   by using the sorted order of type B* suffixes. */
static
int
construct_BWT(const unsigned char *T, int *SA,
              int *bucket_A, int *bucket_B,
              int n, int m) {
  int *i, *j, *k, *orig;
  int s;
  int c0, c1, c2;

  if(0 < m) {
    /* Construct the sorted order of type B suffixes by using
       the sorted order of type B* suffixes. */
    for(c1 = ALPHABET_SIZE - 2; 0 <= c1; --c1) {
      /* Scan the suffix array from right to left. */
      for(i = SA + BUCKET_BSTAR(c1, c1 + 1),
          j = SA + BUCKET_A(c1 + 1) - 1, k = NULL, c2 = -1;
          i <= j;
          --j) {
        if(0 < (s = *j)) {
          assert(T[s] == c1);
          assert(((s + 1) < n) && (T[s] <= T[s + 1]));
          assert(T[s - 1] <= T[s]);
          c0 = T[--s];
          *j = ~((int)c0);
          if((0 < s) && (T[s - 1] > c0)) { s = ~s; }
          if(c0 != c2) {
            if(0 <= c2) { BUCKET_B(c2, c1) = k - SA; }
            k = SA + BUCKET_B(c2 = c0, c1);
          }
          assert(k < j);
          *k-- = s;
        } else if(s != 0) {
          *j = ~s;
#ifndef NDEBUG
        } else {
          assert(T[s] == c1);
#endif
        }
      }
    }
  }

  /* Construct the BWTed string by using
     the sorted order of type B suffixes. */
  k = SA + BUCKET_A(c2 = T[n - 1]);
  *k++ = (T[n - 2] < c2) ? ~((int)T[n - 2]) : (n - 1);
  /* Scan the suffix array from left to right. */
  for(i = SA, j = SA + n, orig = SA; i < j; ++i) {
    if(0 < (s = *i)) {
      assert(T[s - 1] >= T[s]);
      c0 = T[--s];
      *i = c0;
      if((0 < s) && (T[s - 1] < c0)) { s = ~((int)T[s - 1]); }
      if(c0 != c2) {
        BUCKET_A(c2) = k - SA;
        k = SA + BUCKET_A(c2 = c0);
      }
      assert(i < k);
      *k++ = s;
    } else if(s != 0) {
      *i = ~s;
    } else {
      orig = i;
    }
  }

  return orig - SA;
}


/*---------------------------------------------------------------------------*/

/*- Function -*/

int
divsufsort(const unsigned char *T, int *SA, int n) {
  int *bucket_A, *bucket_B;
  int m;
  int err = 0;

  /* Check arguments. */
  if((T == NULL) || (SA == NULL) || (n < 0)) { return -1; }
  else if(n == 0) { return 0; }
  else if(n == 1) { SA[0] = 0; return 0; }
  else if(n == 2) { m = (T[0] < T[1]); SA[m ^ 1] = 0, SA[m] = 1; return 0; }

  bucket_A = (int *)malloc(BUCKET_A_SIZE * sizeof(int));
  bucket_B = (int *)malloc(BUCKET_B_SIZE * sizeof(int));

  /* Suffixsort. */
  if((bucket_A != NULL) && (bucket_B != NULL)) {
    m = sort_typeBstar(T, SA, bucket_A, bucket_B, n);
    construct_SA(T, SA, bucket_A, bucket_B, n, m);
  } else {
    err = -2;
  }

  free(bucket_B);
  free(bucket_A);

  return err;
}

int
divbwt(const unsigned char *T, unsigned char *U, int *A, int n) {
  int *B;
  int *bucket_A, *bucket_B;
  int m, pidx, i;

  /* Check arguments. */
  if((T == NULL) || (U == NULL) || (n < 0)) { return -1; }
  else if(n <= 1) { if(n == 1) { U[0] = T[0]; } return n; }

  if((B = A) == NULL) { B = (int *)malloc((size_t)(n + 1) * sizeof(int)); }
  bucket_A = (int *)malloc(BUCKET_A_SIZE * sizeof(int));
  bucket_B = (int *)malloc(BUCKET_B_SIZE * sizeof(int));

  /* Burrows-Wheeler Transform. */
  if((B != NULL) && (bucket_A != NULL) && (bucket_B != NULL)) {
    m = sort_typeBstar(T, B, bucket_A, bucket_B, n);
    pidx = construct_BWT(T, B, bucket_A, bucket_B, n, m);

    /* Copy to output string. */
    U[0] = T[n - 1];
    for(i = 0; i < pidx; ++i) { U[i + 1] = (unsigned char)B[i]; }
    for(i += 1; i < n; ++i) { U[i] = (unsigned char)B[i]; }
    pidx += 1;
  } else {
    pidx = -2;
  }

  free(bucket_B);
  free(bucket_A);
  if(A == NULL) { free(B); }

  return pidx;
}

// End divsufsort.c

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

// LZ/BWT preprocessor for levels 1..3 compression and e8e9 filter.
// Level 1 uses variable length LZ77 codes like in the lazy compressor:
//
//   00,n,L[n] = n literal bytes
//   mm,mmm,n,ll,r,q (mm > 00) = match 4*n+ll at offset (q<<rb)+r-1
//
// where q is written in 8mm+mmm-8 (0..23) bits with an implied leading 1 bit
// and n is written using interleaved Elias Gamma coding, i.e. the leading
// 1 bit is implied, remaining bits are preceded by a 1 and terminated by
// a 0. e.g. abc is written 1,b,1,c,0. Codes are packed LSB first and
// padded with leading 0 bits in the last byte. r is a number with rb bits,
// where rb = log2(blocksize) - 24.
//
// Level 2 is byte oriented LZ77 with minimum match length m = $4 = args[3]
// with m in 1..64. Lengths and offsets are MSB first:
// 00xxxxxx   x+1 (1..64) literals follow
// yyxxxxxx   y+1 (2..4) offset bytes follow, match length x+m (m..m+63)
//
// Level 3 is BWT with the end of string byte coded as 255 and the
// last 4 bytes giving its position LSB first.

// floor(log2(x)) + 1 = number of bits excluding leading zeros (0..32)
int lg(unsigned x) {
  unsigned r=0;
  if (x>=65536) r=16, x>>=16;
  if (x>=256) r+=8, x>>=8;
  if (x>=16) r+=4, x>>=4;
  assert(x>=0 && x<16);
  return
    "\x00\x01\x02\x02\x03\x03\x03\x03\x04\x04\x04\x04\x04\x04\x04\x04"[x]+r;
}

// return number of 1 bits in x
int nbits(unsigned x) {
  int r;
  for (r=0; x; x>>=1) r+=x&1;
  return r;
}

// Encode inbuf to buf using LZ77. args are as follows:
// args[0] is log2 buffer size in MB.
// args[1] is level (1=var. length, 2=byte aligned lz77, 3=bwt) + 4 if E8E9.
// args[2] is the lz77 minimum match length and context order.
// args[3] is the lz77 higher context order to search first, or else 0.
// args[4] is the log2 hash bucket size (number of searches).
// args[5] is the log2 hash table size. If 21+args[0] then use a suffix array.
// args[6] is the secondary context look ahead
// sap is pointer to external suffix array of inbuf or 0. If supplied and
//   args[0]=5..7 then it is assumed that E8E9 was already applied to
//   both the input and sap and the input buffer is not modified.

class LZBuffer: public libzpaq::Reader {
  libzpaq::Array<unsigned> ht;// hash table, confirm in low bits, or SA+ISA
  const unsigned char* in;    // input pointer
  const int checkbits;        // hash confirmation size or lg(ISA size)
  const int level;            // 1=var length LZ77, 2=byte aligned LZ77, 3=BWT
  const unsigned htsize;      // size of hash table
  const unsigned n;           // input length
  unsigned i;                 // current location in in (0 <= i < n)
  const unsigned minMatch;    // minimum match length
  const unsigned minMatch2;   // second context order or 0 if not used
  const unsigned maxMatch;    // longest match length allowed
  const unsigned maxLiteral;  // longest literal length allowed
  const unsigned lookahead;   // second context look ahead
  unsigned h1, h2;            // low, high order context hashes of in[i..]
  const unsigned bucket;      // number of matches to search per hash - 1
  const unsigned shift1, shift2;  // how far to shift h1, h2 per hash
  const int minMatchBoth;     // max(minMatch, minMatch2)
  const unsigned rb;          // number of level 1 r bits in match code
  unsigned bits;              // pending output bits (level 1)
  unsigned nbits;             // number of bits in bits
  unsigned rpos, wpos;        // read, write pointers
  unsigned idx;               // BWT index
  const unsigned* sa;         // suffix array for BWT or LZ77-SA
  unsigned* isa;              // inverse suffix array for LZ77-SA
  enum {BUFSIZE=1<<14};       // output buffer size
  unsigned char buf[BUFSIZE]; // output buffer

  void write_literal(unsigned i, unsigned& lit);
  void write_match(unsigned len, unsigned off);
  void fill();  // encode to buf

  // write k bits of x
  void putb(unsigned x, int k) {
    x&=(1<<k)-1;
    bits|=x<<nbits;
    nbits+=k;
    while (nbits>7) {
      assert(wpos<BUFSIZE);
      buf[wpos++]=bits, bits>>=8, nbits-=8;
    }
  }

  // write last byte
  void flush() {
    assert(wpos<BUFSIZE);
    if (nbits>0) buf[wpos++]=bits;
    bits=nbits=0;
  }

  // write 1 byte
  void put(int c) {
    assert(wpos<BUFSIZE);
    buf[wpos++]=c;
  }

public:
  LZBuffer(StringBuffer& inbuf, int args[], const unsigned* sap=0);

  // return 1 byte of compressed output (overrides Reader)
  int get() {
    int c=-1;
    if (rpos==wpos) fill();
    if (rpos<wpos) c=buf[rpos++];
    if (rpos==wpos) rpos=wpos=0;
    return c;
  }

  // Read up to p[0..n-1] and return bytes read.
  int read(char* p, int n);
};

// Read n bytes of compressed output into p and return number of
// bytes read in 0..n. 0 signals EOF (overrides Reader).
int LZBuffer::read(char* p, int n) {
  if (rpos==wpos) fill();
  int nr=n;
  if (nr>int(wpos-rpos)) nr=wpos-rpos;
  if (nr) memcpy(p, buf+rpos, nr);
  rpos+=nr;
  assert(rpos<=wpos);
  if (rpos==wpos) rpos=wpos=0;
  return nr;
}

LZBuffer::LZBuffer(StringBuffer& inbuf, int args[], const unsigned* sap):
    ht((args[1]&3)==3 ? (inbuf.size()+1)*!sap      // for BWT suffix array
        : args[5]-args[0]<21 ? 1u<<args[5]         // for LZ77 hash table
        : (inbuf.size()*!sap)+(1u<<17<<args[0])),  // for LZ77 SA and ISA
    in(inbuf.data()),
    checkbits(args[5]-args[0]<21 ? 12-args[0] : 17+args[0]),
    level(args[1]&3),
    htsize(ht.size()),
    n(inbuf.size()),
    i(0),
    minMatch(args[2]),
    minMatch2(args[3]),
    maxMatch(BUFSIZE*3),
    maxLiteral(BUFSIZE/4),
    lookahead(args[6]),
    h1(0), h2(0),
    bucket((1<<args[4])-1), 
    shift1(minMatch>0 ? (args[5]-1)/minMatch+1 : 1),
    shift2(minMatch2>0 ? (args[5]-1)/minMatch2+1 : 0),
    minMatchBoth(max(minMatch, minMatch2+lookahead)+4),
    rb(args[0]>4 ? args[0]-4 : 0),
    bits(0), nbits(0), rpos(0), wpos(0),
    idx(0), sa(0), isa(0) {
  assert(args[0]>=0);
  assert(n<=(1u<<20<<args[0]));
  assert(args[1]>=1 && args[1]<=7 && args[1]!=4);
  assert(level>=1 && level<=3);
  if ((minMatch<4 && level==1) || (minMatch<1 && level==2))
    error("match length $3 too small");

  // e8e9 transform
  if (args[1]>4 && !sap) e8e9(inbuf.data(), n);

  // build suffix array if not supplied
  if (args[5]-args[0]>=21 || level==3) {  // LZ77-SA or BWT
    if (sap)
      sa=sap;
    else {
      assert(ht.size()>=n);
      assert(ht.size()>0);
      sa=&ht[0];
      if (n>0) divsufsort((const unsigned char*)in, (int*)sa, n);
    }
    if (level<3) {
      assert(ht.size()>=(n*(sap==0))+(1u<<17<<args[0]));
      isa=&ht[n*(sap==0)];
    }
  }
}

// Encode from in to buf until end of input or buf is not empty
void LZBuffer::fill() {

  // BWT
  if (level==3) {
    assert(in || n==0);
    assert(sa);
    for (; wpos<BUFSIZE && i<n+5; ++i) {
      if (i==0) put(n>0 ? in[n-1] : 255);
      else if (i>n) put(idx&255), idx>>=8;
      else if (sa[i-1]==0) idx=i, put(255);
      else put(in[sa[i-1]-1]);
    }
    return;
  }

  // LZ77: scan the input
  unsigned lit=0;  // number of output literals pending
  const unsigned mask=(1<<checkbits)-1;
  while (i<n && wpos*2<BUFSIZE) {

    // Search for longest match, or pick closest in case of tie
    unsigned blen=minMatch-1;  // best match length
    unsigned bp=0;  // pointer to best match
    unsigned blit=0;  // literals before best match
    int bscore=0;  // best cost

    // Look up contexts in suffix array
    if (isa) {
      if (sa[isa[i&mask]]!=i) // rebuild ISA
        for (unsigned j=0; j<n; ++j)
          if ((sa[j]&~mask)==(i&~mask))
            isa[sa[j]&mask]=j;
      for (unsigned h=0; h<=lookahead; ++h) {
        unsigned q=isa[(h+i)&mask];  // location of h+i in SA
        assert(q<n);
        if (sa[q]!=h+i) continue;
        for (int j=-1; j<=1; j+=2) {  // search backward and forward
          for (unsigned k=1; k<=bucket; ++k) {
            unsigned p;  // match to be tested
            if (q+j*k<n && (p=sa[q+j*k]-h)<i) {
              assert(p<n);
              unsigned l, l1;  // length of match, leading literals
              for (l=h; i+l<n && l<maxMatch && in[p+l]==in[i+l]; ++l);
              for (l1=h; l1>0 && in[p+l1-1]==in[i+l1-1]; --l1);
              int score=int(l-l1)*8-lg(i-p)-4*(lit==0 && l1>0)-11;
              for (unsigned a=0; a<h; ++a) score=score*5/8;
              if (score>bscore) blen=l, bp=p, blit=l1, bscore=score;
              if (l<blen || l<minMatch || l>255) break;
            }
          }
        }
        if (bscore<=0 || blen<minMatch) break;
      }
    }

    // Look up contexts in a hash table.
    // Try the longest context orders first. If a match is found, then
    // skip the lower order as a speed optimization.
    else if (level==1 || minMatch<=64) {
      if (minMatch2>0) {
        for (unsigned k=0; k<=bucket; ++k) {
          unsigned p=ht[h2^k];
          if (p && (p&mask)==(in[i+3]&mask)) {
            p>>=checkbits;
            if (p<i && i+blen<=n && in[p+blen-1]==in[i+blen-1]) {
              unsigned l;  // match length from lookahead
              for (l=lookahead; i+l<n && l<maxMatch && in[p+l]==in[i+l]; ++l);
              if (l>=minMatch2+lookahead) {
                int l1;  // length back from lookahead
                for (l1=lookahead; l1>0 && in[p+l1-1]==in[i+l1-1]; --l1);
                assert(l1>=0 && l1<=int(lookahead));
                int score=int(l-l1)*8-lg(i-p)-8*(lit==0 && l1>0)-11;
                if (score>bscore) blen=l, bp=p, blit=l1, bscore=score;
              }
            }
          }
          if (blen>=128) break;
        }
      }

      // Search the lower order context
      if (!minMatch2 || blen<minMatch2) {
        for (unsigned k=0; k<=bucket; ++k) {
          unsigned p=ht[h1^k];
          if (p && (p&mask)==(in[i+3]&mask)) {
            p>>=checkbits;
            if (p<i && i+blen<=n && in[p+blen-1]==in[i+blen-1]) {
              unsigned l;
              for (l=0; i+l<n && l<maxMatch && in[p+l]==in[i+l]; ++l);
              int score=l*8-lg(i-p)-2*(lit>0)-11;
              if (score>bscore) blen=l, bp=p, blit=0, bscore=score;
            }
          }
          if (blen>=128) break;
        }
      }
    }

    // If match is long enough, then output any pending literals first,
    // and then the match. blen is the length of the match.
    assert(i>=bp);
    const unsigned off=i-bp;  // offset
    if (off>0 && bscore>0
        && blen-blit>=minMatch+(level==2)*((off>=(1<<16))+(off>=(1<<24)))) {
      lit+=blit;
      write_literal(i+blit, lit);
      write_match(blen-blit, off);
    }

    // Otherwise add to literal length
    else {
      blen=1;
      ++lit;
    }

    // Update index, advance blen bytes
    if (isa)
      i+=blen;
    else {
      while (blen--) {
        if (i+minMatchBoth<n) {
          unsigned ih=((i*1234547)>>19)&bucket;
          const unsigned p=(i<<checkbits)|(in[i+3]&mask);
          assert(ih<=bucket);
          if (minMatch2) {
            ht[h2^ih]=p;
            h2=(((h2*9)<<shift2)
                +(in[i+minMatch2+lookahead]+1)*23456789)&(htsize-1);
          }
          ht[h1^ih]=p;
          h1=(((h1*5)<<shift1)+(in[i+minMatch]+1)*123456791)&(htsize-1);
        }
        ++i;
      }
    }

    // Write long literals to keep buf from filling up
    if (lit>=maxLiteral)
      write_literal(i, lit);
  }

  // Write pending literals at end of input
  assert(i<=n);
  if (i==n) {
    write_literal(n, lit);
    flush();
  }
}

// Write literal sequence in[i-lit..i-1], set lit=0
void LZBuffer::write_literal(unsigned i, unsigned& lit) {
  assert(lit>=0);
  assert(i>=0 && i<=n);
  assert(i>=lit);
  if (level==1) {
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
  else {
    assert(level==2);
    while (lit>0) {
      unsigned lit1=lit;
      if (lit1>64) lit1=64;
      put(lit1-1);
      for (unsigned j=i-lit; j<i-lit+lit1; ++j) put(in[j]);
      lit-=lit1;
    }
  }
}

// Write match sequence of given length and offset
void LZBuffer::write_match(unsigned len, unsigned off) {

  // mm,mmm,n,ll,r,q[mmmmm-8] = match n*4+ll, offset ((q-1)<<rb)+r+1
  if (level==1) {
    assert(len>=minMatch && len<=maxMatch);
    assert(off>0);
    assert(len>=4);
    assert(rb>=0 && rb<=8);
    int ll=lg(len)-1;
    assert(ll>=2);
    off+=(1<<rb)-1;
    int lo=lg(off)-1-rb;
    assert(lo>=0 && lo<=23);
    putb((lo+8)>>3, 2);// mm
    putb(lo&7, 3);     // mmm
    while (--ll>=2) {  // n
      putb(1, 1);
      putb((len>>ll)&1, 1);
    }
    putb(0, 1);
    putb(len&3, 2);    // ll
    putb(off, rb);     // r
    putb(off>>rb, lo); // q
  }

  // x[2]:len[6] off[x-1] 
  else {
    assert(level==2);
    assert(minMatch>=1 && minMatch<=64);
    --off;
    while (len>0) {  // Split long matches to len1=minMatch..minMatch+63
      const unsigned len1=len>minMatch*2+63 ? minMatch+63 :
          len>minMatch+63 ? len-minMatch : len;
      assert(wpos<BUFSIZE-5);
      assert(len1>=minMatch && len1<minMatch+64);
      if (off<(1<<16)) {
        put(64+len1-minMatch);
        put(off>>8);
        put(off);
      }
      else if (off<(1<<24)) {
        put(128+len1-minMatch);
        put(off>>16);
        put(off>>8);
        put(off);
      }
      else {
        put(192+len1-minMatch);
        put(off>>24);
        put(off>>16);
        put(off>>8);
        put(off);
      }
      len-=len1;
    }
  }
}

// Generate a config file from the method argument with syntax:
// {0|x|s|i}[N1[,N2]...][{ciamtswf<cfg>}[N1[,N2]]...]...
string makeConfig(const char* method, int args[]) {
  assert(method);
  const char type=method[0];
  assert(type=='x' || type=='s' || type=='0' || type=='i');

  // Read "{x|s|i|0}N1,N2...N9" into args[0..8] ($1..$9)
  args[0]=0;  // log block size in MiB
  args[1]=0;  // 0=none, 1=var-LZ77, 2=byte-LZ77, 3=BWT, 4..7 adds E8E9
  args[2]=0;  // lz77 minimum match length
  args[3]=0;  // secondary context length
  args[4]=0;  // log searches
  args[5]=0;  // lz77 hash table size or SA if args[0]+21
  args[6]=0;  // secondary context look ahead
  args[7]=0;  // not used
  args[8]=0;  // not used
  if (isdigit(*++method)) args[0]=0;
  for (int i=0; i<9 && (isdigit(*method) || *method==',' || *method=='.');) {
    if (isdigit(*method))
      args[i]=args[i]*10+*method-'0';
    else if (++i<9)
      args[i]=0;
    ++method;
  }

  // "0..." = No compression
  if (type=='0')
    return "comp 0 0 0 0 0 hcomp end\n";

  // Generate the postprocessor
  string hdr, pcomp;
  const int level=args[1]&3;
  const bool doe8=args[1]>=4 && args[1]<=7;

  // LZ77+Huffman, with or without E8E9
  if (level==1) {
    const int rb=args[0]>4 ? args[0]-4 : 0;
    hdr="comp 9 16 0 $1+20 ";
    pcomp=
    "pcomp lazy2 3 ;\n"
    " (r1 = state\n"
    "  r2 = len - match or literal length\n"
    "  r3 = m - number of offset bits expected\n"
    "  r4 = ptr to buf\n"
    "  r5 = r - low bits of offset\n"
    "  c = bits - input buffer\n"
    "  d = n - number of bits in c)\n"
    "\n"
    "  a> 255 if\n";
    if (doe8)
      pcomp+=
      "    b=0 d=r 4 do (for b=0..d-1, d = end of buf)\n"
      "      a=b a==d ifnot\n"
      "        a+= 4 a<d if\n"
      "          a=*b a&= 254 a== 232 if (e8 or e9?)\n"
      "            c=b b++ b++ b++ b++ a=*b a++ a&= 254 a== 0 if (00 or ff)\n"
      "              b-- a=*b\n"
      "              b-- a<<= 8 a+=*b\n"
      "              b-- a<<= 8 a+=*b\n"
      "              a-=b a++\n"
      "              *b=a a>>= 8 b++\n"
      "              *b=a a>>= 8 b++\n"
      "              *b=a b++\n"
      "            endif\n"
      "            b=c\n"
      "          endif\n"
      "        endif\n"
      "        a=*b out b++\n"
      "      forever\n"
      "    endif\n"
      "\n";
    pcomp+=
    "    (reset state)\n"
    "    a=0 b=0 c=0 d=0 r=a 1 r=a 2 r=a 3 r=a 4\n"
    "    halt\n"
    "  endif\n"
    "\n"
    "  a<<=d a+=c c=a               (bits+=a<<n)\n"
    "  a= 8 a+=d d=a                (n+=8)\n"
    "\n"
    "  (if state==0 (expect new code))\n"
    "  a=r 1 a== 0 if (match code mm,mmm)\n"
    "    a= 1 r=a 2                 (len=1)\n"
    "    a=c a&= 3 a> 0 if          (if (bits&3))\n"
    "      a-- a<<= 3 r=a 3           (m=((bits&3)-1)*8)\n"
    "      a=c a>>= 2 c=a             (bits>>=2)\n"
    "      b=r 3 a&= 7 a+=b r=a 3     (m+=bits&7)\n"
    "      a=c a>>= 3 c=a             (bits>>=3)\n"
    "      a=d a-= 5 d=a              (n-=5)\n"
    "      a= 1 r=a 1                 (state=1)\n"
    "    else (literal, discard 00)\n"
    "      a=c a>>= 2 c=a             (bits>>=2)\n"
    "      d-- d--                    (n-=2)\n"
    "      a= 3 r=a 1                 (state=3)\n"
    "    endif\n"
    "  endif\n"
    "\n"
    "  (while state==1 && n>=3 (expect match length n*4+ll -> r2))\n"
    "  do a=r 1 a== 1 if a=d a> 2 if\n"
    "    a=c a&= 1 a== 1 if         (if bits&1)\n"
    "      a=c a>>= 1 c=a             (bits>>=1)\n"
    "      b=r 2 a=c a&= 1 a+=b a+=b r=a 2 (len+=len+(bits&1))\n"
    "      a=c a>>= 1 c=a             (bits>>=1)\n"
    "      d-- d--                    (n-=2)\n"
    "    else\n"
    "      a=c a>>= 1 c=a             (bits>>=1)\n"
    "      a=r 2 a<<= 2 b=a           (len<<=2)\n"
    "      a=c a&= 3 a+=b r=a 2       (len+=bits&3)\n"
    "      a=c a>>= 2 c=a             (bits>>=2)\n"
    "      d-- d-- d--                (n-=3)\n";
    if (rb)
      pcomp+="      a= 5 r=a 1                 (state=5)\n";
    else
      pcomp+="      a= 2 r=a 1                 (state=2)\n";
    pcomp+=
    "    endif\n"
    "  forever endif endif\n"
    "\n";
    if (rb) pcomp+=  // save r in r5
      "  (if state==5 && n>=8) (expect low bits of offset to put in r5)\n"
      "  a=r 1 a== 5 if a=d a> "+itos(rb-1)+" if\n"
      "    a=c a&= "+itos((1<<rb)-1)+" r=a 5            (save r in r5)\n"
      "    a=c a>>= "+itos(rb)+" c=a\n"
      "    a=d a-= "+itos(rb)+ " d=a\n"
      "    a= 2 r=a 1                   (go to state 2)\n"
      "  endif endif\n"
      "\n";
    pcomp+=
    "  (if state==2 && n>=m) (expect m offset bits)\n"
    "  a=r 1 a== 2 if a=r 3 a>d ifnot\n"
    "    a=c r=a 6 a=d r=a 7          (save c=bits, d=n in r6,r7)\n"
    "    b=r 3 a= 1 a<<=b d=a         (d=1<<m)\n"
    "    a-- a&=c a+=d                (d=offset=bits&((1<<m)-1)|(1<<m))\n";
    if (rb)
      pcomp+=  // insert r into low bits of d
      "    a<<= "+itos(rb)+" d=r 5 a+=d a-= "+itos((1<<rb)-1)+"\n";
    pcomp+=
    "    d=a b=r 4 a=b a-=d c=a       (c=p=(b=ptr)-offset)\n"
    "\n"
    "    (while len-- (copy and output match d bytes from *c to *b))\n"
    "    d=r 2 do a=d a> 0 if d--\n"
    "      a=*c *b=a c++ b++          (buf[ptr++]-buf[p++])\n";
    if (!doe8) pcomp+=" out\n";
    pcomp+=
    "    forever endif\n"
    "    a=b r=a 4\n"
    "\n"
    "    a=r 6 b=r 3 a>>=b c=a        (bits>>=m)\n"
    "    a=r 7 a-=b d=a               (n-=m)\n"
    "    a=0 r=a 1                    (state=0)\n"
    "  endif endif\n"
    "\n"
    "  (while state==3 && n>=2 (expect literal length))\n"
    "  do a=r 1 a== 3 if a=d a> 1 if\n"
    "    a=c a&= 1 a== 1 if         (if bits&1)\n"
    "      a=c a>>= 1 c=a              (bits>>=1)\n"
    "      b=r 2 a&= 1 a+=b a+=b r=a 2 (len+=len+(bits&1))\n"
    "      a=c a>>= 1 c=a              (bits>>=1)\n"
    "      d-- d--                     (n-=2)\n"
    "    else\n"
    "      a=c a>>= 1 c=a              (bits>>=1)\n"
    "      d--                         (--n)\n"
    "      a= 4 r=a 1                  (state=4)\n"
    "    endif\n"
    "  forever endif endif\n"
    "\n"
    "  (if state==4 && n>=8 (expect len literals))\n"
    "  a=r 1 a== 4 if a=d a> 7 if\n"
    "    b=r 4 a=c *b=a\n";
    if (!doe8) pcomp+=" out\n";
    pcomp+=
    "    b++ a=b r=a 4                 (buf[ptr++]=bits)\n"
    "    a=c a>>= 8 c=a                (bits>>=8)\n"
    "    a=d a-= 8 d=a                 (n-=8)\n"
    "    a=r 2 a-- r=a 2 a== 0 if      (if --len<1)\n"
    "      a=0 r=a 1                     (state=0)\n"
    "    endif\n"
    "  endif endif\n"
    "  halt\n"
    "end\n";
  }

  // Byte aligned LZ77, with or without E8E9
  else if (level==2) {
    hdr="comp 9 16 0 $1+20 ";
    pcomp=
    "pcomp lzpre c ;\n"
    "  (Decode LZ77: d=state, M=output buffer, b=size)\n"
    "  a> 255 if (at EOF decode e8e9 and output)\n";
    if (doe8)
      pcomp+=
      "    d=b b=0 do (for b=0..d-1, d = end of buf)\n"
      "      a=b a==d ifnot\n"
      "        a+= 4 a<d if\n"
      "          a=*b a&= 254 a== 232 if (e8 or e9?)\n"
      "            c=b b++ b++ b++ b++ a=*b a++ a&= 254 a== 0 if (00 or ff)\n"
      "              b-- a=*b\n"
      "              b-- a<<= 8 a+=*b\n"
      "              b-- a<<= 8 a+=*b\n"
      "              a-=b a++\n"
      "              *b=a a>>= 8 b++\n"
      "              *b=a a>>= 8 b++\n"
      "              *b=a b++\n"
      "            endif\n"
      "            b=c\n"
      "          endif\n"
      "        endif\n"
      "        a=*b out b++\n"
      "      forever\n"
      "    endif\n";
    pcomp+=
    "    b=0 c=0 d=0 a=0 r=a 1 r=a 2 (reset state)\n"
    "  halt\n"
    "  endif\n"
    "\n"
    "  (in state d==0, expect a new code)\n"
    "  (put length in r1 and inital part of offset in r2)\n"
    "  c=a a=d a== 0 if\n"
    "    a=c a>>= 6 a++ d=a\n"
    "    a== 1 if (literal?)\n"
    "      a+=c r=a 1 a=0 r=a 2\n"
    "    else (3 to 5 byte match)\n"
    "      d++ a=c a&= 63 a+= $3 r=a 1 a=0 r=a 2\n"
    "    endif\n"
    "  else\n"
    "    a== 1 if (writing literal)\n"
    "      a=c *b=a b++\n";
    if (!doe8) pcomp+=" out\n";
    pcomp+=
    "      a=r 1 a-- a== 0 if d=0 endif r=a 1 (if (--len==0) state=0)\n"
    "    else\n"
    "      a> 2 if (reading offset)\n"
    "        a=r 2 a<<= 8 a|=c r=a 2 d-- (off=off<<8|c, --state)\n"
    "      else (state==2, write match)\n"
    "        a=r 2 a<<= 8 a|=c c=a a=b a-=c a-- c=a (c=i-off-1)\n"
    "        d=r 1 (d=len)\n"
    "        do (copy and output d=len bytes)\n"
    "          a=*c *b=a c++ b++\n";
    if (!doe8) pcomp+=" out\n";
    pcomp+=
    "        d-- a=d a> 0 while\n"
    "        (d=state=0. off, len don\'t matter)\n"
    "      endif\n"
    "    endif\n"
    "  endif\n"
    "  halt\n"
    "end\n";
  }

  // BWT with or without E8E9
  else if (level==3) {  // IBWT
    hdr="comp 9 16 $1+20 $1+20 ";  // 2^$1 = block size in MB
    pcomp=
    "pcomp bwtrle c ;\n"
    "\n"
    "  (read BWT, index into M, size in b)\n"
    "  a> 255 ifnot\n"
    "    *b=a b++\n"
    "\n"
    "  (inverse BWT)\n"
    "  elsel\n"
    "\n"
    "    (index in last 4 bytes, put in c and R1)\n"
    "    b-- a=*b\n"
    "    b-- a<<= 8 a+=*b\n"
    "    b-- a<<= 8 a+=*b\n"
    "    b-- a<<= 8 a+=*b c=a r=a 1\n"
    "\n"
    "    (save size in R2)\n"
    "    a=b r=a 2\n"
    "\n"
    "    (count bytes in H[~1..~255, ~0])\n"
    "    do\n"
    "      a=b a> 0 if\n"
    "        b-- a=*b a++ a&= 255 d=a d! *d++\n"
    "      forever\n"
    "    endif\n"
    "\n"
    "    (cumulative counts: H[~i=0..255] = count of bytes before i)\n"
    "    d=0 d! *d= 1 a=0\n"
    "    do\n"
    "      a+=*d *d=a d--\n"
    "    d<>a a! a> 255 a! d<>a until\n"
    "\n"
    "    (build first part of linked list in H[0..idx-1])\n"
    "    b=0 do\n"
    "      a=c a>b if\n"
    "        d=*b d! *d++ d=*d d-- *d=b\n"
    "      b++ forever\n"
    "    endif\n"
    "\n"
    "    (rest of list in H[idx+1..n-1])\n"
    "    b=c b++ c=r 2 do\n"
    "      a=c a>b if\n"
    "        d=*b d! *d++ d=*d d-- *d=b\n"
    "      b++ forever\n"
    "    endif\n"
    "\n";
    if (args[0]<=4) {  // faster IBWT list traversal limited to 16 MB blocks
      pcomp+=
      "    (copy M to low 8 bits of H to reduce cache misses in next loop)\n"
      "    b=0 do\n"
      "      a=c a>b if\n"
      "        d=b a=*d a<<= 8 a+=*b *d=a\n"
      "      b++ forever\n"
      "    endif\n"
      "\n"
      "    (traverse list and output or copy to M)\n"
      "    d=r 1 b=0 do\n"
      "      a=d a== 0 ifnot\n"
      "        a=*d a>>= 8 d=a\n";
      if (doe8) pcomp+=" *b=*d b++\n";
      else      pcomp+=" a=*d out\n";
      pcomp+=
      "      forever\n"
      "    endif\n"
      "\n";
      if (doe8)  // IBWT+E8E9
        pcomp+=
        "    (e8e9 transform to out)\n"
        "    d=b b=0 do (for b=0..d-1, d = end of buf)\n"
        "      a=b a==d ifnot\n"
        "        a+= 4 a<d if\n"
        "          a=*b a&= 254 a== 232 if\n"
        "            c=b b++ b++ b++ b++ a=*b a++ a&= 254 a== 0 if\n"
        "              b-- a=*b\n"
        "              b-- a<<= 8 a+=*b\n"
        "              b-- a<<= 8 a+=*b\n"
        "              a-=b a++\n"
        "              *b=a a>>= 8 b++\n"
        "              *b=a a>>= 8 b++\n"
        "              *b=a b++\n"
        "            endif\n"
        "            b=c\n"
        "          endif\n"
        "        endif\n"
        "        a=*b out b++\n"
        "      forever\n"
        "    endif\n";
      pcomp+=
      "  endif\n"
      "  halt\n"
      "end\n";
    }
    else {  // slower IBWT list traversal for all sized blocks
      if (doe8) {  // E8E9 after IBWT
        pcomp+=
        "    (R2 = output size without EOS)\n"
        "    a=r 2 a-- r=a 2\n"
        "\n"
        "    (traverse list (d = IBWT pointer) and output inverse e8e9)\n"
        "    (C = offset = 0..R2-1)\n"
        "    (R4 = last 4 bytes shifted in from MSB end)\n"
        "    (R5 = temp pending output byte)\n"
        "    c=0 d=r 1 do\n"
        "      a=d a== 0 ifnot\n"
        "        d=*d\n"
        "\n"
        "        (store byte in R4 and shift out to R5)\n"
        "        b=d a=*b a<<= 24 b=a\n"
        "        a=r 4 r=a 5 a>>= 8 a|=b r=a 4\n"
        "\n"
        "        (if E8|E9 xx xx xx 00|FF in R4:R5 then subtract c from x)\n"
        "        a=c a> 3 if\n"
        "          a=r 5 a&= 254 a== 232 if\n"
        "            a=r 4 a>>= 24 b=a a++ a&= 254 a< 2 if\n"
        "              a=r 4 a-=c a+= 4 a<<= 8 a>>= 8 \n"
        "              b<>a a<<= 24 a+=b r=a 4\n"
        "            endif\n"
        "          endif\n"
        "        endif\n"
        "\n"
        "        (output buffered byte)\n"
        "        a=c a> 3 if a=r 5 out endif c++\n"
        "\n"
        "      forever\n"
        "    endif\n"
        "\n"
        "    (output up to 4 pending bytes in R4)\n"
        "    b=r 4\n"
        "    a=c a> 3 a=b if out endif a>>= 8 b=a\n"
        "    a=c a> 2 a=b if out endif a>>= 8 b=a\n"
        "    a=c a> 1 a=b if out endif a>>= 8 b=a\n"
        "    a=c a> 0 a=b if out endif\n"
        "\n"
        "  endif\n"
        "  halt\n"
        "end\n";
      }
      else {
        pcomp+=
        "    (traverse list and output)\n"
        "    d=r 1 do\n"
        "      a=d a== 0 ifnot\n"
        "        d=*d\n"
        "        b=d a=*b out\n"
        "      forever\n"
        "    endif\n"
        "  endif\n"
        "  halt\n"
        "end\n";
      }
    }
  }

  // E8E9 or no preprocessing
  else if (level==0) {
    hdr="comp 9 16 0 0 ";
    if (doe8) { // E8E9?
      pcomp=
      "pcomp e8e9 d ;\n"
      "  a> 255 if\n"
      "    a=c a> 4 if\n"
      "      c= 4\n"
      "    else\n"
      "      a! a+= 5 a<<= 3 d=a a=b a>>=d b=a\n"
      "    endif\n"
      "    do a=c a> 0 if\n"
      "      a=b out a>>= 8 b=a c--\n"
      "    forever endif\n"
      "  else\n"
      "    *b=b a<<= 24 d=a a=b a>>= 8 a+=d b=a c++\n"
      "    a=c a> 4 if\n"
      "      a=*b out\n"
      "      a&= 254 a== 232 if\n"
      "        a=b a>>= 24 a++ a&= 254 a== 0 if\n"
      "          a=b a>>= 24 a<<= 24 d=a\n"
      "          a=b a-=c a+= 5\n"
      "          a<<= 8 a>>= 8 a|=d b=a\n"
      "        endif\n"
      "      endif\n"
      "    endif\n"
      "  endif\n"
      "  halt\n"
      "end\n";
    }
    else
      pcomp="end\n";
  }
  else
    error("Unsupported method");
  
  // Build context model (comp, hcomp) assuming:
  // H[0..254] = contexts
  // H[255..511] = location of last byte i-255
  // M = last 64K bytes, filling backward
  // C = pointer to most recent byte
  // R1 = level 2 lz77 1+bytes expected until next code, 0=init
  // R2 = level 2 lz77 first byte of code
  int ncomp=0;  // number of components
  const int membits=args[0]+20;
  int sb=5;  // bits in last context
  string comp;
  string hcomp="hcomp\n"
    "c-- *c=a a+= 255 d=a *d=c\n";
  if (level==2) {  // put level 2 lz77 parse state in R1, R2
    hcomp+=
    "  (decode lz77 into M. Codes:\n"
    "  00xxxxxx = literal length xxxxxx+1\n"
    "  xx......, xx > 0 = match with xx offset bytes to follow)\n"
    "\n"
    "  a=r 1 a== 0 if (init)\n"
    "    a= "+itos(111+57*doe8)+" (skip post code)\n"
    "  else a== 1 if  (new code?)\n"
    "    a=*c r=a 2  (save code in R2)\n"
    "    a> 63 if a>>= 6 a++ a++  (match)\n"
    "    else a++ a++ endif  (literal)\n"
    "  else (read rest of code)\n"
    "    a--\n"
    "  endif endif\n"
    "  r=a 1  (R1 = 1+expected bytes to next code)\n";
  }

  // Generate the context model
  while (*method && ncomp<254) {

    // parse command C[N1[,N2]...] into v = {C, N1, N2...}
    vector<int> v;
    v.push_back(*method++);
    if (isdigit(*method)) {
      v.push_back(*method++-'0');
      while (isdigit(*method) || *method==',' || *method=='.') {
        if (isdigit(*method))
          v.back()=v.back()*10+*method++-'0';
        else {
          v.push_back(0);
          ++method;
        }
      }
    }

    // c: context model
    // N1%1000: 0=ICM 1..256=CM limit N1-1
    // N1/1000: number of times to halve memory
    // N2: 1..255=offset mod N2. 1000..1255=distance to N2-1000
    // N3...: 0..255=byte mask + 256=lz77 state. 1000+=run of N3-1000 zeros.
    if (v[0]=='c') {
      while (v.size()<3) v.push_back(0);
      comp+=itos(ncomp)+" ";
      sb=11;  // count context bits
      if (v[2]<256) sb+=lg(v[2]);
      else sb+=6;
      for (unsigned i=3; i<v.size(); ++i)
        if (v[i]<512) sb+=nbits(v[i])*3/4;
      if (sb>membits) sb=membits;
      if (v[1]%1000==0) comp+="icm "+itos(sb-6-v[1]/1000)+"\n";
      else comp+="cm "+itos(sb-2-v[1]/1000)+" "+itos(v[1]%1000-1)+"\n";

      // special contexts
      hcomp+="d= "+itos(ncomp)+" *d=0\n";
      if (v[2]>1 && v[2]<=255) {  // periodic context
        if (lg(v[2])!=lg(v[2]-1))
          hcomp+="a=c a&= "+itos(v[2]-1)+" hashd\n";
        else
          hcomp+="a=c a%= "+itos(v[2])+" hashd\n";
      }
      else if (v[2]>=1000 && v[2]<=1255)  // distance context
        hcomp+="a= 255 a+= "+itos(v[2]-1000)+
               " d=a a=*d a-=c a> 255 if a= 255 endif d= "+
               itos(ncomp)+" hashd\n";

      // Masked context
      for (unsigned i=3; i<v.size(); ++i) {
        if (i==3) hcomp+="b=c ";
        if (v[i]==255)
          hcomp+="a=*b hashd\n";  // ordinary byte
        else if (v[i]>0 && v[i]<255)
          hcomp+="a=*b a&= "+itos(v[i])+" hashd\n";  // masked byte
        else if (v[i]>=256 && v[i]<512) { // lz77 state or masked literal byte
          hcomp+=
          "a=r 1 a> 1 if\n"  // expect literal or offset
          "  a=r 2 a< 64 if\n"  // expect literal
          "    a=*b ";
          if (v[i]<511) hcomp+="a&= "+itos(v[i]-256);
          hcomp+=" hashd\n"
          "  else\n"  // expect match offset byte
          "    a>>= 6 hashd a=r 1 hashd\n"
          "  endif\n"
          "else\n"  // expect new code
          "  a= 255 hashd a=r 2 hashd\n"
          "endif\n";
        }
        else if (v[i]>=1256)  // skip v[i]-1000 bytes
          hcomp+="a= "+itos(((v[i]-1000)>>8)&255)+" a<<= 8 a+= "
               +itos((v[i]-1000)&255)+
          " a+=b b=a\n";
        else if (v[i]>1000)
          hcomp+="a= "+itos(v[i]-1000)+" a+=b b=a\n";
        if (v[i]<512 && i<v.size()-1)
          hcomp+="b++ ";
      }
      ++ncomp;
    }

    // m,8,24: MIX, size, rate
    // t,8,24: MIX2, size, rate
    // s,8,32,255: SSE, size, start, limit
    if (strchr("mts", v[0]) && ncomp>int(v[0]=='t')) {
      if (v.size()<=1) v.push_back(8);
      if (v.size()<=2) v.push_back(24+8*(v[0]=='s'));
      if (v[0]=='s' && v.size()<=3) v.push_back(255);
      comp+=itos(ncomp);
      sb=5+v[1]*3/4;
      if (v[0]=='m')
        comp+=" mix "+itos(v[1])+" 0 "+itos(ncomp)+" "+itos(v[2])+" 255\n";
      else if (v[0]=='t')
        comp+=" mix2 "+itos(v[1])+" "+itos(ncomp-1)+" "+itos(ncomp-2)
            +" "+itos(v[2])+" 255\n";
      else // s
        comp+=" sse "+itos(v[1])+" "+itos(ncomp-1)+" "+itos(v[2])+" "
            +itos(v[3])+"\n";
      if (v[1]>8) {
        hcomp+="d= "+itos(ncomp)+" *d=0 b=c a=0\n";
        for (; v[1]>=16; v[1]-=8) {
          hcomp+="a<<= 8 a+=*b";
          if (v[1]>16) hcomp+=" b++";
          hcomp+="\n";
        }
        if (v[1]>8)
          hcomp+="a<<= 8 a+=*b a>>= "+itos(16-v[1])+"\n";
        hcomp+="a<<= 8 *d=a\n";
      }
      ++ncomp;
    }

    // i: ISSE chain with order increasing by N1,N2...
    if (v[0]=='i' && ncomp>0) {
      assert(sb>=5);
      hcomp+="d= "+itos(ncomp-1)+" b=c a=*d d++\n";
      for (unsigned i=1; i<v.size() && ncomp<254; ++i) {
        for (int j=0; j<v[i]%10; ++j) {
          hcomp+="hash ";
          if (i<v.size()-1 || j<v[i]%10-1) hcomp+="b++ ";
          sb+=6;
        }
        hcomp+="*d=a";
        if (i<v.size()-1) hcomp+=" d++";
        hcomp+="\n";
        if (sb>membits) sb=membits;
        comp+=itos(ncomp)+" isse "+itos(sb-6-v[i]/10)+" "+itos(ncomp-1)+"\n";
        ++ncomp;
      }
    }

    // a24,0,0: MATCH. N1=hash multiplier. N2,N3=halve buf, table.
    if (v[0]=='a') {
      if (v.size()<=1) v.push_back(24);
      while (v.size()<4) v.push_back(0);
      comp+=itos(ncomp)+" match "+itos(membits-v[3]-2)+" "
          +itos(membits-v[2])+"\n";
      hcomp+="d= "+itos(ncomp)+" a=*d a*= "+itos(v[1])
           +" a+=*c a++ *d=a\n";
      sb=5+(membits-v[2])*3/4;
      ++ncomp;
    }

    // w1,65,26,223,20,0: ICM-ISSE chain of length N1 with word contexts,
    // where a word is a sequence of c such that c&N4 is in N2..N2+N3-1.
    // Word is hashed by: hash := hash*N5+c+1
    // Decrease memory by 2^-N6.
    if (v[0]=='w') {
      if (v.size()<=1) v.push_back(1);
      if (v.size()<=2) v.push_back(65);
      if (v.size()<=3) v.push_back(26);
      if (v.size()<=4) v.push_back(223);
      if (v.size()<=5) v.push_back(20);
      if (v.size()<=6) v.push_back(0);
      comp+=itos(ncomp)+" icm "+itos(membits-6-v[6])+"\n";
      for (int i=1; i<v[1]; ++i)
        comp+=itos(ncomp+i)+" isse "+itos(membits-6-v[6])+" "
            +itos(ncomp+i-1)+"\n";
      hcomp+="a=*c a&= "+itos(v[4])+" a-= "+itos(v[2])+" a&= 255 a< "
           +itos(v[3])+" if\n";
      for (int i=0; i<v[1]; ++i) {
        if (i==0) hcomp+="  d= "+itos(ncomp);
        else hcomp+="  d++";
        hcomp+=" a=*d a*= "+itos(v[5])+" a+=*c a++ *d=a\n";
      }
      hcomp+="else\n";
      for (int i=v[1]-1; i>0; --i)
        hcomp+="  d= "+itos(ncomp+i-1)+" a=*d d++ *d=a\n";
      hcomp+="  d= "+itos(ncomp)+" *d=0\n"
           "endif\n";
      ncomp+=v[1]-1;
      sb=membits-v[6];
      ++ncomp;
    }

    // Read from config file and ignore rest of command
    if (v[0]=='f') {
      string filename=method;  // append .cfg if not already
      int len=filename.size();
      if (len<=4 || filename.substr(len-4)!=".cfg") filename+=".cfg";
      FILE* in=fopen(filename.c_str(), "r");
      if (!in) {
        perror(filename.c_str());
        error("Config file not found");
      }
      string cfg;
      int c;
      while ((c=getc(in))!=EOF) cfg+=(char)c;
      fclose(in);
      return cfg;
    }
  }
  return hdr+itos(ncomp)+"\n"+comp+hcomp+"halt\n"+pcomp;
}

// Compress from in to out in 1 segment in 1 block using the algorithm
// descried in method. If method begins with a digit then choose
// a method depending on type. Save filename and comment
// in the segment header. If comment is 0 then the default is the input size
// as a decimal string, plus " jDC\x01" for a journaling method (method[0]
// is not 's'). type is set as follows: bits 9-2 estimate compressibility
// where 0 means random. Bit 1 indicates x86 (exe or dll) and bit 0 
// indicates English text.
string compressBlock(StringBuffer* in, libzpaq::Writer* out, string method,
                     const char* filename=0, const char* comment=0,
                     unsigned type=512) {
  assert(in);
  assert(out);
  assert(method!="");
  const unsigned n=in->size();  // input size
  const int arg0=max(lg(n+4095)-20, 0);  // block size
  assert((1u<<(arg0+20))>=n+4096);

  // Get hash of input
  libzpaq::SHA1 sha1;
  const char* sha1ptr=0;
  if (!fragile) {
    for (const char* p=in->c_str(), *end=p+n; p<end; ++p)
      sha1.put(*p);
    sha1ptr=sha1.result();
  }

  // Expand default methods
  if (isdigit(method[0])) {
    const int level=method[0]-'0';
    assert(level>=0 && level<=9);

    // build models
    const int doe8=(type&2)*2;
    method="x"+itos(arg0);
    string htsz=","+itos(19+arg0+(arg0<=6));  // lz77 hash table size
    string sasz=","+itos(21+arg0);            // lz77 suffix array size

    // store uncompressed
    if (level==0)
      method="0"+itos(arg0)+",0";

    // LZ77, no model. Store if hard to compress
    else if (level==1) {
      if (type<40) method+=",0";
      else {
        method+=","+itos(1+doe8)+",";
        if      (type<80)  method+="4,0,1,15";
        else if (type<128) method+="4,0,2,16";
        else if (type<256) method+="4,0,2"+htsz;
        else if (type<960) method+="5,0,3"+htsz;
        else               method+="6,0,3"+htsz;
      }
    }

    // LZ77 with longer search
    else if (level==2) {
      if (type<32) method+=",0";
      else {
        method+=","+itos(1+doe8)+",";
        if (type<64) method+="4,0,3"+htsz;
        else method+="4,0,7"+sasz+",1";
      }
    }

    // LZ77 with CM depending on redundancy
    else if (level==3) {
      if (type<20)  // store if not compressible
        method+=",0";
      else if (type<48)  // fast LZ77 if barely compressible
        method+=","+itos(1+doe8)+",4,0,3"+htsz;
      else if (type>=640 || (type&1))  // BWT if text or highly compressible
        method+=","+itos(3+doe8)+"ci1";
      else  // LZ77 with O0-1 compression of up to 12 literals
        method+=","+itos(2+doe8)+",12,0,7"+sasz+",1c0,0,511i2";
    }

    // LZ77+CM, fast CM, or BWT depending on type
    else if (level==4) {
      if (type<12)
        method+=",0";
      else if (type<24)
        method+=","+itos(1+doe8)+",4,0,3"+htsz;
      else if (type<48)
        method+=","+itos(2+doe8)+",5,0,7"+sasz+"1c0,0,511";
      else if (type<900) {
        method+=","+itos(doe8)+"ci1,1,1,1,2a";
        if (type&1) method+="w";
        method+="m";
      }
      else
        method+=","+itos(3+doe8)+"ci1";
    }

    // Slow CM with lots of models
    else {  // 5..9

      // Model text files
      method+=","+itos(doe8);
      if (type&1) method+="w2c0,1010,255i1";
      else method+="w1i1";
      method+="c256ci1,1,1,1,1,1,2a";

      // Analyze the data
      const int NR=1<<12;
      int pt[256]={0};  // position of last occurrence
      int r[NR]={0};    // count repetition gaps of length r
      const unsigned char* p=in->data();
      if (level>0) {
        for (unsigned i=0; i<n; ++i) {
          const int k=i-pt[p[i]];
          if (k>0 && k<NR) ++r[k];
          pt[p[i]]=i;
        }
      }

      // Add periodic models
      int n1=n-r[1]-r[2]-r[3];
      for (int i=0; i<2; ++i) {
        int period=0;
        double score=0;
        int t=0;
        for (int j=5; j<NR && t<n1; ++j) {
          const double s=r[j]/(256.0+n1-t);
          if (s>score) score=s, period=j;
          t+=r[j];
        }
        if (period>4 && score>0.1) {
          method+="c0,0,"+itos(999+period)+",255i1";
          if (period<=255)
            method+="c0,"+itos(period)+"i1";
          n1-=r[period];
          r[period]=0;
        }
        else
          break;
      }
      method+="c0,2,0,255i1c0,3,0,0,255i1c0,4,0,0,0,255i1mm16ts19t0";
    }
  }

  // Compress
  string config;
  int args[9]={0};
  try {

    // Get config
    config=makeConfig(method.c_str(), args);
    assert(n<=(0x100000u<<args[0])-4096);

    // Compress in to out using config
    libzpaq::Compressor co;
    co.setOutput(out);
#ifdef DEBUG
    if (!fragile) co.setVerify(true);
#endif
    StringBuffer pcomp_cmd;
    if (!fragile) co.writeTag();
    co.startBlock(config.c_str(), args, &pcomp_cmd);
    string cs=itos(n);
    if (method[0]!='s') cs+=" jDC\x01";
    if (comment) cs=comment;
    co.startSegment(filename, cs.c_str());
    if (args[1]>=1 && args[1]<=7 && args[1]!=4) {  // LZ77 or BWT
      LZBuffer lz(*in, args);
      co.setInput(&lz);
      co.compress();
    }
    else {  // compress with e8e9 or no preprocessing
      if (args[1]>=4 && args[1]<=7)
        e8e9(in->data(), in->size());
      co.setInput(in);
      co.compress();
    }
    in->reset();
#ifdef DEBUG  // verify pre-post processing are inverses
    if (fragile)
      co.endSegment(0);
    else {
      int64_t outsize;
      const char* sha1result=co.endSegmentChecksum(&outsize);
      assert(sha1result);
      assert(sha1ptr);
      if (memcmp(sha1result, sha1ptr, 20)!=0) {
        fprintf(stderr, "pre size=%d post size=%1.0f method=%s\n",
                n, double(outsize), method.c_str());
        error("Pre/post-processor test failed");
      }
    }
#else
    co.endSegment(sha1ptr);
#endif
    co.endBlock();
  }
  catch(std::exception& e) {
    fprintf(con, "Compression error %s\n", e.what());
    fprintf(con, "\nconfig:\n%s\n", config.c_str());
    fprintf(con, "\nmethod=%s\n", method.c_str());
    for (int i=0; i<9; ++i)
      fprintf(con, "args[%d] = $%d = %d\n", i, i+1, args[i]);
    error("compression error");
  }
  return method;
}

// A CompressJob is a queue of blocks to compress and write to the archive.
// Each block cycles through states EMPTY, FILLING, FULL, COMPRESSING,
// COMPRESSED, WRITING. The main thread waits for EMPTY buffers and
// fills them. A set of compressThreads waits for FULL threads and compresses
// them. A writeThread waits for COMPRESSED buffers at the front
// of the queue and writes and removes them.

// Buffer queue element
struct CJ {
  enum {EMPTY, FULL, COMPRESSING, COMPRESSED, WRITING} state;
  StringBuffer in;       // uncompressed input
  WriteBuffer out;       // compressed output
  string filename;       // to write in filename field
  string comment;        // if "" use default
  string method;         // compression level or "" to mark end of data
  int type;              // redundancy*4 + exe*2 + text
  Semaphore full;        // 1 if in is FULL of data ready to compress
  Semaphore compressed;  // 1 if out contains COMPRESSED data
  CJ(): state(EMPTY), type(512) {}
};

// Instructions to a compression job
class CompressJob {
public:
  Mutex mutex;           // protects state changes
private:
  int job;               // number of jobs
  CJ* q;                 // buffer queue
  unsigned qsize;        // number of elements in q
  int front;             // next to remove from queue
  libzpaq::Writer* out;  // archive
  Semaphore empty;       // number of empty buffers ready to fill
  Semaphore compressors; // number of compressors available to run
public:
  friend ThreadReturn compressThread(void* arg);
  friend ThreadReturn writeThread(void* arg);
  CompressJob(int threads, int buffers, libzpaq::Writer* f):
      job(0), q(0), qsize(buffers), front(0), out(f) {
    q=new CJ[buffers];
    if (!q) throw std::bad_alloc();
    init_mutex(mutex);
    empty.init(buffers);
    compressors.init(threads);
    for (int i=0; i<buffers; ++i) {
      q[i].full.init(0);
      q[i].compressed.init(0);
    }
  }
  ~CompressJob() {
    for (int i=qsize-1; i>=0; --i) {
      q[i].compressed.destroy();
      q[i].full.destroy();
    }
    compressors.destroy();
    empty.destroy();
    destroy_mutex(mutex);
    delete[] q;
  }      
  void write(StringBuffer& s, const char* filename, string method,
             int hits=-1, const char* comment=0);
  vector<int> csize;  // compressed block sizes
};

// Write s at the back of the queue. Signal end of input with method=""
void CompressJob::write(StringBuffer& s, const char* fn, string method,
                        int type, const char* comment) {
  for (unsigned k=(method=="")?qsize:1; k>0; --k) {
    empty.wait();
    lock(mutex);
    unsigned i, j;
    for (i=0; i<qsize; ++i) {
      if (q[j=(i+front)%qsize].state==CJ::EMPTY) {
        q[j].filename=fn?fn:"";
        q[j].comment=comment?comment:"";
        q[j].method=method;
        q[j].type=type;
        q[j].in.reset();
        q[j].in.swap(s);
        q[j].state=CJ::FULL;
        q[j].full.signal();
        break;
      }
    }
    release(mutex);
    assert(i<qsize);  // queue should not be full
  }
}

// Global progress indicator
volatile int64_t total_size=0;  // number of input bytes to process
volatile int64_t bytes_processed=0;  // bytes compressed or decompressed
volatile int64_t bytes_output=0;  // output bytes compressed

// Compress data in the background, one per buffer
ThreadReturn compressThread(void* arg) {
  CompressJob& job=*(CompressJob*)arg;
  int jobNumber=0;
  try {

    // Get job number = assigned position in queue
    lock(job.mutex);
    jobNumber=job.job++;
    assert(jobNumber>=0 && jobNumber<int(job.qsize));
    CJ& cj=job.q[jobNumber];
    release(job.mutex);

    // Work until done
    while (true) {
      cj.full.wait();
      lock(job.mutex);

      // Check for end of input
      if (cj.method=="") {
        cj.compressed.signal();
        release(job.mutex);
        return 0;
      }

      // Compress
      assert(cj.state==CJ::FULL);
      cj.state=CJ::COMPRESSING;
      int insize=cj.in.size(), start=0, frags=0;
      if (insize>=8 && size(cj.filename)==28 && cj.comment==""
          && cj.filename.substr(0, 3)=="jDC" && cj.filename[17]=='d') {
        const char* p=cj.in.c_str()+insize-8;
        start=btoi(p);
        frags=btoi(p);
        if (!start)
          start=atoi(cj.filename.c_str()+18);
      }
      release(job.mutex);
      int64_t now=mtime();
      job.compressors.wait();
      string m=compressBlock(&cj.in, &cj.out, cj.method, cj.filename.c_str(),
          cj.comment=="" ? 0 : cj.comment.c_str(), cj.type);
      job.compressors.signal();
      lock(job.mutex);
      bytes_processed+=insize-8-4*frags;
      bytes_output+=cj.out.size();
      int64_t eta=(mtime()-global_start+0.0)
           *(total_size-bytes_processed)/(bytes_processed+0.5)/1000.0;
      if (bytes_processed>0)
        fprintf(con, "%d:%02d:%02d",
            int(eta/3600), int(eta/60%60), int(eta%60));
      if (quiet==MAX_QUIET-1) {
        fprintf(con, " to go: %1.6f -> %1.6f MB (%5.2f%%)     \r",
            bytes_processed/1000000.0, bytes_output/1000000.0,
            (bytes_processed+0.5)*100.0/(total_size+0.5));
        fflush(con);
      }
      else {
        fprintf(con, " ");
        if (cj.comment!="") printUTF8(cj.filename.c_str(), con);
        else if (frags==0) fprintf(con, "[%d...]", start);
        else fprintf(con, "[%d-%d]", start, start+frags-1);
        fprintf(con,
            " %d -> %d (%1.2fs), %d%c %s\n",
            insize, int(cj.out.size()), (mtime()-now)*0.001,
            cj.type/4, " teb"[cj.type&3], m.c_str());
      }
      cj.in.reset();
      cj.state=CJ::COMPRESSED;
      cj.compressed.signal();
      release(job.mutex);
    }
  }
  catch (std::exception& e) {
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
      if (cj.method=="") {
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
        cj.out.save(job.out);
        cj.out.reset();
        lock(job.mutex);
      }
      cj.state=CJ::EMPTY;
      job.front=(job.front+1)%job.qsize;
      job.empty.signal();
      release(job.mutex);
    }
  }
  catch (std::exception& e) {
    fprintf(stderr, "zpaq exiting from writeThread: %s\n", e.what());
    exit(1);
  }
  return 0;
}

// Write a ZPAQ compressed JIDAC block header. Output size should not
// depend on input data.
void writeJidacHeader(libzpaq::Writer *out, int64_t date,
                      int64_t cdata, unsigned htsize) {
  if (!out) return;
  assert(date>=19700000000000LL && date<30000000000000LL);
  StringBuffer is;
  is+=ltob(cdata);
  compressBlock(&is, out, "0",
                ("jDC"+itos(date, 14)+"c"+itos(htsize, 10)).c_str());
}

// Maps sha1 -> fragment ID in ht with known size
class HTIndex {
  enum {N=1<<22};   // size of hash table t
  vector<HT>& htr;  // reference to ht
  vector<vector<unsigned> > t;  // sha1 prefix -> list of indexes
  unsigned htsize;  // number of IDs in t

  // Compuate a hash index for sha1[20]
  unsigned hash(const unsigned char* sha1) {
    return (sha1[0]|(sha1[1]<<8)|(sha1[2]<<16))&(N-1);
  }

public:
  HTIndex(vector<HT>& r): htr(r), t(N), htsize(0) {
    update();
  }

  // Find sha1 in ht. Return its index or 0 if not found.
  unsigned find(const char* sha1) {
    vector<unsigned>& v=t[hash((const unsigned char*)sha1)];
    for (unsigned i=0; i<v.size(); ++i)
      if (memcmp(sha1, htr[v[i]].sha1, 20)==0)
        return v[i];
    return 0;
  }

  // Update index of ht. Do not index if fragment size is unknown.
  void update() {
    for (; htsize<htr.size(); ++htsize)
      if (htr[htsize].csize!=HT_BAD && htr[htsize].usize>=0)
        t[hash(htr[htsize].sha1)].push_back(htsize);
  }    
};

// Sort by sortkey, then by full path
bool compareFilename(DTMap::iterator ap, DTMap::iterator bp) {
  if (ap->second.sortkey!=bp->second.sortkey)
    return ap->second.sortkey<bp->second.sortkey;
  return ap->first<bp->first;
}

// For writing to two archives at once
struct WriterPair: public libzpaq::Writer {
  libzpaq::Writer *a, *b;
  void put(int c) {
    if (a) a->put(c);
    if (b) b->put(c);
  }
  void write(const char* buf, int n) {
    if (a) a->write(buf, n);
    if (b) b->write(buf, n);
  }
  WriterPair(): a(0), b(0) {}
};

// Add or delete files from archive. Return 1 if error else 0.
int Jidac::add() {

  // Read archive (preferred) or index into ht, dt, ver.
  int errors=0;
  int64_t header_pos=0;  // end of archive
  int64_t index_pos=0;   // end of index
  const string part1=subpart(archive, 1);
  const string part0=subpart(archive, 0);
  if (exists(part1)) {
    if (part0!=part1 && exists(part0)) {  // compare archive with index
      Jidac jidac(*this);
      header_pos=read_archive(&errors);
      index_pos=jidac.read_archive(&errors, part0.c_str());
      if (index_pos+dhsize!=header_pos || ver.size()!=jidac.ver.size()) {
        fprintf(stderr, "Index ");
        printUTF8(part0.c_str(), stderr);
        fprintf(stderr, " shows %1.0f bytes in %d versions\n"
            " but archive has %1.0f bytes in %d versions.\n",
            index_pos+dhsize+0.0, size(jidac.ver)-1,
            header_pos+0.0, size(ver)-1);
        error("index does not match multi-part archive");
      }
    }
    else {  // archive with no index
      header_pos=read_archive(&errors);
      index_pos=header_pos-dhsize;
    }
  }
  else if (exists(part0)) {  // read index of remote archive
    index_pos=read_archive(&errors, part0.c_str());
    if (dcsize!=0) error("index contains data");
    dcsize=dhsize;  // assumed
    header_pos=index_pos+dhsize;
    printUTF8(part0.c_str(), con);
    fprintf(con, ": assuming %1.0f bytes in %d versions\n",
        dhsize+index_pos+0.0, size(ver)-1);
  }

  // Set method and block size
  if (method=="") {  // set default method
    if (dhsize>0 && dcsize==0) method="i";  // index 
    else method="1";  // archive
  }
  if (size(method)==1) {  // set default blocksize
    if (method[0]>='2' && method[0]<='9') method+="6";
    else method+="4";
  }
  fprintf(con, "Compressing with -method %s\n", method.c_str());
  if (strchr("0123456789xsi", method[0])==0)
    error("-method must begin with 0..5, x, s, or i");
  assert(size(method)>=2);
  unsigned blocksize=(1u<<(20+atoi(method.c_str()+1)))-4096;
  if (fragment<0 || fragment>19 || (1u<<(12+fragment))>blocksize)
    error("fragment size too large");

  // Don't mix archives and indexes
  if (method[0]=='i' && dcsize>0) error("archive is not an index");
  if (method[0]!='i' && dcsize!=dhsize) error("archive is an index");

  // Make list of files to add or delete
  read_args();
  for (int i=0; i<size(files); ++i)
    scandir(files[i].c_str(), true);

  // Sort the files to be added by filename extension and decreasing size
  vector<DTMap::iterator> vf;
  unsigned deletions=0;
  total_size=0;
  for (DTMap::iterator p=dt.begin(); p!=dt.end(); ++p) {
    if (p->second.edate && (force || p->second.dtv.size()==0
       || p->second.edate!=p->second.dtv.back().date
       || (p->second.eattr && p->second.dtv.back().attr
           && p->second.eattr!=p->second.dtv.back().attr)
       || p->second.esize!=p->second.dtv.back().size)) {
      total_size+=p->second.esize;

      // Key by first 5 bytes of filename extension, case insensitive
      int sp=0;  // sortkey byte position
      for (string::const_iterator q=p->first.begin(); q!=p->first.end(); ++q){
        uint64_t c=*q&255;
        if (c>='A' && c<='Z') c+='a'-'A';
        if (c=='/') sp=0, p->second.sortkey=0;
        else if (c=='.') sp=8, p->second.sortkey=0;
        else if (sp>3) p->second.sortkey+=c<<(--sp*8);
      }

      // Key by descending size rounded to 16K
      int64_t s=p->second.esize>>14;
      if (s>=(1<<24)) s=(1<<24)-1;
      p->second.sortkey+=(1<<24)-s-1;

      vf.push_back(p);
    }
    if (!nodelete && p->second.written==0 && p->second.edate==0)
      ++deletions;
  }
  std::sort(vf.begin(), vf.end(), compareFilename);

  // Test if any files are to be added or deleted
  if (vf.size()==0 && deletions==0) {
    fprintf(con, "Archive %s not updated: nothing to add or delete.\n",
        archive.c_str());
    return errors>0;
  }

  // Open index to append
  WriterPair wp;  // wp.a points to output, wp.b to index
  Archive index;
  if (part0!=part1 && (exists(part0) || !exists(part1))) {
    if (method[0]=='s')
      error("Cannot update indexed archive in streaming mode");
    if (!index.open(part0.c_str(), password, 'w', index_pos))
      error("Index open failed");
    index_pos=index.tell();
    wp.b=&index;
  }

  // Open archive to append
  Archive out;
  Counter counter;
  if (archive=="")
    wp.a=&counter;
  else if (part0!=part1 && exists(part0) && !exists(part1)) {  // remote
    char salt[32]={0};
    if (password) {  // get salt from index
      index.close();
      if (index.open(part0.c_str()) && index.read(salt, 32)==32) {
        salt[0]^=0x4d;
        index.close();
      }
      else error("cannot read salt from index");
      if (!index.open(part0.c_str(), password, 'w'))
        error("index reopen failed");
    }
    string part=subpart(archive, ver.size());
    fprintf(con, "Creating ");
    printUTF8(part.c_str(), con);
    fprintf(con, " dated %s assuming %1.0f prior bytes\n",
         dateToString(date).c_str(), header_pos+0.0);
    if (exists(part)) error("output archive part exists");
    if (!out.open(part.c_str(), password, 'w', header_pos, header_pos, salt))
      error("Archive open failed");
    header_pos=out.tell();
    wp.a=&out;  
  }
  else {
    if (!out.open(archive.c_str(), password, 'w', header_pos))
      error("Archive open failed");
    header_pos=out.tell();
    fprintf(con, "%s ", (header_pos>32 ? "Updating" : "Creating"));
    printUTF8(archive.c_str(), con);
    fprintf(con, " version %d at %s\n",
      size(ver), dateToString(date).c_str());
    wp.a=&out;
  }
  if (method[0]=='i') {  // create index
    wp.b=wp.a;
    wp.a=0;
  }
  counter.pos=header_pos;

  // Start compress and write jobs
  vector<ThreadID> tid(threads*2-1);
  ThreadID wid;
  CompressJob job(threads, tid.size(), wp.a);
  if (deletions>0)
    fprintf(con, "Deleting %d files.\n", deletions);
  if (size(vf)>0)
    fprintf(con,
        "Adding %1.6f MB in %d files using %d jobs in %d threads.\n",
        total_size/1000000.0, size(vf), size(tid), threads);
  for (int i=0; i<size(tid); ++i) run(tid[i], compressThread, &job);
  run(wid, writeThread, &job);

  // Append in streaming mode. Each file is a separate block. Large files
  // are split into blocks of size blocksize.
  int64_t inputsize=0;  // total input size
  if (method[0]=='s') {
    StringBuffer sb(blocksize+4096-128);
    for (unsigned fi=0; fi<vf.size(); ++fi) {
      DTMap::iterator p=vf[fi];
      if (!p->first.size() || p->first[p->first.size()-1]=='/') continue;
      InputFile in;
      if (!in.open(p->first.c_str())) {
        ++errors;
        continue;
      }
      int64_t i=0;
      while (true) {
        int c=in.get();
        if (c!=EOF) ++i, sb.put(c);
        if (c==EOF || sb.size()==blocksize) {
          string filename="";
          string comment=itos(sb.size());
          if (i<=blocksize) {
            filename=p->first;
            comment+=" "+itos(p->second.edate);
            if ((p->second.eattr&255)>0) {
              comment+=" ";
              comment+=char(p->second.eattr&255);
              comment+=itos(p->second.eattr>>8);
            }
          }
          inputsize+=sb.size();
          job.write(sb, filename.c_str(), method, 512, comment.c_str());
          assert(sb.size()==0);
        }
        if (c==EOF) break;
      }
      in.close();
    }

    // Wait for jobs to finish
    job.write(sb, 0, "");  // signal end of input
    for (int i=0; i<size(tid); ++i) join(tid[i]);
    join(wid);

    // Done
    const int64_t outsize=out.isopen() ? out.tell() : counter.pos;
    fprintf(con, "%1.0f + (%1.0f -> %1.0f) = %1.0f\n",
        double(header_pos),
        double(inputsize),
        double(outsize-header_pos),
        double(outsize));
    out.close();
    return errors>0;
  }  // end if streaming

  // Adjust date to maintain sequential order
  if (ver.size() && ver.back().date>=date) {
    const int64_t newdate=decimal_time(unix_time(ver.back().date)+1);
    fprintf(stderr, "Warning: adjusting date from %s to %s\n",
      dateToString(date).c_str(), dateToString(newdate).c_str());
    assert(newdate>date);
    date=newdate;
  }

  // Build htinv for fast lookups of sha1 in ht
  HTIndex htinv(ht);

  // reserve space for the header block
  const unsigned htsize=ht.size();  // fragments at start of update
  writeJidacHeader(&wp, date, -1, htsize);
  const int64_t header_end=out.isopen() ? out.tell() : counter.pos;

  // Compress until end of last file
  assert(method!="");
  const unsigned MIN_FRAGMENT=64<<fragment;   // fragment size limits
  const unsigned MAX_FRAGMENT=8128<<fragment;
  StringBuffer sb(blocksize+4096-128);  // block to compress
  unsigned frags=0;    // number of fragments in sb
  unsigned redundancy=0;  // estimated bytes that can be compressed out of sb
  unsigned text=0;     // number of fragents containing text
  unsigned exe=0;      // number of fragments containing x86 (exe, dll)
  const int ON=4;      // number of order-1 tables to save
  unsigned char o1prev[ON*256]={0};  // last ON order 1 predictions
  libzpaq::Array<char> fragbuf(MAX_FRAGMENT);

  // For each file to be added
  for (unsigned fi=0; fi<vf.size(); ++fi) {
    assert(vf[fi]->second.eptr.size()==0);
    DTMap::iterator p=vf[fi];
    string filename=p->first;

    // Skip directory
    if (filename!="" && filename[filename.size()-1]=='/') {
      if (quiet<=0) {
        fprintf(con, "Adding directory ");
        printUTF8(p->first.c_str(), con);
        fprintf(con, "\n");
      }
      continue;
    }

    // Open input file
    InputFile in;
    if (!in.open(filename.c_str())) {  // skip if not found
      p->second.edate=0;
      lock(job.mutex);
      total_size-=p->second.esize;
      release(job.mutex);
      ++errors;
      continue;
    }
    else if (quiet<=p->second.esize) {
      fprintf(con, "%6u ", (unsigned)ht.size());
      if (p->second.dtv.size()==0 || p->second.dtv.back().date==0) {
        fprintf(con, "Adding   %12.0f ", double(p->second.esize));
        printUTF8(p->first.c_str(), con);
      }
      else {
        fprintf(con, "Updating %12.0f ", double(p->second.esize));
        printUTF8(p->first.c_str(), con);
      }
      if (p->first!=filename) {
        fprintf(con, " from ");
        printUTF8(filename.c_str(), con);
      }
      fprintf(con, "\n");
    }

    // Read fragments
    assert(in.isopen());
    for (unsigned fj=0; true; ++fj) {
      int c=0;  // current byte
      int c1=0;  // previous byte
      unsigned h=0;  // rolling hash for finding fragment boundaries
      int64_t sz=0;  // fragment size;
      libzpaq::SHA1 sha1;
      unsigned char o1[256]={0};
      unsigned hits=0;
      while (true) {
        c=in.get();
        if (c!=EOF) {
          if (c==o1[c1]) h=(h+c+1)*314159265u, ++hits;
          else h=(h+c+1)*271828182u;
          o1[c1]=c;
          c1=c;
          sha1.put(c);
          fragbuf[sz++]=c;
        }
        if (c==EOF || (h<(1u<<22>>fragment) && sz>=MIN_FRAGMENT)
           || sz>=MAX_FRAGMENT)
          break;
      }
      assert(sz<=MAX_FRAGMENT);
      inputsize+=sz;

      // Look for matching fragment
      char sh[20];
      assert(uint64_t(sz)==sha1.usize());
      memcpy(sh, sha1.result(), 20);
      unsigned htptr=htinv.find(sh);
      if (htptr==0) {  // not matched

        // Analyze fragment for redundancy, x86, text.
        // Test for text: letters, digits, '.' and ',' followed by spaces
        //   and no invalid UTF-8.
        // Test for exe: 139 (mov reg, r/m) in lots of contexts.
        // 4 tests for redundancy, measured as hits/sz. Take the highest of:
        //   1. Successful prediction count in o1.
        //   2. Non-uniform distribution in o1 (counted in o2).
        //   3. Fraction of zeros in o1 (bytes never seen).
        //   4. Fraction of matches between o1 and previous o1 (o1prev).
        int text1=0, exe1=0;
        int64_t h1=sz;
        unsigned char o1ct[256]={0};  // counts of bytes in o1
        static const unsigned char dt[256]={  // 32768/((i+1)*204)
          160,80,53,40,32,26,22,20,17,16,14,13,12,11,10,10,
            9, 8, 8, 8, 7, 7, 6, 6, 6, 6, 5, 5, 5, 5, 5, 5,
            4, 4, 4, 4, 4, 4, 4, 4, 3, 3, 3, 3, 3, 3, 3, 3,
            3, 3, 3, 3, 3, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
            2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
            1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
            1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
            1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
            1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
            1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
        for (int i=0; i<256; ++i) {
          if (o1ct[o1[i]]<255) h1-=(sz*dt[o1ct[o1[i]]++])>>15;
          if (o1[i]==' ' && (isalnum(i) || i=='.' || i==',')) ++text1;
          if (o1[i] && (i<9 || i==11 || i==12 || (i>=14 && i<=31) || i>=240))
            --text1;
          if (i>=192 && i<240 && o1[i] && (o1[i]<128 || o1[i]>=192))
            --text1;
          if (o1[i]==139) ++exe1;
        }
        text1=(text1>=3);
        exe1=(exe1>=5);
        if (sz>0) h1=h1*h1/sz; // Test 2: near 0 if random.
        unsigned h2=h1;
        if (h2>hits) hits=h2;
        h2=o1ct[0]*sz/256;  // Test 3: bytes never seen or that predict 0.
        if (h2>hits) hits=h2;
        h2=0;
        for (int i=0; i<256*ON; ++i)  // Test 4: compare to previous o1.
          h2+=o1prev[i]==o1[i&255];
        h2=h2*sz/(256*ON);
        if (h2>hits) hits=h2;
        if (hits>sz) hits=sz;

        // Start a new block if the current block is almost full, or at
        // the start of a file that won't fit or doesn't share mutual
        // information with the current block.
        bool newblock=false;
        if (frags>0 && fj==0) {
          const unsigned newsize=sb.size()+p->second.esize
               +(p->second.esize>>(8+fragment))+4096+frags*4; // size if added
          if (newsize>blocksize/4 && redundancy<sb.size()/128) newblock=true;
          if (newblock) {  // test for mutual information
            unsigned ct=0;
            for (unsigned i=0; i<256*ON; ++i)
              if (o1prev[i] && o1prev[i]==o1[i&255]) ++ct;
            if (ct>ON*2) newblock=false;
          }
          if (newsize>=blocksize) newblock=true;  // won't fit?
        }
        if (sb.size()+sz+80+frags*4>=blocksize) newblock=true; // full?
        if (frags<1) newblock=false;  // block is empty?

        // Pad sb with fragment size list unless fragile, then compress
        if (newblock) {
          assert(sb.size()>0);
          assert(frags>0);
          assert(frags<ht.size());
          for (unsigned i=ht.size()-frags; !fragile && i<ht.size(); ++i)
            sb+=itob(ht[i].usize);  // list of frag sizes
          sb+=itob(0); // omit first frag ID to make block movable
          sb+=itob(frags*!fragile);  // number of frags
          job.write(sb,
              ("jDC"+itos(date, 14)+"d"+itos(ht.size()-frags, 10)).c_str(),
              method,
              redundancy/(sb.size()/256+1)*4+(exe>frags)*2+(text>frags));
          assert(sb.size()==0);
          ht[ht.size()-frags].csize=-1;  // mark block start
          frags=redundancy=text=exe=0;
          memset(o1prev, 0, sizeof(o1prev));
        }

        // Append fragbuf to sb and update block statistics
        sb.write(&fragbuf[0], sz);
        ++frags;
        redundancy+=hits;
        exe+=exe1*4;
        text+=text1*2;
        if (sz>=MIN_FRAGMENT) {
          memmove(o1prev, o1prev+256, 256*(ON-1));
          memcpy(o1prev+256*(ON-1), o1, 256);
        }
      }  // end if not matched

      // Point file to this fragment
      if (htptr==0) {  // not matched in ht
        htptr=ht.size();
        ht.push_back(HT(sh, sz, 0));
        htinv.update();
      }
      else {
        lock(job.mutex);
        bytes_processed+=sz;
        release(job.mutex);
      }
      p->second.eptr.push_back(htptr);

      if (c==EOF)
        break;
    }  // end for each fragment
    in.close();
  }  // end for each file

  // Compress any remaining data
  if (frags>0) {
    assert(frags<ht.size());
    for (unsigned i=ht.size()-frags; !fragile && i<ht.size(); ++i)
      sb+=itob(ht[i].usize);
    sb+=itob(0);
    sb+=itob(frags*!fragile);
    job.write(sb,
        ("jDC"+itos(date, 14)+"d"+itos(ht.size()-frags, 10)).c_str(),
        method,
        redundancy/(sb.size()/256+1)*4+(exe>frags)*2+(text>frags));
    assert(sb.size()==0);
    ht[ht.size()-frags].csize=-1;
  }

  // Wait for jobs to finish
  assert(sb.size()==0);
  job.write(sb, 0, "");  // signal end of input
  for (int i=0; i<size(tid); ++i)
    join(tid[i]);
  join(wid);

  // Fill in compressed sizes in ht
  unsigned j=0;
  for (unsigned i=htsize; i<ht.size() && j<job.csize.size(); ++i)
    if (ht[i].csize==-1)
      ht[i].csize=job.csize[j++];
  assert(j==job.csize.size());

  // Append compressed fragment tables to archive
  fprintf(con, "Updating with %d files, %d blocks, %d fragments.\n",
          int(vf.size()), j, int(ht.size()-htsize));
  int64_t cdatasize=(out.isopen() ? out.tell() : counter.pos)-header_end;
  StringBuffer is;
  unsigned block_start=0;
  for (unsigned i=htsize; i<=ht.size(); ++i) {
    if ((i==ht.size() || ht[i].csize>0) && is.size()>0) {  // write a block
      assert(block_start>=htsize && block_start<i);
      compressBlock(&is, &wp, "0",
                    ("jDC"+itos(date, 14)+"h"+itos(block_start, 10)).c_str());
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
  for (DTMap::iterator p=dt.begin(); p!=dt.end();) {
    const DT& dtr=p->second;

    // Remove file if external does not exist and is currently in archive
    if (!nodelete && dtr.written==0 && !dtr.edate && dtr.dtv.size()
        && dtr.dtv.back().date) {
      is+=ltob(0)+p->first+'\0';
      if (quiet<=dtr.dtv.back().size) {
        fprintf(con, "Removing %12.0f ", dtr.dtv.back().size+0.0);
        printUTF8(p->first.c_str(), con);
        fprintf(con, "\n");
      }
    }

    // Update file if compressed and anything differs
    if (dtr.edate && (force || dtr.dtv.size()==0
       || dtr.edate!=dtr.dtv.back().date
       || (dtr.eattr && dtr.dtv.back().attr && dtr.eattr!=dtr.dtv.back().attr)
       || dtr.esize!=dtr.dtv.back().size)) {

      // Append to index if anything changed
      if (dtr.dtv.size()==0 // new file
         || dtr.edate!=dtr.dtv.back().date  // date change
         || (dtr.eattr && dtr.dtv.back().attr
             && dtr.eattr!=dtr.dtv.back().attr)  // attr change
         || dtr.esize!=dtr.dtv.back().size  // size change
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
      compressBlock(&is, &wp, "1",
                    ("jDC"+itos(date)+"i"+itos(++dtcount, 10)).c_str());
      assert(is.size()==0);
    }
    if (p==dt.end()) break;
  }

  // Back up and write the header
  int64_t archive_end=0;
  if (!out.isopen())
    archive_end=counter.pos;
  else {
    archive_end=out.tell();
    out.seek(header_pos, SEEK_SET);
    if (wp.b) index.seek(index_pos, SEEK_SET);
    writeJidacHeader(wp.a, date, cdatasize, htsize);
    if (wp.b) writeJidacHeader(wp.b, date, 0, htsize);
  }
  fprintf(con, "\n%1.0f + (%1.0f -> %1.0f) = %1.0f\n",
         double(header_pos),
         double(inputsize),
         double(archive_end-header_pos),
         double(archive_end));
  out.close();
  index.close();
  return errors>0;
}

/////////////////////////////// extract ///////////////////////////////

// Return true if the internal file p (version vi, or last if -1)
// and external file contents are equal or neither exists.
// If filename is 0 then return true if it is possible to compare.
bool Jidac::equal(DTMap::const_iterator p, const char* filename, int vi) {

  // default version is the latest
  if (vi<0) vi=p->second.dtv.size()-1;

  // test if all fragment sizes and hashes exist
  if (filename==0) {
    static const char zero[20]={0};
    if (p->second.dtv[vi].size<0) return false;
    for (unsigned i=0; i<p->second.dtv[vi].ptr.size(); ++i) {
      unsigned j=p->second.dtv[vi].ptr[i];
      if (j<1 || j>=ht.size() || ht[j].csize==HT_BAD
          || ht[j].usize<0 || !memcmp(ht[j].sha1, zero, 20))
        return false;
    }
    return true;
  }

  // internal or neither file exists
  if (vi<0 || p->second.dtv[vi].date==0) return !exists(filename);

  // directories always match
  if (p->first!="" && p->first[p->first.size()-1]=='/')
    return exists(filename);

  // compare sizes
  InputFile in;
  in.open(filename);
  if (!in.isopen()) return false;
  in.seek(0, SEEK_END);
  if (in.tell()!=p->second.dtv[vi].size) return false;

  // compare hashes
  in.seek(0, SEEK_SET);
  libzpaq::SHA1 sha1;
  for (unsigned i=0; i<p->second.dtv[vi].ptr.size(); ++i) {
    unsigned f=p->second.dtv[vi].ptr[i];
    if (f<1 || f>=ht.size() || ht[f].csize==HT_BAD) return false;
    for (int j=ht[f].usize; j>0; --j) {
      int c=in.get();
      if (c==EOF) return false;
      sha1.put(c);
    }
    if (memcmp(sha1.result(), ht[f].sha1, 20)!=0) return false;
  }
  return in.get()==EOF;  // should be true
}

// An extract job is a set of blocks with at least one file pointing to them.
// Blocks are extracted in separate threads, set READY -> WORKING.
// A block is extracted to memory up to the last fragment that has a file
// pointing to it. Then the checksums are verified. Then for each file
// pointing to the block, each of the fragments that it points to within
// the block are written in order.

struct Block {  // list of fragments
  int64_t offset;       // location in archive
  vector<DTMap::iterator> files;  // list of files pointing here
  unsigned start;       // index in ht of first fragment
  int size;             // number of fragments to decompress
  bool streaming;       // must decompress sequentially?
  enum {READY, WORKING, GOOD, BAD} state;
  Block(unsigned s, int64_t o):
    offset(o), start(s), size(0), streaming(false), state(READY) {}
};

struct ExtractJob {         // list of jobs
  Mutex mutex;              // protects state
  Mutex write_mutex;        // protects writing to disk
  int job;                  // number of jobs started
  int next;                 // next block to extract (usually)
  vector<Block> block;      // list of blocks to extract
  Jidac& jd;                // what to extract
  OutputFile outf;          // currently open output file
  DTMap::iterator lastdt;   // currently open output file name
  double maxMemory;         // largest memory used by any block (test mode)
  ExtractJob(Jidac& j):
      job(0), next(0), jd(j), lastdt(j.dt.end()), maxMemory(0) {
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
  Archive in;

  // Get job number
  lock(job.mutex);
  jobNumber=++job.job;
  release(job.mutex);

  // Open archive for reading
  if (!in.open(job.jd.archive.c_str(), job.jd.password)) return 0;
  WriteBuffer out;

  // Look for next READY job
  while (true) {
    lock(job.mutex);
    unsigned i, k=0;
    for (i=0; i<job.block.size(); ++i) {
      k=i+job.next;
      if (k>=job.block.size()) k-=job.block.size();
      assert(k<job.block.size());
      Block& b=job.block[k];
      if (b.state==Block::READY && b.size>0 && !b.streaming) {
        b.state=Block::WORKING;
        break;
      }
    }
    if (i<job.block.size()) job.next=k;
    release(job.mutex);
    if (i>=job.block.size()) break;
    Block& b=job.block[k];

    // Get uncompressed size of block
    unsigned output_size=0;  // minimum size to decompress
    unsigned max_size=0;     // uncompressed full block size
    assert(b.start>0);
    int j;
    for (j=0; j<b.size; ++j) {
      assert(b.start+j<job.jd.ht.size());
      assert(job.jd.ht[b.start+j].usize>=0);
      assert(j==0 || job.jd.ht[b.start+j].csize==-j);
      output_size+=job.jd.ht[b.start+j].usize;
    }
    max_size=output_size+j*4+8;  // uncompressed full block size
    for (; b.start+j<job.jd.ht.size() && job.jd.ht[b.start+j].csize<0
           && job.jd.ht[b.start+j].csize!=HT_BAD; ++j) {
      assert(job.jd.ht[b.start+j].csize==-j);
      max_size+=job.jd.ht[b.start+j].usize+4;
    }

    // Decompress
    double mem=0;  // how much memory used to decompress
    try {
      assert(b.start>0);
      assert(b.start<job.jd.ht.size());
      assert(b.size>0);
      assert(b.start+b.size<=job.jd.ht.size());
      const int64_t now=mtime();
      in.seek(job.jd.ht[b.start].csize, SEEK_SET);
      libzpaq::Decompresser d;
      d.setInput(&in);
      out.reset();
      out.setLimit(max_size);
      d.setOutput(&out);
      libzpaq::SHA1 sha1;
      if (job.jd.all) d.setSHA1(&sha1);
      if (!d.findBlock(&mem)) error("archive block not found");
      if (mem>job.maxMemory) job.maxMemory=mem;
      while (d.findFilename()) {
        StringWriter comment;
        d.readComment(&comment);
        if (!job.jd.all && comment.s.size()>=5
            && comment.s.substr(comment.s.size()-5)==" jDC\x01") {
          while (out.size()<output_size && d.decompress(1<<14));
          break;
        }
        else {
          char s[21];
          d.decompress();
          d.readSegmentEnd(s);
          if (job.jd.all && s[0]==1 && memcmp(s+1, sha1.result(), 20))
            error("checksum error");
        }
      }
      if (out.size()<output_size)
        error("unexpected end of compressed data");
      if (quiet<MAX_QUIET-1) {
        fprintf(con, "Job %d: [%d..%d] %1.0f -> %d (%1.3f s, %1.3f MB)\n",
            jobNumber, b.start, b.start+b.size-1,
            double(in.tell()-job.jd.ht[b.start].csize),
            size(out), (mtime()-now)*0.001, mem/1000000);
      }

      // Verify fragment checksums if present
      int64_t q=0;  // fragment start
      for (unsigned j=b.start; j<b.start+b.size; ++j) {
        if (!fragile) {
          char sha1result[20];
          out.sha1(sha1result, q, job.jd.ht[j].usize);
          q+=job.jd.ht[j].usize;
          if (memcmp(sha1result, job.jd.ht[j].sha1, 20)) {
            for (int k=0; k<20; ++k) {
              if (job.jd.ht[j].sha1[k]) {  // all zeros is OK
                lock(job.mutex);
                fprintf(stderr, 
                       "Job %d: fragment %d size %d checksum failed\n",
                       jobNumber, j, job.jd.ht[j].usize);
                release(job.mutex);
                error("bad checksum");
              }
            }
          }
        }
        lock(job.mutex);
        job.jd.ht[j].csize=EXTRACTED;
        release(job.mutex);
      }
    }

    // If out of memory, let another thread try
    catch (std::bad_alloc& e) {
      lock(job.mutex);
      fprintf(stderr, "Job %d killed to save memory\n", jobNumber);
      b.state=Block::READY;
      release(job.mutex);
      in.close();
      return 0;
    }

    // Other errors: assume bad input
    catch (std::exception& e) {
      lock(job.mutex);
      fprintf(stderr, "Job %d: skipping frags %u-%u at offset %1.0f: %s\n",
              jobNumber, b.start, b.start+b.size-1,
              double(in.tell()), e.what());
      release(job.mutex);
      continue;
    }

    // Write the files in dt that point to this block
    lock(job.write_mutex);
    for (unsigned ip=0; ip<b.files.size(); ++ip) {
      DTMap::iterator p=b.files[ip];
      DT& dtr=p->second;
      if (dtr.written<0 || size(dtr.dtv)==0 
          || dtr.written>=size(dtr.dtv.back().ptr))
        continue;  // don't write

      // Look for pointers to this block
      const vector<unsigned>& ptr=dtr.dtv.back().ptr;
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
            assert(job.lastdt->second.written
                   <size(job.lastdt->second.dtv.back().ptr));
            job.outf.close();
          }
          job.lastdt=job.jd.dt.end();
        }

        // Open file for output
        if (job.lastdt==job.jd.dt.end()) {
          filename=job.jd.rename(p->first);
          assert(!job.outf.isopen());
          if (dtr.written==0) {
            makepath(filename);
            if (quiet<=dtr.dtv.back().size) {
              fprintf(con, "Job %d: extracting %1.0f ", jobNumber,
                  p->second.dtv.back().size+0.0);
              printUTF8(filename.c_str(), con);
              fprintf(con, "\n");
            }
            if (job.outf.open(filename.c_str()))  // new file
              job.outf.truncate();
          }
          else
            job.outf.open(filename.c_str());  // update existing file
          if (!job.outf.isopen()) break;  // skip file if error
          job.lastdt=p;
          assert(job.outf.isopen());
        }
        assert(job.lastdt==p);

        // Find block offset of fragment
        int64_t q=0;  // fragment offset from start of block
        for (unsigned k=b.start; k<ptr[j]; ++k)
          q+=job.jd.ht[k].usize;
        assert(q>=0);
        assert(q<=out.size()-job.jd.ht[ptr[j]].usize);

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
        assert(q+usize<=out.size());
        out.save(job.outf, offset, q, usize);
        offset+=usize;
        bytes_processed+=usize;
        if (dtr.written==size(ptr)) {  // close file
          assert(dtr.dtv.size()>0);
          assert(dtr.dtv.back().date);
          assert(job.lastdt!=job.jd.dt.end());
          assert(job.outf.isopen());
          job.outf.truncate(dtr.dtv.back().size);
          job.outf.close(dtr.dtv.back().date, dtr.dtv.back().attr);
          job.lastdt=job.jd.dt.end();
        }
      } // end for j
    } // end for ip

    // Last file
    release(job.write_mutex);

    // Update display
    lock(job.mutex);
    if (bytes_processed>0) {
      int64_t eta=(mtime()-global_start+0.0)
           *(total_size-bytes_processed)/(bytes_processed+0.5)/1000.0;
      if (bytes_processed>0)
        fprintf(con, "%d:%02d:%02d to go: ",
            int(eta/3600), int(eta/60%60), int(eta%60));        }
    if (quiet<=MAX_QUIET-1) {
      fprintf(con, "%1.6f MB (%5.2f%%)    %c", bytes_processed/1000000.0,
          (bytes_processed+0.5)*100.0/(total_size+0.5),
          quiet==MAX_QUIET-1 ? '\r' : '\n');
      fflush(con);
    }
    release(job.mutex);
  } // end while true

  // Last block
  in.close();
  return 0;
}

// Extract files from archive. If force is true then overwrite
// existing files and set the dates and attributes of exising directories.
// Otherwise create only new files and directories. Return 1 if error else 0.
int Jidac::extract() {

  // Read HT, DT and mark selected files with written=0
  if (!read_archive()) return 1;
  read_args();

  // Skip existing output files. If force then skip only if equal
  // and set date and attributes. 
  {
    int files=0, dirs=0, eqfiles=0, eqdirs=0, diffs=0;  // counts
    for (DTMap::iterator p=dt.begin(); p!=dt.end(); ++p) {
      if (p->second.written==0) {
        DTV& dtv=p->second.dtv.back();
        const bool isdir=p->first!="" && p->first[size(p->first)-1]=='/';
        isdir ? ++dirs : ++files;
        const string fn=rename(p->first);
        const bool isexist=exists(fn.c_str());
        const bool isequal=isexist
            && (!force || isdir || equal(p, fn.c_str()));
        if (isequal && !isdir) p->second.written=-1;  // unmark
        diffs+=isexist && !isdir && !isequal;
        eqfiles+=isexist && !isdir && isequal;
        eqdirs+=isdir && isequal;
        if (isequal && !isdir && force) {  // update date, attr
          OutputFile out;
          if (out.open(fn.c_str())) out.close(dtv.date, dtv.attr);
        }
        if (dtv.size>=quiet) {
          if (isequal && !isdir) {
            fprintf(con, "Skipping %12.0f ", dtv.size+0.0);
            printUTF8(fn.c_str(), con);
            fprintf(con, "\n");
          }
        }
      }
    }
    fprintf(con, "%d of %d files", eqfiles+diffs, files);
    if (force) fprintf(con, " (%d identical)", eqfiles);
    fprintf(con, " and %d of %d directories found.\n", eqdirs, dirs);
  }

  // Map fragments to blocks.
  // Mark blocks with unknown or large fragment sizes as streaming.
  ExtractJob job(*this);
  vector<unsigned> hti(ht.size());  // fragment index -> block index
  for (unsigned i=1; i<ht.size(); ++i) {
    if (ht[i].csize!=HT_BAD) {
      if (ht[i].csize>=0)
        job.block.push_back(Block(i, ht[i].csize));
      assert(job.block.size()>0);
      hti[i]=job.block.size()-1;
      if (ht[i].usize<0 || ht[i].usize>(1<<30))
        job.block.back().streaming=true;
    }
  }

  // Make a list of files and the number of fragments to extract
  // from each block. If the file size is unknown, then mark
  // all blocks that it points to as streaming.

  total_size=0;  // total bytes to be extracted
  bytes_processed=0;  // total bytes extracted so far
  int total_files=0;
  for (DTMap::iterator p=dt.begin(); p!=dt.end(); ++p) {
    if (p->second.written==0) {
      assert(p->second.dtv.size()>0);
      for (unsigned i=0; i<p->second.dtv.back().ptr.size(); ++i) {
        unsigned j=p->second.dtv.back().ptr[i];
        if (j==0 || j>=ht.size() || ht[j].csize==HT_BAD) {
          printUTF8(p->first.c_str(), stderr);
          fprintf(stderr, ": bad frag IDs, skipping...\n");
          continue;
        }
        assert(j>0 && j<ht.size());
        assert(ht.size()==hti.size());
        int64_t c=-ht[j].csize;
        if (c<0) c=0;  // position of fragment in block
        j=hti[j];  // block index
        assert(j>=0 && j<job.block.size());
        if (job.block[j].size<=c) job.block[j].size=c+1;
        if (job.block[j].files.size()==0 || job.block[j].files.back()!=p)
          job.block[j].files.push_back(p);
        if (p->second.dtv.back().size<0) job.block[j].streaming=true;
      }
      total_size+=p->second.dtv.back().size;
      if (p->first!="" && p->first[size(p->first)-1]!='/') ++total_files;
    }
  }

  // Decompress archive in parallel
  fprintf(con, "Extracting %1.6f MB in %d files with %d jobs\n",
      total_size/1000000.0, total_files, threads);
  vector<ThreadID> tid(threads);
  for (int i=0; i<size(tid); ++i) run(tid[i], decompressThread, &job);

  // Decompress streaming files in a single thread
  Archive in;
  if (!in.open(archive.c_str(), password)) return 1;
  OutputFile out;
  DTMap::iterator p=dt.end();  // currently open output file (initially none)
  string lastfile=archive;  // default output file: drop .zpaq from archive
  if (lastfile.size()>5 && lastfile.substr(lastfile.size()-5)==".zpaq")
    lastfile=lastfile.substr(0, lastfile.size()-5);
  bool first=true;
  for (unsigned i=0; i<job.block.size(); ++i) {
    Block& b=job.block[i];
    if (b.size==0 || !b.streaming) continue;
    if (quiet<MAX_QUIET-1)
      fprintf(con, "main:  [%d..%d] block %d\n", b.start, b.start+b.size-1,
              i+1);
    try {
      libzpaq::Decompresser d;
      libzpaq::SHA1 sha1;
      d.setInput(&in);
      d.setSHA1(&sha1);
      if (out.isopen()) d.setOutput(&out);
      else d.setOutput(0);
      in.seek(b.offset, SEEK_SET);
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
          if (out.isopen()) {
            out.close();
            p=dt.end();
          }
          first=false;
          string newfile;
          p=dt.find(lastfile);
          if (p!=dt.end() && p->second.written==0) {  // todo
            newfile=rename(lastfile);
            makepath(newfile);
            if (out.open(newfile.c_str())) {
              if (quiet<MAX_QUIET-1) {
                fprintf(con, "main: extracting ");
                printUTF8(newfile.c_str(), con);
                fprintf(con, "\n");
              }
              out.truncate(0);
            }
            if (out.isopen()) d.setOutput(&out);
            else {
              d.setOutput(0);
              p=dt.end();
            }
          }
        }
        filename.s="";

        // Decompress, verify checksum
        if (j<b.size) {
          d.decompress();
          char sha1out[21];
          d.readSegmentEnd(sha1out);
          if (!fragile && sha1out[0] && memcmp(sha1out+1, sha1.result(), 20))
            error("checksum error");
          else {
            assert(b.start+j<ht.size());
            lock(job.mutex);
            ht[b.start+j].csize=EXTRACTED;
            release(job.mutex);
            if (p!=dt.end()) ++p->second.written;
          }
        }
        else
          break;
      }
    }
    catch (std::exception& e) {
      fprintf(stderr, "main: skipping frags %u-%u at offset %1.0f: %s\n",
              b.start, b.start+b.size-1, double(in.tell()), e.what());
      continue;
    }
  }

  // Wait for threads to finish
  for (int i=0; i<size(tid); ++i) join(tid[i]);

  // Create empty directories and set directory dates and attributes
  for (DTMap::reverse_iterator p=dt.rbegin(); p!=dt.rend(); ++p) {
    if (p->second.written==0 && p->second.dtv.size()
        && p->second.dtv.back().date && p->first!=""
        && p->first[p->first.size()-1]=='/') {
      string s=rename(p->first);
      if (p->second.dtv.size())
        makepath(s, p->second.dtv.back().date, p->second.dtv.back().attr);
    }
  }

  // Report failed extractions
  unsigned extracted=0, errors=0;
  for (DTMap::iterator p=dt.begin(); p!=dt.end(); ++p) {
    string fn=rename(p->first);
    if (p->second.written>=0 && p->second.dtv.size()
        && p->second.dtv.back().date && fn!="" && fn[fn.size()-1]!='/') {
      DTV& dtv=p->second.dtv.back();
      ++extracted;
      unsigned f=0;  // fragments extracted OK
      for (unsigned j=0; j<dtv.ptr.size(); ++j) {
        const unsigned k=dtv.ptr[j];
        if (k>0 && k<ht.size() && ht[k].csize==EXTRACTED) ++f;
      }
      if (f!=dtv.ptr.size() || f!=unsigned(p->second.written)) {
        if (++errors==1)
          fprintf(stderr,
          "\nFailed (extracted,written/total fragments, version, file):\n");
        fprintf(stderr, "%u,%u/%u %d ",
                f, p->second.written, int(dtv.ptr.size()), dtv.version);
        printUTF8(fn.c_str(), stderr);
        fprintf(stderr, "\n");
      }
    }
  }
  if (errors>0) {
    fprintf((errors>0 ? stderr : con),
        "\nExtracted %u of %u files OK (%u errors)"
        " using %1.3f MB x %d threads\n",
        extracted-errors, extracted, errors, job.maxMemory/1000000,
        size(tid));
  }
  return errors>0;
}

/////////////////////////////// list //////////////////////////////////

// For counting files and sizes by -list -summary
struct TOP {
  double csize;  // compressed size
  int64_t size;  // uncompressed size
  int count;     // number of files
  TOP(): csize(0), size(0), count(0) {}
  void inc(int64_t n) {size+=n; ++count;}
  void inc(DTMap::const_iterator p) {
    if (p->second.dtv.size()>0) {
      size+=p->second.dtv.back().size;
      csize+=p->second.dtv.back().csize;
      ++count;
    }
  }
};

void Jidac::list_versions(int64_t csize) {
  fprintf(con, "\n"
         "Ver Last frag Date      Time (UT) Files Deleted"
         "   Original MB  Compressed MB\n"
         "---- -------- ---------- -------- ------ ------ "
         "-------------- --------------\n");
  assert(since>=0);
  for (int i=since; i<size(ver); ++i) {
    int64_t osize=((i<size(ver)-1 ? ver[i+1].offset : csize)-ver[i].offset);
    if (i==0 && ver[i].updates==0
        && ver[i].deletes==0 && ver[i].date==0 && ver[i].usize==0)
      continue;
    fprintf(con, "%4d %8d %s %6d %6d %14.6f %14.6f\n", i,
      i<size(ver)-1 ? ver[i+1].firstFragment-1 : size(ht)-1,
      dateToString(ver[i].date).c_str(),
      ver[i].updates, ver[i].deletes, ver[i].usize/1000000.0,
      osize/1000000.0);
  }
}

// Return p<q for sorting files by decreasing size, then fragment ID list
bool compareFragmentList(DTMap::const_iterator p, DTMap::const_iterator q) {
  if (q->second.dtv.size()==0) return false;
  if (p->second.dtv.size()==0) return true;
  int64_t d=p->second.dtv.back().size-q->second.dtv.back().size;
  if (d!=0) return d>0;
  if (p->second.dtv.back().ptr<q->second.dtv.back().ptr) return true;
  if (q->second.dtv.back().ptr<p->second.dtv.back().ptr) return false;
  return p->first<q->first;
}

// List contents
int Jidac::list() {

  // Read archive to compare
  Jidac other(*this);
  if (compare!="" && archive2!="") other.read_archive(0, archive2.c_str());

  // Read archive, which may be "" for empty.
  int64_t csize=0;
  if (archive!="") {
    csize=read_archive();
    if (csize==0) exit(1);
  }
  read_args();

  // Adjust since to 1..versions
  if (since<0) since+=size(ver);
  if (since<1) since=1;

  // Summary. Show only the largest files and directories, sorted by size,
  // and block and fragment usage statistics.
  if (summary) {

    // Report biggest files, directories, and extensions
    fprintf(con,
      "\nRank      Size (MB) Ratio     Files File, Directory/, or .Type\n"
      "---- -------------- ------ --------- --------------------------\n");
    map<string, TOP> top;  // filename or dir -> total size and count
    vector<int> frag(ht.size());  // frag ID -> reference count
    int unknown_ref=0;  // count fragments and references with unknown size
    int unknown_size=0;
    for (DTMap::const_iterator p=dt.begin(); p!=dt.end(); ++p) {
      if (p->second.dtv.size() && p->second.dtv.back().date
          && p->second.dtv.back().version>=since && p->second.written==0) {
        top[""].inc(p);
        top[p->first].inc(p);
        int ext=0;  // location of . in filename
        for (unsigned i=0; i<p->first.size(); ++i) {
          if (p->first[i]=='/') {
            top[p->first.substr(0, i+1)].inc(p);
            ext=0;
          }
          else if (p->first[i]=='.') ext=i;
        }
        if (ext)
          top[lowercase(p->first.substr(ext))].inc(p);
        else
          top["."].inc(p);
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
        fprintf(con, "%4d %14.6f %6.4f %9d ", i, (-p->first)/1000000.0,
               top[p->second[j].c_str()].csize/max(int64_t(1), -p->first),
               top[p->second[j].c_str()].count);
        printUTF8(p->second[j].c_str(), con);
        fprintf(con, "\n");
      }
    }

    // Report block and fragment usage statistics
    fprintf(con, "\nShares Fragments Deduplicated MB    Extracted MB\n"
             "------ --------- --------------- ---------------\n");
    map<unsigned, TOP> fr, frc; // refs -> deduplicated, extracted count, size
    if (since<size(ver)) {
      for (unsigned i=ver[since].firstFragment; i<frag.size(); ++i) {
        assert(i<ht.size());
        int j=frag[i];
        if (j>10) j=10;
        fr[j].inc(ht[i].usize);
        fr[-1].inc(ht[i].usize);
        frc[j].inc(int64_t(ht[i].usize)*frag[i]);
        frc[-1].inc(int64_t(ht[i].usize)*frag[i]);
        if (ht[i].usize<0) ++unknown_size;
      }
    }
    for (map<unsigned, TOP>::const_iterator p=fr.begin(); p!=fr.end(); ++p) {
      if (int(p->first)==-1) fprintf(con, " Total ");
      else if (p->first==10) fprintf(con, "   10+ ");
      else fprintf(con, "%6u ", p->first);
      fprintf(con, "%9d %15.6f %15.6f\n", p->second.count,
        p->second.size/1000000.0, frc[p->first].size/1000000.0);
    }

    // Print versions
    list_versions(csize);

    // Report fragments with unknown size
    fprintf(con, "\n%d references to %d of %d fragments have unknown size.\n",
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
    fprintf(con, "%d of %d blocks used.\nCompression %1.6f -> %1.6f MB",
           used, blocks, usize/1000000.0, csize/1000000.0);
    if (usize>0) fprintf(con, " (ratio %1.3f%%)", csize*100.0/usize);
    fprintf(con, "\n");
    return 0;
  }

  // Make list of files to list
  vector<DTMap::const_iterator> filelist;
  for (DTMap::const_iterator p=dt.begin(); p!=dt.end(); ++p)
    if (p->second.written==0)
      filelist.push_back(p);
  if (duplicates)
    sort(filelist.begin(), filelist.end(), compareFragmentList);

  // Ordinary list
  int64_t usize=0;
  unsigned nfiles=0, shown=0, matches=0, mismatches=0, notfound=0, unknown=0;
  fprintf(con, "\n"
    " Ver  Date      Time (UT) %s        Size Ratio  File\n"
    "----- ---------- -------- %s------------ ------ ----\n",
    noattributes?"":"Attr   ", noattributes?"":"------ ");
  for (unsigned fi=0; fi<filelist.size(); ++fi) {
    DTMap::const_iterator p=filelist[fi];
    for (unsigned i=0; i<p->second.dtv.size(); ++i) {
      if (p->second.dtv[i].version>=since
          && (all || (i+1==p->second.dtv.size() && p->second.dtv[i].date))) {
        string s=rename(p->first);
        char type=' ';  // comparison result
        if (archive2!="" && compare!="") {  // compare to other archive
          DTMap::iterator q=other.dt.find(s);
          if (q==other.dt.end() || q->second.dtv.size()==0
              || q->second.dtv.back().date==0) type='/';
          else {
            const DTV& dp=p->second.dtv[i];
            const DTV& dq=q->second.dtv.back();
            if (dp.size<0 || dq.size<0) type='?';
            else if (dp.size!=dq.size) type='#';
            else if (dp.ptr.size()!=dq.ptr.size()) type='?';
            else {
              type='=';
              for (unsigned j=0; type=='=' && j<dp.ptr.size(); ++j) {
                const unsigned j1=dp.ptr[j];
                const unsigned j2=dq.ptr[j];
                if (j1>=ht.size() || j2>=other.ht.size()) type='?';
                else if (ht[j1].usize<0 || other.ht[j2].usize<0) type='?';
                else if (ht[j1].usize!=other.ht[j2].usize) type='?';
                else if (memcmp(ht[j1].sha1, other.ht[j2].sha1, 20)) type='#';
              }
            }
          }
        }
        else if (compare!="") {  // compare to external file
          if (!exists(s.c_str())) type='/';
          else if (!equal(p, 0)) type='?';
          else if (equal(p, s.c_str(), i)) type='=';
          else type='#';
        }
        else if (duplicates && fi>0 && filelist[fi-1]->second.dtv.size()
            && p->second.dtv[i].ptr==filelist[fi-1]->second.dtv.back().ptr)
          type='=';  // compare to previous file
        else type='>';  // no compare
        if (type=='=') ++matches;
        if (type=='#') ++mismatches;
        if (type=='/') ++notfound;
        if (type=='?') ++unknown;
        if (p->second.dtv[i].size>=quiet
            && (compare=="" || !strchr(compare.c_str()+1, type))) {
          fprintf(con, "%c%4d ", type, p->second.dtv[i].version);
          if (p->second.dtv[i].date) {
            ++shown;
            usize+=p->second.dtv[i].size;
            double ratio=1.0;
            if (p->second.dtv[i].size>0)
              ratio=p->second.dtv[i].csize/p->second.dtv[i].size;
            if (ratio>9.9999) ratio=9.9999;
            fprintf(con, "%s %s%12.0f %6.4f ",
                   dateToString(p->second.dtv[i].date).c_str(),
                   noattributes ? "" :
                     (attrToString(p->second.dtv[i].attr)+" ").c_str(),
                   double(p->second.dtv[i].size), ratio);
          }
          else {
            fprintf(con, "%-40s", "Deleted");
            if (!noattributes) fprintf(con, "       ");
          }
          printUTF8(p->first.c_str(), con);
          if (quiet<-1) {  // frag pointers
            const vector<unsigned>& ptr=p->second.dtv[i].ptr;
            bool hyphen=false;
            for (int j=0; j<int(ptr.size()); ++j) {
              if (j==0 || j==int(ptr.size())-1 || ptr[j]!=ptr[j-1]+1
                  || ptr[j]!=ptr[j+1]-1) {
                if (!hyphen) fprintf(con, " ");
                hyphen=false;
                fprintf(con, "%d", ptr[j]);
              }
              else {
                if (!hyphen) fprintf(con, "-");
                hyphen=true;
              }
            }
          }
          if (s!=p->first) {  // -to new name
            fprintf(con, " -> ");
            printUTF8(s.c_str(), con);
          }
          fprintf(con, "\n");
        }
      }
    }
    if (p->second.dtv.size() && p->second.dtv.back().date) ++nfiles;
  }
  fprintf(con, "%u of %u files shown. %1.0f -> %1.0f\n",
         shown, nfiles, double(usize), double(csize+dhsize-dcsize));
  if (compare!="")
    fprintf(con, "%d =matches, %d #mismatches, %d /not found, %d ?unknown.\n",
        matches, mismatches, notfound, unknown);
  if (dhsize!=dcsize)  // index?
    fprintf(con, "Note: %1.0f of %1.0f compressed bytes are in archive\n",
        dcsize+0.0, dhsize+0.0);
  if (all) list_versions(csize);
  return compare!="" && mismatches+notfound+unknown>0;
}

/////////////////////////////// purge /////////////////////////////////

// Block list element
struct BL {
  int64_t start;  // archive offset
  int64_t end;    // last byte + 1
  unsigned used;  // number of references
  unsigned firstFragment;
  bool streaming; // not journaling?
  BL(): start(-1), end(-1), used(0), firstFragment(0), streaming(true) {}
};

// Find filename in ZPAQ segment header of form "jDC<date>d<num>"
// and substitute date (14 digits) and num (10 digits). Assume that
// s[0..n-1] is the start of a ZPAQ block with or without a tag.
// Return 0 if successful else error code > 0
int setFilename(char* s, int n, int64_t date, unsigned num) {
  if (!s) return 1;
  if (*s=='7' && n>13) s+=13, n-=13;  // skip tag
  if (n<7) return 2;
  if (s[0]!='z') return 3;
  if (s[1]!='P') return 4;
  if (s[2]!='Q') return 5;
  int hsize=(s[5]&255)+(s[6]&255)*256+7;
  s+=hsize, n-=hsize;
  if (n<30) return 6;
  if (s[0]!=1) return 7;
  if (s[1]!='j') return 8;
  if (s[2]!='D') return 9;
  if (s[3]!='C') return 10;
  if (s[29]!=0) return 11;
  string sd=itos(date, 14)+s[18]+itos(num, 10);
  memcpy(s+4, sd.c_str(), 25);
  return 0;
}

// Copy current version only to first to.zpaq.
// If archive2 is ".zpaq" then check for errors but discard output.
void Jidac::purge() {

  // Check -to option
  Archive in, out;
  Counter counter;
  libzpaq::Writer* outp=&out;
  string output=archive2;
  if (output==".zpaq")
    outp=&counter;
  for (int i=0; i<size(output); ++i)
    if (output[i]=='?' || output[i]=='*')
      error("Output archive cannot be multi-part");
  if (output==archive)
    error("Cannot purge to self");
  else if (!force && exists(output, 1))
    error("Output archive already exists");

  // Copy only, possibly with concatenation or a different key
  int errors=0;
  int64_t archive_size=read_archive(&errors);
  if (archive_size==0) return;
  if (all) {
    if (!in.open(archive.c_str(), password, 'r'))
      error("archive not found");
    if (!out.open(output.c_str(), new_password, 'w', 0))
      error("cannot create output archive");
    const int BUFSIZE=1<<14;
    char buf[BUFSIZE];
    while (true) {
      int n;
      if (archive_size>BUFSIZE) n=in.read(buf, BUFSIZE);
      else n=in.read(buf, archive_size);
      if (n<1) break;
      archive_size-=n;
      out.write(buf, n);
    }
    printUTF8(archive.c_str(), con);
    fprintf(con, " %1.0f -> ", in.tell()+0.0);
    printUTF8(output.c_str(), con);
    fprintf(con, " %1.0f\n", out.tell()+0.0);
    out.close();
    in.close();
    return;
  }

  // Read archive to purge
  if (errors) error("cannot purge archive with errors");
  read_args();

  // Make a list of data blocks. Each block ends at the start of the
  // next block or at end of archive.
  vector<BL> blist(1);  // first element unused
  for (unsigned i=1; i<ht.size(); ++i) {
    if (ht[i].csize>=0 && ht[i].csize!=HT_BAD) {
      BL bl;
      blist.back().end=bl.start=ht[i].csize;
      bl.end=archive_size;
      bl.firstFragment=i;
      blist.push_back(bl);
    }
  }

  // Chop blocks if a version header or index starts in the middle of it.
  // Mark blocks between the header and index as not streaming.
  for (unsigned i=1; i<ver.size(); ++i) {
    if (ver[i].csize>=0) {  // header and index exists?
      for (unsigned j=1; j<blist.size(); ++j) {
        if (ver[i].offset>blist[j].start && ver[i].offset<blist[j].end)
          blist[j].end=ver[i].offset;
        if (ver[i].firstFragment>=1 && ver[i].firstFragment<ht.size()
            && ht[ver[i].firstFragment].csize>=0) {
          int64_t end=ht[ver[i].firstFragment].csize+ver[i].csize;
          if (end>blist[j].start && end<blist[j].end)
            blist[j].end=end;
          if (blist[j].start>ver[i].offset && blist[j].end<=end)
            blist[j].streaming=false;
        }
      }
    }
  }
      
  // Test that blocks are sorted, have non-negative start and size,
  // don't overlap, and are not streaming. Build index bx.
  map<int64_t, unsigned> bx;  // block start -> block number
  for (unsigned i=1; i<blist.size(); ++i) {
    if (blist[i].start<0)
      error("negative block start");
    if (blist[i].end<blist[i].start)
      error("negative block size");
    if (i>0 && blist[i].start<blist[i-1].end)
      error("unsorted block list");
    if (blist[i].streaming)
      error("cannot purge archive with streaming data");
    bx[blist[i].start]=i;
  }

  // Mark used blocks if referenced by files in current version.
  for (DTMap::iterator p=dt.begin(); p!=dt.end(); ++p) {
    if (p->second.written==0) {
      for (unsigned i=0; i<p->second.dtv.back().ptr.size(); ++i) {
        unsigned j=p->second.dtv.back().ptr[i];
        if (j==0 || j>=ht.size() || ht[j].csize==HT_BAD)
          error("bad fragment pointer");
        if (ht[j].csize<0) j+=ht[j].csize;  // start of block
        if (j<1 || j>=ht.size() || ht[j].csize==HT_BAD)
          error("bad fragment offset");
        j=bx[ht[j].csize];  // block number
        if (j<1 || j>=blist.size()) error("missing block");
        ++blist[j].used;
      }
    }
  }

  // Pack fragment ids to remove gaps
  vector<unsigned> fmap(ht.size());  // old -> new fragment id
  for (unsigned i=1, k=1; i<blist.size(); ++i) {
    for (unsigned j=blist[i].firstFragment;
         j<ht.size() && (i+1>=blist.size() || j<blist[i+1].firstFragment);
         ++j) {
      if (blist[i].used && ht[j].csize!=HT_BAD)
        fmap[j]=k++;
    }
  }

  // Prepare temp header
  StringBuffer hdr;
  writeJidacHeader(&hdr, date, -1, 1);

  // Report space saved
  int64_t deleted_bytes=0;
  unsigned deleted_blocks=0;
  for (unsigned i=1; i<blist.size(); ++i) {
    if (!blist[i].used) {
      deleted_bytes+=blist[i].end-blist[i].start;
      ++deleted_blocks;
    }
  }
  fprintf(con, "%1.0f bytes in %u blocks will be purged\n",
      double(deleted_bytes), deleted_blocks);

  // Open input
  if (!in.open(archive.c_str(), password, 'r')) return;

  // Test blocks. They should start with "7kS" or "zPQ" and end with 0xff.
  for (unsigned i=1; i<blist.size(); ++i) {
    in.seek(blist[i].start, SEEK_SET);
    int c1=in.get();
    int c2=in.get();
    int c3=in.get();
    if ((c1!='7' || c2!='k' || c3!='S') && (c1!='z' || c2!='P' || c3!='Q'))
      error("bad block start");
    in.seek(blist[i].end-1, SEEK_SET);
    c1=in.get();
    if (c1!=255) error("bad block end");
  }
  fprintf(con, "%d block locations test OK\n", size(blist)-1);

  // Open output.zpaq for output
  if (output!="") {
    if (!out.open(output.c_str(), new_password, 'w', 0))
      error("Archive open failed");
  }

  // Write temporary header
  outp->write(hdr.c_str(), hdr.size());

  // Copy referenced data blocks
  const int N=1<<17;
  libzpaq::Array<char> buf(N);
  const int64_t cdatastart=out.tell();
  for (unsigned i=1; i<blist.size(); ++i) {
    if (blist[i].used) {
      in.seek(blist[i].start, SEEK_SET);
      int n=0;
      bool first=true;
      for (int64_t j=blist[i].start; j<=blist[i].end; ++j) {
        if (n==N || (n>0 && j==blist[i].end)) {
          if (first) {
            unsigned f=blist[i].firstFragment;
            if (f<1 || f>=fmap.size())
              error("blist[i].firstFragment out of range");
            f=fmap[f];
            if (f<1) error("unmapped firstFragment");
            if (setFilename(&buf[0],n, date, f))
              error("d block filename update failed");
            first=false;
          }
          outp->write(&buf[0], n);
          n=0;
        }
        assert(n<N);
        if (j<blist[i].end) {
          int c=in.get();
          if (c==EOF) error("unexpected EOF");
          buf[n++]=c;
        }
      }
    }
  }
  in.close();
  const int64_t cdatasize=out.tell()-cdatastart;

  // Write fragment tables
  StringBuffer is;
  for (unsigned i=1; i<blist.size(); ++i) {
    unsigned j=blist[i].firstFragment;
    assert(j>0 && j<ht.size() && ht[j].csize!=HT_BAD);
    assert(is.size()==0);
    if (blist[i].used) {
      is+=itob(blist[i].end-blist[i].start);
      for (unsigned k=j; k<ht.size() && (k==j || j-ht[k].csize==k); ++k)
        is+=string(ht[k].sha1, ht[k].sha1+20)+itob(ht[k].usize);
      assert(fmap[j]>0);
      compressBlock(&is, outp, "0",
          ("jDC"+itos(date, 14)+"h"+itos(fmap[j], 10)).c_str());
    }
  }

  // Append compressed index to archive
  int dtcount=0;
  assert(is.size()==0);
  for (DTMap::const_iterator p=dt.begin(); p!=dt.end();) {
    if (p->second.written==0) {
      const DTV& dtr=p->second.dtv.back();
      is+=ltob(dtr.date)+rename(p->first)+'\0';
      if ((dtr.attr&255)=='u') {  // unix attributes
        is+=itob(3);
        is.put('u');
        is.put(dtr.attr>>8&255);
        is.put(dtr.attr>>16&255);
      }
      else if ((dtr.attr&255)=='w') {  // windows attributes
        is+=itob(5);
        is.put('w');
        is+=itob(dtr.attr>>8);
      }
      else is+=itob(0);
      is+=itob(size(dtr.ptr));  // list of frag pointers
      for (int i=0; i<size(dtr.ptr); ++i) {
        unsigned j=dtr.ptr[i];
        if (j<1 || j>=fmap.size()) error("bad unmapped frag pointer");
        j=fmap[j];
        if (j<1 || j>=fmap.size()) error("bad mapped frag pointer");
        is+=itob(j);
      }
    }
    ++p;
    if (is.size()>16000 || (is.size()>0 && p==dt.end())) {
      compressBlock(&is, outp, "1",
                    ("jDC"+itos(date)+"i"+itos(++dtcount, 10)).c_str());
      assert(is.size()==0);
    }
    if (p==dt.end()) break;
  }

  // Complete the update
  int64_t new_archive_size=0;
  if (outp==&out) {
    new_archive_size=out.tell();
    out.seek(32*(new_password!=0), SEEK_SET);
    writeJidacHeader(&out, date, cdatasize, 1);
    if (out.tell()!=size(hdr)+32*(new_password!=0))
      error("output header wrong size");
    out.close();
  }
  else
    new_archive_size=counter.pos;
  fprintf(con, "%1.0f -> %1.0f\n",
      double(archive_size), double(new_archive_size));
}

/////////////////////////////// test //////////////////////////////////

// Test archive integrity, exit on first error
int Jidac::test() {
  StringWriter filename, comment;
  int block=0;
  int versions=0;
  int64_t offset=0;
  bool jdc=false;
  string fn="";
  double mem=0;
  Archive in;
  StringBuffer sb;
  int64_t limit=0;
  int incomplete=0;
  int64_t fdate=0;
  int total_updates=0, total_deletions=0;
  string bad_dates;
  int errcode=0;
  ht.resize(0);
  try {

    // Open archive
    if (!in.open(archive.c_str(), password)) error("open failed");
    fprintf(con, "Testing %s\n", archive.c_str());

    // Decompress blocks
    libzpaq::Decompresser d;
    libzpaq::SHA1 sha1;
    char sha1out[21]={0};
    string lastblockname="";
    d.setInput(&in);
    d.setOutput(&sb);
    d.setSHA1(&sha1);
    while (d.findBlock(&mem)) {
      ++block;
      while (d.findFilename(&filename)) {
        d.readComment(&comment);
        int len=comment.s.size();
        jdc=false;
        if (len>=5 && comment.s.substr(len-5)==" jDC\x01")
          comment.s=comment.s.substr(0, len-=5), jdc=true;
        fprintf(con, "%s %s",
            filename.s.c_str(), comment.s.c_str());
        if (jdc && (size(filename.s)!=28 || filename.s.substr(0, 3)!="jDC"))
          error("filename format not jDC");
        limit=0;  // read size from comment
        for (int i=0; i<size(comment.s) && isdigit(comment.s[i]); ++i)
          limit=limit*10+comment.s[i]-'0';
        sb.reset();
        sha1.result();  // reset
        if (limit>0) sb.setLimit(limit);
        d.decompress();
        d.readSegmentEnd(sha1out);
        fprintf(con, " -> %1.0f", in.tell()+1.0-offset);
        if (sha1out[0]==0) {
          if (!fragile) error("no checksum (try -fragile)");
          fprintf(con, " ?");
          ++incomplete;
        }
        else if (sha1out[0]!=1) error("unknown checksum type");
        else if (memcmp(sha1out+1, sha1.result(), 20))
          error("checksum mismatch");
        else fprintf(con, " OK");
        offset=in.tell()+1;

        // Test output size
        if (limit>0 && int64_t(sb.size())!=limit)
          error("wrong segment size");

        // Test journaling blocks
        if (jdc) {
          assert(size(filename.s)==28);
          if (filename.s<=lastblockname) error("blocks out of order");
          lastblockname=filename.s;
          const int type=filename.s[17];
          const unsigned ffrag=atoi(filename.s.c_str()+18);
          const char* const end=sb.c_str()+sb.size();
          fdate=0;
          for (int i=3; i<17; ++i) {
            if (!isdigit(filename.s[i])) error("non-digit in filename date");
            fdate=fdate*10+filename.s[i]-'0';
          }

          // Test C blocks
          if (type=='c') {
            if (sb.size()!=8) error("bad C block size");
            const char* p=sb.c_str();
            fprintf(con, " ver %d size %1.0f OK", ++versions, btol(p)+0.0);
          }

          // Save D block hashes in HT
          if (type=='d') {
            if (sb.size()<8) error("data block too small");
            const char* p=end-8;
            unsigned n=btoi(p);
            unsigned f=btoi(p);
            if (n!=0 && n!=ffrag) error("bad fragment start");

            // Save sizes in HT
            if (sb.size()<size_t(f*4+8)) error("block too small for frag list");
            p=end-f*4-8;
            size_t sum=0;
            for (unsigned i=ffrag; i<ffrag+f; ++i) {
              while (i>=ht.size()) ht.push_back(HT());
              sum+=ht[i].usize=btoi(p);
            }
            if (f==0 && sb.size()!=8 && !fragile)
              error("missing frag size list (try -fragile)");
            if (f!=0 && sum+4*f+8!=sb.size())
              error("bad frag size list");

            // Save fragment hashes in HT
            libzpaq::SHA1 sha;
            p=sb.c_str();
            for (unsigned i=ffrag; i<ffrag+f; ++i) {
              for (int j=0; j<ht[i].usize; ++j) sha.put(*p++);
              memcpy(ht[i].sha1, sha.result(), 20);
            }
            if (f) fprintf(con, " hashed %u..%u", ffrag, ffrag+f-1);
            else fprintf(con, " no hashes computed"), ++incomplete;
          }

          // Compare H block sizes and hashes to saved from D
          if (type=='h') {
            if (sb.size()<4) error("H block too small");
            if (sb.size()%24!=4) error("bad H block size");
            const unsigned f=sb.size()/24;  // number of frags
            const char* p=sb.c_str();
            const unsigned bsize=btoi(p);
            unsigned i=ffrag;
            for (; p<end; ++i) {
              if (i>=ht.size() || ht[i].usize<0) {
                if (!fragile)
                  error("cannot verify fragment hashes (try -fragile)");
                ++incomplete;
                break;
              }
              for (int j=0; j<20; ++j, ++p)
                if (ht[i].sha1[j]!=(*p&255)) error("frag hash mismatch");
              if (ht[i].usize!=btoi(p)) error("frag size mismatch");
            }
            fprintf(con, " %d in %d..%d %s", bsize, ffrag, ffrag+f-1,
                i==ffrag+f ? "OK" : "?");
          }

          // Test I blocks
          if (type=='i') {
            int updates=0, deletions=0, bd=0;
            for (const char* p=sb.c_str(); p<end;) {
              if (p+8>end) error("date truncated");
              int64_t d=btol(p);
              fn=p;  // filename
              while (p<end && *p) ++p;
              if (p+1>end) error("filename truncated");
              ++p;  // nul
              if (d==0) ++deletions, ++total_deletions;
              else {
                ++updates;
                ++total_updates;
                if (p+4>end) error("attribute length truncated");
                unsigned a=btoi(p);  // number of attribute bytes
                if (p+a+4>end) error("attribute truncated");
                p+=a;
                if (p+4>end) error("ptr list size truncated");
                a=btoi(p);  // number of fragment pointers
                if (p+4*a>end) error("ptr list truncated");
                for (; a && p+4<=end; --a) {
                  unsigned ptr=btoi(p);
                  if ((ptr<1 || ptr>=ht.size()) && !fragile)
                    error("fragment ptr out of range (try -fragile)");
                }

                // Test for bad dates
                if (d<19000000000000LL || d>=30000000000000LL
                    || d/100000000%100<1 || d/100000000%100>12
                    || d/1000000%100<1 || d/10000%100>31
                    || d/100%100>59 || d%100>59) {
                  bad_dates+=itos(d)+" "+fn+"\n";
                  ++bd;
                }
              }
            }
            fprintf(con, " +%d -%d", updates, deletions);
            if (bd) fprintf(con, " %d bad dates!", bd);
            else fprintf(con, " OK");
            fn="";
          }
        }
        fprintf(con, "\n");
        filename.s="";
        comment.s="";
      }  // end while findFilename

      // roll back
      if (versions<1) versions=1;
      if (version>=100000000 && fdate>version) break;
      if (version<100000000 && versions>version) break;
    }  // end while findBlock
  }  // end try
  catch(std::exception& e) {
    fprintf(stderr, "\n%s\n", e.what());
    fprintf(stderr, "in %s %s %s\n", 
        filename.s.c_str(), comment.s.c_str(), fn.c_str());
    fprintf(stderr, "at offset %1.0f, version %d, block %d at %1.0f.\n",
        in.tell()+0.0, versions, block, offset+0.0);
    fprintf(stderr, "Decompressed %1.0f of %1.0f in block using %1.3f MB.\n",
         sb.size()+0.0, limit+0.0, mem*0.000001);
    int count=0;
    for (unsigned i=0; i<ht.size(); ++i) count+=ht[i].usize>=0;
    fprintf(stderr, "Tested %d fragments up to %d.\n", count, size(ht)-1);
    fprintf(stderr, "%d incomplete tests prior to error.\n", incomplete);
    errcode=1;
  }
  if (bad_dates!="") {
    fprintf(stderr, "Error: incorrect file dates: %s\n", bad_dates.c_str());
    errcode=1;
  }
  fprintf(con,
      "Tested %u fragments in %d blocks in %d versions in %1.0f bytes.\n",
      size(ht), block, versions, offset+0.0);
  fprintf(con, "+%d updates and -%d deletions.\n",
      total_updates, total_deletions);
  if (block<1) error("no data found (password incorrect?)");
  if (incomplete)
    fprintf(stderr, "Warning: %d tests not completed\n", incomplete);
  return errcode;
}

/////////////////////////////// main //////////////////////////////////

// Convert argv to UTF-8 and replace \ with /
#ifdef unix
int main(int argc, const char** argv) {
#else
int main() {
  int argc=0;
  LPWSTR* argw=CommandLineToArgvW(GetCommandLine(), &argc);
  vector<string> args(argc);
  libzpaq::Array<const char*> argp(argc);
  for (int i=0; i<argc; ++i) {
    args[i]=wtou(argw[i]);
    argp[i]=args[i].c_str();
  }
  const char** argv=&argp[0];
#endif

  global_start=mtime();  // get start time
  int errorcode=0;
  try {
    Jidac jidac;
    errorcode=jidac.doCommand(argc, argv);
  }
  catch (std::exception& e) {
    fprintf(stderr, "zpaq exiting from main: %s\n", e.what());
    errorcode=1;
  }
  fprintf(con, "%1.3f seconds", (mtime()-global_start)/1000.0);
  if (errorcode) fprintf(con, " (with errors)\n");
  else fprintf(con, " (all OK)\n");
  return errorcode;
}
