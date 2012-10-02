bmp_j4 ZPAQ model for compressing .bmp files.
Written by Jan Ondrus in 2009. Updated Oct. 1, 2012 by Matt Mahoney
to work with zpaq v6.xx.

To compress:   zpaq -add rafale.zpaq rafale.bmp -method bmp_j4 -tiny
To decompress: zpaq -extract rafale rafale.bmp -to output.bmp

See zpaq.cpp for complete command syntax.
Use -solid, or -tiny modes and only compress one .bmp
file at a time. -streaming mode should work with multiple files
if none are larger than 16 MB.
Pre/post processing will fail on non .bmp files.
Both bmp_j4.cfg and colorpre.cfg must be in the current directory
to compress. They are not needed to extract.