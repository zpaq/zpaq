/*  zp v2.00 archiver and file compressor.
    Written by Matt Mahoney, matmahoney@yahoo.com, Sept. 29, 2010.

Copyright (C) 2010, Dell Inc.

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

Usage: zp command archive.zpaq files...

Commands:

  l  - list contents of archive.zpaq
  v  - verbose (detailed) list
  x  - extract with full path names (files... overrides stored names)
  e  - extract to current directory
  xN, eN - extract block N only (starting with 1)
  cN - create new archive with compression option N
  aN - append to archive with option N

Compression options N are 1=fast, 2=medium (default), 3=small.

Archives created with zp conform to the ZPAQ level 1 standard described
at http://mattmahoney.net/dc/
Archives are read/write compatible with other compliant programs such
as zpaq, unzpaq, and zpipe.

To compile, libzpaq.cpp and libzpaq.h must be in the current directory.

  g++ zp.cpp libzpaq.cpp -o zp

Compile with -Dunix in Linux. (Usually this is automatic).
Compile with -DNDEBUG to turn off run time checks.
Use options for optimization as appropriate.

Command details:

The archive name must end with ".zpaq". All commands will add the
extension automatically if you don't specify it. For example:

  zp c3 arc file1 file2
  zp a1 arc file3

will create archive arc.zpaq, compress file1 and file2 with smallest
(slowest) compression, and append file3 with the fastest (least)
compression. The commands "c" and "a" are equivalent to "c2" and "a2"
(medium compression). The files are grouped into one block (solid archive)
for each command.

  zp l arc

will show the contents of arc.zpaq. It will show that file1 and file2
are stored in block 1, and file3 in block 2.

  zp x arc

will extract file1, file2, and file3. You can extract from just one
block:

  zp x1 arc

will extract file1 and file2 only.

  zp x2 arc

will extract file3 only. If you specify file names on the command line
then the output files will be renamed in the order they are listed
and extracted.

  zp x arc newfile1

will extract file1 as newfile1. It will not extract file2 or file3.

  zp x2 arc newfile3

will extract the first file of block 2 (file3) as newfile3. Blocks
are "solid" which means you cannot extract files within a block
without extracting the earlier files. For example, you cannot extract
file2 without also extracting file1.

zp will not clobber existing files during extraction unless you specify
the filenames on the command line.

  zp x arc                    (Error: file1 exists)
  zp x arc file1 file2 file3  (Overwrites file1, file2, file3)

File names are stored in the archive as they appear on the command line.
If you specify a path to a different directory, the path is stored,
and created during extraction. The "e" command extracts to the current
directory.

  zp c arc dir1\dir2\file1
  zp x arc

will create dir1 and dir1\dir2 in the current directory if they do
not already exist, then create dir1\dir2\file1

  zp e arc

will create file1 in the current directory (unless it exists).
If you specify the output filenames, then "e" behaves the same as "x".

If you compress in Windows and extract in Linux, then the program will
change "\" to "/" during extraction and vice versa. Slashes can be stored
with either convention. (The program guesses the operating system
by counting "/" and "\" in the PATH environment variable. If this
heuristic fails (PATH not defined) then no slash translation is done).

Paths must be relative to the current directory. The program will warn
if you store an absolute path. You can only extract such files with
"e" or by overriding the filename.

  zp c arc \dir1\dir2\file1    (Warning: starts with "\")
  zp x arc                     (Error: bad filename)
  zp e arc                     (OK: extracts file1 to current directory)
  zp x arc newfile             (OK: extracts newfile to current directory)
  zp x arc \dir3\dir4\newfile  (OK: creates \dir3\dir4 if needed)

Also, the same rule applies to file names containing control characters,
or longer than 511 characters, or that start with a drive letter like "C:"
or that go up directories (contain ../ or ..\).

If this program is run in Linux or UNIX or compiled with g++ in Windows
then it will interpret wildcards on the command line in the usual way.
A * matches any string and ? matches any character.

  zp c arc *

will compress all files in the current directory to arc.zpaq. However, it
will not recurse directories. You need to specify the files in each
directory that you want to add.

The program does not save file timestamps or permissions like some other
archivers do. Extracted files are dated from the time of extraction
with default permissions. If you need these capabilities, then create a
tar file and compress that instead.

The compression option 1, 2, or 3 means compress fast, medium, or small
respectively. Better compression requires more time and memory.
Decompression speed and memory are the same as for compression. Speed
(T3200, 2.0 GHz) and memory usage are as follows. zip -9 compression is
shown for comparison. All modes compress better (but slower) than zip.

              Memory     Speed     Calgary corpus
              ------  -----------  ---------------
  1 (fast)     38 MB  0.7  sec/MB    807,214 bytes
  2 (mid)     111 MB  2.3  sec/MB    699,586 bytes
  3 (max)     246 MB  6.4  sec/MB    644,545 bytes
  zip -9       <1 MB  0.13 sec/MB  1,020,719 bytes

Options 1, 2, 3 are equivalent to fast.cfg, mid.cfg, and max.cfg
respectively. For example, "zp c3 arc file" is equivalent to
"zpaq ocmax.cfg arc.zpaq file".

mid.cfg and max.cfg are the same as in the ZPAQ 1.10 distribution.
(There is also a min.cfg which is different from fast.cfg.

This program stores a filename, comment, and SHA-1 checksum for each file.
Other programs may omit these, but this program will still be able to
decompress them. This program follows the convention
that if the name is omitted, then the contents should be appended to
the previous file.  If the first filename is omitted, then you must supply it
on the command line during extraction. Each filename on the command line
replaces one named file in the archive.

The comment is the original file size as a decimal string (exact to
2^52, over 4000 TB).

*/

