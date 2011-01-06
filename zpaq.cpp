/*  zpaq v2.05 archiver and file compressor.

(C) 2009-2011, Dell Inc.
    Written by Matt Mahoney, matmahoney@yahoo.com, Jan. 5, 2010

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

This program compresses files into archives and decompresses them.
The archive format is compatible with other ZPAQ level 1 compliant
programs. It has 3 built in compression levels and allows user
configuration files and preprocessors to specify arbitrary compression
algorithms. See http://mattmahoney.net/dc/zpaq.html for the latest version
of this program and full documentation. See usage() below for brief summary
of commands.

To compile, you need libzpaq from the above link:

  g++ -O3 -DNDEBUG -DOPT="..." zpaq.cpp libzpaq.cpp libzpaqo.cpp -o zpaq

-DNDEBUG turns off run time checks for better speed.

-DOPT should be omitted if no C++ compiler is available on the
machine where zpaq will be run. Otherwise, -DOPT enables the "o" modifier
which speeds up compression or decompression (typically twice as fast)
when using compression levels other than the 3 defaults (1, 2, 3, or
fast, mid, max).

When "o" is used with any compression or decompression command,
zpaq will create zpaqopt.cpp in the current directory, compile it to
zpaqopt.exe, and run it with the same arguments. zpaqopt.exe works like zpaq
except that it runs faster for the given config file (compression) or
archive (decompression). It works by replacing libzpaqo.cpp with zpaqopt.cpp.
OPT is the compile command that generates zpaqopt.exe in the current directory.
For example:

  -DOPT="\"g++ -O3 -DNDEBUG zpaq.cpp libzpaq.cpp zpaqopt.cpp -o zpaqopt.exe\""

although this would only work if all of the source code (including libzpaq.h)
were in the current directory. Usually these files will be in some fixed
location that the OPT command must specify. Alternatively, OPT could be
set to a script that contains the commands. zpaq will then execute the script.
For example:

  -DOPT=\"makezpaq\"

libzpaq.cpp and zpaq.cpp might be compiled in advance to .o files which
will speed up the build of zpaqopt.exe. Build them like this:

  g++ -O3 -DNDEBUG -c zpaq.cpp libzpaq.cpp

(Note that -DOPT is not used here. Thus, do not build zpaq or zpaq.exe
from this zpaq.o).

The script install.bat will install in Windows using either the MinGW g++,
Visual C++, Borland, or Mars compiler. However, only g++ expands wildcards.
Otherwise, a command like:

  zpaq c1 archive *.txt

won't compress all files with a .txt extension. It will fail because
you don't have a file named exactly "*.txt".

To use the installation script:

  install c:\bin g++.exe

This will create the following files:

  c:\bin\zpaq.exe
  c:\bin\zpaq\zpaq.o
  c:\bin\zpaq\libzpaq.o
  c:\bin\zpaq\libzpaq.h

You should have c:\bin in your PATH. If not, then either add it or choose
another destination for the first argument to install.bat that is already
in your PATH. The other 3 files will be placed in a subdirectory zpaq under
the installation directory. Those files are needed to use the "o" modifier.
The second argument can be one of the following (ordered from fastest to
slowest):

  g++.exe     (MinGW g++)
  cl.exe      (Microsoft Visual C++)
  bcc32.exe   (Borland)
  dmc.exe     (Mars)

The compiler must be in your PATH when you run install.bat and whenever you
run zpaq.exe with "o". For most compilers, you probably did this when you
installed it. For Visual C++, there are several environment variables to
set. The easiest way to set them is to start the Visual Studio and
select "Tools/Open command line window" from the menu, then use that window
to run install.bat or zpaq.exe. For the other compilers the normal
installation path is:

  c:\mingw\bin\g++.exe
  c:\borland\bin\bcc32.exe
  c:\dm\bin\dmc.exe

In Linux, the files might be installed:

  /usr/bin/zpaq
  /usr/lib/zpaq/zpaq.o
  /usr/lib/zpaq/libzpaq.o
  /usr/include/libzpaq.h

To create the installation files:

  g++ -O3 -DNDEBUG -c zpaq.cpp libzpaq.cpp
  g++ -O3 -DNDEBUG zpaq.cpp libzpaq.cpp libzpaqo.cpp -o zpaq \
    -DOPT="\"g++ -O3 zpaqopt.cpp /usr/lib/zpaq/zpaq.o /usr/lib/zpaq/libzpaq.o -o zpaqopt.exe\""

-DNDEBUG has no effect on zpaqopt.cpp so it is not needed.

You can customize compiler optimizations to your local machine.
Recommended options are:

  g++ -O3 -s -march=native -fomit-frame-pointer -DNDEBUG
  cl /Ox /GL /DNDEBUG
  bcc32 -O -6 -DNDEBUG
  dmc -o -6 -DNDEBUG

All compilers understand -D, -I and -c  (for cl: /D, /I, /c).
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <time.h>

#ifdef unix
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#include "libzpaq.h"

// Print help message and exit
void usage() {
  fprintf(stderr, "ZPAQ v2.05 archiver, (C) 2009-2011, Dell Inc.\n"
    "Written by Matt Mahoney, " __DATE__ ".\n"
    "This is free software under GPL v3, http://www.gnu.org/copyleft/gpl.html\n"
    "\n"
    "To compress: zpaq [nsiptokv]c|a[L|F[,N...]] archive [folder/] [files]...\n"
    "  n = don't store filenames (extraction will concatenate)\n"
    "  s = don't store SHA1 checksums (saves 20 bytes)\n"
    "  i = don't store file sizes as comments (saves a few bytes)\n"
    "  p = store full path names (default is bare filenames)\n"
    "  t = append locator tag to non-ZPAQ data such as zpsfx.exe\n"
    "  c = create new archive.zpaq with 1 block\n"
    "  a = or append 1 block to existing archive or archive.zpaq\n"
    "  L = compression level 1=fast, 2=mid, 3=max\n"
    "  F = or use configuration file F.cfg\n"
    "  ,N = pass numeric arguments to F.cfg\n"
    "  folder/ = store path for extraction (default = filename only)\n"
    "  files... = input files, default is archive\n"
    "To list contents: zpaq [okv]l archive\n"
    "To extract: zpaq [ok]x[B] archive [folder/] [files...]\n"
    "  B = extract only block B (1, 2, 3...)\n"
    "  folder/ = extract to folder (default = stored paths)\n"
    "  files... = rename extracted files (clobbers)\n"
    "      otherwise use stored names (does not clobber)\n"
    "      If no name is stored, extract archive.zpaq to archive\n"
    "To debug configuration file F.cfg: zpaq [ptokv]rF[,N...] [args...]\n"
    "  p = run PCOMP (default is to run HCOMP)\n"
    "  t = trace (single step), args are numeric inputs\n"
    "      otherwise args are input, output (default stdin, stdout)\n"
    "  ,N = pass numeric arguments to F\n"
    "For all commands:\n"
    "  v = verbose (echo F.cfg or detailed listing)\n"
    );
#ifdef OPT
  fprintf(stderr,
    "  o = compress or decompress faster (requires C++ compiler) using:\n"
    "    %s\n"
    "  k = with o, keep zpaqopt.cpp, zpaqopt.exe\n",
    OPT);
#endif
#ifndef NDEBUG
  fprintf(stderr, "DEBUG version (not compiled with -DNDEBUG)\n");
#endif
  exit(0);
}

// Print an error message and exit
namespace libzpaq {
  void error(const char* msg="") {
    fprintf(stderr, "\nError: %s\n", msg);
    exit(1);
  }
}
using libzpaq::error;

// FILE type with a byte counter
struct File: public libzpaq::Reader, public libzpaq::Writer {
  FILE* f;
  double count;  // number of bytes get or put to f
  File(FILE* f_=0): f(f_), count(0) {}

  // Read and count a byte
  int get() {
    int c=getc(f);
    if (c!=EOF) count+=1;
    return c;
  }

  // Write and count a byte
  void put(int c) {
    count+=1;
    putc(c, f);
  }
};

/////////////////////////////// String ////////////////////////////

// Sort of like std::string but with get(), put() for libzpaq
class String: public libzpaq::Writer, public libzpaq::Reader {
  char* a;  // NUL terminated array
  int size; // allocated size
  int len_; // user size, len() < size
  void resize(int n);  // increase allocation to allow len() == n
public:
  String(const char* s=0, int n=-1);  // Create with length n, default strlen
  explicit String(char c);  // convert to length 1 String
  String(const String& s);  // copy constructor
  String& operator=(const String& s);  // assignment
  ~String() {if (a) free(a);}  // destructor
  const char* c_str() const {return a?a:"";}  // convert to const char*
  int len() const {return len_;}  // user length
  void put(int c);  // append c
  int get();  // read and remove first char or -1 if empty
  char& operator[](int i) {assert(i>=0 && i<len()); return a[i];}  // char
  int operator()(unsigned int i) const {  // byte, or 0 if out of bounds
    return i<(unsigned int)len() ? (a[i]&255) : 0;}
  String& operator += (const String& s);  // append
  String sub(int i, int n) const;  // substring from i, length n with clipping
  String sub(int i) const {return sub(i, len()-i);}  // substring from i to end
};

// Increase size > n to allow len() == n
// if a == 0 then init len() = 0, else leave unchanged
void String::resize(int n) {
  n=(n+64)&-64;
  assert(n>=0);
  if (!a) size=len_=0;
  if (n>size) {
    char* tmp=(char*)calloc(n, 1);
    if (!tmp) libzpaq::error("String out of memory");
    if (a) {
      memcpy(tmp, a, size);
      free(a);
    }
    a=tmp;
    size=n;
  }
  assert(a);
  assert(size>len());
  assert(len()>=0);
}

// Convert n (default to NUL) chars of s to String
String::String(const char* s, int n): a(0) {
  if (s==0)
    size=len_=0;
  else {
    if (n<0) n=strlen(s);
    resize(n);
    memcpy(a, s, n);
    a[len_=n]=0;
    assert(size>=len());
  }
}

// Convert char to 1 character String
String::String(char c): a(0) {
  resize(1);
  a[0]=c;
  a[1]=0;
  len_=1;
  assert(size>len());
}

// Copy constructor
String::String(const String& s): a(0) {
  resize(s.len());
  assert(a);
  if (s.a) memmove(a, s.a, s.len()+1);
  len_=s.len();
  assert(size>len());
}

// Assignment
String& String::operator=(const String& s) {
  if (&s==this) return *this;
  if (a) free(a);
  a=0;
  resize(s.len());
  len_=s.len();
  assert(a);
  assert(size>len());
  assert(s.size>len());
  memcpy(a, s.a, s.len()+1);
  return *this;
}

// Concatenate
String& String::operator+=(const String& s) {
  resize(len()+s.len());
  assert(size>len()+s.len());
  memcpy(a+len(), s.c_str(), s.len());
  len_+=s.len();
  assert(size>len());
  a[len()]=0;
  return *this;
}

// Append a char
void String::put(int c) {
  resize(len()+1);
  a[len()]=c;
  a[++len_]=0;
  assert(size>len());
}

// Return n chars starting at i, clipping if out of range
String String::sub(int i, int n) const {
  if (!a) return "";
  if (i<0) n+=i, i=0;
  if (i+n>len()) n=len()-i;
  if (n<=0) return "";
  return String(a+i, n);
}

// Read a char
int String::get() {
  if (len()==0) return -1;
  int c=(*this)(0);
  *this=sub(1);
  return c;
}

// Concatenate Strings
String operator+(String s1, const String& s2) {
  return s1+=s2;
}

// Compare Strings
bool operator==(const String& s1, const String& s2) {
  return s1.len()==s2.len() && memcmp(s1.c_str(), s2.c_str(), s1.len())==0;
}

bool operator!=(const String& s1, const String& s2) {
  return !(s1==s2);
}

/*
// Alternate but equivalent String class (not used)
// Does not work with Mars compiler

#include <string>

// Improved std:string with output by appending to it
struct String: public libzpaq::Writer, public libzpaq::Reader, public std::string {
  String(const std::string& s): std::string(s) {} // base to derived conversions
  String(const char* s=""): std::string(s) {}
  explicit String(char c): std::string(1, c) {}
  void put(int c) {*this+=char(c);}     // append 1 byte
  int get();                            // read and remove first byte or EOF
  int len() const {return int(size());} // size as a signed int
  int operator()(unsigned int i) const; // i'th byte, bounds checked
  String sub(int i, int n) const;       // clipped substr(i, n)
  String sub(int i) const;              // clipped substr(i)
};

int String::get() {
  if (len()==0) return -1;
  int c=(*this)(0);
  *this=sub(1);
  return c;
}

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
*/

