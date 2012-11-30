README for ZPAQ v1.04
Matt Mahoney - Sept. 18, 2009, matmahoney (at) yahoo (dot) com.

ZPAQ is a configurable file compressor and archiver. Its goal
is a high compression ratio in an open format without loss of
compatibility between versions as advanced compression techniques
are discovered.

Versions 1.00 and higher are compatible with the ZPAQ level 1
standard, which was first released Mar. 12, 2009.
The latest version can be found at http://mattmahoney.net/dc/

There are 3 programs. unzpaq is a reference decoder.
It is an integral part of the standard. zpaq is both a compressor
and decompressor. It is not part of the standard. zpaqsfx
is a stub for creating self extracting archives.

Unzpaq works like zpaq except that it understands only the x (extract)
and l (list) commands. zpaq understand the following:

  a archive files... - Compress files and append to archive.
  c archive files... - Compress files to new archive (clobbers).
  x archive - Extract all files using stored names (does not clobber).
  x archive files... - Extract and rename (clobbers).
  l archive - List archive contents.

zpaq (but not unzpaq) can read and append to self
extracting archives. To make a Windows self-extracting archive,
append to a copy of zpaqsfx.exe. When the appended program is
run, it will list its contents and prompt to use the x command
to extract its contents.

Advanced options:

  v archive - List archive contents verbosely.
  b archive files... - Compress files and append with no checksum.
  k{a|b|c} archive file [m [n]] - {Append|no checksum|create} archive
    from n (default all) bytes of file skipping first m (default 0).
  [k]{a|b|c}config - Use compression options in config file.
  r[k]{a|b|c} - Store paths.
  t archive [files...] - extract (like x) without postprocessing.
  hconfig args... - Run HCOMP in config with numeric args (no archive).
  pconfig in out  - Run PCOMP on files (default stdin/stdout).
  sconfig - Compile header to a list of bytes to stdout.

By default, zpaq will create archives and store the file name
without a path. Files will be extracted to the current directory
using the stored name. For example:

  zpaq c books.zp c:calgary\book1 /tmp/book2

will create archive books.zp, compress the two named files
and store the names as book1 and book2 with no path. Then

  zpaq x books.zp

will extract book1 and book2 to the current directory. If either
of those files already exist, then zpaq will not overwrite it
and quit. To extract elsewhere, you can rename them, for example:

  zpaq x /tmp/foo book2

will extract book1 to /tmp/foo (provided directory /tmp exists)
and book2 to book2 in the current directory even if that file
already exists. zpaq will not create directories. If you name
less than 2 files, then it will extract only the files you named.

To store path names as entered, use the command "ra" or "rc".
However, zpaq will refuse to extract files with a drive letter
or absolute path unless you explicitly name them during extraction, so

  zpaq rc books.zp c:calgary\book1 /tmp/book2

will store exactly as entered. However

  zpaq x books.zp

will fail because drive letters and absolute paths are not allowed, but

  zpaq x books.zp c:calgary\book1 /tmp/book2

will succeed provided the named directories exist. zpaq will not
create directories.

To specify compression options, append the name of a configuration
file after the "a", "c", "ra", or "rc" command.
Three examples are supplied:

  min.cfg - Fast, minimal compression (LZP + order 3). Requires 4 MB memory.
  mid.cfg - Average compression and speed. Requires 111 MB.
  max.cfg - Slow but good compression. Requires 278 MB.

