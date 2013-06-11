zpaqd627.zip, June 11, 2013. Contents:

zpaqd.exe     6.27   ZPAQ development tool for 32 bit Windows.
zpaqd64.exe   6.27   For 64 bit Windows.
zpaqd.cpp     6.27   User's guide and source code.

All versions of this software can be found at
http://mattmahoney.net/dc/zpaq.html
Please report bugs to Matt Mahoney at mattmahoneyfl@gmail.com
This software is written by Matt Mahoney and released to the public domain.

zpaqd is a tool for developing, testing, and debugging new compression
algorithms in the ZPAQ streaming archive format described in
http://mattmahoney.net/dc/zpaq202.pdf

It will compress files into a streaming archive using 3 built-in
compression levels or using a compression algorithm described in
a config file in the ZPAQL language and possibly pre-processed
by an external program. See libzpaq.h for a description of the
ZPAQL language. zpaqd has commands to run or trace config files as
stand-alone programs as a debugging tool. You can use zpaqd to
decompress single files, blocks, or segments, or use zpaq to
decompress multiple files. You can list archives and it will
display the decompression code in a format suitable for pasting
into a config file.

zpaqd was compiled with MinGW g++ 4.8.0 as follows:

  g++ -O3 -s -m32 -static zpaqd.cpp libzpaq.cpp -o zpaqd
  g++ -O3 -s -m64 -static zpaqd.cpp libzpaq.cpp -o zpaqd64
  upx zpaqd.exe

To compile, you will need libzpaq.cpp and libzpaq.h from the latest
version of zpaq from http://mattmahoney.net/dc/zpaq.html
This version was compiled with libzpaq.h v6.25 and libzpaq.cpp v6.26
from http://mattmahoney.net/dc/zpaq631.zip

upx is optional. It compresses the executable. It does not work if
compiled for 64 bit Windows with -m64

The 32 bit version will work in 64 bit Windows but can only use 2 GB
of memory even if you have more. The 64 bit version requires 64 bit Windows.

You only need to use -static if you plan to run the program on a different
computer than you compiled it on. -O3 optimizes. -s strips debugging
symbols to make the executable smaller (optional).

To compile for Linux, you may need to include the option -Dunix

To turn on run time checks, compile with -DDEBUG

To compile for a non-x86 architecture or an old processor that
does not support SSE2 (Intel before 2000, AMD before 2002), compile
with -DNOJIT

To compile in Visual Studio: cl /O2 /EHsc zpaqd.cpp libzpaq.cpp