//////////////////////////////// compile ///////////////////////////

// This code is to read configuration files containing custom
// compression algorithms written in ZPAQL.

// Globals
bool verbose=false;  // display config file as it compiles?
int args[9]={0};     // configuration file arguments
bool keep_option=false;  // keep temporary files?

// Symbolic constants
typedef enum {NONE,CONST,CM,ICM,MATCH,AVG,MIX2,MIX,ISSE,SSE,
  JT=39,JF=47,JMP=63,LJ=255,
  POST=256,PCOMP,END,IF,IFNOT,ELSE,ENDIF,DO,
  WHILE,UNTIL,FOREVER,IFL,IFNOTL,ELSEL,SEMICOLON} CompType;

// Component names
static const char* compname[256]=
  {"","const","cm","icm","match","avg","mix2","mix","isse","sse",0};

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
  int top;
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

// Compile config file in cmd, like "3" or "min,2,1". If it starts
// with a nonzero digit then return the number and leave the strings empty.
// Otherwise, fill hcomp, pcomp, and pcomp_cmd from the config file
// with or without a .cfg extension (min or min.cfg) and put the
// numeric arguments in args[9] (args[0]=2, args[1]=1), and return 0.
int compile_cmd(const char* cmd, String& hcomp,
                String& pcomp, String& pcomp_cmd) {
  int level=0;
  if (isdigit(cmd[0]))
    level=atoi(cmd);
  if (level==0) {

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
    compile(in, hcomp, pcomp, pcomp_cmd);
    fclose(in);
  }
  return level;
}

