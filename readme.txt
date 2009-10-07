bwt_j3.zip contains a configuration file and preprocessor for
data compression with ZPAQ 1.07 and higher. It enables BWT compression.

To compress:   zpaq cbwt_j3.cfg,N archive files...
To decompress: zpaq x archive [files...]
or:            unzpaq x archive [files...]
or:            zpipe <archive >output

The compression option N is a number from 0 to 18. It selects a BWT
block size of 2^(N+10)-256. Memory usage is 5 x 2^N KB + 100 MB.
N = 18 requires 1443 MB to compress or decompress.

Compression requires bwt_j3.cfg and bwtpre.exe in the current directory
and requires ZPAQ v1.07 or higher. Decompression is possible with
any ZPAQ compatible decompresser including unzpaq, zpipe, and older
versions of zpaq back to v1.00. Decompression requires the same memory
that was used during compression. zpaq, unzpaq, and zpipe can be downloaded
from http://mattmahoney.net/dc/

bwt_j3.cfg is a configuration file for zpaq v1.07 and higher.
It contains ZPAQL code for an inverse BWT transform and a compression
model optimized for BWT transformed data.

bwtpre.exe v1.2 is a BWT preprocessor called from bwt_j3.cfg.
It takes 3 arguments:

  bwtpre N input output

The input is divided into blocks of size n = 2^(N+10)-256 bytes. Each
block is encoded as (n,p,B) where n is the block size as a 32 bit
number MSB first. p is the location of the first byte of the block
after the BWT transform as a 32 bit number MSB first, in the range
0 to n-1. B is the transformed data where the bytes are sorted by their
right lexicographical context with wraparound. For the last block, n
may be smaller, but at least 1. The output does not end with 4 zero bytes
as it did with bwtpre v1.1 included with bwt_j2.cfg.

bwtpre.cpp v1.2 is the source code for bwtpre.exe. It is based on fast
mode compression from bbb.cpp, a BWT compressor. It was compiled with
MinGW g++ 4.4.0 and upx 3.00w.

  g++ bwtpre.cpp -O2 -march=pentiumpro -fomit-frame-pointer -s -o bwtpre.exe
  upx bwtpre.exe

Results: compressed size, file compression and decompression times and
memory used in MB. Timed on a 2 GHz Pentium T3200, 3 GB, Vista 32-bit.
Decompression times are with unzpaq 1.06.

  Size      File       Options        Comp Deco  Mem
----------- ------     -------------  ---- ----  ----
 20,756,743 enwik8     cbwt_j3.cfg,18   476 301  1443 (256 MB blocks)
    765,048 calgary\*  cbwt_j3.cfg,13    48   9   143 (8 MB blocks)

Each file is compressed as a separate BWT block.

  BIB    111261 ->  26392
  BOOK1  768771 -> 210797
  BOOK2  610856 -> 144954
  GEO    102400 ->  51659
  NEWS   377109 -> 111161
  OBJ1    21504 ->  10066
  OBJ2   246814 ->  72541
  PAPER1  53161 ->  15645
  PAPER2  82199 ->  23528
  PIC    513216 ->  44674
  PROGC   39611 ->  11887
  PROGL   71646 ->  14730
  PROGP   49379 ->  10136
  TRANS   93695 ->  16878

History:

Oct. 6, 2009 - bwt_j1.cfg by Jan Ondrus. Inverse transform for bwt.
Oct. 6, 2008 - bwt_j2.cfg and bwtpre.cpp v1.1 by Matt Mahoney. Adds
  block size parameter. bwtpre is released under GPL v3.
Oct. 7, 2009 - bwt_j3.cfg bug fix by Jan Ondrus to compress multiple files.
  bwtpre.cpp modified so that it does not output 4 zero bytes at the end.
