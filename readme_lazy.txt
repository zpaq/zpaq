lazy2 v1.0 (C) 2012, Dell Inc. Licensed under GPL v3.
Written by Matt Mahoney, Oct. 31, 2012.

Contents:

  lazy2.exe - 32 bit Windows file compressor.
  lazy2.cpp - Source code.
  lazy2.cfg - Equivalent config file for ZPAQ.

To compress:   lazy2 3 input output
To decompress: lazy2 d input output

lazy2 compressed like lazy, except it has an E8E9 filter to improve
compression of .exe and .dll files. Also, unlike lazy,
uncompressed files cannot be larger than 1 GiB (2^30 bytes).
Memory usage is a little over 1 GB. The first argument may range
from 1 (fastest) to 5 (best). Decompression is faster than compression
and doesn't depend on compression level.

lazy2.cfg can be used to create ZPAQ archives using the same
compression algorithm. It contains ZPAQL code to decompress,
equivalent to "lazy d". To create an archive:

  zpaq -method lazy -add archive files...

To decompress, you don't need any of these file because the decompression
code from lazy.cfg is embedded in the archive and zpaq will just
run it:

  zpaq -extract archive

You could, if you want, use zpaq to decompress a file compressed
with lazy using lazy.cfg. The following are equivalent:

  lazy d input output
  zpaq -method lazy -run pcomp input output

except that lazy.cfg is limited to 16 MB files. To increase this to 1 GB,
change the "24" in the line "comp 0 0 0 24 0" to 30. Memory usage will
increase accordingly. 24 is sufficient for zpaq because data is compressed
in 16 MB blocks in normal and -streaming mode. If you use -solid or
-tiny compression on large files, you will need to increase this value.

