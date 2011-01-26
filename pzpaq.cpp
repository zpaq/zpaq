/* pzpaq.cpp v0.02 - Parallel ZPAQ compressor

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
See usage() below for brief description.

See http://mattmahoney.net/dc/pzpaq.html for complete documenation.

See http://mattmahoney.net/dc/zpaq.html for the latest version
of this software and for libzpaq which you will need to compile.

To compile in Linux:

  g++ -O3 -DNDEBUG pzpaq.cpp libzpaq.cpp libzpaqo.cpp -lpthread

To compile in Windows

  g++ -O3 -DNDEBUG pzpaq.cpp libzpaq.cpp libzpaqo.cpp -lpthreadGC2

For Windows you also need these files from pthreads-win32 from
http://sourceware.org/pthreads-win32/
  
  libpthreadGC2.a   in C:\mingw\lib or -L
  pthread.h         in C:\mingw\include or -I
  sched.h
  semaphore.h
  pthreadGC2.dll    in PATH (to run)
*/

#include "libzpaq.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <assert.h>
#include <pthread.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <string>
#include <vector>

// Borland: compile with -Dint64_t=__int64
#ifndef int64_t
#include <stdint.h>
#endif

void usage() {
  fprintf(stderr,
  "pzpaq 0.02 - Parallel ZPAQ compressor\n"
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
// the thread finishes, it sets its state to FINISHED, signals
// main (using cv, protected by mutex), and exits with a status
// of 0 or a pointer to an error message as a static string.
// main then waits on the thread, receives the return status, and
// updates the state to OK or ERR.
typedef enum {READY, RUNNING, FINISHED, ERR, OK} State;

pthread_cond_t cv=PTHREAD_COND_INITIALIZER;  // to signal FINISHED
pthread_mutex_t mutex=PTHREAD_MUTEX_INITIALIZER; // protects cv

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
    fprintf(stderr, "Appending to %s from %s ... ", file1, file2);
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
  if (verbose)
    fprintf(stderr, "%1.0f\n", double(filesize(out)));
  if (out!=stdout) fclose(out);
  if (in!=stdin) fclose(in);
  if (in!=stdin && remove(file2))
    perror(file2);
  return true;
}

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
      printf("Block %d command %d needs %d MB\n",
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
void *thread(void *arg) {

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
  check(pthread_mutex_lock(&mutex));
  job->state=FINISHED;
  check(pthread_cond_signal(&cv));
  check(pthread_mutex_unlock(&mutex));
  return (void*)result;
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
  if (command=='d' || command=='e' || command=='x') {
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
              bool first_segment=true;
              while (d.findFilename(&filename)) {
                d.readComment();
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
  pthread_attr_t attr; // thread joinable attribute
  check(pthread_attr_init(&attr));
  check(pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE));

  // Aquire lock on jobs[i].state.
  // Threads can access only while waiting on a FINISHED signal.
  check(pthread_mutex_lock(&mutex));
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
      check(pthread_create(&jobs[bi].tid, &attr, thread, &jobs[bi]));
    }

    // If no jobs can start then wait for one to finish
    else {
      if (thread_count<1) { // no jobs to wait on?
        fprintf(stderr, "Not enough memory, try larger -m\n");
        break;
      }
      check(pthread_cond_wait(&cv, &mutex));  // wait

      // Join any finished threads. Usually that is the one
      // that signaled it, but there may be others.
      for (int i=0; i<size(jobs); ++i) {
        if (jobs[i].state==FINISHED) {
          void* status;
          check(pthread_join(jobs[i].tid, &status));
          jobs[i].state=status?ERR:OK;
          ++job_count;
          --thread_count;
          memory_count-=jobs[i].memory;
        }
      }
    }
  }
  check(pthread_mutex_unlock(&mutex));

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

