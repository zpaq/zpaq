readme_bwt.txt - Mar. 16, 2011

This archive contains a ZPAQ preprocessor and 4 configuration files
for Burrows-Wheeler Transform (BWT) based compression. To use, put
the files in the current directory before compressing with ZPAQ.
Get ZPAQ from http://mattmahoney.net/dc/zpaq.html

Contents:

  readme_bwt.txt  This file
  bwt1.cfg        Config file for fast BWT
  bwt2.cfg        Config file for slow BWT (best compression)
  bwtrle1.cfg     Config file for fast BWT + RLE (fastest)
  bwtrle2.cfg     Config file for slow BWT + RLE
  bwtrle.exe      Preprocessor executable for Windows
  bwtrle.cpp      Preprocessor source code
  divsufsort.c    Preprocessor source from libdivsufsort-lite
  divsufsort.h      from from http://code.google.com/p/libdivsufsort/

bwt* is (C) 2011, Dell Inc. Written by Matt Mahoney. It is licensed
under GPL v3. http://www.gnu.org/copyleft/gpl.html

divsufsort.* is (C) 2003-2008, Yuta Mori. See source code for license.

To compress: zpaq ocbwtrle1 archive files...

Or replace bwtrle1 with one of bwt1, bwt2, bwtrle2.

Compression is limited to files smaller than 128 MB and requires 671 MB
memory. You can pass an argument to the config file to adjust this.
A positive increment like "zpaq ocbwt1,1" doubles the maximum file
size and doubles the required memory. Each negative increment halves
the maximum file size and memory required. For example, "zpaq ocbwt1,-3"
requires 1/8 as much memory to compress or decompress, but does not work
for files larger than 16 MB.

Memory required to decompress is the same as memory required to compress.

Benchmarks:

  config  calgary.tar enwik8    Time    Memory
  ------  ----------- ------   -------  ------
  original 3152896  100000000    0   0    0
  bwt1      844305   21965906   95  69  671
  bwt2      800498   21246030  122  93  671
  bwtrle1   861300   22823439   82  56  671
  bwtrle2   828249   21985593   97  66  672

  min      1027299   33460930   41  43    4
  fast      806959   24837469   78  86   38
  mid       699191   20941558  265 301  111
  max       644190   19448650  716 727  246

Times are process times in seconds on one of two cores to compress
and decompress enwik8 on a 2.0 GHz T3200 with 3 GB memory under 32
bit Windows.

bwtrle.exe was compiled with g++ 4.5.0 and compressed with
upx 3.06w as follows:

  g++ -O2 -march=pentiumpro -fomit-frame-pointer -s \
    bwtrle.cpp divsufsort.c -o bwtrle.exe
  upx -9 bwtrle.exe

In Linux, you will need to compile bwtrle and put the executable
somewhere in your PATH.

Technical details:

bwtrle is the preprocessor called from each of the *.cfg files.
It takes 3 arguments:

  bwtrle CMD input output

where CMD is one of:

  c = BWT encode
  d = BWT decode
  e = BWT+RLE encode
  f = RLE+BWT decode

Only commands c and e are used from the .cfg files. bwt1.cfg and bwt2.cfg
use BWT encoding (command c). bwtrle1.cfg and bwtrle2.cfg use BWT+RLE
encoding (command e). The decoding is implemented in ZPAQL in the POST
sections of the .cfg files. The d and f commands show the equivalent
C++ code but are not used by ZPAQ.

BWT and BWT+RLE encoding is always in a single block. It requires
5 times the file size in memory.

BWT encoding increases the size by 5 bytes. It is equivalent to
appending EOF (-1) to the input and suffix sorting the characters.
In the output, EOF is replaced by 0xFF (255). Including the EOF byte
in the output simplifies decoding. The location of this byte
is appended to the output as a 4 byte number, LSB first, in the range 0
to n, where the original input is n bytes and the output is n+5 bytes.

Decoding in ZPAQ with argument A allocates 5<<(A+27) bytes memory and
allows files up to n <= (1<<(A+27))-257 bytes. Decoding with "bwtrle d"
allocates 5n+5 bytes, which is generally smaller.

BWT+RLE encoding (bwtrle e) follows BWT with run length encoding
in which runs of 2 to 257 identical characters are encoded as the
first 2 characters followed by a count byte in the range 0..255.
In the worst case, the output size may be 3*(n+5)/2. Memory required
to decode depends on n as before.

Encoding and decoding times for enwik8 without compression
are as follows:

          encode    decode    decode
          bwtrle    bwtrle    ZPAQL      Size
          ------    ------    ------   ---------
  BWT       30.6      23.3      26.4   100000005
  BWT+RLE   29.2      24.5      26.0    57081548

The commands for decoding using ZPAQL are "zpaq oprbwt1 input output"
for BWT decoding and "zpaq oprbwtrle1 input output" for BWT+RLE
decoding. The "o" prefix says to compile the ZPAQL code to C++.
This was done using ZPAQ installed with the OPT command set
to "g++ -O3 -march=native -s -fomit-frame-pointer", which requires
an external g++ 4.5.0 compiler. Without compiling, BWT+RLE decoding
takes 61 seconds.

The PCOMP sections of bwt1.cfg and bwt2.cfg are the same. The inverse
BWT first counts bytes in the array H[-1..-256], then constructs
a linked list in H[0..n]. Thus, H must have a size of at least n+257.
ZPAQL also requires the size to be a power of 2 (selected by the
argument to the config file). The array M is the same size and
contains the BWT transformed data.

The PCOMP sections of bwtrle1.cfg and bwtrle2.cfg are the same.
As the data is decompressed, it is RLE decoded before storing in M
rather than stored directly. The remainder of the inverse BWT
is the same.

bwt1.cfg uses an indirect order 0 model. bwt2.cfg uses an order
0-1 ISSE chain. bwtrle1.cfg and bwtrle2.cfg use similar models
except that the RLE parse state is included in all contexts. The RLE
state has two possible values, either a normal byte or a count
byte. The HCOMP code detects when a count byte is expected by
comparing the last two decoded bytes.

Notes:

ZPAQ verifies preprocessors at compress time by running the
postprocessor and comparing the output SHA-1 checksum with the
original input. Skipping this check would speed up compression
by 26 seconds.

Compression and decompression could be sped up at a cost in size
and memory by dividing the input into smaller blocks and compressing
or decompressing them in parallel. ZPAQ does not have the capability
to preprocess segments of a file. However it would be possible
to split a file into smaller pieces, compress them separately
with all but the first being unnamed (ZPAQ prefix n), and concatenate
them into an archive that a parallel program like PZPAQ could
decompress in parallel in separate threads. Splitting a file would
also remove the file size limits.
