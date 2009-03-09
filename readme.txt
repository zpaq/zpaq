README for ZPAQ v0.08.
Matt Mahoney - Mar. 8, 2009, matmahoney (at) yahoo (dot) com.

ZPAQ is a configurable file compressor and archiver. Its goal
is a high compression ratio in an open format without loss of
compatibility between versions as advanced compression techniques
are discovered.

ZPAQ versions before 1.00 are experimental pre-release (level 0)
implementations of the proposed ZPAQ open standard for highly compressed
data. They implement only the accompanying version of the ZPAQ standard.
They are not intended to be compatible with other versions of ZPAQ
or the upcoming level 1 standard. When the level 1 standard
is released, all versions will be compatible with one another.
Level 2 and higher will be backward compatible i.e. able to
read level 1. When level 1 is released, all level 0 programs
(including this one) and the file formats they produce
will be obsolete.

The current version of ZPAQ can be found at
http://cs.fit.edu/~mmahoney/compression/

Compression requires a configuration file. Two examples are
supplied:

  min.cfg - Fast, minimal compression (LZP + order 3). Requires 4 MB memory.
  mid.cfg - Average compression and speed. Requires 111 MB.
  max.cfg - Slow but good compression. Requires 278 MB.

To create an archive:

  zpaq cconfig archive files...

where config is a configuration file, archive is the compressed
file to create, and files... are the files to compress. There
should be no space between the "c" command and the file name. For
example:

  zpaq cmax.cfg calgary.zpaq calgary\*

will compress the Calgary corpus (14 files) as follows
in 43 seconds on a 2 GHz Pentium T3200. The file names are
stored in the archive as given on the command line.

278.477 MB memory required.
calgary\BIB 111261 -> 23083
calgary\BOOK1 768771 -> 198364
calgary\BOOK2 610856 -> 123840
calgary\GEO 102400 -> 46785
calgary\NEWS 377109 -> 90758
calgary\OBJ1 21504 -> 8842
calgary\OBJ2 246814 -> 56268
calgary\PAPER1 53161 -> 11193
calgary\PAPER2 82199 -> 17123
calgary\PIC 513216 -> 28623
calgary\PROGC 39611 -> 9144
calgary\PROGL 71646 -> 11067
calgary\PROGP 49379 -> 7986
calgary\TRANS 93695 -> 11652
-> 644728

To append to an existing archive:

  zpaq aconfig archive files...

To append without saving a SHA1 checksum (saves 20 bytes per file):

  zpaq bconfig archive files...

If a checksum is present, the decompressor will compute the SHA1
hash of the extracted file and compare it with the stored checksum
and report a warning if they don't match.

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
named output file. At the end of input, the program is run
with input 2^32-1 to signal EOF. The defaults are standard
input and standard output.

  zpaq sconfig

Compiles config and outputs HCOMP as a list of numbers suitable
for initializing an array in C/C++

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
instructions. Notes:

- Operands of 2 byte instructions must be separated by a space.
  For example, "A=0" is one byte, "A= 0" is 2 bytes. "A=1" is
  not valid because there is no 1 byte opcode for it. It must
  be written "A= 1".
- JT, JF, and JMP accept operands in the range (-128...127).
- LJ (long jump) accepts an operand in the range (0...65535).
  It is coded as 2 bytes, LSB first. This is the only 3 byte
  instruction.
- All other numeric operands must be in the range (0...255).

The hconfig command runs the program in the HCOMP section using
arrays H and M with sizes 2^hh and 2^hm respectively. The pconfig
command uses 2^ph and 2^pm respectively.

When developing a config file, it is useful to run with the
hconfig command without arguments to check for compilation
errors and check the targets of relative jump instructions
(which are displayed as absolute). Once it is correct, it can
be run again with arguments so that you can check if the program
is behaving correctly.

The following POST commands are accepted:

  0 (no preprocessing)
  x (E8E9 transform for better .exe and .dll compression)
  p esc minlen hmul (LZP transform)

The 0 transform compresses the file unchanged. Set ph=0, pm=0 to
save memory.

The x transform improves x86 compression by replacing the relative
addresses of the CALL and JMP instructions (0xE8 and 0xE9) with
absolute addresses by adding the offset from the start of the file
to the 4 byte number (LSB first) that follows the instruction.
When used, set ph=0, pm=3.