#include <stdio.h>
#include <stdlib.h>
#include <string>
using std::string;

// Some types for writing decimal numbers or discarding output.
class NumberWriter{};
class Null{};

// FILE type with a byte counter
struct FileCounter {
  FILE* f;
  double count;  // number of bytes get or put to f
  FileCounter(FILE* f_=0): f(f_), count(0) {}
};

// libzpaq requires error(), get(), put() definitions.
namespace libzpaq {

  // libzpaq will call error(msg) with an English language message
  // in case of corrupt input or out of memory.
  void error(const char* msg) {
    fprintf(stderr, "zp error: %s\n", msg);
    exit(1);
  }

  // get() may be overloaded for pointer to any type. It should read
  // and return one byte (0..255) or -1 at end of input.
  int get(FILE* in) {
    return getc(in);
  }

  // Read and count a byte
  int get(FileCounter* fc) {
    int c=getc(fc->f);
    if (c!=EOF) fc->count+=1;
    return c;
  }

  // put() may be overloaded for pointer to any type. It should write
  // the low 8 bits of c. This version writes to a file or stdout.
  void put(int c, FILE* out) {
    putc(c, out);
  }

  // Append c to a string
  void put(int c, string* out) {
    (*out)+=char(c);
  }

  // Write one byte c as a decimal number.
  void put(int c, NumberWriter*) {
    printf("%d,", c);
  }

  // Write one byte c to the bit bucket.
  void put(int c, Null*) {}

  // Write one byte and count it
  void put(int c, FileCounter* fc) {
    fc->count+=1;
    putc(c, fc->f);
  }
}
#include "libzpaq.h"

#include <string.h>
#include <errno.h>
#include <time.h>

#ifdef unix
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

/////////////////////////// Decompress ///////////////////////

