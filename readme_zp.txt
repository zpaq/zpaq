README for zp v1.03 - May 26, 2011

zp is a ZPAQ compatible multithreaded compressor and decompresser
with 4 compression levels. The latest version can be found at
http://mattmahoney.net/dc/zpaq.html  Contents:

  zp.exe - 32 bit Windows executable
  zp     - 64 bit Linux executable
  zp.cpp - Source code

zp v1.03 replaces both zp v1.02 (compressor) and unzp v1.00
(decompresser). For quick help, run zp with no arguments.
For detailed usage, see zp.cpp.

To compile from source, you need libzpaq from the above website
and libdivsufsort-lite from http://code.google.com/p/libdivsufsort/
The Windows executable was compiled as follows with
MinGW g++ 4.5.0 and UPX 3.06w:

  g++ -O2 -march=pentiumpro -fomit-frame-pointer -s -DNDEBUG \
    zp.cpp libzpaq.cpp divsufsort.c -o zp.exe
  upx -9 zp.exe

The 64 bit Linux executable was compiled with g++ 4.5.2:

  g++ -O3 -s -DNDEBUG -fopenmp -lpthread -static \
    zp.cpp libzpaq.cpp divsufsort.c -o zp

JIT optimization of archives produced by other ZPAQ programs
using an external C++ compiler is not enabled in the supplied
binaries. To enable it, zp must be compiled with options specific
to your installation. See instructions in zp.cpp.

zp is (C) 2011, Dell Inc. It is written by Matt Mahoney.
It is licensed under GPL v3.

libdivsufsort-lite is (C) 2003-2008, Yuta Mori.
See source code for license.

