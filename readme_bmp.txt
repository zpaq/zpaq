bmp_j4 ZPAQ model for compressing .bmp files.
Written by Jan Ondrus in 2009. Updated Jan. 24, 2013 by Matt Mahoney
to work with zpaq v6.19.

To compress, either:
  zpaqd c     bmp_j4 rafale.bmp.zpaq rafale.bmp
  zpaqd cnist bmp_j4 rafale.bmp.zpaq rafale.bmp   (a little smaller)

To decompress, either:
  zpaqd d rafale.bmp.zpaq output.bmp
  zpaq  x rafale.bmp.zpaq rafale.bmp -to output.bmp

Both bmp_j4.cfg and colorpre.cfg must be in the current directory
to compress. They are not needed to extract.
