README for zpaq v4.04 compressing archiver - Mar 26, 2012.

To compile, you need additional files from http://mattmahoney.net/zpaq/

  libzpaq501.zip    -> libzpaq.cpp, libzpaq.h
  divsufsort200.zip -> divsufsort.c, divsufsort.h

divsufsort.* is also available from libdivsufsort-lite v2.00 from
http://code.google.com/p/libdivsufsort/
(C) 2008, Yuta Mori (MIT license).

zpaq.exe was compiled with MinGW g++ 4.6.1 for 32 bit Windows like this:

  g++ -O3 -msse2 -s -static -DNDEBUG zpaq.cpp libzpaq.cpp divsufsort.c -o zpaq
  upx zpaq.exe

The option -static is only necessary if you distribute the executable.
-DNDEBUG turns off run time checks in divsufsort.c. It is off by default
in the other files. Use -DDEBUG to turn it on.
-O3 -msse2 -s are optimization options. Feel free to adjust.

upx is an optional executable compressor from http://upx.sourceforge.net/

To compile for Linux, add the option -fopenmp

To compile for Windows with VC++, open Visual Studio and select
Tools/Command Window, and enter the command:

  cl /O2 /EHsc /DNDEBUG zpaq.cpp libzpaq.cpp divsufsort.c

This package contains:

  zpaq.cpp - source code.
  zpaq.1.pod - source for document zpaq.1.html created with pod2html.

You can use pod2man to create a Linux man page.

zpaq 4.04 fixed a bug in the r command that truncated the output
file.

zpaq 4.03 fixes a bug in u (did not save filenames without args).
It adds -f (x force overwrite), -r (recurse directories) and -n
(don't save comments, checksums, or locator tags). -bs now saves
these by default.

zpaq 4.02 adds commands c (create archive), x out/ (extract to
directory), and l with no archive (show hcomp, pcomp strings).

zpaq v4.01 adds incremental update. Before updating the archive,
it compares the files and skips if identical. Extraction also
compares and shows the results (as = or #) if the files exist,
but does not overwrite. You will have to delete them to extract.

zpaq v4.00 and libzpaq v4.00 replace version 3.01 of both. The main
difference is that source level just-in-time (JIT) optimization is
replaced with internal x86-32 and x86-64, so you no longer need
an external C++ compiler to get the best performance. However,
JIT will not work on non-x86 processors, or older processors that
don't support the SSE2 instruction set. To disable JIT for these
processors, compile with -DNOJIT and also drop the -msse2 option.
The program will run about twice as slow.

You can compile with -DDEBUG to turn on assertion checks to assist
in debugging. It will run slower if you do. Note that in earlier
versions, assertion checking was on by default in both zpaq and libzpaq
unless turned off with -DNDEBUG. Now both are off by default.

I have only tested the JIT code in 32 bit Windows and 64 bit Linux
(Ubuntu). There is code to support 64 bit Windows and 32 bit Linux
but it is not tested.

-- Matt Mahoney

