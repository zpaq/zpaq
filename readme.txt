exe_j1.zip contains a preprocessor and config file optimized
for compression of .exe and .dll files for ZPAQ. It requires
zpaq v1.07 or higher for compression, available from
http://mattmahoney.net/dc/

This software was written by Jan Ondrus on Oct. 7, 2009.
zpaq and this readme file are by Matt Mahoney.

To compress:

  zpaq cexe_j1.cfg archive files...

To decompress, any of:

  zpaq x archive [files...]    (versions older than v1.07 OK)
  unzpaq x archive [files...]
  zpipe d <archive >output

exe_jo.exe must be in the current directory for compression.
It was compiled for 32 bit Windows. To run on non Windows systems,
first compile exe_jo.cpp to exe_jo.

exe_jo.exe was compiled with MinGW g++ 4.4.0 and UPX 3.00w.

  g++ -O2 -march=pentiumpro -fomit-frame-pointer -s exe_jo.cpp -o exe_jo
  upx exe_jo.exe

The E8E9 transform improves compression of 32 bit x86 .exe and .dll
files by transforming JMP and CALL instrutions (E8 and E9 hex) from
relative to absolute addresses. This improves compression because
the same address may appear multiple times. This transform extends the
E8E9 transform to conditional jumps (opcodes 0F80...0F8F hex).
It also adds EXE detection so non-EXE files can be compressed without
the transform.

exe_jo.cpp is derived from the same transform in paq8px_v64 added
by Jan Ondrus. paq8px_v64 is released under the GNU General Public
License (v2 or higher). The inverse transform in ZPAQL is derived from
max.cfg and exe.cfg originally written by Matt Mahoney and released
under GPL (v3 or higher). See http://www.gnu.org/copyleft/gpl.html

Results. CT and DT are compression and decompression times in seconds
for zpaq 1.07 on a 2 GHz T3200, 3 GB, 32 bit Vista. Memory usage
is the same for compression and decompression. For comparison,
results with exe.cfg are shown. See http://maximumcompression.com for
other comparisons.

size      file                CT   DT  MEM      exe.cfg
--------- -----------------   --   --  ---      -------------
1,084,657 acrord32.exe.zpaq   47   48  278 MB   1,131,872
1,464,603 mso97.dll.zpaq      49   51  278 MB   1,530,715

  