// Open archive. Append .zpaq to file name if missing.
// filename and mode are as in fopen(). Error if cannot open.
FILE *open_archive(const char *filename, const char *mode) {
  assert(filename);
  assert(mode);
  string newname=filename;
  if (int(newname.size())<5 || newname.substr(newname.size()-5)!=".zpaq")
    newname+=".zpaq";
  FILE *f=fopen(newname.c_str(), mode);
  if (!f) perror(newname.c_str()), libzpaq::error("cannot open archive");
  switch(mode[0]) {
    case 'r': printf("Reading from archive %s\n", newname.c_str()); break;
    case 'w': printf("Created archive %s\n", newname.c_str()); break;
    case 'a': printf("Appending to archive %s\n", newname.c_str()); break;
  }
  return f;
}

// Reject archive filenames with absolute paths, drive letters
// or control characters or that are too long.
static bool validate_filename(const char* filename) {
  int len=strlen(filename);
  if (len<1) return true;  // No name is OK
  if (len>511) return false;  // name too long
  if (strstr(filename, "../")) return false; // no backward paths
  if (strstr(filename, "..\\")) return false;
  if (filename[0]=='/' || filename[0]=='\\') return false; // no absolute path
  for (int i=0; i<len; ++i)  // no control chars or drive letters
    if ((filename[i]&255)<32 || (i==1 && filename[i]==':')) return false;
  return true;
}

// Skip n blocks
template <class Reader, class Writer>
void skip_block(libzpaq::Decompresser<Reader, Writer>& d, int n) {
  for (; n>0 && d.findBlock(); --n) {
    while (d.findFilename()) {
      d.readComment();
      d.readSegmentEnd();
    }
  }
}

// Remove path from filename
static string strip(const string& filename) {
  for (int i=int(filename.size())-1; i>=0; --i) {
    if (filename[i]=='/' || filename[i]=='\\' || (i==1 && filename[i]==':'))
      return filename.substr(i+1);
  }
  return filename;
}

// Open filename. Depending on OS, change slashes to / or \.
// If this fails then try creating directories in its path.
// If it fails again, return 0, else return FILE*.
static FILE* create(string filename) {

  // Find last slash in filename
  int slash=-1;
  for (int i=0; i<int(filename.size()); ++i)
    if (filename[i]=='/' || filename[i]=='\\')
      slash=i;

  // If there is no path, then open file and return
  if (slash<0)
    return fopen(filename.c_str(), "wb");

  // Guess the OS by counting / (Linux) or \ (Windows) in PATH
  const char* path=getenv("PATH");
  static int os=0; // <0 if Windows, >0 if Linux, 0 if unknown
  if (os==0) {
    for (int i=0; path && path[i]; ++i) {
      if (path[i]=='/') ++os;
      if (path[i]=='\\') --os;
    }
  }

  // Change slashes in filename per OS if known.
  for (int i=0; i<int(filename.size()); ++i) {
    if (os>0 && filename[i]=='\\') filename[i]='/';
    if (os<0 && filename[i]=='/') filename[i]='\\';
  }

  // Try opening file
  FILE *f=fopen(filename.c_str(), "wb");
  if (f) return f;

  // If this doesn't work, try creating a directory for it using "mkdir"
  if (os && errno==ENOENT) {
    string cmd = os<=0 ? "mkdir " : "mkdir -p ";
    cmd+=filename.substr(0, slash);
    printf("%s\n", cmd.c_str());
    system(cmd.c_str());

    // Last try
    return fopen(filename.c_str(), "wb");
  }
  return 0;
}