/////////////////////////// optimize ///////////////////////

// This code is to convert ZPAQL to C++.

// Pad pcomp string with an empty COMP header with ph,pm from hcomp
void fix_pcomp(const String& hcomp, String& pcomp) {
  if (hcomp.len()>=8 && pcomp.len()>=2) {
    pcomp=hcomp.sub(0, 8)+pcomp.sub(2);
    assert(pcomp.len()>7);
    pcomp[0]=(pcomp.len()-2)&255;  // new length of PCOMP
    pcomp[1]=(pcomp.len()-2)>>8;
    pcomp[6]=pcomp[7]=0;  // n=0 components
  }
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

// Test if a file exists and exit with error if not
void testfile(const char* filename) {
  if (!exists(filename)) {
    fprintf(stderr, "File not found: %s\n", filename);
    exit(1);
  }
}

// Print and run a command
int run_cmd(const String& cmd) {
  fprintf(stderr, "%s\n", cmd.c_str());
  return system(cmd.c_str());
}

// ZPAQ install directory, defined in zpaqopt.cpp
extern const char* zpaqdir;
#ifdef OPT
const char* zpaqdir=0;
#endif

// Return '/' in Linux or '\' in Windows
char slash() {
#ifdef unix
  return '/';
#else
  return '\\';
#endif
}

#ifdef OPT

// Generate one case of predict()
void opt_predict(FILE *out, const String& models, int p, int select) {
  assert(models.len()>p+7);
  int n=models(p+6);
  fprintf(out,
    "    case %d: {\n"
    "      // %d components\n", select, n);

  // Code each component
  p+=7;
  for (int i=0; i<n; ++i) {
    int cp[10]={0};
    for (int j=0; j<10 && p+j<models.len(); ++j)
      cp[j]=models(p+j);
    switch(cp[0]) {
      case CONST:  // c
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
        error("unknown component type");
    }
    assert(libzpaq::compsize[cp[0]]>0);
    p+=libzpaq::compsize[cp[0]];
    assert(p<models.len());
  }
  assert(models(p)==NONE);
  if (n<1)
    fprintf(out,
      "      return predict0();\n"
      "    }\n");
  else
    fprintf(out,
      "      return squash(p[%d]);\n"
      "    }\n", n-1);
}

void opt_update(FILE *out, const String& models, int p, int select) {
  assert(models.len()>p+7);
  int n=models(p+6);
  fprintf(out,
    "    case %d: {\n"
    "      // %d components\n", select, n);

  // Code each component
  p+=7;
  for (int i=0; i<n; ++i) {
    int cp[10]={0};
    for (int j=0; j<10 && p+j<models.len(); ++j)
      cp[j]=models(p+j);
    switch(cp[0]) {
      case CONST:  // c
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
        error("unknown component type");
    }
    assert(libzpaq::compsize[cp[0]]>0);
    p+=libzpaq::compsize[cp[0]];
    assert(p<models.len());
  }
  assert(models(p)==NONE);
  fprintf(out,
    "      break;\n"
    "    }\n");
}

// Generate optimization code for the HCOMP section of z
void opt_hcomp(FILE *out, const String& models, int p, int select) {

  /* Instruction translation table. It was generated from
  the body of ZPAQL::run0() with the following perl script,
  then hand editing OUT, JT, JF, JMP, and LJ.

  for ($i=0; $i<256; ++$i) {
    $a[$i]="    \"err();\",";
  }
  while (<>) {
    chomp;
    if (/case (\d+): (.*) break; *\/\/(.*)/) {
      $n=$1;
      $op=$2;
      $comment=$3;
      $op=~s/header\[pc\+\+\]/%d/;
      $op="\"".$op."\",";
      $a[$n]=sprintf("    %-26s // $n ".$comment, $op);
    }
  }
  for ($i=0; $i<256; ++$i) {
    print("$a[$i]\n");
  }
  */
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
  assert(models.len()>p+8);
  const int end=p+models(p)+256*models(p+1)+2;
  assert(models.len()>=end+2);
  int n=models(p+6);
  p+=7;
  for (int i=0; i<n; ++i) {
    assert(models(p)>0 && libzpaq::compsize[models(p)]>0);
    p+=libzpaq::compsize[models(p)];
    assert(p<models.len()-1 && p<end);
  }
  assert(models(p)==0);
  ++p;
  assert(p<=end);

  // Generate a map of jump targets
  if (p==end) return;
  libzpaq::Array<char> targets(0x10000);
  for (int i=p; i<end-1; ++i) {
    int op=models(i);
    if (op==LJ && p<end-2)
      targets[models(i+1)+256*models(i+2)]=1, ++i;
    if (op==JT || op==JF || op==JMP) {
      int addr=i+2+(models(i+1)<<24>>24)-p;
      if (addr>=0 && addr<0x10000) targets[addr]=1;
      else error("goto target out of range");
    }
    if (op%8==7) ++i;  // 2 byte instruction (LJ is 3)
  }

  // Generate instructions. The output code will not compile
  // if any ZPAQL instructions jump to the middle of a 2 or 3
  // byte instruction (legal) or out of range (legal if not executed).
  fprintf(out, "      a = input;\n");
  for (int i=p; i<end-1; ++i) {
    int op=models(i);
    assert(i-p<0x10000);
    if (targets[i-p]) {
      fprintf(out, "L%d:\n", select*100000+(i-p)); // goto label
      targets[i-p]=0;
    }
    int operand=0;
    operand=models(i+1);  // numeric operand
    if (op==JT || op==JF || op==JMP)  // label
      operand=select*100000+i+2+(operand<<24>>24)-p;
    if (op==LJ) {
      if (i<end-2)
        operand=select*100000+models(i+1)+256*models(i+2);  // label
      ++i;
    }
    if (op%8==7) ++i; // 2 byte instruction
    fprintf(out, "      ");
    fprintf(out, inst[op], operand);
    fprintf(out, "\n");
  }
}

// Search list of models for comp, return true if a match is found
bool findModel(const String& models, const String& comp) {
  if (comp.len()<8) return false;
  for (int p=0; p<models.len()-1; p+=models(p)+models(p+1)*256+2) {
    bool mismatch=false;
    for (int i=0; !mismatch && i<comp.len(); ++i)
      mismatch=i+p>=models.len() || models(i+p)!=comp(i);
    if (!mismatch) return true;
  }
  return false;
}

// Read model string from archive in format suitable for libzpaq::models[]
// If oneblock is true then read only one block
String getModels(libzpaq::Decompresser& d, bool oneblock) {
  String result;
  while (d.findBlock()) {
    String hcomp, pcomp;
    d.hcomp(&hcomp);
    if (!findModel(result, hcomp)) result+=hcomp;
    bool firstSegment=true;
    while (d.findFilename()) {
      d.readComment();
      if (firstSegment) {
        d.decompress(0);
        if (d.pcomp(&pcomp)) {
          fix_pcomp(hcomp, pcomp);
          if (!findModel(result, pcomp)) result+=pcomp;
        }
        firstSegment=false;
      }
      d.readSegmentEnd();
      if (oneblock) break;
    }
  }
  result.put(0);
  result.put(0);
  return result;
}

// Combine hcomp and pcomp into 1 or 2 models suitable for libzpaq::models[]
String combine(String hcomp, String pcomp) {
  if (pcomp!="") {
    fix_pcomp(hcomp, pcomp);
    hcomp+=pcomp;
  }
  hcomp.put(0);
  hcomp.put(0);
  return hcomp;
}

// Print models[p..] for model i
void dump(FILE* out, const String& models, int p, int n) {
  assert(models.len()>p+1);
  const int len=models(p)+models(p+1)*256+2;
  assert(models.len()>=p+len);
  fprintf(out,
  "\n"
  "  // Model %d\n  ", n);
  for (int i=0; i<len; ++i) {
    fprintf(out, "%d,", char(models(p+i)));
    if (i%16==15) fprintf(out, "\n  ");
  }
  fprintf(out, "\n");
}

// Generate C++ source code from a list of models
// Then compile and run it with argc, argv
void optimize(const String& models, int argc, char** argv) {

  // Find the command c, a, x, l, r
  char cmd=0;
  for (int i=0; (cmd=argv[1][i])!=0; ++i)
    if (strchr("caxlr", cmd))
      break;

  // Open output file
  String filename="zpaqopt.cpp";
  FILE* out=fopen(filename.c_str(), "w");
  if (!out) perror(filename.c_str()), exit(1);

  // Print models[]
  fprintf(out,
  "// zpaqopt.cpp generated by zpaq\n"
  "\n"
  "#include \"libzpaq.h\"\n"
  "namespace libzpaq {\n"
  "\n"
  "const char models[]={\n");
  int p, i;
  for (p=0, i=1; p<models.len()-2; p+=models(p)+models(p+1)*256+2, ++i)
    dump(out, models, p, i);
  assert(p==models.len()-2);
  assert(models(p)==0 && models(p+1)==0);
  fprintf(out, "\n  0,0};\n");  // end of list

  // Print predict()
  // Write Predictor::predict()
  fprintf(out,
    "\n"
    "int Predictor::predict() {\n"
    "  switch(z.select) {\n");
  for (p=0, i=1; p<models.len()-2; p+=models(p)+models(p+1)*256+2, ++i)
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
  for (p=0, i=1; p<models.len()-2; p+=models(p)+models(p+1)*256+2, ++i)
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
  for (p=0, i=1; p<models.len()-2; p+=models(p)+models(p+1)*256+2, ++i) {
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
  testfile(filename.c_str());
  fprintf(stderr, "Created %s\n", filename.c_str());

  // Run makefile.bat with the same path as this program
  // to compile zpaqopt.cpp to zpaqopt.exe
  unlink("zpaqopt.exe");
  run_cmd(OPT);

  // Run it
  testfile("zpaqopt.exe");
  String command=String(".")+String(slash())+"zpaqopt.exe";
  for (int i=1; i<argc; ++i) {
    command+=" ";
    command+=argv[i];
  }
  run_cmd(command);
  if (!keep_option) {
    unlink("zpaqopt.exe");
    unlink("zpaqopt.cpp");
    unlink("zpaqopt.obj");
    unlink("zpaqopt.map");
    unlink("zpaqopt.tds");
    fprintf(stderr, "zpaqopt.cpp and zpaqopt.exe deleted\n");
  }
  exit(0);
}

#endif // ifdef OPT

/////////////////////////// Decompress ///////////////////////

// Open archive. filename and mode are as in fopen().
// In read or append mode check if filename exists, and if not
// then try filename.zpaq. In write mode, always add .zpaq extension
// if there is not already one.
FILE *open_archive(const char *filename, const char *mode) {
  assert(filename);
  assert(mode);
  String newname=filename;
  if (mode[0]=='w' || !exists(filename)) {
    if (newname.sub(newname.len()-5)!=".zpaq")
      newname+=".zpaq";
  }
  FILE*  f=fopen(newname.c_str(), mode);
  if (!f) perror(newname.c_str()), libzpaq::error("cannot open archive");
  switch(mode[0]) {
    case 'r': fprintf(stderr, 
      "Reading from archive %s\n", newname.c_str()); break;
    case 'w': fprintf(stderr, 
       "Created archive %s\n", newname.c_str()); break;
    case 'a': fprintf(stderr, 
       "Appending to archive %s\n", newname.c_str()); break;
  }
  return f;
}

// Reject archive filenames with absolute paths, drive letters
// or control characters or that are too long.
bool validate_filename(const char* filename) {
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
void skip_block(libzpaq::Decompresser& d, int n) {
  for (; n>0 && d.findBlock(); --n) {
    while (d.findFilename()) {
      d.readComment();
      d.readSegmentEnd();
    }
  }
}

// Remove path from filename
String strip(const String& filename) {
  for (int i=filename.len()-1; i>=0; --i) {
    if (filename(i)=='/' || filename(i)=='\\' || (i==1 && filename(i)==':'))
      return filename.sub(i+1);
  }
  return filename;
}

// Open filename. Depending on OS, change slashes to / or \.
// If this fails then try creating directories in its path.
// If it fails again, return 0, else return FILE*.
FILE* create(String filename) {

  // Find last slash in filename
  int slashpos=-1;
  for (int i=0; i<filename.len(); ++i)
    if (filename(i)=='/' || filename(i)=='\\')
      slashpos=i;

  // If there is no path, then open file and return
  if (slashpos<0)
    return fopen(filename.c_str(), "wb");

  // Change slashes in filename per OS.
  char slashchar=slash();
  for (int i=0; i<filename.len(); ++i) {
    if (slashchar=='/' && filename[i]=='\\') filename[i]='/';
    if (slashchar=='\\' && filename[i]=='/') filename[i]='\\';
  }

  // Try opening file
  FILE *f=fopen(filename.c_str(), "wb");
  if (f) return f;

  // If this doesn't work, try creating a directory for it using "mkdir"
  if (slashchar) {
    String cmd = slashchar=='\\' ? "mkdir " : "mkdir -p ";
    cmd+=filename.sub(0, slashpos);
    fprintf(stderr, "\n");
    run_cmd(cmd);

    // Last try
    return fopen(filename.c_str(), "wb");
  }
  return 0;
}

// Decompress: [ovk]x[N] archive [path/] [files...]
void decompress(int argc, char** argv) {
  assert(argc>=3);

  // Get options
  bool ocmd=false;
  int blocknum=0;
  const char* cmd=argv[1];
  assert(cmd);
  while (*cmd) {
    if (*cmd=='o') ocmd=true;
    else if (*cmd=='v') verbose=true;
    else if (*cmd=='k') keep_option=true;
    else if (*cmd=='x') break;
    else usage();
    ++cmd;
  }
  if (cmd[0]!='x') usage();
  if (cmd[1]) blocknum=atoi(cmd+1);

  // Get output path if any
  const char* path=0;
  if (argc>3 && argv[3][0]) {
    const char slash=argv[3][strlen(argv[3])-1];
    if (slash=='/' || slash=='\\') {
      path=argv[3];
      fprintf(stderr, "Output folder is %s\n", path);
    }
  }

  // Open input archive
  File in(open_archive(argv[2], "rb"));
  libzpaq::Decompresser d;
  d.setInput(&in);

  // If user specifies N then skip N-1 blocks
  int block=atoi(argv[1]+1);
  if (block>0)
    skip_block(d, block-1);

  // Optimize one block or entire archive
#ifdef OPT
  if (ocmd)
    optimize(getModels(d, block!=0), argc, argv);
#endif

  // Read the archive
  File out(0);  // output file
  int filecount=0;  // number of files extracted
  libzpaq::SHA1 sha1;
  d.setSHA1(&sha1);
  while (d.findBlock()) {
    for (String filename; d.findFilename(&filename); filename="") {
      String comment;
      d.readComment(&comment);
      fprintf(stderr, "%s %s ", filename.c_str(), comment.c_str());

      // open output file.
      // If filename is empty, use the previously opened file, or if none
      // then get the filename from the command line.
      if (filename!="" || !out.f) {

        // close last file
        if (out.f) {
          fclose(out.f);
          out.f=0;
          ++filecount;
        }

        // if the user gave an output file starting at argv[3], use it instead.
        if (argc>3+(path!=0)) {
          if (filecount+3+(path!=0)>=argc) {
            fprintf(stderr, "and remaining files not extracted\n");
            goto end;
          }
          String name;
          if (path) name+=path;
          name+=argv[filecount+3+(path!=0)];
          out.f=create(name.c_str());
          if (!out.f) {
            perror(name.c_str());
            goto end;
          }
          else
            fprintf(stderr, "-> %s ", name.c_str());
        }

        // Otherwise, use the names in the archive, but don't clobber
        // or use suspicious filenames
        else {
          String newname=filename;
          if (path) newname=path+strip(filename);

          // If the first segment is not named then use the archive
          // name without .zpaq
          if (newname=="") {
            newname=argv[2];
            if (newname.sub(newname.len()-5)==".zpaq")
              newname=newname.sub(0, newname.len()-5);
          }
          if (newname!=filename)
            fprintf(stderr, "-> %s ", newname.c_str());
          if (!path && !validate_filename(newname.c_str())) {
            fprintf(stderr, "Error: bad filename\n");
            goto end;
          }
          if (exists(newname.c_str())) {
            fprintf(stderr, "Error: won't overwrite\n");
            goto end;
          }
          else {
            out.f=create(newname.c_str());
            if (!out.f) {
              perror(newname.c_str());
              goto end;
            }
          }
        }
      }
      if (!out.f) {
        fprintf(stderr, "Output filename not specified\n");
        goto end;
      }

      // Decompress and report progress every 100 KB
      d.setOutput(&out);
      fprintf(stderr, "-> ");
      while (d.decompress(100000)) {
        for (int i=fprintf(stderr, "%1.0f ", sha1.size()); i>0; --i)
          putc('\b', stderr);
        fflush(stderr);
      }

      // Verify checksum
      char sha1string[21];
      d.readSegmentEnd(sha1string);
      bool sha1result=memcmp(sha1string+1, sha1.result(), 20);
      if (sha1string[0]) {
        if (sha1result) fprintf(stderr, "WARNING: CHECKSUM MISMATCH\n");
        else fprintf(stderr, "OK, checksum verified\n");
      }
      else fprintf(stderr, "OK, no checksum   \n");
    }
    if (block) break;
  }

  // Close files
end:
  if (out.f) fclose(out.f), ++filecount;
  fclose(in.f);
  fprintf(stderr, "%d file(s) extracted\n", filecount);
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

// Compress files: [onsiptvk][ca][N][F[,N]...] archive [folder/] files...
static void compress(int argc, char** argv) {
  assert(argc>=3);

  // Get command options
  bool ncmd=false, scmd=false, icmd=false, pcmd=false, // options
       tcmd=false, ocmd=false, acmd=false, ccmd=false;
  char *cmd=argv[1];
  while (cmd && cmd[0]) {
    if (cmd[0]=='v') verbose=true;
    else if (cmd[0]=='n') ncmd=true;
    else if (cmd[0]=='i') icmd=true;
    else if (cmd[0]=='s') scmd=true;
    else if (cmd[0]=='p') pcmd=true;
    else if (cmd[0]=='t') tcmd=true;
    else if (cmd[0]=='o') ocmd=true;
    else if (cmd[0]=='k') keep_option=true;
    else if (cmd[0]=='a') {acmd=true; break;}
    else if (cmd[0]=='c') {ccmd=true; break;}
    else usage();
    ++cmd;
  }
  ++cmd;
  if (acmd==ccmd) usage();

  // Compile config file
  String hcomp, pcomp, pcomp_cmd;
  int level=compile_cmd(cmd, hcomp, pcomp, pcomp_cmd);

#ifdef OPT
  if (ocmd && level==0)
    optimize(combine(hcomp, pcomp), argc, argv);
#endif

  // Get output path (folder) if any
  const char* path=0;
  if (argc>3 && argv[3][0]) {
    const char slash=argv[3][strlen(argv[3])-1];
    if (slash=='/' || slash=='\\') {
      path=argv[3];
      fprintf(stderr, "Folder for extraction is %s\n", path);
    }
  }

  // Compress
  libzpaq::Compressor c;
  libzpaq::SHA1 sha1, sha2;  // input, postprocessor output
  libzpaq::PostProcessor pp; // for testing pre/post equivalence
  pp.setSHA1(&sha2);
  if (hcomp.len()>5)
    pp.init(hcomp(4), hcomp(5));  // ph, pm array sizes
  String tmp=String(argv[2])+".zpaq.pre";  // preprocessed input filename

  // Compress files in argv[3...argc-1]
  int filecount=0;  // number of files compressed
  File out(0);  // output file
  double start=0;  // output byte count at start of each file
  for (int i=3+(path!=0)-(argc==3); i<argc; ++i) {

    // Ignore directories
    if (!is_file(argv[i])) {
      fprintf(stderr, "%s: not a regular file\n", argv[i]);
      continue;
    }

    // Open input file
    File in(fopen(argv[i], "rb"));
    if (!in.f) {
      perror(argv[i]);
      continue;
    }

    // Get checksum and file size
    char comment[20]={0};  // file size
    int ch;
    while ((ch=getc(in.f))!=EOF)
      sha1.put(ch);
    rewind(in.f);
    sprintf(comment, "%1.0f", sha1.size());
    const char* sha1result=sha1.result();

    // Preprocess to a temporary file archive.zpaq.pre
    // Test by running through a PostProcessor and comparing checksums.
    // If OK, then compress the temporary file, else skip.
    // Look for preprocessor in the current directory first, then
    // in the install directory.
    if (pcomp!="") {
      fclose(in.f);
      in.count=0;
      String cmd=pcomp_cmd+" "+argv[i]+" "+tmp;
      run_cmd(cmd);

      // Test whether post(pre(in)) == in
      in.f=fopen(tmp.c_str(), "rb");
      if (!in.f) perror(tmp.c_str()), exit(1);
      if (filecount==0) {
        pp.write(1);
        for (int i=0; i<pcomp.len(); ++i)
          pp.write(pcomp(i));
      }
      int ch;
      while ((ch=in.get())!=EOF)
        pp.write(ch);
      pp.write(-1);
      fprintf(stderr, 
        "%s -> %1.0f -> %1.0f\n", comment, in.count, sha2.size());
      if (memcmp(sha1result, sha2.result(), 20)) {
        fprintf(stderr, "pre/post check failed, skipping...\n");
        fclose(in.f);
        continue;
      }

      rewind(in.f);
      in.count=0;
    }
      
    // Open archive for first file
    if (filecount==0) {

      // Create or append archive
      out.f=open_archive(argv[2], acmd?"ab":"wb");
      c.setOutput(&out);

      // Write block header
      if (tcmd) c.writeTag();
      if (level)
        c.startBlock(level);
      else
        c.startBlock(hcomp.c_str());
    }

    // Write segment header
    String filename=pcmd?String(argv[i]):strip(argv[i]);
    if (path) filename=path+filename;
    if (!ncmd && !validate_filename(filename.c_str()))
      fprintf(stderr, "Warning: filename %s not valid for extraction\n",
          filename.c_str());
    c.startSegment(ncmd?0:filename.c_str(), icmd?0:comment);
    if (filecount==0)
      c.postProcess(pcomp=="" ? 0 : pcomp.c_str());

    // Compress and report progress every 100K
    fprintf(stderr, "%s %s ", argv[i], comment);
    c.setInput(&in);
    while (c.compress(100000)) {
      for (int j=fprintf(stderr,
          "%1.0f -> %1.0f ", in.count, out.count-start); j>0; --j)
        putc('\b', stderr);
      fflush(stderr);
    }
    fprintf(stderr, "-> %1.0f               \n", out.count-start);
    start=out.count;
    fclose(in.f);
    if (pcomp!="") unlink(tmp.c_str());

    // Append SHA-1 checksum to end of segment
    c.endSegment(scmd?0:sha1result);
    ++filecount;
  }

  // End block
  if (filecount>0) {
    c.endBlock();
    fprintf(stderr, "%d file(s) compressed to %s -> %1.0f\n",
      filecount, argv[2], out.count);
    c.stat(0);
    fclose(out.f);
  }
  else
    fprintf(stderr, "Archive %s not updated\n", argv[2]);
}

// Print component statistics
int libzpaq::Predictor::stat(int) {
  fprintf(stderr, "\nMemory utilization:\n");
  int cp=7;
  for (int i=0; i<z.header[6]; ++i) {
    assert(cp<z.header.size());
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
      for (int j=0; j<cr.cm.size(); ++j)
        if (cr.cm[j]) ++count;
      fprintf(stderr, ": buffer=%d/%d index=%d/%d (%1.2f%%)",
        cr.limit/8, cr.ht.size(), count, cr.cm.size(),
        count*100.0/cr.cm.size());
    }
    else if (type==SSE) {
      assert(cr.cm.size()>0);
      int count=0;
      for (int j=0; j<cr.cm.size(); ++j) {
        if (int(cr.cm[j])!=(squash((j&31)*64-992)<<17|z.header[cp+3]))
          ++count;
      }
      fprintf(stderr, ": %d/%d (%1.2f%%)", count, cr.cm.size(),
        count*100.0/cr.cm.size());
    }
    else if (type==CM) {
      assert(cr.cm.size()>0);
      int count=0;
      for (int j=0; j<cr.cm.size(); ++j)
        if (cr.cm[j]!=0x80000000) ++count;
      fprintf(stderr, ": %d/%d (%1.2f%%)", count, cr.cm.size(),
        count*100.0/cr.cm.size());
    }
    else if (type==MIX) {
      int count=0;
      int m=z.header[cp+3];
      assert(m>0);
      for (int j=0; j<cr.cm.size(); ++j)
        if (int(cr.cm[j])!=65536/m) ++count;
      fprintf(stderr, ": %d/%d (%1.2f%%)", count, cr.cm.size(),
        count*100.0/cr.cm.size());
    }
    else if (type==MIX2) {
      int count=0;
      for (int j=0; j<cr.a16.size(); ++j)
        if (int(cr.a16[j])!=32768) ++count;
      fprintf(stderr, ": %d/%d (%1.2f%%)", count, cr.a16.size(),
        count*100.0/cr.a16.size());
    }
    else if (cr.ht.size()>0) {
      int hcount=0;
      for (int j=0; j<cr.ht.size(); ++j)
        if (cr.ht[j]>0) ++hcount;
      fprintf(stderr, ": %d/%d (%1.2f%%)",
          hcount, cr.ht.size(), hcount*100.0/cr.ht.size());
    }
    cp+=compsize[type];
    fprintf(stderr, "\n");
  }
  return 0;
}     

////////////////////////// list //////////////////////////

// Decompile ZPAQL starting at s[i]
void printCode(const String& s, int i) {
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

// Archive listing: [okv]l archive
// v = verbose. If not verbose, show for each block the filename
// and comment. If verbose show also the SHA-1 checksum for each segment
// and show the hcomp and pcomp code for each block in a format that
// can be read back as a config file.
// ok = optimize: generates zpaqopt.cpp, zpaqopt.exe but does not use.
// The code may be used to speed extraction by other programs like zpsfx.
static void list(int argc, char** argv) {

  const char* cmd=argv[1];
  assert(cmd);
  bool ocmd=false;
  while (*cmd) {
    if (*cmd=='o') ocmd=true;
    else if (*cmd=='k') keep_option=true;
    else if (*cmd=='v') verbose=true;
    else if (*cmd=='l') break;
    else usage();
    ++cmd;
  }

  // Verbose?
  if (argv[1][0]=='v') verbose=true;

  // Some variables to hold values read from the archive. An archive
  // is a sequence of independent blocks. Each block describes the
  // decompression algorithm in two strings called hcomp and pcomp.
  // Each block holds one or more segments that must be decompressed
  // in order from the start of the block. Each segment has an optional
  // filename string, and optional comment string, some compressed data,
  // and an optional 20 byte SHA-1 checksum of the data before compression.
  double memory;  // required by block
  String filename, comment;  // from segment header
  char sha1string[21];  // from segment trailer
  double start=-1;  // file offset of last segment

  // Create object d to decompress ZPAQ archives.
  libzpaq::Decompresser d;

  // Set the input to the archive
  File in(open_archive(argv[2], "rb"));
  d.setInput(&in);

  // Optimize archive. The "o" command will generate optimized source
  // code for a new version of zpaq and run it. This doesn't list any
  // faster, but the code can be kept with "k" and used by other
  // programs like zpsfx to speed extraction.
#ifdef OPT
  if (ocmd)
    optimize(getModels(d, false), argc, argv);
#endif

  // Search for the next block and return false when done.
  // If true, calculate memory required for decompression.
  for (int i=1; d.findBlock(&memory); ++i) {
    if (verbose) printf("\n");
    printf("Block %d needs %1.3f MB memory\n", i, memory/1e6);
    bool firstSegment=true;  // first segment in block?

    // Find the next segment in the block. If found, read the file
    // name from the segment header and write it to filename.
    // The argument can be any type derived from libzpaq::Writer.
    // If there are no more segments in the block, return false.
    while (d.findFilename(&filename)) {

      // Read the comment from the segment header (like filename).
      // There is no limit on how long the filename or comment might be.
      d.readComment(&comment);

      // If this is the first segment in the block, then print the
      // hcomp string. This string contains ZPAQL code which describes
      // the decompression algorithm. The argument can be any type
      // derived from Writer. The format is suitable
      // for passing to Compressor::startBlock(hcomp). The first 2
      // bytes is the length of the rest of the string in little-endian
      // (LSB, MSB) format. The maximum possible size is 65537.
      // Here we decompile the string to config file format.
      if (firstSegment) {
        if (verbose) {
          String hcomp;
          d.hcomp(&hcomp);
          if (hcomp.len()<7) error("hcomp too small");

          // Print COMP section
          printf("comp %d %d %d %d %d (hh hm ph pm n)\n",
            hcomp[2], hcomp[3], hcomp[4], hcomp[5], hcomp[6]);
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

          // Print HCOMP section
          printf("hcomp\n");
          printCode(hcomp, op+1);

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
          // it to w, where w is any type derived from Writer.
          // If no pcomp string is present, then it returns
          // false without writing anything. If present, the output
          // format is suitable for passing to Compressor::postProcess(pcomp).
          // The first 2 bytes of the string is the length of the rest
          // of the string in little-endian format. The maximum output
          // is 65537 bytes. Here we decompile it to config file format.
          String pcomp;
          if (!d.pcomp(&pcomp))
            printf("post\n  0\nend\n\n");
          else {
            printf("pcomp (?) ;\n");
            printCode(pcomp, 2);
            printf("end\n\n");
          }

          // Display what built in optimized model was detected
          printf("Compression model %d, postprocessing model %d\n",
            d.getModel(), d.getPostModel());
        }
        firstSegment=false;
      }

      // Read the SHA-1 checksum at the end of the segment, skipping
      // any remaining compressed data. The checksum is optional.
      // If present, then a 1 will be written to sha1string[0]
      // and the 20 byte checksum will be written to sha1string[1] through
      // sha1string[20]. If there is no checksum, then a 0 will be
      // written to sha1string[0].
      d.readSegmentEnd(sha1string);
      if (verbose) {
        printf("  ");
        if (sha1string[0]) {
          for (int i=1; i<21; ++i)
            printf("%02x", sha1string[i]&255);
        }
      }

      // Write the filename and comment. We have to clear the
      // strings afterward because we defined put() to append bytes.
      printf("  %s %s -> %1.0f\n", filename.c_str(),
         comment.c_str(), in.count-start);
      start=in.count;
      filename=comment="";
    }
  }
  fclose(in.f);
  printf("\n");
}


//////////////////////////// run ///////////////////////////

// Execute program input and show progress
namespace libzpaq {
int ZPAQL::step(U32 input, int ishex) {
  assert(cend>6);
  assert(hbegin>=cend+128);
  assert(hend>=hbegin);
  assert(hend<header.size()-130);
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
    assert(pc>=cend && pc<header.size());
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
  for (int i=0; i<r.size(); i+=4) {
    if (r(i) || r(i+1) || r(i+2) || r(i+3))
      printf(ishex ? "%8X: %08X %08X %08X %08X\n"
                   : "%10u: %10u %10u %10u %10u\n",
        i, r(i), r(i+1), r(i+2), r(i+3));
  }

  // Print H, skipping rows of 4 zeros
  printf("\nH (size %d) = (rows of all 0 omitted)\n", h.size());
  for (int i=0; i<h.size(); i+=4) {
    if (h(i) || h(i+1) || h(i+2) || h(i+3))
      printf(ishex ? "%8X: %08X %08X %08X %08X\n"
                   : "%10u: %10u %10u %10u %10u\n",
        i, h(i), h(i+1), h(i+2), h(i+3));
  }

  // Print M, skipping rows of 16 zeros
  printf("\nM (size %d) = (rows of all 0 omitted)\n", m.size());
  for (int i=0; i<m.size(); i+=16) {
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

// Debug config file: [opvtk]rF[,N...] [args...]
// p=run PCOMP, v=verbose, t=trace once per numeric arg
// otherwise args are output, input (default stdout, stdin),
// h=trace in hexadecimal, o=generate zpaqopt.h.
void run(int argc, char** argv) {
  assert(argc>=2);

  // Get options
  bool ocmd=false, pcmd=false, tcmd=false;
  char *cmd=argv[1];
  assert(cmd);
  while (cmd[0]) {
    if (cmd[0]=='p') pcmd=true;
    else if (cmd[0]=='o') ocmd=true;
    else if (cmd[0]=='v') verbose=true;
    else if (cmd[0]=='t') tcmd=true;
    else if (cmd[0]=='k') keep_option=true;
    else if (cmd[0]=='r') break;
    else usage();
    ++cmd;
  }
  ++cmd; // now points config file name
  if (!cmd[0]) usage();

  // Parse comma separated arguments after config file (now in cmd)
  String hcomp, pcomp, pcomp_cmd;
  if (compile_cmd(cmd, hcomp, pcomp, pcomp_cmd))
    error("no config file");

#ifdef OPT
  if (ocmd)
    optimize(combine(hcomp, pcomp), argc, argv);
#endif

  // Initialze virtual machine
  libzpaq::ZPAQL z;
  if (pcmd) {
    if (pcomp.len()<2) error("no PCOMP section");
    fix_pcomp(hcomp, pcomp);
    z.read(&pcomp);
    z.initp();
  }
  else {
    z.read(&hcomp);
    z.inith();
  }

  // Run the program
  if (tcmd) {  // trace with numeric args
    for (int i=2; i<argc; ++i) 
      z.step(ntoi(argv[i]), tolower(argv[i][0])=='x');
  }
  else {  // run F input output
    FILE *in=stdin;
    File out(stdout);
    z.output=&out;
    if (argc>2) {
      in=fopen(argv[2], "rb");
      if (!in) perror(argv[2]), exit(1);
    }
    if (argc>3) {
      out.f=fopen(argv[3], "wb");
      if (!out.f) perror(argv[3]), exit(1);
    }
    int c;
    while ((c=getc(in))!=EOF)
      z.run(c);
    if (pcmd) z.run(-1);
  }
}

///////////////////////////// Main ///////////////////////////

// Command syntax as in usage()
int main(int argc, char** argv) {

  // Check usage
  if (argc<2) 
    usage();

  // Find the command c, a, x, l, r
  char cmd=0;
  for (int i=0; (cmd=argv[1][i])!=0; ++i)
    if (strchr("caxlr", cmd))
      break;

  // Do the command
  if (argc>=3 && (cmd=='a' || cmd=='c'))
    compress(argc, argv);
  else if (argc>=3 && cmd=='x')
    decompress(argc, argv);
  else if (argc>=3 && cmd=='l')
    list(argc, argv);
  else if (cmd=='r')
    run(argc, argv);
  else
    usage();

  // Print time used
  fprintf(stderr, "Time %1.2f sec.\n", double(clock())/CLOCKS_PER_SEC);
  return 0;
}
