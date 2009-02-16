README for ZPAQ v0.01.
Matt Mahoney - Feb. 15, 2009.

ZPAQ is a configurable file compressor and archiver. Its goal
is a high compression ratio in an open format without loss of
compatibility between versions as advanced compression techniques
are discovered.

ZPAQ v. 0.01 is an experimental pre-release (level 0) implementation
of the proposed ZPAQ open standard for highly compressed data.
It implements version 0.29 of the ZPAQ standard, dated Feb. 15, 2009.
It is not intended to be compatible with other versions of ZPAQ
or the upcoming level 1 standard. When the level 1 standard
is released, all versions will be compatible with one another.
Level 2 and higher will be backward compatible i.e. able to
read level 1. When level 1 is released, all level 0 programs
(including this one) and the file formats they produce
will be obsolete.

The current version of ZPAQ can be found at
http://cs.fit.edu/~mmahoney/compression/

Version 0.01 does not support post-processing as described
in the specification.

Compression requires a configuration file. Two examples are
supplied:

  min.cfg - Fast, minimal compression with an order 4 context
            model - requires 4 MB memory.
  max.cfg - Slow but good compression. Requires 93 MB.

To create an archive:

  zpaq cconfig archive files...

where config is a configuration file, archive is the compressed
file to create, and files... are the files to compress. There
should be no space between the "c" command and the file name. For
example:

  zpaq cmax.cfg calgary.zpaq calgary/*

will display the contents of max.cfg and then
compress the Calgary corpus (14 files) to 660,575 bytes
in 38 seconds on a 2 GHz Pentium T3200. The file names are
stored in the archive as given on the command line.

  calgary/BIB 111261 -> 23361
  calgary/BOOK1 768771 -> 202943
  calgary/BOOK2 610856 -> 127307
  calgary/GEO 102400 -> 47532
  calgary/NEWS 377109 -> 92596
  calgary/OBJ1 21504 -> 9035
  calgary/OBJ2 246814 -> 58183
  calgary/PAPER1 53161 -> 11534
  calgary/PAPER2 82199 -> 17666
  calgary/PIC 513216 -> 29383
  calgary/PROGC 39611 -> 9357
  calgary/PROGL 71646 -> 11316
  calgary/PROGP 49379 -> 8238
  calgary/TRANS 93695 -> 12124
  Used 38.58 seconds

To append to an existing archive:

  zpaq aconfig archive files...

To list the contents of an archive:

  zpaq l archive

This shows the file names and sizes before and after compression
and the memory required to extract. To list verbosely:

  zpaq v archive

This also shows the model used to compress and the ZPAQL program
used to compute the contexts from the original config file. The
config file is not needed to extract.

To extract files using the stored file names:

  zpaq x archive

If the files to be extracted already exist, then zpaq will
refuse to clobber them and skip to the next file. If the files
are compressed with a path (folder or directory), then that
directory must exist when the file is extracted. zpaq will
not create directories.

To extract and rename:

  zpaq x archive files...

Files are extracted in the same order they are saved and renamed.
Unlike using stored names, if the file exists, then it is
overwritten (clobbered). Only files named on the command line
are extracted. Any additional files in the archive are ignored.
For example:

  zpaq x calgary.zpaq foo bar

will extract BIB to foo, BOOK1 to bar, and then stop.

The following commands are useful for debugging or developing
config files:

  zpaq t archive [files...]

Extracts files like x, but without post-processing.

  zpaq hconfig [args...]

Displays config, then executes the HCOMP section (a program written
in ZPAQL; see the ZPAQ specification) as a context hash function
once for each argument as input. args... should be decimal numbers.
For each argument, the value is loaded into the A register and
the program is run with each execution step displayed along
with the contents of registers. At the end of each execution,
the entire machine state (M and H arrays) is dumped to the
screen.

  zpaq pconfig [input [output]]

Runs config as a postprocessing program. The program is run
once for each byte of input, with output redirected to the
named output file. The defaults are standard input and standard
output.

A config file has the format:

  comp hh hm ph pm n
    (numbered list of components from 0 to n-1)
  hcomp
    (program to compute context hashes)
  post
    (post-processing options)
  end

The file is case-insensitive, and free-format (all whitespace
is equivalent). Comments may be written in parenthesis and
may be nested. See min.cfg and max.cfg for examples. See the
ZPAQ specification for descriptions of components and ZPAQL
instructions.

The hconfig command runs the program in the HCOMP section using
arrays H and M with sizes 2^hh and 2^hm respectively. The pconfig
command uses 2^ph and 2^pm respectively.

The only POST command currently implemented is PASS, which does
no post-processing.

When developing a config file, it is useful to run with the
hconfig command without arguments to check for compilation
errors and check the targets of relative jump instructions
(which are displayed as absolute). Once it is correct, it can
be run again with arguments so that you can check if the program
is behaving correctly.

Contents:

  zpaq029.pdf -Version 0.29 of the ZPAQ specification, valid only
               for zpaq v0.01.

  zpaq.cpp -   Source code.

  zpaq.exe -   32 bit Windows executable, compiled as follows:
               g++ -O3 -s -fomit-frame-pointer -march=pentiumpro \
                   -DNDEBUG zpaq.cpp -o zpaq.exe
               upx zpaq.exe

  zpaqd.exe -  Slower executable with assertions turned on.
               If zpaq.exe crashes, try this program instead
               and let me know what happened. Compiled as above
               without -DNDEBUG

  min.cfg -    Config file for fast compression.

  max.cfg -    Config file for good compression.

  readme.txt - This file

  