// Decompress: eN|xN archive [files...]
static void decompress(int argc, char** argv) {
  assert(argc>=3);

  // Open archive
  libzpaq::Decompresser<FILE, FILE> d;
  FILE* in=open_archive(argv[2], "rb");
  d.setInput(in);

  // If user specifies N then skip N-1 blocks
  int block=atoi(argv[1]+1);
  if (block>0)
    skip_block(d, block-1);

  // Read the archive
  FILE* out=0;  // output file
  int filecount=0;  // number of files extracted
  libzpaq::SHA1 sha1;
  d.setSHA1(&sha1);
  while (d.findBlock()) {
    for (string filename; d.findFilename(&filename); filename="") {
      string comment;
      d.readComment(&comment);
      printf("%s %s ", filename.c_str(), comment.c_str());

      // open output file
      // if filename is empty, use the previously opened file
      if (filename!="") {

        // close last file
        if (out) {
          fclose(out);
          out=0;
          ++filecount;
        }

        // if the user gave an output file starting at argv[3], use it instead.
        if (argc>3) {
          if (filecount+3>=argc) {
            printf("and remaining files not extracted\n");
            goto end;
          }
          char* name=argv[filecount+3];
          out=create(name);
          if (!out) {
            perror(name);
            goto end;
          }
          else
            printf("-> %s ", name);
        }

        // Otherwise, use the names in the archive, but don't clobber
        // or use suspicious filenames
        else {
          string newname=filename;
          if (argv[1][0]=='e') newname=strip(filename);
          if (newname!=filename)
            printf("-> %s ", newname.c_str());
          if (!validate_filename(newname.c_str())) {
            printf("Error: bad filename\n");
            goto end;
          }
          out=fopen(newname.c_str(), "rb");
          if (out) {
            fclose(out);
            out=0;
            printf("Error: won't overwrite\n");
            goto end;
          }
          else {
            out=create(newname.c_str());
            if (!out) {
              perror(newname.c_str());
              goto end;
            }
          }
        }
      }
      if (!out) {
        printf("Output filename not specified\n");
        goto end;
      }

      // Decompress and report progress every 100 KB
      d.setOutput(out);
      printf("-> ");
      while (d.decompress(100000)) {
        for (int i=printf("%1.0f ", sha1.size()); i>0; --i)
          putchar('\b');
        fflush(stdout);
      }

      // Verify checksum
      char sha1string[21];
      d.readSegmentEnd(sha1string);
      bool sha1result=memcmp(sha1string+1, sha1.result(), 20);
      if (sha1string[0]) {
        if (sha1result) printf("WARNING: CHECKSUM MISMATCH\n");
        else printf("OK, checksum verified\n");
      }
      else printf("OK, no checksum   \n");
    }
    if (block) break;
  }

  // Close files
end:
  if (out) fclose(out), ++filecount;
  fclose(in);
  printf("%d file(s) extracted\n", filecount);
}

//////////////////////////// Compress ////////////////////////////

// Test for regular file (Linux)
static bool is_file(const char* filename) {
#ifdef unix
  struct stat st;
  return stat(filename, &st)==0 && (st.st_mode & S_IFREG);
#endif
  return true;
}

// Compress files: c|a[N] archive files...
// Command (c, c1, c2, c3, a, a1, a2, a3) is in argv[1]
// Archive file name is in argv[2]
// Files to compress are in argv[3]...argv[argc-1]
// Commands: c=compress, a=append, N=compression level (1,2,3)
static void compress(int argc, char** argv) {
  assert(argc>=3);

  libzpaq::Compressor<FileCounter, FileCounter> c;
  libzpaq::SHA1 sha1;

  // Select compression level 1, 2, or 3
  int level=atoi(argv[1]+1);
  if (level<1) level=2;

  // Compress files in argv[3...argc-1]
  int filecount=0;  // number of files compressed
  FileCounter out(0);  // output file
  double start=0;  // output byte count at start of each file
  for (int i=3; i<argc; ++i) {

    // Ignore directories
    if (!is_file(argv[i])) {
      fprintf(stderr, "%s: not a regular file\n", argv[i]);
      continue;
    }

    // Strip path unless in current directory
    if (!validate_filename(argv[i])) {
      fprintf(stderr, "%s: not in current directory hierarchy\n",
        argv[i]);
      continue;
    }

    // Open input file
    FileCounter in;
    in.f=fopen(argv[i], "rb");
    if (!in.f) {
      perror(argv[i]);
      continue;
    }

    // Get checksum and file size
    int ch;
    while ((ch=getc(in.f))!=EOF)
      sha1.put(ch);
    rewind(in.f);
    char comment[20];
    sprintf(comment, "%1.0f", sha1.size());

    // Open archive for first file
    if (filecount==0) {

      // Create or append archive
      out.f=open_archive(argv[2], argv[1][0]=='a'?"ab":"wb");
      c.setOutput(&out);

      // Write block header
      c.startBlock(level);
    }

    // Write segment header
    c.startSegment(argv[i], comment);
    if (filecount==0)
      c.postProcess();

    // Compress and report progress every 100K
    printf("%s %s ", argv[i], comment);
    c.setInput(&in);
    while (c.compress(100000)) {
      for (int j=printf("%1.0f -> %1.0f ", in.count,out.count-start);j>0;--j)
        putchar('\b');
      fflush(stdout);
    }
    printf("-> %1.0f               \n", out.count-start);
    start=out.count;

    // Append SHA-1 checksum to end of segment
    c.endSegment(sha1.result());
    ++filecount;
  }

  // End block
  if (filecount>0) {
    c.endBlock();
    printf("%d file(s) compressed to %s -> %1.0f\n",
      filecount, argv[2], out.count);
    fclose(out.f);
  }
  else
    printf("Archive %s not updated\n", argv[2]);
}