The p transform performs a simple, fast compression that improves
speed by replacing duplicate, consecutive strings that occur in the
same context with a code to indicate the length of the match. The
sequence (esc len), len > 0, codes a match of length len+minlen.
The sequence (esc 0) codes esc. The context hash is updated for
each byte C by: hash := hash * hmul + C (mod 2^ph). The match
must be found in a rotating buffer of size 2^pm. For example:

  comp x x 18 20 x (ph=18, pm=20)
    ...
  post
    p 127 3 40
  end

says to code matches using escape bytes with the value 127. Match
lengths are in the range (4...258). This uses an order 6 context hash
because 40 = 5*2^3 effectively shifts the context hash left by 3 bits
and ph/3 = 18/3 = 6.

LZP helps speed at the expense of compression for all but the fastest
configurations (e.g. min.cfg). It is recommended to use esc = any
value that rarely occurs in the input, minlen = 3 or more, hmul to
select a context of order < minlen, pm = ph + 2 and pm > 8. Allowed
values are (0...255).

Contents:

  zpaq037.pdf -Version 0.38 of the ZPAQ specification, valid only
               for zpaq v0.08.

  zpaq.cpp -   Source code.

  zpaq.exe -   32 bit Windows executable, compiled as follows:
               g++ -O2 -s -fomit-frame-pointer -march=pentiumpro \
                   -DNDEBUG zpaq.cpp -o zpaq.exe
               upx zpaq.exe

  min.cfg -    Config file for fast compression.

  mid.cfg -    Config file for average compression.

  max.cfg -    Config file for good compression.

  readme.txt - This file

Changes:

v0.01 - Feb. 15, 2009. Original release. Conforms to v0.29 of spec.
        except does not support postprocessing.

v0.02 - Feb. 18, 2009. Adds R=X, X=R, and LJ
        instructions and R[256] register. Removes .= instruction.
        Spaces are required before ZPAQL operands. Adds end of segment
        signal to decoder. Adds "x" transform (E8E9). PASS transform
        is changed to "0". Adds a header byte to describe HCOMP
        language. Not compatible with v0.01. Conforms to v0.32 of spec.
        Current max.cfg does poorly with maximumcompression.com.
        Expect more changes.

v0.03 - Feb. 19, 2009. Fixed MIX, MIX2, and IMIX spec. to reduce overflow,
        which resulted in poor compression of large files. Modified
        stretch function for better compression.

        Block 1: requires 314.476 MB memory (with POST X to turn on E8E9)
          maxcomp\a10.jpg  842468 -> 829159
          maxcomp\acrord32.exe  3870784 -> 1154882
          maxcomp\english.dic  4067439 -> 476099
          maxcomp\FlashMX.pdf  4526946 -> 3649140
          maxcomp\fp.log  20617071 -> 432826
          maxcomp\mso97.dll  3782416 -> 1545417
          maxcomp\ohs.doc  4168192 -> 757538
          maxcomp\rafale.bmp  4149414 -> 763314
          maxcomp\vcfiu.hlp  4121418 -> 499321
          maxcomp\world95.txt  2988578 -> 441130
        53,134,726 -> 10,548,826

v0.04 - Feb. 21, 2009. Fixed train() spec. to fix poor compression with
        SSE and possibly other components. Modifed squash() for better
        compression. New max.cfg.

v0.05 - Feb. 26, 2009. Changed representation of squashed probabilities
        to 15 bits (0..32767) and stretched to 6 bit scale in (-2048..2047),
	and mixer weights to 20 bit signed numbers. Mixers are now guaranteed
	not to overflow. The higher resolution improves compression on highly
	redundant files. MIX2 now has weights constrained to add to 1 which
	also improves compression.

v0.06 - Feb. 27, 2009. Optionally appends a SHA1 hash of the input file
	for each segment, which is checked by the decompressor. Added
	"b" command to append without a checksum. Replaced IMIX2 with
	ISSE. Compression prints memory usage by component.

v0.07 - Feb. 28, 2009. Modified ISSE to use decreasing learning rate
        on the fixed size inversely proportional to a count. ISSE drops the
        c and rate parameters. SSE drops the mask parameter. Bit history
        next-state tables are updated by removing some of the n0=0 or n1=0
        states and adding other states.

v0.08 - Mar. 8, 2009. Added LZP preprocessor. Improved memory utilization
        reporting. Minor speed improvements. Added mid.cfg. Changed
        MATCH so that the buffer and hash table sizes are specified
        separately. Clarified role of comment field. Removed zpaqd.exe.
