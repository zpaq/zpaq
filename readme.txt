lazy v1.0 (C) 2012, Dell Inc. Licensed under GPL v3.
Written by Matt Mahoney, Oct. 10, 2012.

Contents:

  lazy.exe - 32 bit Windows file compressor.
  lazy.cpp - Source code.
  lazy.cfg - Equivalent config file for ZPAQ.

To compress:   lazy 3 input output
To decompress: lazy d input output

The first argument may range from 1 (fastest) to 5 (best).
Memory ranges from 36 MB for 1 to 96 MB for 5. Decompression
always takes 16 MB. Decompression is faster than compression
and doesn't depend on compression level.

lazy.cfg can be used to create ZPAQ archives using the same
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