////////////////////////// list //////////////////////////

// List archive contents: l archive
// Archive file name is in argv[2]. For each block, show memory required
// to decompress. For each segment, show the filename and comment.
static void list(int argc, char** argv) {
  assert(argc>2 && argv[2]);
  libzpaq::Decompresser<FILE, FILE> d;
  double memory;
  d.setInput(open_archive(argv[2], "rb"));
  for (int i=1; d.findBlock(&memory); ++i) {
    printf("======== Block %d requires %1.3f MB memory\n", i, memory/1e6);
    while (d.findFilename(stdout)) {
      printf(" ");
      d.readComment(stdout);
      printf("\n");
      d.readSegmentEnd();
    }
  }
}

// Verbose archive listing: v archive
// Archive file name is in argv[2]. For each block, show memory required
// to decompress and show the hcomp and pcomp strings containing the
// decompression algorithm. For each segment, show the filename,
// comment, and SHA-1 checksum (if any).
static void verbose(int argc, char** argv) {

  // Some variables to hold values read from the archive. An archive
  // is a sequence of independent blocks. Each block describes the
  // decompression algorithm in two strings called hcomp and pcomp.
  // Each block holds one or more segments that must be decompressed
  // in order from the start of the block. Each segment has an optional
  // filename string, and optional comment string, some compressed data,
  // and an optional 20 byte SHA-1 checksum of the data before compression.
  double memory;
  string filename, comment;
  char sha1string[21];

  // Create object d to decompress ZPAQ archives. Input will be read
  // from a FILE*. Decompressed output would go to a Null* which just
  // discards any output. Normally that would also be a FILE.
  libzpaq::Decompresser<FILE, Null> d;

  // Set the input to the archive
  FILE* in=open_archive(argv[2], "rb");
  d.setInput(in);

  // Search for the next block and return false when done.
  // If true, calculate memory required for decompression.
  for (int i=1; d.findBlock(&memory); ++i) {
    printf("Block %d needs %1.6f MB memory\n", i, memory/1e6);
    bool firstSegment=true;  // first segment in block?

    // Find the next segment in the block. If found, read the file
    // name from the segment header and write it to filename.
    // The argument can be any pointer type T* that defines put(int, T*).
    // If there are no more segments in the block, return false.
    while (d.findFilename(&filename)) {

      // Read the comment from the segment header (like filename).
      // There is no limit on how long the filename or comment might be.
      d.readComment(&comment);

      // If this is the first segment in the block, then print the
      // hcomp string. This string contains ZPAQL code which describes
      // the decompression algorithm. The argument can be any type T*
      // that defines put(int, T*). Here we just write the code as
      // a list of bytes as decimal numbers. The format is suitable
      // for passing to Compressor::startBlock(hcomp). The first 2
      // bytes is the length of the rest of the string in little-endian
      // (LSB, MSB) format. The maximum possible size is 65537.
      if (firstSegment) {
        printf("hcomp=");
        d.hcomp((NumberWriter*)0);

        // Decompress 0 bytes. We need to do this before getting the
        // pcomp string because it is compressed prior to the data in
        // the beginning of the first segment using the compression
        // algorithm described in hcomp. decompress(0) has the effect
        // of decompressing this string but stopping before decompressing
        // any data. On the other hand, decompress(n) would decompress
        // up to n bytes and return true if there is more data remaining.
        // d.decompress(-1) or d.decompress() would decompress the whole
        // setment and return false. The decompressed output would go
        // to the destination set by d.setOutput().
        d.decompress(0);

        // Print the pcomp string if present. pcomp describes a
        // postprocessing algorithm in ZPAQL code. It is optional.
        // If omitted, then the decompressed data is output directly.
        // pcomp(w) returns true if a string is present and writes
        // it to w, where w is any pointer of type T* where put(int, T*)
        // is defined. If no pcomp string is present, then it returns
        // false without writing anything. If present, the output
        // format is suitable for passing to Compressor::postProcess(pcomp).
        // The first 2 bytes of the string is the length of the rest
        // of the string in little-endian format. The maximum output
        // is 65537 bytes. Here we write the output as a list of
        // decimal numbers.
        printf("\npcomp=");
        if (!d.pcomp((NumberWriter*)0))
          printf("(empty)");
        printf("\n");
        firstSegment=false;
      }

      // Read the SHA-1 checksum at the end of the segment, skipping
      // any remaining compressed data. The checksum is optional.
      // If present, then a 1 will be written to sha1string[0]
      // and the 20 byte checksum will be written to sha1string[1] through
      // sha1string[20]. If there is no checksum, then a 0 will be
      // written to sha1string[0].
      d.readSegmentEnd(sha1string);
      printf("  ");
      if (sha1string[0]) {
        for (int i=1; i<21; ++i)
          printf("%02x", sha1string[i]&255);
      }
      else printf("                    ");

      // Write the filename and comment. We have to clear the
      // strings afterward because we defined put() to append bytes.
      printf(" %10s %s\n", comment.c_str(), filename.c_str());
      filename=comment="";
    }
  }
  fclose(in);
  printf("\n");
}

