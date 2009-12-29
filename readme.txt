README for ZPAQ v1.10
Matt Mahoney - Dec. 28, 2009, matmahoney (at) yahoo (dot) com.

ZPAQ is a configurable file compressor and archiver. Its goal
is a high compression ratio in an open format without loss of
compatibility between versions as new compression algorithms
are discovered. ZPAQ includes tools to help develop and test
new algorithms.

All software is (C) 2009, Ocarina Networks Inc. and written
by Matt Mahoney. It is open source licensed under GPL v3.
http://www.gnu.org/copyleft/gpl.html

Contents:

  zpaq.exe -     The ZPAQ compressor, decompressor, and environment for
                 developing new compression algorithms in the ZPAQ format.
                 Compiled for 32 bit Windows.

  zpaq.cpp, zpaq.h - Source code (GPL) for zpaq.exe. See comments for usage.

  zpaqmake.bat - Script used by ZPAQ to build optimized code.

  min.cfg -      ZPAQ config file for fast compression.

  mid.cfg -      Config file for average compression (default).

  max.cfg -      Config file for good compression.

  lzppre.exe -   LZP preprocessor, required with min.cfg.

  lzppre.cpp -   Source code for lzppre.exe.

  readme.txt -   This file.

Brief usage summary:

To compress:      zpaq ocmax.cfg,2 archive files...
To decompress:    zpaq ox archive files...
To list contents: zpaq l archive
For help:         zpaq

For compression, "c" means compress. To append to an existing
archive, use "a" instead, as "zpaq oamax.cfg,2 archive files...".

"o" means optimize (run faster). You need a C++ compiler installed
to use this option. If not, drop the "o". You can still use zpaq
but it will take about twice as long to run.

"max.cfg" selects maximum (but slow) compression. min.cfg selects
minimum but fast compression. mid.cfg is in the middle.
Decompression speed will be the same as compression.

",2" means use 4 times more memory. Each increment doubles usage.
You need the same memory to decompress.

"ox" means extract fast. You can extract more slowly with "x"
if you don't have C++ installed. Output files are renamed in
the same order they are stored and listed. If you don't rename the
output files, then the files will be extracted to the current
directory with the same names they had when stored.

See zpaq.cpp for complete descriptions, many other options,
and how to write config files for custom compression algorithms,
and installation instructions.


History
-------

Versions prior to 1.00 are not compatible with the ZPAQ
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

v1.07 - Oct. 2, 2009. zpaq config files now accept arguments. Fixed
        a bug in min.cfg. Cleaned up "tr" command display. min.cfg,
        mid.cfg, max.cfg accept an argument to change memory.
        min.cfg takes a second argument to change LZP minimum match.
        pcomp external preprocessor command must end with ;

v1.08 - Oct. 14, 2009. Added optimization, which makes zpaq about
        twice as fast if an external C++ compiler is available.
        The "o" option compiles the model and creates a temporary
        program optimized for the current input, and runs it.
        Also changed meaning of "nx" to mean decompress all output
        to one file. Fixed ZPAQL shift instructions to be consistent
        with spec on non x86 machines.

v1.09 - Oct 21, 2009. Port to Linux. Preprocessor temporary files
        now go in %TEMP% or $TEMP. TMPDIR not used. Optimized
        decompressor now verifies header contents matches code.
        File size display fixed for sizes over 2 GB. Added q option
        (quiet) to suppress output. Compression shows preprocessed
        size if different.

v1.10 - Dec. 28, 2009. zpaq.cpp bug fix for g++ 4.4.1/Linux. Thanks to
        Tom Hargreaves for a patch. zpaq.h is still v1.09.
