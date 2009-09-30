README for ZPAQ v1.06
Matt Mahoney - Sept. 29, 2009, matmahoney (at) yahoo (dot) com.

ZPAQ is a configurable file compressor and archiver. Its goal
is a high compression ratio in an open format without loss of
compatibility between versions as new compression algorithms
are discovered. ZPAQ includes tools to help develop and test
new algorithms.

All software is (C) 2009, Ocarina Networks Inc. and written
by Matt Mahoney. It is open source licensed under GPL v3.
http://www.gnu.org/copyleft/gpl.html

Contents:

  zpaq1.pdf -    The ZPAQ open standard format for highly compressed
                 data. Revision 1 last updated Sept.29, 2009.

  unzpaq106.cpp -Reference standard decoder (GPL). It is part of the
                 specification. v1.03 last updated Sept. 29, 2009.

  zpaq.exe -     The ZPAQ compressor, decompressor, and environment for
                 developing new compression algorithms in the ZPAQ format.
                 Compiled for 32 bit Windows.

  zpaq106.cpp -  Source code (GPL) for zpaq.exe. See comments for usage.

  min.cfg -      ZPAQ config file for fast compression.

  mid.cfg -      Config file for average compression (default).

  max.cfg -      Config file for good compression.

  exe.cfg -      Config file for good compression of x86 .exe and .dll files.

  lzppre.exe -   LZP preprocessor, required with min.cfg.

  lzppre.cpp -   Source code for lzppre.exe.

  exepre.cfg -   E8E9 preprocessor in ZPAQL, required with exe.cfg

  zpaqsfx.exe -  Stub for making self extracting archives.

  zpaqsfx.cpp -  Source (GPL) for zpaqsfx v1.06.

  zpaqsfx.tag -  16 byte locater tag appended to zpaqsfx.exe.

  readme.txt -   This file.

Brief usage summary:

To create a new archive: zpaq c archive files...
To append to an archive: zpaq a archive files...
To list contents:        zpaq l archive
To extract:              zpaq x archive
To extract and rename:   zpaq x archive files...
For help:                zpaq

To make a self extracting archive, copy zpaqsfx.exe and append to it
with the "a" command. When the copy is run, it will list its contents
and prompt to use the x command to extract. For example:

  copy zpaqsfx.exe books.exe
  zpaq a books.exe book1 book2
  books.exe    (will list contents)
  books.exe x  (will create book1 and book2)

Compression options are stored in config files. To use them,
append the name after the "c" or "a" with no space, for example:

  zpaq cmax.cfg archive files...  (for good but slow compression)
  zpaq amin.cfg archive files...  (for poor but fast compression)

Decompression usually requires about the same time and memory as
compression. Some config files require external preprocessors be
present during compression. Config files and preprocessors are not
needed to list or extract files.

A config file describes a context mixing algorithm, a program in
a sandboxed interpreted language called ZPAQL to compute contexts,
and an optional external preprocessor and corresponding ZPAQL
postprocessor. See the zpaq106.cpp source code comments for guidelines
on writing and modifying config files and preprocessors. ZPAQ has advanced
options to support testing and debugging new compression algorithms.

History: Versions prior to 1.00 are not compatible with the ZPAQ
standard and are obsolete. All versions 1.00 and higher are forward
and backward compatible.

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

v1.05 - Sept. 28, 2009. Removed built in x (E8E9) and p (LZP)
        preprocessors and made these external programs (included).
        Config files now specify an external preprocessor command
        line and ZPAQL code to invert the transform. The inversion
        is verified before compression. Added structured programming
        (if/ifnot-else-endif, do-while/until/forever) to ZPAQL.
        Reorganized the less commonly used commands. New commands
        to extract from single blocks, extract with paths (default
        is now to current directory), extract unnamed blocks as
        separate files, compress without filenames or with full paths,
        or without comments, debug both the HCOMP and new PCOMP sections
        of config files, and display trace in either decimal or
        hexadecimal. Fixed detection of corrupted input in decoder.
        unzpaq.exe not included in distribution because zpaq.exe
        has all the same functions.

v1.06 - Sept. 29, 2009. Updated specification zpaq1.pdf to include
        a recommendation of adding a 13 byte locater tag to mark the
        start of a ZPAQ archive embedded in other data. Updated
        zpaq.cpp, unzpaq.cpp, and zpaqsfx.cpp to find this tag.
        Also added "ta" to append this tag. Some minor bug fixes
        and porting issues fixed. Changed unzpaq to extract to current
        directory by default.