///////////////////////////// Main ///////////////////////////

// Print help message and exit
static void usage() {
  printf("ZP v2.00 archiver, (C) 2010, Dell Inc.\n"
    "Written by Matt Mahoney, " __DATE__ ".\n"
    "Licensed under GPL v3, http://www.gnu.org/copyleft/gpl.html\n"
    "\n"
    "Usage: zp command archive.zpaq [files...]\n"
    "Commands:\n"
    "  l, v    List archive contents (regular, verbose)\n"
    "  x       Extract with full path names (files... overrides stored names)\n"
    "  e       Extract to current directory\n"
    "  xN, eN  Extract only block N (1, 2, 3...)\n"
    "  c       Create new archive\n"
    "  a       Append to archive\n"
    "  cN, aN  Compress with option N\n"
    "Compression options:\n"
    "  1,2,3   Fast, medium, small (default is 2)\n"
    );
  exit(0);
}

// Command syntax as in usage()
int main(int argc, char** argv) {

  // Check usage
  if (argc<2) 
    usage();

  // Do the command
  char cmd=argv[1][0];
  if (argc>=4 && (cmd=='a' || cmd=='c'))
    compress(argc, argv);
  else if (argc>=3 && (cmd=='x' || cmd=='e'))
    decompress(argc, argv);
  else if (argc>=3 && cmd=='l')
    list(argc, argv);
  else if (argc>=3 && cmd=='v')
    verbose(argc, argv);
  else
    usage();

  // Print time used
  printf("Elapsed time %1.2f seconds.\n", 
    double(clock())/CLOCKS_PER_SEC);
  return 0;
}