The default is mid.cfg. Thus, either:

  zpaq cmid.cfg calgary.zp calgary/*
  zpaq c        calgary.zp calgary/*

will compress the 14 file Calgary corpus to 699474 bytes in 11 seconds
using 111 MB memory on a 2 GHz Pentium T3200.

  zpaq cmax.cfg calgary.zp calgary\*

will compress to 644436 bytes in 48 seconds using 278 MB. min.cfg
will compress to 1027462 bytes in 1.5 seconds with 4 MB.

To make a self extracting archive, make a copy of zpaqsfx.exe
and append to it with zpaq:

  copy zpaqsfx.exe calgary.exe
  zpaq amax.cfg calgary.exe calgary/*

When the resulting archive is run with no arguments, it will
list its contents. When run with argument x and optional filenames,
it will extract like unzpaq.

  calgary                (lists contents (like unzpaq l))
  calgary x              (extracts all files (like unzpaq x))
  calgary x file1 file2  (extracts first 2 files as file1, file2)

To append without saving a SHA1 checksum (saves 20 bytes per file):

  zpaq b archive files...

If a checksum is present, the decompressor will compute the SHA1
hash of the extracted file and compare it with the stored checksum
and report a warning if they don't match.

  zpaq v archive

This also shows the model used to compress and the ZPAQL program
used to compute the contexts from the original config file. The
config file is not needed to extract.

Files can be split into segments by prepending "k" to the compress
command:

  zpaq [r]k{a|b|c}[config] archive file [offset [length]]

which means to skip 'offset' bytes of the file and compress
the next 'length' bytes to the archive. r, a, b, and c have
their usual meaning. For example:

  zpaq kcmin.cfg book1.zp book1 0 5000
  zpaq ka        book1.zp book1 5000 2000
  zpaq kamax.cfg book1.zp book1 7000

will compress book1 to 3 blocks of size 5000, 2000, and the rest
of the file using 3 different compression options. If the offset
is not 0, then the file name is not stored, which signals the
decompressor to append the block to the previous file. Thus

  zpaq x book1.zp

will extract book1 as usual.

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

Compiles config and outputs header as a list of numbers suitable
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

  zpaq100.pdf -  Version 1.00 of the ZPAQ specification. It became
                 standard on Apr. 11, 2009 because it was not
                 superceded by a newer version for 30 days after release.

  unzpaq103.cpp -Reference standard decompressor. It is
                 part of the specification.

  unzpaq.exe -   32 bit Windows executable, compiled as follows with
                 MinGW g++ 4.4:
                 g++ -O2 -s -fomit-frame-pointer -march=pentiumpro  \
                   -DNDEBUG unzpaq103.cpp -fno-exceptions -fno-rtti \
                   -o unzpaq.exe 
                 upx unzpaq.exe

  zpaq104.cpp -  Compressor source code, not a part of the standard.
                 Compiled as above.

  zpaq.exe -     32 bit Windows executable.

  min.cfg -      Config file for fast compression.

  mid.cfg -      Config file for average compression (default).

  max.cfg -      Config file for good compression.

  zpaqsfx.cpp -  Source for zpaqsfx v1.03.

  zpaqsfx.tag -  16 random bytes appended to zpaqsfx.exe.

  zpaqsfx.exe -  Stub for self extracting archives created as follows
                 with MINGW g++ 4.4.0 and upx 3.00w:
                 g++ -O2 -s -fomit-frame-pointer -march=pentiumpro \
                   -DNDEBUG zpaqsfx.cpp -fno-exceptions -fno-rtti
                 upx --all-methods --all-filters a.exe
                 copy/b a.exe+zpaqsfx.tag zpaqsfx.exe

  readme.txt -   This file

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

v0.09 - Mar. 9, 2009. Removed counters from ISSE and ICM and replaced
        bit history map with initial estimates based on n1/(n0+n1) to
        improve speed. Fixed a bug where x clobbers files when it says
        it isn't.

v1.00 - Mar. 12, 2009. First level 1 candidate. Simplified the
        bit history tables and replaced with code to generate them
        in both the documentation and code. First release of the
        reference standard unzpaq1 v1.00. Improved compression on 
        some files.

v1.01 - Apr. 27, 2009. Updated unzpaq to fix VS2005 compiler issues.

v1.02 - June 14, 2009. Updated zpaq and unzpaq to close files
        immediately after extraction instead of when program exits.
        Fixed g++ 4.4 compiler warnings.

v1.03 - Sept. 8, 2009. unzpaq and zpaq: added support for appending
        unnamed segments to the previous file. In unzpaq 1.02 and earlier
        you would need to extract each segment to a different file
        and concatenate them manually. Also, unzpaq will refuse
        to extract filenames stored with an absolute path, drive letter,
        or that have upward links "../" or "..\" or that have
        control characters (ASCII 0-31) in the file name unless
        a filename is given on the command line (in which case
        any name is allowed). Quits on the first error rather
        than skipping files. zpaq only: made mid.cfg the default
        configuration. Also added the k command
        to create segmented files. When the offset is not 0 the
        segment is stored with no name to signal the decompressor
        to append to the previous file (which may be in a different
        ZPAQ block). Added the r command to store full paths.
        1.02 and earlier always did this. By default, 1.03 stores
        only the file name. Updated the s command to output the
        full header as a C array.

        Sept. 14, 2009. Added zpaqsfx 1.03.

v1.04 - Sept. 18, 2009. zpaq will extract from self extracting archives.
        Added progress meter. zpaqsfx.exe is slightly smaller. Fixed
        zpaqsfx.cpp compiler issue (replaced "and" with "&&" in main()).
