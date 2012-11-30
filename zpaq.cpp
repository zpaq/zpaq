/*  zpaq v1.10 archiver and file compressor.

(C) 2009, Ocarina Networks, Inc.
    Written by Matt Mahoney, matmahoney@yahoo.com, Dec. 28, 2009.

    LICENSE

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 3 of
    the License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful, but
    WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    General Public License for more details at
    Visit <http://www.gnu.org/copyleft/gpl.html>.

This program compresses files into archives and decompresses them.
The archive format is compatible with other ZPAQ level 1 compliant
programs. See http://mattmahoney.net/dc/


Installation
------------

ZPAQ runs from a command window. In Windows, the following files
need to be somewhere in your PATH or in the current directory when run:

  zpaq.exe     - this program.
  zpaqmake.bat - script used for optimization.
  lzppre.exe   - preprocessor for fast compression (min.cfg)

In addition you will need a C++ compiler and zpaq.cpp and zpaq.h
to use the optimization feature. Optimization speeds up compression
and decompression typically by 50% to 100% or more. It works by
generating C++ code for an optimized version of ZPAQ tuned to the
input, then compiling and running it.

The script zpaqmake.bat should take one argument %1, which will be a
C++ program without the .cpp extension. It should compile %1.cpp
with options -DOPT -DNDEBUG, link to zpaq.cpp (wherever you put it)
and name the result %1.exe. The compiler needs to know where zpaq.h and
zpaq.cpp can be found. For example, suppose that zpaq.cpp
and zpaq.h are placed in the directory C:\zpaq. Then zpaqmake.bat
would contain at a minimum:

  g++ -O2 -DOPT -DNDEBUG %1.cpp -IC:\zpaq C:\zpaq\zpaq.cpp -o %1.exe

g++ 4.4.0 is the recommended compiler, but others will work.

-DOPT is required. It removes features not needed for specialized
compressors.

-DNDEBUG removes run time checks for better speed.

%1.cpp is the input file. %1 is passed to the script. It includes
the full path, normally %TEMP%/filename where filename is "zpaq_"
followed by 40 hexadecimal digits.

-IC:\zpaq tells g++ to look in C:\zpaq for zpaq.h

C:\zpaq\zpaq.cpp is this file. Alternatively it may be compiled
in advance with -DOPT -DNDEBUG to zpaq.o and it could be linked.

-o %1.exe names the output file.

-O2 optimizes for speed. It often works better than -O3.
Other optimizations should be specific
to the installed computer. Some useful options are:

-s to strip debugging symbols to save space.
-fomit-frame-pointer improves speed a bit.
-march=pentiumpro is the oldest target that doesn't hurt speed much.

You can also compile zpaq.cpp to zpaq.o in advance and link to it
in the script. If you do so, compile zpaq.cpp with -c -DOPT -DNDEBUG
and other optimizations as appropriate.

The script can also compress the resulting .exe, e.g.

  upx -qqq %1.exe

If you don't use the optimization feature then you don't need a C++
compiler, source code, or script. You only need zpaq.exe and lzppre.exe
in your PATH.

Linux Installation
------------------

If you are installing in Linux then you will need to write an equivalent
shell script called zpaqmake (no .bat extension) and make it executable.
The output should still have a .exe extension. For example, suppose
that zpaq.cpp and zpaq.h are in /usr/zpaq

  #!/bin/csh
  g++ -O2 -DOPT -DNDEBUG $1.cpp -I/usr/zpaq /usr/zpaq/zpaq.cpp -o $1.exe

You will also need to compile zpaq.cpp and lzppre.cpp and put the
executables somewhere in your $PATH. Compile with -DNDEBUG. Don't
add a .exe extension.

If you want to use /tmp for temporary files, then you need to set

  setenv TEMP /tmp

prior to running zpaq.


Command summary
---------------

To compress to new archive: zpaq [opnsitqv]c[F[,N...]] archive files...
To append to archive:       zpaq [opnsitqv]a[F[,N...]] archive files...
Optional modifiers:
  o = compress faster (requires C++ compiler)
  p = store filename paths in archive
  n = don't store filenames (names will be needed to decompress)
  s = don't store SHA1 checksums (saves 20 bytes)
  i = don't store file sizes as comments (saves a few bytes)
  t = append locator tag to non-ZPAQ data
  q = quiet
  v = verbose (show F as it compiles)
  F = use options in configuration file F (min.cfg, max.cfg)
  ,N = pass numeric arguments to F
To list contents: zpaq l archive
To extract: zpaq [opntq]x[N] archive [files...]
  o = extract faster (requires C++ compiler)
  p = extract to stored paths instead of current directory
  n = decompress all to one file
  t = don't post-process (for debugging)
  q = quiet
  N = extract only block N (1, 2, 3...)
  files... = rename extracted files (clobbers)
      otherwise use stored names (does not clobber)
To debug configuration file F: zpaq [pthv]rF[,N...] [args...]
  p = run PCOMP (default is to run HCOMP)
  t = trace (single step), args are numeric inputs
      otherwise args are input, output (default stdin, stdout)
  h = trace display in hexadecimal
  v = verbose compile
  ,N = pass numeric arguments to F


Basic commands
--------------

  zpaq c archive files...

Compresses one or more files and creates a new archive. If the
archive exists then it is overwritten. File names are stored
without a path ("C:\tmp\foo.txt" is saved as "foo.txt").

  zpaq a archive files...

Compresses files and appends to archive. If the archive does not
exist then it is created as with the c command.

  zpaq l archive

List the archive contents. Files are listed in the same order they
were added.

  zpaq x archive

Extract the contents of the archive. New files are created and named
according to the stored filenames. Does not clobber existing files.
Extracts to current directory.

  zpaq x archive files...

Extract files and renames in the order they were added to the
archive. Clobbers any already existing output files. The number
of files extracted is the smaller of the number of filenames
on the command line or the number of files in the archive.


Archive format
--------------

The precise archive format is ZPAQ level 1 revision 1, found at
http://mattmahoney.net/dc/zpaq1.pdf revised Sept. 29, 2009.

A ZPAQ archive consists of a sequence of blocks that can be
decompressed independently. Each block contains one or more
segments that must be decompressed in sequence from the start
of the block. A block header describes the compression algorithm.
A segment contains an optional filename field, an optional comment
string, the compressed file, and optionally the SHA1 checksum of the
data prior to compression. The "c" and "a" commands create or append
one block with each file in a separate segment. The "l" and "x"
commands list or extract all of the blocks in the order that they
were added.

An archive may be mixed with other data provided that each ZPAQ
block sequence is preceded by a locator tag (appended with "ta").
ZPAQ will ignore the other data. One use for this technique
is to append ZPAQ blocks to a self extracting archive stub.


Compression options
-------------------

  zpaq [opnsitv]ca[F[,N...]] archive files...

Create (c) or append (a) archive with optional modifiers and an optional
configuration file F. Three files are included:

  min.cfg - for fast but poor compression.
  max.cfg - for slow but good compression.
  mid.cfg - for moderate speed and compression (default).

Other config files are available as add-on options or you can
write them as explained later.

A numeric argument may be appended to F to increase memory
usage for better compression. Each increment doubles usage.
There should be no space before or after the comma. For example:

  zpaq cmax.cfg archive files...    = 246 MB
  zpaq cmax.cfg,1 archive files...  = 476 MB
  zpaq cmax.cfg,2 archive files...  = 938 MB
  zpaq cmax.cfg,3 archive files...  = 1861 MB
  zpaq cmax.cfg,-1 archive files... = 130 MB (negative values allowed)

Modifiers may be in any order before the "c" or "a" command.
The modifiers, command, and configuration file must be written
together without any spaces, for example

  zpaq ipscmax.cfg books.zpaq book1 book2

creates archive books.zpaq with options i, p, s, and configuration
file max.cfg. Modifiers have the following meaning:

  p

Store file name paths as given on the command line. The default
is to store the name without the path. For example,
"zpaq pc books.zpaq tmp\book1" will store the name as "tmp\book1"
instead of "book1". If the p option is also given during extraction,
then ZPAQ will attempt to extract book1 to the subdirectory tmp
instead of the current directory. This will fail if tmp does not exist.
ZPAQ does not create directories as needed.

  n

Do not store filenames. The effect is to require that filenames
be given during decompression.

  s

Do not store SHA1 checksums. This saves 20 bytes. The decompressor
will not check that the output is identical to the original input.

  i

Do not store anything in the comment field. Normally the input
file size is stored as a decimal string, taking a few bytes.
The comment field has no effect on the program except that
it is displayed by the "l" and "x" commands.

  v

Verbose. Show config file F (if present) as it is compiled.
This is useful for error checking.

  q

Quiet. Don't display compression progress on the screen.

  t

Append a locator tag to non-ZPAQ data. The tag is a string of
13 bytes that allows ZPAQ and UNZPAQ to find the start
of a sequence of ZPAQ blocks embedded in other data.
zpaqsfx.exe already has this tag at the end. However, if a
new stub is compiled from the source then the t command should
be used when appending the first file.

  o

means optimize. If successful, compression is typically 50% to 100%
faster. ZPAQ will look for a program named zpaq_X.exe in the
temporary directory, where X is derived from the SHA1 checksum
of the block header produced by config file F with arguments N. If the
program exists, then ZPAQ will call it with the same arguments to
perform the compression. If it does not exist then ZPAQ will create
a source code file zpaq_X.cpp in the temporary directory, compile it,
and link it to zpaq.cpp or zpaq.o depending on the installation.

The temporary directory is specified by the environment variable
TEMP if it exists, or else the current directory.

The program zpaq_X.exe will compress its input in the same format
as described by F, but faster. If F specifies a preprocessor, then
zpaq_X.exe will expect to find it too. It will also decompress archive
blocks in the same configuration but fail if it attempts to decompress
blocks in any other configuration.

zpaq_X.exe will accept the "c", "a" and "x" commands with all of the
same modifiers, but will ignore the "v" and "o" modifiers and ignore
any config file F and arguments passed to it. It will not accept the
"l" or "r" commands. Extraction requires a block number ("x1", "x2",
etc). A different optimized program is used to extract each block.

ZPAQ will call the external program zpaqmake to compile zpaq_X.cpp,
passing it zpaq_X as an argument. Normally this will be a script
that calls a C++ compiler to produce zpaq_X.o, links to zpaq.o
and outputs zpaq_X.exe. The script could link to zpaq.cpp instead
of zpaq.o.


Extraction options
------------------

  zpaq [opntq]x[N] archive [files...]

  p

means extract using stored paths if present. The default is
to extract to the current directory regardless of how the file
names are stored. Stored paths must be relative to the current
directory, not start with a "/", "\", a drive letter like "C:"
or contain "../" or "..\". If extracting to a subdirectory, it
must already exist. It will not be created.

[files...] overrides and has no restrictions on file names. Each
segment extracts to a different file. If any segments do not
have a stored filename then they can only be extracted using 
the "p" or "n" modifiers.

  n

means to ignore stored filenames and append all output to one file,
the first file in [files...].

  t

means extract without postprocessing (for debugging). Expect
checksum errors.

  q

means quiet. Don't display decompression progress.

  N

means to extract only from block number N, where 1 is the
first block. Otherwise all blocks are extracted. The "l"
command shows which files are in each block.

  o

means optimize. This typically speeds up decompression by 50% 
to 100%. For each block in the archive, ZPAQ will
compute the SHA1 checksum X of the block
header including compressed postprocessor code,
and call zpaq_X.exe in the temporary direcory with the same arguments
but with the block number N appended. If zpaq_X.exe does not exist in
the temporary directory (TMPDIR, else TEMP, else the current
directory), then it will create it by calling the external
program zpaqmake passing zpaq_X as an argument. The resulting
program will work like the one created with "oc" or "oa" except
that it won't be able to pre-process. If the block uses postprocessing,
then X will be different than the corresponding compressor.


Development options
-------------------

  zpaq [pthv]*rF[,N...] [args...]

Run the ZPAQL program in HCOMP section of configuration file F.
The program is run once for each byte of input from the file
named in the first argument and once at EOF with the input byte
(or -1) in the A register. Output is to the file named in the second
argument. If run with no arguments then take input from stdin
and output to stdout. Modifiers:

  p

Run the PCOMP section rather than HCOMP.

  t

Trace (single step) the program. The arguments should be numbers
rather than file names. The program is run once for each argument
with the value in the A register. As each instruction is executed
the register contents are shown. At HALT, memory contents are
displayed.

  h

When tracing, display register and memory contents in hexadecimal
instead of decimal.

  v

Verbose. Display the config file as it is being compiled. If an
error occurs, it will be easier to locate. v is also useful
for displaying jump targets.

  ,N

Pass up to 9 numeric arguments to config file F (like the c and a
commands).


Configuration files
-------------------

ZPAQ uses a configurable compression algorithm based on
bitwise prediction and arithmetic coding, and optional
pre- and post-processing. The algorithm is described
precisely in http://mattmahoney.net/dc/zpaq1.pdf

The compression and decompression algorithm is described
in a configuration file. The decompression algorithm is
stored in the ZPAQ archive. The configuration file is only
needed during compression. It has 3 parts:

COMP - a description of a sequence of bit predictors.
Each component takes a context and earlier predictions
as input, and outputs a prediction for the next bit.
The last component's prediction is output to the arithmetic
coder which codes the bit at a cost of log2(1/p), where p is
the probability guessed for that bit. (Thus, better prediction
leads to better compression). Bits are coded in MSB (most
significant bit) to LSB (least significant bit) order.

HCOMP - a program that is called once for each byte of
uncompressed data with that byte as input, and outputs
an array of 32-bit contexts, one for each component
in the COMP section. The program is written in ZPAQL,
a sandboxed assembler-like language designed for small size
and fast interpretation (rather than for easy development).

POST/PCOMP - an optional pair of programs to preprocess the
input before compression and postprocess the output after
decoding for decompression. POST indicates no pre- or
postprocessing. The model described by COMP and HCOMP
sees a 0 byte followed by a concatenation of the uncompressed files.
During decompression, the leading 0 indicates no postprocessing.

PCOMP describes an external program to preprocess the input
files during compression, and a ZPAQL program to perform the
reverse conversion to restore the original input. Unlike
COMP and HCOMP, two programs are needed because the compression
and decompression models are not the same. During compression,
ZPAQ will run the input through both programs and compare the
output with the input. If they don't match, then ZPAQ will
refuse to compress the file. If they do match, then the
input files are preprocessed and compressed, along with
the postprocessing program that will be used later to
invert the preprocessing transform. The compression model
described in the COMP and HCOMP sections sees a 1 as the
first byte to indicate that the decoded data should be
postprocessed before output. This is followed by a 2 byte
program length (LSB first), the ZPAQL postprocessor code, and a
concatenation of the preprocessed input files. The PCOMP
code sees just the preprocessed files as input, each ending
with EOS (-1).

The preprocessor is an external program. It is not needed for
decompression so it is not saved in the archive. It expects to be
called with an input filename and output filename as its last 2
arguments.

The postprocessor is a ZPAQL program that is called once for each
input byte and once with input EOS (-1) at the end of each segment.
The program is initialized at the beginning of a block but
maintains state information between segments within the same
block. Its input is from archive.$zpaq.pre during compression testing
and from the decoder during decompression.

The configuration file has the following format:

  COMP hh hm ph pm n
    (n numbered component descriptions)
  HCOMP
    (program to generate contexts, memory size = hh, hm)
  POST (for no pre/post procesing)
    0
  END

Or (for custom pre/post processing):

  COMP hh hm ph pm n
    (...)
  HCOMP
    (...)
  PCOMP preprocessor-command ;
    (postprocessor program, memory size = ph, pm)
  END

Configuration files are free format (all white space is
the same) and mostly not case sensitive. They may contain comments in
((nested) parenthesis).

For example, mid.cfg:

  comp 3 3 0 0 8 (hh hm ph pm n)
    0 icm 5 (chain of indirect model orders 0 to 5)
    1 isse 13 0
    2 isse 17 1
    3 isse 18 2
    4 isse 18 3
    5 isse 19 4
    6 match 22 24 (order 7 match model with 16 MB buffer)
    7 mix 16 0 7 24 255 (order 1 mixer, output to arithmetic coder)
  hcomp
    c++ *c=a b=c a=0 (save in rotating buffer)
    d= 1 hash *d=a (order 1 context for isse 1)
    b-- d++ hash *d=a (order 2 context)
    b-- d++ hash *d=a (order 3 context)
    b-- d++ hash *d=a (order 4 context)
    b-- d++ hash *d=a (order 5 context)
    b-- d++ hash b-- hash *d=a (order 7 context for match)
    d++ a=*c a<<= 8 *d=a (order 1 context for mix)
    halt
  post (no pre/post processing)
    0
  end


Components
----------

The COMP section has 5 arguments (hh, hm, ph, pm, n) followed
by a list of n components numbered consecutively from 0 through
n-1. hh, hm, ph, and pm describe the sizes of the arrays
used by the HCOMP and PCOMP virtual machines as described later.
Each machine has two arrays, H and M. Their sizes are 2^hh and
2^hm respectively in HCOMP, and 2^ph and 2^pm in PCOMP. The
HCOMP program computes the context for the n components by placing
them in H[0] through H[n-1] as 32-bit numbers. Thus, hh should
be set so that 2^hh >= n. In mid.cfg, n = 8 and hh = 3, allowing
up to 8 contexts. Larger values would work but waste memory.
Memory usage is 2^(hh+2) + 2^hm + 2^(ph+2) + 2^pm bytes.

mid.cfg does not use pre/post processing. Thus, there is no
PCOMP virtual machine, so ph and pm are set to 0.

The 9 possible component types are:

  CONST c
  CM s limit
  ICM s
  MATCH s b
  AVG j k wt
  MIX2 s j k rate mask
  MIX s j m rate mask
  ISSE s j
  SSE s j start limit

All component parameters are numbers in the range 0..255.

Each component outputs a "stretched" probability in the
form ln(p(1)/p(0)). where p(1) and p(0) are the model's estimated
probabilities that the next bit will be a 1 or 0, respectively.
Thus, negative numbers predict a 0 and positive predict 1.
The magnitude is the confidence of the prediction. The output is
a number in the range -32 to 32 with precision 1/64 (a 12 bit
signed number). Components are as follows:

  CONST c  (constant)

Output is (c-128)/16. Thus, numbers larger than 128 predict 1
and smaller predict 0, regardless of context. CONST is very
fast and uses no memory.

  CM s limit  (context model)

Outputs a prediction by looking up the context in a table of
size 2^s using the s low bits of the H[i] (for component i)
XORed with a 9 bit expansion of the previously coded (high
order) bits of the current byte. (Recall that H[i] is updated
once per byte). Each table entry contains a prediction p(1)
and a count in the range 0..limit*4 (max 1020). The prediction
is updated in proportion to the prediction error and inversely
proportional to the count. A large limit (max 255) is best
for stationary sources. A smaller value makes the model more
adaptive to changing statistics. Memory usage is 2^(s+2) bytes.

  ICM s  (indirect context model)

Outputs a prediction by looking up the context in a hash
table of size 64 * 2^s bit histores (1 byte states). The
histories index a second table of size 256 that outputs
a prediction. The table is updated by adjusting the prediction
to reduce the prediction error (at a slow, fixed rate) and
updating the bit history. A bit history represents a recent
count of 0 and 1 bits and indicates whether the most recent
bit was a 0 or 1. The hash table is indexed by the low s+10
bits of H[i] and the previous bits of the current byte, with
highest 8 bits (s+9..s+2) used to detect hash collisions.
An ICM works best on nonstationary sources or where memory
efficiency is important. It uses 2^(s+6) bytes.

  MATCH s b

Outputs a prediction by searching for the previous occurrence
of the current context in a history buffer of size 2^b bytes,
and predicting whatever bit came next, with a confidence
proportional to the length of the match. Matches are found
using an index of 2^s pointers into the history buffer, each
of which points to the previous occurrence of the current
context. A MATCH is useful for any data that has repeat occurrences
of strings longer than about 6-8 bytes within a window of size
2^b. Generally, larger b (up to the file size) gives better
compression, and s = b-2 gives adequate indexing. The context
should be a high order hash. Memory usage is 4*2^s + 2^b bytes.

  AVG j k wt

Averages the predictions of components j and k (which must
precede i, the current component). The average is
weighted by wt/256 for component j and 1 - wt/256 for
component k. Often, averaging two predictions gives
better results than either prediction by itself. wt should be
selected to favor the more accurate component. AVG is very
fast and uses no memory.

  MIX2 s j k rate mask

Averages the predictions of components j and k (which must precede
i) adaptively. The weight is selected from a table of size 2^s
by the low s bits of H[i] added to the masked, previously coded
bits of the current byte (an 8 bit value). A mask of 255 includes
the current byte, and a mask of 0 excludes it. (Other masks
are rarely useful). The adaptation rate is selectable. Typical
values are around 8 to 32. Lower values are best for stationary
sources. Higher rates are more adaptive. A MIX2 generally gives
better compression than AVG but at a cost in speed and memory.
Uses 2^(s+1) bytes of memory.

  MIX s j m rate mask

A MIX works like a MIX2 but with m inputs over a range of
components j..j+m-1, all of which must precede i.
A typical use is as the final component, taking all other
components as input with a low order context. A MIX with 2
inputs is different than a MIX2 in that the weights are not
constrained to add to 1. This sometimes gives better compression,
sometimes worse. Memory usage is m*2^(s+2) bytes. Execution time
is proportional to m.

  ISSE s j  (indirect secondary symbol estimator)

An ISSE takes a prediction and a context as input and outputs
an adjusted prediction. The low s+10 bits of H[i] and the previous
bits of the current byte index a hash table of size 2^(s+6)
bit histories as with an ICM. The bit history is used as an 8 bit
context to select a pair of weights for a 2 input MIX (not
a MIX2) with component j (preceding i) as one input and
a CONST 144 as the other. The effect is to adjust the previous
prediction using a (typically longer) context. A typical use
is a chain of ISSE after a low order CM or ICM working up
to higher order contexts as in mid.cfg. (This architecture is
also used in the PAQ9A compressor). Uses 2^(s+6) bytes.

  SSE s j start limit  (secondary symbol estimator)

An SSE takes a predicion and context as input (like an ISSE)
and outputs an adjusted prediction. The mapping is direct,
however. The input from component j and the context are mapped to
a 2^s by 64 CM table by quantizing the prediction to 64 levels and
interpolating between the two nearest values. The context is
formed by adding the partial current byte to the low s bits
of H[i]. The table is updated in proportion to the prediction
error and inversely proportional to a count as with a CM.
The count is initialized to start and has the range
(start..limit*4). A large limit is best for stationary sources.
A smaller limit is more adaptive. The starting count does
not start at 0 because the table is initialized so that
output predictions are the same as input predictions regardless
of context. If the initial guess is close, then a higher start
value works better.

An SSE sometimes gives better compression than an ISSE,
especially on stationary sources where a CM works better than
an ICM. But it uses 2^12 times more memory for the same
context size, so it is useful mostly for low order
contexts. A typical use is to adjust the output of a MIX.
It is sometimes followed by an AVG to average its input and output,
typically weighted 75% to 90% in favor of the output. Sometimes
more than one SSE or SSE-AVG pair is used in series with
progressively higher order contexts, or may be used in
parallel and mixed. An SSE uses 2^(s+8) bytes.

All components are designed to work with context hashes that
are uniformly distributed over the low order bits (depending on
the s parameter for that component). A CM, MIX2, MIX, or SSE may
also be used effectively with direct context lookup for low
orders. In this case, the low 9 bits of a CM or low 8 bits of the
other components should be cleared to leave space to combine with
the bits of the current byte. This is summarized:

  Component              Context size    Memory
  -------------------    ------------    ------
  CONST c                0               0
  CM s limit             s               2^(s+2)
  ICM s                  s+10            2^(s+6)
  MATCH s b              s               2^(s+2) + 2^b
  AVG j k wt             0               0
  MIX2 s j k rate mask   s               2^(s+1)
  MIX s j m rate mask    s               m*2^(s+2)
  ISSE s j               s+10            2^(s+6)
  SSE s j start limit    s               2^(s+8)

Although the ZPAQ standard does not specify a maximum for s,
this program will not create arrays 2GB (2^31) or larger.


ZPAQL
-----

There are one or two ZPAQL programs in a configuration file.
The first, HCOMP, describes a program that computes the context
hashes. The second, PCOMP, is optional. It describes the code
that inverts any preprocessing performed by an external program
prior to compression. The COMP and HCOMP sections are stored
in the block headers uncompressed. PCOMP, if used, is appended
to the start of the input data and compressed along with it.

Each virtual machine has the following state:

  4 general purpose 32 bit registers, A, B, C, D.
  A 1 bit flag register F.
  A 16 bit program counter, PC.
  256 32-bit registers R0 through R255.
  An array of 32 bit elements, H, of size 2^hh (HCOMP) or 2^ph (PCOMP).
  An array of 8 bit elements, M, of size 2^hm (HCOMP) or 2^pm (PCOMP).

Recall that the first line of a configuration file is:

  COMP hh hm ph pm n

HCOMP is called once per byte of input to be compressed or
decompressed with that byte in the A register. It returns with
context hashes for the n components in H[0] through H[n-1].

PCOMP is called once per decompressed byte with that byte in
the A register. At the end of a segment, it is called with EOS
(-1) in A. Output is by the OUT instruction. The output should
be the uncompressed data exactly as it was originally input
prior to preprocessing. H has no special meaning.

All state variables are initialized to 0 at the beginning of
a block. State is maintained between calls (and across segment
boundaries) except for A (used for input) and PC, which is
reset to 0 (the first instruction).

The A register is used as the destination of most arithmetic
or logical operations. B and C may be used as pointers into
M. D points into H. F stores the result of comparisons and
is used to decide conditional jumps. R0 through R255 are
used for auxilary storage. All operations are modulo 2^32.
All array index operations are modulo the size of the array
(i.e. using the low bits of the pointer). The instruction set
is as follows:

- Y=Z (assignment)
  - where Y is A B C D *B *C *D
  - where Z is A B C D *B *C *D (0...255)
- AxZ (binary operations)
  - where x is += -= *= /= %= &= &~ |= ^= <<= >>= == < >
  - where Z is as above.
- Yx (unary operations)
  - where Y is as above
  - where x is <>A ++ -- ! =0
  - except A<>A is not valid.
- J N (conditional jumps)
  - where J is JT JF JMP
  - where N is a number in (-128...127).
- LJ NN (long jump)
  - where NN is in (0...65535).
- X=R N (read R array)
  - where X is A B C D
  - where N is in (0...255).
- R=A N (write R array)
  - where N is in (0...255).
- ERROR
- HALT
- OUT
- HASH
- HASHD

All instructions except LJ are 1 or 2 bytes, where the second
byte is a number in the range 0..255 (-128..127 for jumps).
A 2 byte instruction must be written as 2 tokens separated by
a space, e.g. "A= 3", not "A=3" or "A = 3". The exception is
assigning 0, which has a 1 byte form, "A=0".

The notation *B, *C, and *D mean M[B], M[C], and H[D] respectively,
modulo the array sizes. For example "*B=*D" assigns M[B]=H[D]
(discarding the high 24 bits of H[D] because M[B] is a byte).

Binary operations always put the result in A.

=, +=, -=, *=, &=, |=, ^= have the same meanings as in C/C++.

/=, %= have the result 0 if the right operand is 0.

A&~B means A &= ~B;

A<<=B, A>>=B mean the same as in C/C++ but are explicitly defined
when B > 31 to mean the low 5 bits of B.

==, <, > compare and put the result in F as 1 (true) or 0 (false).
Comparison is unsigned. Thus PCOMP would test for EOS (-1) as
"A> 255". There are no !=, <=, or >= operators.

B<>A means swap B with A. A must be the right operand. "A<>B" is
not valid. When 32 and 8 bit values are swapped as in "*B<>A", the
high bits are unchanged.

++ and -- increment and decrement as in C/C++ but must be written
in postfix form. "++A" is not valid. Note that "*B++" increments
*B, not B.

! means to complement all bits. Thus, "A!" means A = ~A;

JT (jump if true), JF (jump if false), and JMP (jump) operands
are relative to the next instruction in the range -128..127.
Thus "A> 255 JT 1 A++" increments A not to exceed 256. A jump
outside the range of the program is a run time error.

LJ is a long jump. It is 3 bytes but the operand is written as
a number in the range 0..65535 but not exceeding the size of
the program. Thus, "A> 255 JT 3 LJ 0" jumps to the beginning
of the program if A <= 255.

The R registers can only be read or written, as in "R=A 3 B=R 3"
which assigns A to R3, then R3 to B. These registers can only be
assigned from A or to A, B, C, or D.

ERROR causes an error like an undefined instruction, but is
not reserved for future use (possibly in ZPAQ level 2) like
other undefined instructions.

HALT causes the program to end (and compression to resume).
A program should always execute HALT.

OUT in PCOMP outputs the low 8 bits of A as one byte to the
file being extracted. In HCOMP it has no effect.

HASH is equivalent to A = (A + *B + 512) * 773;
HASHD is equivalent to *D = (*D + A + 512) * 773;
These are convenient for computing context hashes that work
well with the COMP components. They are not required, however.
For example, "A+=*D A*= 12 *D=A" updates a rolling
order s/2 context hash for an s-bit wide component pointed
to by D. In general, an order ceil(s/k) hash can be updated
by using a multiplier which is an odd multiple of 2^k. HASH and
HASHD are not rolling hashes. They must be computed completely
for each context. HASH is convenient when M is used as a
history buffer.

In most programs it is not necessary to code jump instructions.
ZPAQL supports the following structured programming constructs:

  IF ... ENDIF              (execute ... if F is true)
  IF ... ELSE ... ENDIF     (execute 1st part if true, 2nd if false)
  IFNOT ... ENDIF           (execute ... if F is false)
  IFNOT ... ELSE ... ENDIF  (execute 1st part if false, 2nd if true)
  DO ... WHILE              (loop while true (test F at end))
  DO ... UNTIL              (loop while false)
  DO ... FOREVER            (loop forever)

These constructs may be nested 1000 deep. However IF statements and
DO loops nest independently and may be crossed. For example, the
following loop outputs a 0 terminated string pointed to by *B
by breaking out when it finds a 0.

  DO
    A=*B A> 0 IF (JF endif)
      OUT B++
    FOREVER (JMP do)
  ENDIF

IF, IFNOT, and ELSE are coded as JF, JT and JMP respectively.
They can only jump over at most 127 instructions. If the
code in these sections are longer, then use the long forms
IFL, IFNOTL, or ELSEL. These behave the same but are coded using
LJ instead. There are no special forms for WHILE, UNTIL, or FOREVER.
The compiler will automatically use the long forms when needed.


Parameters
----------

In a config file, paramaters may be passed as $1, $2, ..., $9. These
are replaced with numeric values passed on the command line. For example:

  zpaq cmax.cfg,3,4 archive files...

would have the effect of replacing $1 with 3 and $2 with 4. The default
value is 0, i.e. $3 through $9 are replaced with 0.

In addition, a parameter may have the form $N+M, where N is 1 through
9 and M is a number. The effect is to add M. For example,
$2+10 would be replaced with 14. Parameters may be used anywhere in the
config file where a number is allowed.


Pre/Post processing
-------------------

The PCOMP/POST section has the form:

  POST 0 END

to indicate no preprocessing or postprocessing, or

  PCOMP preprocessor-command ;
    (postprocessing code)
  END

to preprocess with an external program and to invert the transform
with postprocessing code written in ZPAQL. The preprocessing
command must end with a space followed by a semicolon.
The command may contain spaces or options. The program is expected
to take as two additional arguments an input file and an output
file. ZPAQ will call the program by appending the input file and
a temporary file "%TEMP%\archive.zpaq.pre" formed by appending the
extension to the archive name. If the program needs to save
any state information then it should do so in a file named
"%TEMP%\archive.zpaq.tmp" (i.e. replace ".pre" with ".tmp" in the output
filename). ZPAQ will delete this file before compressing the first file to
initialize the state of the preprocessor, and again after compressing
the last file to clean up. It will also delete archive.zpaq.pre
before and after compressing each file.

Before each file is compressed, ZPAQ will verify that the transformed
data in archive.zpaq.pre will be converted back to the original input file
by inputting archive.zpaq.pre to the ZPAQL program in PCOMP and comparing
its output to the original input. If the output is verified then 
the file is compressed. Otherwise it is skipped. The algorithm is:

  Command: zpaq [pnsitvo]ca[F][,N...]] archive inputfiles...

  if F then compile header Z from F else use default header Z
  If "c" then open archive for write
  If "a" then open archive for append
  Delete archive.zpaq.tmp
  FIRST = true
  For each inputfile FILENAME loop
    Open FILENAME as IN
    If open fails then continue
    CHECK1 = SHA1(IN)
    SIZE = |IN|
    if Z.PCOMP then
      Close IN
      Delete archive.zpaq.pre
      Run Z.preprocessor-command FILENAME archive.zpaq.pre
      CHECK2 = SHA1(Z.PCOMP(archive.zpaq.pre, EOS))
      if CHECK1 != CHECK2 then continue
      Open archive.zpaq.pre as IN
    Else if Z.POST then
      Rewind IN
    If FIRST then
      Code start of block
      Code Z.COMP, Z.HCOMP
    Code start of segment
    If not "p" then strip path from FILENAME
    If not "n" then code FILENAME
    If not "i" then code SIZE as comment
    If FIRST then
      If Z.PCOMP then compress 1, |Z.PCOMP|, Z.PCOMP
      Else if Z.POST then compress 0
    Compress IN
    Close IN
    If not "s" then code CHECK1
    Code end of segment
    FIRST = false
  Code end of block
  Close archive
  Delete archive.zpaq.tmp, archive.zpaq.pre

Temporary files will be placed in %TEMP% in Windows or $TEMP
in Linux. Windows normally defines %TEMP% as a directory for
temporary files. If the environment variable TEMP is not set, then
temporary files will be placed in the current directory. To
use /tmp in Linux, use the command "setenv TEMP /tmp" before
running ZPAQ.

Example: Suppose a preprocessor program, caesar.exe,
implements a Caesar cipher. It takes a number, input file, and
output file as 3 arguments. It encrypts by adding the number to
each byte of the input file. For example:

  caesar 3 book1 book1.enc

would encrypt book1 to book1.enc by changing A to D, B to E, etc.
To decrypt to book1.out:

  caesar -3 book1.enc book1.out

Then the following config file would use caesar.exe as a preprocessor
with a key of 5 and compress with a simple stationary order 0 model.

  COMP 0 0 0 0 1
    0 cm 9 255
  HCOMP
    halt
  PCOMP caesar 5 ;
    a> 255 jf 1 halt (ignore EOS)
    a-= 5 out halt (subtract 5 from each byte)
  END

The ZPAQL code inverts the transform by subtracting 5 from each
byte. During decompression, the code is called once for
each (transformed) decompressed byte in the A register, and once
with EOS (0xFFFFFFFF) at the end of file, which is ignored.


To compile
----------

g++ -O2 -march=pentiumpro -fomit-frame-pointer -s zpaq.cpp -o zpaq

To turn off run time checks for better speed, compile with -DNDEBUG

If linking to optimized code generated by "oc", "oa", or "ox"
then compile with -DOPT. This also removes some features to save space.

*/

#include "zpaq.h"
#include <ctype.h>
#include <math.h>
#include <time.h>

// Print an error message and exit
void error(const char* msg="") {
#ifdef OPT
  fprintf(stderr, "\nOPT error: %s\n", msg);
#else
  fprintf(stderr, "\nError: %s\n", msg);
#endif
  exit(1);
}

// Append string s to array a, enlarging as needed
void append(Array<char>& a, const char* s) {
  if (!s) return;
  if (!a.size()) a.resize(strlen(s)+1);
  int len=strlen(&a[0])+strlen(s)+1;
  if (len>a.size()) {
    Array<char> tmp(a.size());
    strcpy(&tmp[0], &a[0]);
    a.resize(len*5/4+64);
    strcpy(&a[0], &tmp[0]);
  }
  strcat(&a[0], s);
}

//////////////////////////// SHA-1 //////////////////////////////

// The SHA1 class is used to compute segment checksums.
// SHA-1 code modified from RFC 3174.
// http://www.faqs.org/rfcs/rfc3174.html


int SHA1::result(int i) {
  assert(i>=0 && i<20);
  if (!Computed && shaSuccess != SHA1Result(result_buf))
    error("SHA1 failed\n");
  return result_buf[i];
}

/*
 *  SHA1Reset
 *
 *  Description:
 *      This function will initialize the SHA1Context in preparation
 *      for computing a new SHA1 message digest.
 *
 *  Parameters: none
 *
 *  Returns:
 *      sha Error Code.
 *
 */
int SHA1::SHA1Reset()
{
    Length_Low             = 0;
    Length_High            = 0;
    Message_Block_Index    = 0;

    Intermediate_Hash[0]   = 0x67452301;
    Intermediate_Hash[1]   = 0xEFCDAB89;
    Intermediate_Hash[2]   = 0x98BADCFE;
    Intermediate_Hash[3]   = 0x10325476;
    Intermediate_Hash[4]   = 0xC3D2E1F0;

    Computed   = 0;
    Corrupted  = 0;

    return shaSuccess;
}

/*
 *  SHA1Result
 *
 *  Description:
 *      This function will return the 160-bit message digest into the
 *      Message_Digest array  provided by the caller.
 *      NOTE: The first octet of hash is stored in the 0th element,
 *            the last octet of hash in the 19th element.
 *
 *  Parameters:
 *      Message_Digest: [out]
 *          Where the digest is returned.
 *
 *  Returns:
 *      sha Error Code.
 *
 */
int SHA1::SHA1Result(U8 Message_Digest[SHA1HashSize])
{
    int i;

    if (!Message_Digest)
    {
        return shaNull;
    }

    if (Corrupted)
    {
        return Corrupted;
    }

    if (!Computed)
    {
        SHA1PadMessage();
        for(i=0; i<64; ++i)
        {
            /* message may be sensitive, clear it out */
            Message_Block[i] = 0;
        }
//        Length_Low = 0;    /* and DON'T clear length */
//        Length_High = 0;
        Computed = 1;

    }

    for(i = 0; i < SHA1HashSize; ++i)
    {
        Message_Digest[i] = Intermediate_Hash[i>>2]
                            >> 8 * ( 3 - ( i & 0x03 ) );
    }

    return shaSuccess;
}

/*
 *  SHA1Input
 *
 *  Description:
 *      This function accepts an array of octets as the next portion
 *      of the message.
 *
 *  Parameters:
 *      message_array: [in]
 *          An array of characters representing the next portion of
 *          the message.
 *      length: [in]
 *          The length of the message in message_array
 *
 *  Returns:
 *      sha Error Code.
 *
 */
int SHA1::SHA1Input(const U8  *message_array, unsigned length)
{
    if (!length)
    {
        return shaSuccess;
    }

    if (!message_array)
    {
        return shaNull;
    }

    if (Computed)
    {
        Corrupted = shaStateError;

        return shaStateError;
    }

    if (Corrupted)
    {
         return Corrupted;
    }
    while(length-- && !Corrupted)
    {
    Message_Block[Message_Block_Index++] =
                    (*message_array & 0xFF);

    Length_Low += 8;
    if (Length_Low == 0)
    {
        Length_High++;
        if (Length_High == 0)
        {
            /* Message is too long */
            Corrupted = 1;
        }
    }

    if (Message_Block_Index == 64)
    {
        SHA1ProcessMessageBlock();
    }

    message_array++;
    }

    return shaSuccess;
}

/*
 *  SHA1ProcessMessageBlock
 *
 *  Description:
 *      This function will process the next 512 bits of the message
 *      stored in the Message_Block array.
 *
 *  Parameters:
 *      None.
 *
 *  Returns:
 *      Nothing.
 *
 *  Comments:

 *      Many of the variable names in this code, especially the
 *      single character names, were used because those were the
 *      names used in the publication.
 *
 *
 */
void SHA1::SHA1ProcessMessageBlock()
{
    const U32 K[] =    {       /* Constants defined in SHA-1   */
                            0x5A827999,
                            0x6ED9EBA1,
                            0x8F1BBCDC,
                            0xCA62C1D6
                            };
    int      t;                 /* Loop counter                */
    U32      temp;              /* Temporary word value        */
    U32      W[80];             /* Word sequence               */
    U32      A, B, C, D, E;     /* Word buffers                */

    /*
     *  Initialize the first 16 words in the array W
     */
    for(t = 0; t < 16; t++)
    {
        W[t] = Message_Block[t * 4] << 24;
        W[t] |= Message_Block[t * 4 + 1] << 16;
        W[t] |= Message_Block[t * 4 + 2] << 8;
        W[t] |= Message_Block[t * 4 + 3];
    }

    for(t = 16; t < 80; t++)
    {
       W[t] = SHA1CircularShift(1,W[t-3] ^ W[t-8] ^ W[t-14] ^ W[t-16]);
    }

    A = Intermediate_Hash[0];
    B = Intermediate_Hash[1];
    C = Intermediate_Hash[2];
    D = Intermediate_Hash[3];
    E = Intermediate_Hash[4];

    for(t = 0; t < 20; t++)
    {
        temp =  SHA1CircularShift(5,A) +
                ((B & C) | ((~B) & D)) + E + W[t] + K[0];
        E = D;
        D = C;
        C = SHA1CircularShift(30,B);

        B = A;
        A = temp;
    }

    for(t = 20; t < 40; t++)
    {
        temp = SHA1CircularShift(5,A) + (B ^ C ^ D) + E + W[t] + K[1];
        E = D;
        D = C;
        C = SHA1CircularShift(30,B);
        B = A;
        A = temp;
    }

    for(t = 40; t < 60; t++)
    {
        temp = SHA1CircularShift(5,A) +
               ((B & C) | (B & D) | (C & D)) + E + W[t] + K[2];
        E = D;
        D = C;
        C = SHA1CircularShift(30,B);
        B = A;
        A = temp;
    }

    for(t = 60; t < 80; t++)
    {
        temp = SHA1CircularShift(5,A) + (B ^ C ^ D) + E + W[t] + K[3];
        E = D;
        D = C;
        C = SHA1CircularShift(30,B);
        B = A;
        A = temp;
    }

    Intermediate_Hash[0] += A;
    Intermediate_Hash[1] += B;
    Intermediate_Hash[2] += C;
    Intermediate_Hash[3] += D;
    Intermediate_Hash[4] += E;

    Message_Block_Index = 0;
}

/*
 *  SHA1PadMessage
 *

 *  Description:
 *      According to the standard, the message must be padded to an even
 *      512 bits.  The first padding bit must be a '1'.  The last 64
 *      bits represent the length of the original message.  All bits in
 *      between should be 0.  This function will pad the message
 *      according to those rules by filling the Message_Block array
 *      accordingly.  It will also call the ProcessMessageBlock function
 *      provided appropriately.  When it returns, it can be assumed that
 *      the message digest has been computed.
 *
 *  Parameters:
 *      ProcessMessageBlock: [in]
 *          The appropriate SHA*ProcessMessageBlock function
 *  Returns:
 *      Nothing.
 *
 */

void SHA1::SHA1PadMessage()
{
    /*
     *  Check to see if the current message block is too small to hold
     *  the initial padding bits and length.  If so, we will pad the
     *  block, process it, and then continue padding into a second
     *  block.
     */
    if (Message_Block_Index > 55)
    {
        Message_Block[Message_Block_Index++] = 0x80;
        while(Message_Block_Index < 64)
        {
            Message_Block[Message_Block_Index++] = 0;
        }

        SHA1ProcessMessageBlock();

        while(Message_Block_Index < 56)
        {
            Message_Block[Message_Block_Index++] = 0;
        }
    }
    else
    {
        Message_Block[Message_Block_Index++] = 0x80;
        while(Message_Block_Index < 56)
        {

            Message_Block[Message_Block_Index++] = 0;
        }
    }

    /*
     *  Store the message length as the last 8 octets
     */
    Message_Block[56] = Length_High >> 24;
    Message_Block[57] = Length_High >> 16;
    Message_Block[58] = Length_High >> 8;
    Message_Block[59] = Length_High;
    Message_Block[60] = Length_Low >> 24;
    Message_Block[61] = Length_Low >> 16;
    Message_Block[62] = Length_Low >> 8;
    Message_Block[63] = Length_Low;

    SHA1ProcessMessageBlock();
}

//////////////////////////// ZPAQL //////////////////////////////


// Symbolic constants, instruction size, and names
typedef enum {NONE,CONST,CM,ICM,MATCH,AVG,MIX2,MIX,ISSE,SSE,
  JT=39,JF=47,JMP=63,LJ=255,
  POST=256,PCOMP,END,IF,IFNOT,ELSE,ENDIF,DO,
  WHILE,UNTIL,FOREVER,IFL,IFNOTL,ELSEL,SEMICOLON} CompType;
static const int compsize[256]={0,2,3,2,3,4,6,6,3,5};
bool verbose=false;  // global: display lots of stuff?
bool quiet=false;    // global: display less stuff?
static const char* compname[]=
  {"","const","cm","icm","match","avg","mix2","mix","isse","sse",0};

#ifndef OPT
// Opcodes from ZPAQ spec, table 1, without operands (N, M)".
static const char* opcodelist[272]={
"error","a++",  "a--",  "a!",   "a=0",  "",     "",     "a=r",
"b<>a", "b++",  "b--",  "b!",   "b=0",  "",     "",     "b=r",
"c<>a", "c++",  "c--",  "c!",   "c=0",  "",     "",     "c=r",
"d<>a", "d++",  "d--",  "d!",   "d=0",  "",     "",     "d=r",
"*b<>a","*b++", "*b--", "*b!",  "*b=0", "",     "",     "jt",
"*c<>a","*c++", "*c--", "*c!",  "*c=0", "",     "",     "jf",
"*d<>a","*d++", "*d--", "*d!",  "*d=0", "",     "",     "r=a",
"halt", "out",  "",     "hash", "hashd","",     "",     "jmp",
"a=a",  "a=b",  "a=c",  "a=d",  "a=*b", "a=*c", "a=*d", "a=",
"b=a",  "b=b",  "b=c",  "b=d",  "b=*b", "b=*c", "b=*d", "b=",
"c=a",  "c=b",  "c=c",  "c=d",  "c=*b", "c=*c", "c=*d", "c=",
"d=a",  "d=b",  "d=c",  "d=d",  "d=*b", "d=*c", "d=*d", "d=",
"*b=a", "*b=b", "*b=c", "*b=d", "*b=*b","*b=*c","*b=*d","*b=",
"*c=a", "*c=b", "*c=c", "*c=d", "*c=*b","*c=*c","*c=*d","*c=",
"*d=a", "*d=b", "*d=c", "*d=d", "*d=*b","*d=*c","*d=*d","*d=",
"",     "",     "",     "",     "",     "",     "",     "",
"a+=a", "a+=b", "a+=c", "a+=d", "a+=*b","a+=*c","a+=*d","a+=",
"a-=a", "a-=b", "a-=c", "a-=d", "a-=*b","a-=*c","a-=*d","a-=",
"a*=a", "a*=b", "a*=c", "a*=d", "a*=*b","a*=*c","a*=*d","a*=",
"a/=a", "a/=b", "a/=c", "a/=d", "a/=*b","a/=*c","a/=*d","a/=",
"a%=a", "a%=b", "a%=c", "a%=d", "a%=*b","a%=*c","a%=*d","a%=",
"a&=a", "a&=b", "a&=c", "a&=d", "a&=*b","a&=*c","a&=*d","a&=",
"a&~a", "a&~b", "a&~c", "a&~d", "a&~*b","a&~*c","a&~*d","a&~",
"a|=a", "a|=b", "a|=c", "a|=d", "a|=*b","a|=*c","a|=*d","a|=",
"a^=a", "a^=b", "a^=c", "a^=d", "a^=*b","a^=*c","a^=*d","a^=",
"a<<=a","a<<=b","a<<=c","a<<=d","a<<=*b","a<<=*c","a<<=*d","a<<=",
"a>>=a","a>>=b","a>>=c","a>>=d","a>>=*b","a>>=*c","a>>=*d","a>>=",
"a==a", "a==b", "a==c", "a==d", "a==*b","a==*c","a==*d","a==",
"a<a",  "a<b",  "a<c",  "a<d",  "a<*b", "a<*c", "a<*d", "a<",
"a>a",  "a>b",  "a>c",  "a>d",  "a>*b", "a>*c", "a>*d", "a>",
"",     "",     "",     "",     "",     "",     "",     "",
"",     "",     "",     "",     "",     "",     "",     "lj",
"post", "pcomp","end",  "if",   "ifnot","else", "endif","do",
"while","until","forever","ifl","ifnotl","elsel",";",    0};
#endif


// Constructor
ZPAQL::ZPAQL() {
  cend=hbegin=hend=0;  // COMP and HCOMP locations
  a=b=c=d=f=pc=0;      // machine state
  output=0;
  sha1=0;
  select=0;
}

// Read header, return number of bytes read
int ZPAQL::read(Reader r) {

  // Get header size and allocate
  int hsize=r.get();
  hsize+=r.get()*256;
  header.resize(hsize+300);
  cend=hbegin=hend=0;
  header[cend++]=hsize&255;
  header[cend++]=hsize>>8;
  while (cend<7) header[cend++]=r.get(); // hh hm ph pm n

  // Read COMP
  int n=header[cend-1];
  for (int i=0; i<n; ++i) {
    int type=r.get();  // component type
    if (type==EOF) error("unexpected end of file");
    header[cend++]=type;  // component type
    int size=compsize[type];
    if (size<1) error("Invalid component type");
    if (cend+size>header.size()-8) error("COMP list too big");
    for (int j=1; j<size; ++j)
      header[cend++]=r.get();
  }
  if ((header[cend++]=r.get())!=0) error("missing COMP END");

  // Insert a guard gap and read HCOMP
  hbegin=hend=cend+128;
  while (hend<hsize+129) {
    assert(hend<header.size()-8);
    int op=r.get();
    if (op==EOF) error("unexpected end of file");
    header[hend++]=op;
  }
  if ((header[hend++]=r.get())!=0) error("missing HCOMP END");

  assert(cend>=7 && cend<header.size());
  assert(hbegin==cend+128 && hbegin<header.size());
  assert(hend>hbegin && hend<header.size());
  assert(hsize==header[0]+256*header[1]);
  assert(hsize==cend-2+hend-hbegin);
  return cend+hend-hbegin;
}

// Write header. Return number of bytes written.
int ZPAQL::write(FILE* out) {
  assert(out);
  assert(cend>=7 && cend<header.size());
  assert(hbegin==cend+128 && hbegin<header.size());
  assert(hend>hbegin && hend<header.size());
  assert(header[0]+256*header[1]==cend-2+hend-hbegin);
  fwrite(&header[0], 1, cend, out);
  fwrite(&header[hbegin], 1, hend-hbegin, out);
  return cend+hend-hbegin;
}

// Verify header matches zlist (select==1) or pzlist (select==2)
void ZPAQL::verify() {
#ifdef OPT
  if (select<1 || select>2) return;
  const U8* list=select==1?zlist:pzlist;
  int hsize=list[0]+256*list[1];
  if (hsize!=cend+hend-hbegin-2 || memcmp(&header[0], list, cend)
      || memcmp(&header[hbegin], list+cend, hend-hbegin))
    error("block header verify");
#endif
}

// Initialize machine state as HCOMP
void ZPAQL::inith() {
  assert(header.size()>6);
  init(header[2], header[3]); // hh, hm
}

// Initialize machine state as PCOMP
void ZPAQL::initp() {
  assert(header.size()>6);
  init(header[4], header[5]); // ph, pm
}

// Initialize machine state to run a program.
// Set select to nonzero if header matches anything in the cache
// or else add it.
void ZPAQL::init(int hbits, int mbits) {
  assert(header.size()>0);
  assert(h.size()==0);
  assert(m.size()==0);
  assert(cend>=7);
  assert(hbegin>=cend+128);
  assert(hend>=hbegin);
  assert(hend<header.size()-130);
  assert(header[0]+256*header[1]==cend-2+hend-hbegin);
  h.resize(1, hbits);
  m.resize(1, mbits);
  r.resize(256);
  a=b=c=d=pc=f=0;
}

// Run program on input by interpreting header
void ZPAQL::run0(U32 input) {
  assert(cend>6);
  assert(hbegin>=cend+128);
  assert(hend>=hbegin);
  assert(hend<header.size()-130);
  assert(m.size()>0);
  assert(h.size()>0);
  assert(header[0]+256*header[1]==cend+hend-hbegin-2);
  pc=hbegin;
  a=input;
#ifdef OPT
  error("no model");
#else
  while (execute()) ;
#endif
}

#ifndef OPT
// Execute program input and show progress
void ZPAQL::step(U32 input, bool ishex) {
  assert(cend>6);
  assert(hbegin>=cend+128);
  assert(hend>=hbegin);
  assert(hend<header.size()-130);
  assert(m.size()>0);
  assert(h.size()>0);
  pc=hbegin;
  a=input;
  printf("\n"
  "  pc   opcode  f      a          b      *b      c      *c      d         *d\n"
  "----- -------- - ---------- ---------- --- ---------- --- ---------- ----------\n");
  printf(ishex ?
    "               %d   %08X   %08X  %02X   %08X  %02X   %08X   %08X\n" :
    "               %d %10u %10u %3u %10u %3u %10u %10u\n",
    f, a, b, m(b), c, m(c), d, h(d));
  while (1) {
    assert(pc>=cend && pc<header.size());
    int op=header[pc];
    printf("%5d ", pc-hbegin);
    char inst[16];
    if (op==255)
      sprintf(inst, "%s %d", opcodelist[op], header[pc+1]+256*header[pc+2]);
    else if ((op&7)==7)
      sprintf(inst, "%s %d", opcodelist[op], header[pc+1]);
    else
      sprintf(inst, "%s", opcodelist[op]);
    printf("%-8s", inst);
    if (!execute()) break;
    printf(ishex ?
      " %d   %08X   %08X  %02X   %08X  %02X   %08X   %08X\n" :
      " %d %10u %10u %3u %10u %3u %10u %10u\n",
      f, a, b, m(b), c, m(c), d, h(d));
  }

  // Print R, skipping rows of 4 zeros
  printf("\n\nR (size %d) = (rows of all 0 omitted)\n", r.size());
  for (int i=0; i<r.size(); i+=4) {
    if (r(i) || r(i+1) || r(i+2) || r(i+3))
      printf(ishex ? "%8X: %08X %08X %08X %08X\n"
                   : "%10u: %10u %10u %10u %10u\n",
        i, r(i), r(i+1), r(i+2), r(i+3));
  }

  // Print H, skipping rows of 4 zeros
  printf("\nH (size %d) = (rows of all 0 omitted)\n", h.size());
  for (int i=0; i<h.size(); i+=4) {
    if (h(i) || h(i+1) || h(i+2) || h(i+3))
      printf(ishex ? "%8X: %08X %08X %08X %08X\n"
                   : "%10u: %10u %10u %10u %10u\n",
        i, h(i), h(i+1), h(i+2), h(i+3));
  }

  // Print M, skipping rows of 16 zeros
  printf("\nM (size %d) = (rows of all 0 omitted)\n", m.size());
  for (int i=0; i<m.size(); i+=16) {
    bool found=false;
    for (int j=0; j<16; ++j)
      if (m(i+j)) found=true;
    if (found) {
      printf(ishex ? "%8X:" : "%10u:", i);
      for (int j=0; j<16; ++j) {
        printf(ishex ? " %02X" : " %3d", m(i+j));
        if (j%4==3) printf(" ");
      }
      printf("\n");
    }
  }
  printf("\n\n");
}

// Return memory requirement in bytes
double ZPAQL::memory() {
  double mem=pow(2.0,header[2]+2)+pow(2.0,header[3])  // hh hm
            +pow(2.0,header[4]+2)+pow(2.0,header[5])  // ph pm
            +header.size();
  int cp=7;  // start of comp list
  for (int i=0; i<header[6]; ++i) {  // n
    assert(cp<cend);
    double size=pow(2.0, header[cp+1]); // sizebits
    switch(header[cp]) {
      case CM: mem+=4*size; break;
      case ICM: mem+=64*size+1024; break;
      case MATCH: mem+=4*size+pow(2.0, header[cp+2]); break; // bufbits
      case MIX2: mem+=2*size; break;
      case MIX: mem+=4*size*header[cp+3]; break; // m
      case ISSE: mem+=64*size+2048; break;
      case SSE: mem+=128*size; break;
    }
    cp+=compsize[header[cp]];
  }
  return mem;
}
#endif

#ifndef OPT
// Execute one instruction, return 0 after HALT else 1
inline int ZPAQL::execute() {

/* Switch statement below generared by the PERL script shown here.
   The input is a 256 byte text file pasted from table 1 of the ZPAQ spec
   with one opcode per line.

#!/usr/bin/perl
# Generate ZPAQL interpreter from ZPAQ1.PDF table 1
$go="pc+=(header[pc]+128&255)-127";
$code=-1;
print"  switch(header[pc++]) {\n";
while (<>) {
 chomp;
 $code++;
 if ($_ ne "") {
  $comment=$_;
  s/ N$/N/;
  if    (/^([ABCD])(=)(R)/) {($a,$op,$b)=($1,$2,$3);}
  elsif (/^(R)(=)(A)/) {($a,$op,$b)=($1,$2,$3);}
  elsif (/^(\*?[ABCD])(\W*)(\*[ABCDN0])$/) {($a,$op,$b)=($1,$2,$3);}
  elsif (/^(\*?[ABCD])(\W*)([ABCDN0])$/) {($a,$op,$b)=($1,$2,$3);}
  elsif (/^(\*?[ABCD])(\W*)$/) {($a,$op,$b)=($1,$2);}
  else {($a,$op,$b)=($_);}
  $a=~tr/A-Z/a-z/;
  $b=~tr/A-Z/a-z/;
  $a=~s/\*([bc])/m($1)/;
  $b=~s/\*([bc])/m($1)/;
  $a=~s/\*d/h(d)/;
  $b=~s/\*d/h(d)/;
  $b=~s/n/header[pc++]/;
  $op=~s/&~/&= ~/;
  $a=~s/error//;
  $a=~s/halt/return 0/;
  print("    case $code: ");
  if ($a eq "jtn") {print"if (f) $go; else ++pc;";}
  elsif ($a eq "lj n m") {print"if((pc=hbegin+header[pc]+256*header[pc+1])>=hend)err();";}
  elsif ($a eq "jfn") {print"if (!f) $go; else ++pc;";}
  elsif ($a eq "jmpn") {print"$go;";}
  elsif ($a eq "out") {print"if (output) putc(a, output); if (sha1) sha1->put(a);";}
  elsif ($a eq "hash") {print"a = (a+m(b)+512)*773;"}
  elsif ($a eq "hashd") {print"h(d) = (h(d)+a+512)*773;"}
  elsif ($op eq "<>") {print"swap($a);";}
  elsif ($op eq "==" || $op eq "<" || $op eq ">") {print"f = ($a $op $b);";}
  elsif ($op eq "++" || $op eq "--") {print"$op$a;";}
  elsif ($op eq "!") {print"$a = ~$a;";}
  elsif ($op eq ".=") {print"$a = ($a<<8)+$b;";}
  elsif ($op eq "/=") {print"div($b);";}
  elsif ($op eq "%=") {print"mod($b);";}
  elsif ($b eq "r") {print"$a = r[header[pc++]];";}
  elsif ($a eq "r") {print"r[header[pc++]] = $b;";}
  elsif ($a) {print("$a $op $b;");}
  else {print"err();";}
  if ($a ne "return 0") {print" break;"}
  if ($comment eq "") {$comment="undefined";}
  print" // $comment\n";
 }
}
print"    default: err();\n  }\n";

*/
  switch(header[pc++]) {
    case 0: err(); break; // ERROR
    case 1: ++a; break; // A++
    case 2: --a; break; // A--
    case 3: a = ~a; break; // A!
    case 4: a = 0; break; // A=0
    case 7: a = r[header[pc++]]; break; // A=R N
    case 8: swap(b); break; // B<>A
    case 9: ++b; break; // B++
    case 10: --b; break; // B--
    case 11: b = ~b; break; // B!
    case 12: b = 0; break; // B=0
    case 15: b = r[header[pc++]]; break; // B=R N
    case 16: swap(c); break; // C<>A
    case 17: ++c; break; // C++
    case 18: --c; break; // C--
    case 19: c = ~c; break; // C!
    case 20: c = 0; break; // C=0
    case 23: c = r[header[pc++]]; break; // C=R N
    case 24: swap(d); break; // D<>A
    case 25: ++d; break; // D++
    case 26: --d; break; // D--
    case 27: d = ~d; break; // D!
    case 28: d = 0; break; // D=0
    case 31: d = r[header[pc++]]; break; // D=R N
    case 32: swap(m(b)); break; // *B<>A
    case 33: ++m(b); break; // *B++
    case 34: --m(b); break; // *B--
    case 35: m(b) = ~m(b); break; // *B!
    case 36: m(b) = 0; break; // *B=0
    case 39: if (f) pc+=((header[pc]+128)&255)-127; else ++pc; break; // JT N
    case 40: swap(m(c)); break; // *C<>A
    case 41: ++m(c); break; // *C++
    case 42: --m(c); break; // *C--
    case 43: m(c) = ~m(c); break; // *C!
    case 44: m(c) = 0; break; // *C=0
    case 47: if (!f) pc+=((header[pc]+128)&255)-127; else ++pc; break; // JF N
    case 48: swap(h(d)); break; // *D<>A
    case 49: ++h(d); break; // *D++
    case 50: --h(d); break; // *D--
    case 51: h(d) = ~h(d); break; // *D!
    case 52: h(d) = 0; break; // *D=0
    case 55: r[header[pc++]] = a; break; // R=A N
    case 56: return 0  ; // HALT
    case 57: if (output) putc(a, output); if (sha1) sha1->put(a); break; // OUT
    case 59: a = (a+m(b)+512)*773; break; // HASH
    case 60: h(d) = (h(d)+a+512)*773; break; // HASHD
    case 63: pc+=((header[pc]+128)&255)-127; break; // JMP N
    case 64: a = a; break; // A=A
    case 65: a = b; break; // A=B
    case 66: a = c; break; // A=C
    case 67: a = d; break; // A=D
    case 68: a = m(b); break; // A=*B
    case 69: a = m(c); break; // A=*C
    case 70: a = h(d); break; // A=*D
    case 71: a = header[pc++]; break; // A= N
    case 72: b = a; break; // B=A
    case 73: b = b; break; // B=B
    case 74: b = c; break; // B=C
    case 75: b = d; break; // B=D
    case 76: b = m(b); break; // B=*B
    case 77: b = m(c); break; // B=*C
    case 78: b = h(d); break; // B=*D
    case 79: b = header[pc++]; break; // B= N
    case 80: c = a; break; // C=A
    case 81: c = b; break; // C=B
    case 82: c = c; break; // C=C
    case 83: c = d; break; // C=D
    case 84: c = m(b); break; // C=*B
    case 85: c = m(c); break; // C=*C
    case 86: c = h(d); break; // C=*D
    case 87: c = header[pc++]; break; // C= N
    case 88: d = a; break; // D=A
    case 89: d = b; break; // D=B
    case 90: d = c; break; // D=C
    case 91: d = d; break; // D=D
    case 92: d = m(b); break; // D=*B
    case 93: d = m(c); break; // D=*C
    case 94: d = h(d); break; // D=*D
    case 95: d = header[pc++]; break; // D= N
    case 96: m(b) = a; break; // *B=A
    case 97: m(b) = b; break; // *B=B
    case 98: m(b) = c; break; // *B=C
    case 99: m(b) = d; break; // *B=D
    case 100: m(b) = m(b); break; // *B=*B
    case 101: m(b) = m(c); break; // *B=*C
    case 102: m(b) = h(d); break; // *B=*D
    case 103: m(b) = header[pc++]; break; // *B= N
    case 104: m(c) = a; break; // *C=A
    case 105: m(c) = b; break; // *C=B
    case 106: m(c) = c; break; // *C=C
    case 107: m(c) = d; break; // *C=D
    case 108: m(c) = m(b); break; // *C=*B
    case 109: m(c) = m(c); break; // *C=*C
    case 110: m(c) = h(d); break; // *C=*D
    case 111: m(c) = header[pc++]; break; // *C= N
    case 112: h(d) = a; break; // *D=A
    case 113: h(d) = b; break; // *D=B
    case 114: h(d) = c; break; // *D=C
    case 115: h(d) = d; break; // *D=D
    case 116: h(d) = m(b); break; // *D=*B
    case 117: h(d) = m(c); break; // *D=*C
    case 118: h(d) = h(d); break; // *D=*D
    case 119: h(d) = header[pc++]; break; // *D= N
    case 128: a += a; break; // A+=A
    case 129: a += b; break; // A+=B
    case 130: a += c; break; // A+=C
    case 131: a += d; break; // A+=D
    case 132: a += m(b); break; // A+=*B
    case 133: a += m(c); break; // A+=*C
    case 134: a += h(d); break; // A+=*D
    case 135: a += header[pc++]; break; // A+= N
    case 136: a -= a; break; // A-=A
    case 137: a -= b; break; // A-=B
    case 138: a -= c; break; // A-=C
    case 139: a -= d; break; // A-=D
    case 140: a -= m(b); break; // A-=*B
    case 141: a -= m(c); break; // A-=*C
    case 142: a -= h(d); break; // A-=*D
    case 143: a -= header[pc++]; break; // A-= N
    case 144: a *= a; break; // A*=A
    case 145: a *= b; break; // A*=B
    case 146: a *= c; break; // A*=C
    case 147: a *= d; break; // A*=D
    case 148: a *= m(b); break; // A*=*B
    case 149: a *= m(c); break; // A*=*C
    case 150: a *= h(d); break; // A*=*D
    case 151: a *= header[pc++]; break; // A*= N
    case 152: div(a); break; // A/=A
    case 153: div(b); break; // A/=B
    case 154: div(c); break; // A/=C
    case 155: div(d); break; // A/=D
    case 156: div(m(b)); break; // A/=*B
    case 157: div(m(c)); break; // A/=*C
    case 158: div(h(d)); break; // A/=*D
    case 159: div(header[pc++]); break; // A/= N
    case 160: mod(a); break; // A%=A
    case 161: mod(b); break; // A%=B
    case 162: mod(c); break; // A%=C
    case 163: mod(d); break; // A%=D
    case 164: mod(m(b)); break; // A%=*B
    case 165: mod(m(c)); break; // A%=*C
    case 166: mod(h(d)); break; // A%=*D
    case 167: mod(header[pc++]); break; // A%= N
    case 168: a &= a; break; // A&=A
    case 169: a &= b; break; // A&=B
    case 170: a &= c; break; // A&=C
    case 171: a &= d; break; // A&=D
    case 172: a &= m(b); break; // A&=*B
    case 173: a &= m(c); break; // A&=*C
    case 174: a &= h(d); break; // A&=*D
    case 175: a &= header[pc++]; break; // A&= N
    case 176: a &= ~ a; break; // A&~A
    case 177: a &= ~ b; break; // A&~B
    case 178: a &= ~ c; break; // A&~C
    case 179: a &= ~ d; break; // A&~D
    case 180: a &= ~ m(b); break; // A&~*B
    case 181: a &= ~ m(c); break; // A&~*C
    case 182: a &= ~ h(d); break; // A&~*D
    case 183: a &= ~ header[pc++]; break; // A&~ N
    case 184: a |= a; break; // A|=A
    case 185: a |= b; break; // A|=B
    case 186: a |= c; break; // A|=C
    case 187: a |= d; break; // A|=D
    case 188: a |= m(b); break; // A|=*B
    case 189: a |= m(c); break; // A|=*C
    case 190: a |= h(d); break; // A|=*D
    case 191: a |= header[pc++]; break; // A|= N
    case 192: a ^= a; break; // A^=A
    case 193: a ^= b; break; // A^=B
    case 194: a ^= c; break; // A^=C
    case 195: a ^= d; break; // A^=D
    case 196: a ^= m(b); break; // A^=*B
    case 197: a ^= m(c); break; // A^=*C
    case 198: a ^= h(d); break; // A^=*D
    case 199: a ^= header[pc++]; break; // A^= N
    case 200: a <<= (a&31); break; // A<<=A
    case 201: a <<= (b&31); break; // A<<=B
    case 202: a <<= (c&31); break; // A<<=C
    case 203: a <<= (d&31); break; // A<<=D
    case 204: a <<= (m(b)&31); break; // A<<=*B
    case 205: a <<= (m(c)&31); break; // A<<=*C
    case 206: a <<= (h(d)&31); break; // A<<=*D
    case 207: a <<= (header[pc++]&31); break; // A<<= N
    case 208: a >>= (a&31); break; // A>>=A
    case 209: a >>= (b&31); break; // A>>=B
    case 210: a >>= (c&31); break; // A>>=C
    case 211: a >>= (d&31); break; // A>>=D
    case 212: a >>= (m(b)&31); break; // A>>=*B
    case 213: a >>= (m(c)&31); break; // A>>=*C
    case 214: a >>= (h(d)&31); break; // A>>=*D
    case 215: a >>= (header[pc++]&31); break; // A>>= N
    case 216: f = (a == a); break; // A==A
    case 217: f = (a == b); break; // A==B
    case 218: f = (a == c); break; // A==C
    case 219: f = (a == d); break; // A==D
    case 220: f = (a == U32(m(b))); break; // A==*B
    case 221: f = (a == U32(m(c))); break; // A==*C
    case 222: f = (a == h(d)); break; // A==*D
    case 223: f = (a == U32(header[pc++])); break; // A== N
    case 224: f = (a < a); break; // A<A
    case 225: f = (a < b); break; // A<B
    case 226: f = (a < c); break; // A<C
    case 227: f = (a < d); break; // A<D
    case 228: f = (a < U32(m(b))); break; // A<*B
    case 229: f = (a < U32(m(c))); break; // A<*C
    case 230: f = (a < h(d)); break; // A<*D
    case 231: f = (a < U32(header[pc++])); break; // A< N
    case 232: f = (a > a); break; // A>A
    case 233: f = (a > b); break; // A>B
    case 234: f = (a > c); break; // A>C
    case 235: f = (a > d); break; // A>D
    case 236: f = (a > U32(m(b))); break; // A>*B
    case 237: f = (a > U32(m(c))); break; // A>*C
    case 238: f = (a > h(d)); break; // A>*D
    case 239: f = (a > U32(header[pc++])); break; // A> N
    case 255: if((pc=hbegin+header[pc]+256*header[pc+1])>=hend)err();break;//LJ
    default: err();
  }
  return 1;
}
#endif

// Print illegal instruction error message and exit
void ZPAQL::err() {
  --pc;
  fprintf(stderr, "\nExecution aborted: pc=%d a=%d b=%d->%d c=%d->%d d=%d->%d\n",
    pc-hbegin, a, b, m(b), c, m(c), d, h(d));
  if (pc>=hbegin && pc<hend) fprintf(stderr, "opcode = %d\n",
    header[pc-hbegin]);
  else
    fprintf(stderr, "pc out of range. Program size is %d\n", hend-hbegin);
  exit(1);
}

//////////////////////////////// compile ///////////////////////////

#ifndef OPT
int args[9]={0};  // global configuration file arguments

// Read a token and return it, or return 0 at EOF. Skip (comments).
// Convert to lower case. Tokens are separated by white space.
// In verbose mode, print the token.
const char* token(FILE* in, bool lowercase=true) {
  static char s[512];  // result
  int len=0;  // length of s

  // skip to start of token
  int paren=0, c=0;
  while (c<=' ' || paren>0) {
    c=getc(in);
    if (c=='(') ++paren;
    if (c==')') --paren, c=' ';
    if (c==EOF) return 0;
  }

  // read token separated by whitespace
  do {
    if (lowercase && isupper(c)) c=tolower(c);
    s[len++]=c;
  }
  while (len<511 && (c=getc(in))!=EOF && c>' ');
  s[len++]=0;
  if (verbose) printf("%s ", s);

  // Substitute parameters $1..$9 with args[0..8], $i+n with args[i-1]+n
  if (s[0]=='$' && s[1]>='1' && s[1]<='9') {
    int i=s[1]-'1';
    assert(i>=0 && i<9);
    int val=args[i];
    if (s[2]=='+')
      val+=atoi(s+3);
    sprintf(s, "%d", val);
    if (verbose) printf("(%s) ", s);
  }
  return s;
}

// Read a token, which must be in the NULL terminated list or else
// exit with an error. If found, return its index.
int rtoken(FILE* in, const char* list[]) {
  assert(in);
  assert(list);
  const char* tok=token(in);
  if (!tok) fprintf(stderr, "\nUnexpected end of configuration file\n"), exit(1);
  for (int i=0; list[i]; ++i)
    if (!strcmp(list[i], tok))
      return i;
  fprintf(stderr, "\nConfiguration file error at %s\n", tok), exit(1);
  assert(0);
  return -1; // not reached
}

// Read a token which must be the specified value s
void rtoken(FILE* in, const char* s) {
  assert(s);
  const char* t=token(in);
  if (!t) fprintf(stderr, "\nExpected %s, found EOF\n", s), exit(1);
  if (strcmp(s, t)) fprintf(stderr, "\nExpected %s, found %s\n", s, t), exit(1);
}

// Read a number in (low...high) or exit with an error
int rtoken(FILE* in, int low, int high) {
  const char* tok=token(in);
  if (!tok) fprintf(stderr, "\nUnexpected end of configuration file\n"), exit(1);
  int n=0;
  const char* p=tok;
  int sign=1;
  if (*p=='-') sign=-1, ++p;
  while (*p) {
    if (isdigit(*p))
      n=n*10+*p-'0';
    else
      fprintf(stderr, "\nConfiguration file error at %s: expected a number\n", tok),
      exit(1);
    ++p;
  }
  n*=sign;
  if (n>=low && n<=high)
    return n;
  fprintf(stderr, "\nConfiguration file error: expected (%d...%d), found %d\n",
    low, high, n);
  exit(1);
  return 0;
}

// Stack of n elements of type T
template<class T>
class Stack {
  Array<T> s;
  int top;
public:
  Stack(int n): s(n), top(0) {}
  void push(const T& x) {
    if (top>=s.size()) error("stack full");
    s[top++]=x;
  }
  T pop() {
    if (top<=0) error("stack empty");
    return s[--top];
  }
};

// Compile HCOMP or PCOMP code. Exit on error. Return
// code for end token (POST, PCOMP, END)
CompType compile_comp(FILE *in, ZPAQL& z) {
  int op=0;
  Stack<U16> if_stack(1000), do_stack(1000);  // IF, DO saved addresses
  if (verbose) printf("\n");
  int indent=0;  // program listing indentation
  while (z.hend<0x10000) {
    if (verbose) {
      printf("(%4d) ", z.hend-z.hbegin);
      for (int i=0; i<indent; ++i) printf("  ");
    }
    op=rtoken(in, opcodelist);
    if (op==POST || op==PCOMP || op==END) break;
    int operand=-1; // 0...255 if 2 bytes
    int operand2=-1;  // 0...255 if 3 bytes
    if (op==IF) {
      op=JF;
      operand=0; // set later
      if_stack.push(z.hend+1); // save jump target location
      ++indent;
    }
    else if (op==IFNOT) {
      op=JT;
      operand=0;
      if_stack.push(z.hend+1); // save jump target location
      ++indent;
    }
    else if (op==IFL || op==IFNOTL) {  // long if
      if (op==IFL) z.header[z.hend++]=JT;
      if (op==IFNOTL) z.header[z.hend++]=JF;
      z.header[z.hend++]=3;
      op=LJ;
      operand=operand2=0;
      if_stack.push(z.hend+1);
      if (verbose)
        printf("(%s 3 (%d 3) lj 0 0)",
          opcodelist[z.header[z.hend-2]], z.header[z.hend-2]);
      ++indent;
    }
    else if (op==ELSE || op==ELSEL) {
      if (op==ELSE) op=JMP, operand=0;
      if (op==ELSEL) op=LJ, operand=operand2=0;
      int a=if_stack.pop();  // conditional jump target location
      assert(a>z.hbegin && a<z.hend);
      if (z.header[a-1]!=LJ) {  // IF, IFNOT
        assert(z.header[a-1]==JT || z.header[a-1]==JF || z.header[a-1]==JMP);
        int j=z.hend-a+1+(op==LJ); // offset at IF
        assert(j>=0);
        if (j>127) error("IF too big, try IFL, IFNOTL");
        z.header[a]=j;
        if (verbose) printf("((%d) %s %d (to %d)) ",
          a-z.hbegin-1, opcodelist[z.header[a-1]], j, z.hend-z.hbegin+2);
      }
      else {  // IFL, IFNOTL
        int j=z.hend-z.hbegin+2+(op==LJ);
        assert(j>=0);
        z.header[a]=j&255;
        z.header[a+1]=(j>>8)&255;
        if (verbose) printf("((%d) lj %d) ", a-z.hbegin-1, j);
      }
      if_stack.push(z.hend+1);  // save JMP target location
    }
    else if (op==ENDIF) {
      int a=if_stack.pop();  // jump target address
      assert(a>z.hbegin && a<z.hend);
      int j=z.hend-a-1;  // jump offset
      assert(j>=0);
      if (z.header[a-1]!=LJ) {
        assert(z.header[a-1]==JT || z.header[a-1]==JF || z.header[a-1]==JMP);
        if (j>127) error("IF too big, try IFL, IFNOTL, ELSEL\n");
        z.header[a]=j;
        if (verbose) printf("((%d) %s %d (to %d))\n",
          a-z.hbegin-1, opcodelist[z.header[a-1]], j, z.hend-z.hbegin);
      }
      else {
        j=z.hend-z.hbegin;
        z.header[a]=j&255;
        z.header[a+1]=(j>>8)&255;
        if (verbose) printf("((%d) lj %d)\n", a-1, j);
      }
      --indent;
    }
    else if (op==DO) {
      do_stack.push(z.hend);
      if (verbose) printf("\n");
      ++indent;
    }
    else if (op==WHILE || op==UNTIL || op==FOREVER) {
      int a=do_stack.pop();
      assert(a>=z.hbegin && a<z.hend);
      int j=a-z.hend-2;
      assert(j<=-2);
      if (j>=-127) {  // backward short jump
        if (op==WHILE) op=JT;
        if (op==UNTIL) op=JF;
        if (op==FOREVER) op=JMP;
        operand=j&255;
        if (verbose)
          printf("(%s %d (to %d)) ", opcodelist[op], j, z.hend-z.hbegin+2+j);
      }
      else {  // backward long jump
        j=a-z.hbegin;
        assert(j>=0 && j<z.hend-z.hbegin);
        if (op==WHILE) {
          z.header[z.hend++]=JF;
          z.header[z.hend++]=3;
          if (verbose) printf("(jf 3) ");
        }
        if (op==UNTIL) {
          z.header[z.hend++]=JT;
          z.header[z.hend++]=3;
          if (verbose) printf("(jt 3) ");
        }
        op=LJ;
        operand=j&255;
        operand2=j>>8;
        if (verbose) printf("(lj %d) ", j);
      }
      --indent;
    }
    else if ((op&7)==7) { // 2 byte operand, read N
      if (op==LJ) {
        operand=rtoken(in, 0, 65535);
        operand2=operand>>8;
        operand&=255;
        if (verbose) printf("(to %d) ", operand+256*operand2);
      }
      else if (op==JT || op==JF || op==JMP) {
        operand=rtoken(in, -128, 127);
        if (verbose) printf("(to %d) ", z.hend-z.hbegin+2+operand);
        operand&=255;
      }
      else
        operand=rtoken(in, 0, 255);
    }
    if (verbose) {
      if (operand2>=0)
        printf("(%d %d %d)\n", op, operand, operand2);
      else if (operand>=0)
        printf("(%d %d)\n", op, operand);
      else if (op>=0 && op<=255)
        printf("(%d)\n", op);
    }
    if (op>=0 && op<=255)
      z.header[z.hend++]=op;
    if (operand>=0)
      z.header[z.hend++]=operand;
    if (operand2>=0)
      z.header[z.hend++]=operand2;
    if (z.hend-z.hbegin>=0x10000 || z.hend>z.header.size()-144)
      error("program too big");
  }
  z.header[z.hend++]=0; // END
  return CompType(op);
}

// Compile a configuration file. Store COMP/HCOMP section in z.
// If there is a PCOMP section, store it in pz and store the PCOMP
// command in pcomp_cmd. Replace "$1..$9+n" with args[0..8]+n
void compile(FILE* in, ZPAQL& z, ZPAQL& pz, Array<char>& pcomp_cmd,
             int args[]) {

  // Allocate header
  z.header.resize(0x11000);
 
  // Compile the COMP section of header
  z.cend=z.hbegin=z.hend=2;
  rtoken(in, "comp");
  z.header[z.cend++]=rtoken(in, 0, 255); // hh
  z.header[z.cend++]=rtoken(in, 0, 255); // hm
  z.header[z.cend++]=rtoken(in, 0, 255); // ph
  z.header[z.cend++]=rtoken(in, 0, 255); // pm
  int n=z.header[z.cend++]=rtoken(in, 0, 255); // n
  if (verbose) printf("\n");
  for (int i=0; i<n; ++i) {
    if (verbose) printf("  ");
    rtoken(in, i, i);
    CompType type=CompType(z.header[z.cend++]=rtoken(in, compname));
    int clen=compsize[type];
    assert(clen>0 && clen<10);
    for (int j=1; j<clen; ++j)
      z.header[z.cend++]=rtoken(in, 0, 255);
    if (verbose) printf("\n");
  }
  z.header[z.cend++]=0; // END

  // Compile HCOMP
  z.hbegin=z.hend=z.cend+128;  // leave a guard gap to catch backwards jumps
  rtoken(in, "hcomp");
  CompType op=compile_comp(in, z);
  if (verbose) printf("\n");
  if (z.hend>=0x10000) printf("\nProgram too big\n"), exit(1);

  // Compute header size
  int hsize=z.hend-z.hbegin+z.cend-2;
  z.header[0]=hsize&255;
  z.header[1]=hsize>>8;

  // Compile POST 0 END
  if (op==POST) {
    rtoken(in, 0, 0);
    rtoken(in, "end");
  }

  // Compile PCOMP pcomp_cmd\n program... END
  else if (op==PCOMP) {
    pz.header.resize(0x10300);
    pz.header[4]=z.header[4]; // copy ph
    pz.header[5]=z.header[5]; // copy pm
    pz.cend=8;  // empty COMP section

    // get pcomp_cmd ending with ";" (case sensitive)
    const char *tok;
    while ((tok=token(in, false))!=0 && strcmp(tok, ";")) {
      if (pcomp_cmd.size() && pcomp_cmd[0]) append(pcomp_cmd, " ");
      append(pcomp_cmd, tok);
    }
    pz.hbegin=pz.hend=pz.cend+128;
    op=compile_comp(in, pz);
    if (op!=END)
      error("Expected END in configuation file");

    // Compute header size
    int hsize=pz.hend-pz.hbegin+pz.cend-2;
    pz.header[0]=hsize&255;
    pz.header[1]=hsize>>8;
  }
}
#endif // ifndef OPT

///////////////////////////// Predictor ///////////////////////////

Component::Component(): limit(0), cxt(0), a(0), b(0), c(0) {}

U8 StateTable::ns[1024]={0};
const int StateTable::bound[B]={20,48,15,8,6,5}; // n0 -> max n1, n1 -> max n0

// How many states with count of n0 zeros, n1 ones (0...2)
int StateTable::num_states(int n0, int n1) {
  if (n0<n1) return num_states(n1, n0);
  if (n0<0 || n1<0 || n0>=N || n1>=N || n1>=B || n0>bound[n1]) return 0;
  return 1+(n1>0 && n0+n1<=17);
}

// New value of count n0 if 1 is observed (and vice versa)
void StateTable::discount(int& n0) {
  n0=(n0>=1)+(n0>=2)+(n0>=3)+(n0>=4)+(n0>=5)+(n0>=7)+(n0>=8);
}

// compute next n0,n1 (0 to N) given input y (0 or 1)
void StateTable::next_state(int& n0, int& n1, int y) {
  if (n0<n1)
    next_state(n1, n0, 1-y);
  else {
    if (y) {
      ++n1;
      discount(n0);
    }
    else {
      ++n0;
      discount(n1);
    }
    // 20,0,0 -> 20,0
    // 48,1,0 -> 48,1
    // 15,2,0 -> 8,1
    //  8,3,0 -> 6,2
    //  8,3,1 -> 5,3
    //  6,4,0 -> 5,3
    //  5,5,0 -> 5,4
    //  5,5,1 -> 4,5
    while (!num_states(n0, n1)) {
      if (n1<2) --n0;
      else {
        n0=(n0*(n1-1)+(n1/2))/n1;
        --n1;
      }
    }
  }
}

// Initialize next state table ns[state*4] -> next if 0, next if 1, n0, n1
StateTable::StateTable() {

  // Assign states by increasing priority
  U8 t[N][N][2]={{{0}}}; // (n0,n1,y) -> state number
  int state=0;
  for (int i=0; i<N; ++i) {
    for (int n1=0; n1<=i; ++n1) {
      int n0=i-n1;
      int n=num_states(n0, n1);
      assert(n>=0 && n<=2);
      if (n) {
        t[n0][n1][0]=state;
        t[n0][n1][1]=state+n-1;
        state+=n;
      }
    }
  }
       
  // Generate next state table
  for (int n0=0; n0<N; ++n0) {
    for (int n1=0; n1<N; ++n1) {
      for (int y=0; y<num_states(n0, n1); ++y) {
        int s=t[n0][n1][y];
        assert(s>=0 && s<256);
        int s0=n0, s1=n1;
        next_state(s0, s1, 0);
        assert(s0>=0 && s0<N && s1>=0 && s1<N);
        ns[s*4+0]=t[s0][s1][0];
        s0=n0, s1=n1;
        next_state(s0, s1, 1);
        assert(s0>=0 && s0<N && s1>=0 && s1<N);
        ns[s*4+1]=t[s0][s1][1];
        ns[s*4+2]=n0;
        ns[s*4+3]=n1;
      }
    }
  }
}

// Print component statistics
void Predictor::stat() {
  printf("\nMemory utilization:\n");
  int cp=7;
  for (int i=0; i<z.header[6]; ++i) {
    assert(cp<z.header.size());
    int type=z.header[cp];
    assert(compsize[type]>0);
    printf("%2d %s", i, compname[type]);
    for (int j=1; j<compsize[type]; ++j)
      printf(" %d", z.header[cp+j]);
    Component& cr=comp[i];
    if (type==MATCH) {
      assert(cr.cm.size()>0);
      assert(cr.ht.size()>0);
      int count=0;
      for (int j=0; j<cr.cm.size(); ++j)
        if (cr.cm[j]) ++count;
      printf(": buffer=%d/%d index=%d/%d (%1.2f%%)", cr.limit/8, cr.ht.size(),
        count, cr.cm.size(), count*100.0/cr.cm.size());
    }
    else if (type==SSE) {
      assert(cr.cm.size()>0);
      int count=0;
      for (int j=0; j<cr.cm.size(); ++j) {
        if (int(cr.cm[j])!=(squash((j&31)*64-992)<<17|z.header[cp+3]))
          ++count;
      }
      printf(": %d/%d (%1.2f%%)", count, cr.cm.size(),
        count*100.0/cr.cm.size());
    }
    else if (type==CM) {
      assert(cr.cm.size()>0);
      int count=0;
      for (int j=0; j<cr.cm.size(); ++j)
        if (cr.cm[j]!=0x80000000) ++count;
      printf(": %d/%d (%1.2f%%)", count, cr.cm.size(),
        count*100.0/cr.cm.size());
    }
    else if (type==MIX) {
      int count=0;
      int m=z.header[cp+3];
      assert(m>0);
      for (int j=0; j<cr.cm.size(); ++j)
        if (int(cr.cm[j])!=65536/m) ++count;
      printf(": %d/%d (%1.2f%%)", count, cr.cm.size(),
        count*100.0/cr.cm.size());
    }
    else if (type==MIX2) {
      int count=0;
      for (int j=0; j<cr.a16.size(); ++j)
        if (int(cr.a16[j])!=32768) ++count;
      printf(": %d/%d (%1.2f%%)", count, cr.a16.size(),
        count*100.0/cr.a16.size());
    }
    else if (cr.ht.size()>0) {
      int hcount=0;
      for (int j=0; j<cr.ht.size(); ++j)
        if (cr.ht[j]>0) ++hcount;
      printf(": %d/%d (%1.2f%%)",
          hcount, cr.ht.size(), hcount*100.0/cr.ht.size());
    }
    cp+=compsize[type];
    printf("\n");
  }
}     

// Initailize the model
Predictor::Predictor(ZPAQL& zr): c8(1), hmap4(1), z(zr) {
  assert(sizeof(U8)==1);
  assert(sizeof(U16)==2);
  assert(sizeof(U32)==4);
  assert(sizeof(short)==2);
  assert(sizeof(int)==4);

  // Initialize tables
  for (int i=0; i<1024; ++i)
    dt[i]=(1<<17)/(i*2+3)*2;
  for (int i=0; i<32768; ++i)
    stretcht[i]=int(log((i+0.5)/(32767.5-i))*64+0.5+100000)-100000;
  for (int i=0; i<4096; ++i)
    squasht[i]=int(32768.0/(1+exp((i-2048)*(-1.0/64))));

  // Verify floating point math for squash() and stretch()
  U32 sqsum=0, stsum=0;
  for (int i=32767; i>=0; --i)
    stsum=stsum*3+stretch(i);
  for (int i=4095; i>=0; --i)
    sqsum=sqsum*3+squash(i-2048);
  assert(stsum==3887533746u);
  assert(sqsum==2278286169u);

  // Initialize context hash function
  z.inith();

  // Initialize predictions
  for (int i=0; i<256; ++i) p[i]=0;

  // Initialize components
  int n=z.header[6]; // hsize[0..1] hh hm ph pm n (comp)[n] END 0[128] (hcomp) END
  if (n<1 || n>255) error("n must be 1..255 components");
  const U8* cp=&z.header[7];  // start of component list
  for (int i=0; i<n; ++i) {
    assert(cp<&z.header[z.cend]);
    assert(cp>&z.header[0] && cp<&z.header[z.header.size()-8]);
    Component& cr=comp[i];
    switch(cp[0]) {
      case CONST:  // c
        p[i]=(cp[1]-128)*4;
        break;
      case CM: // sizebits limit
        cr.cm.resize(1, cp[1]);  // packed CM (22 bits) + CMCOUNT (10 bits)
        cr.limit=cp[2]*4;
        for (int j=0; j<cr.cm.size(); ++j)
          cr.cm[j]=0x80000000;
        break;
      case ICM: // sizebits
        cr.limit=1023;
        cr.cm.resize(256);
        cr.ht.resize(64, cp[1]);
        for (int j=0; j<cr.cm.size(); ++j)
          cr.cm[j]=st.cminit(j);
        break;
      case MATCH:  // sizebits
        cr.cm.resize(1, cp[1]);  // index
        cr.ht.resize(1, cp[2]);  // buf
        cr.ht(0)=1;
        break;
      case AVG: // j k wt
        break;
      case MIX2:  // sizebits j k rate mask
        if (cp[3]>=i) error("MIX2 k >= i");
        if (cp[2]>=i) error("MIX2 j >= i");
        cr.c=(1<<cp[1]); // size (number of contexts)
        cr.a16.resize(1, cp[1]);  // wt[size][m]
        for (int j=0; j<cr.a16.size(); ++j)
          cr.a16[j]=32768;
        break;
      case MIX: {  // sizebits j m rate mask
        if (cp[2]>=i) error("MIX j >= i");
        if (cp[3]<1 || cp[3]>i-cp[2])
          error("MIX m not in 1..i-j");
        int m=cp[3];  // number of inputs
        assert(m>=1);
        cr.c=(1<<cp[1]); // size (number of contexts)
        cr.cm.resize(m, cp[1]);  // wt[size][m]
        for (int j=0; j<cr.cm.size(); ++j)
          cr.cm[j]=65536/m;
        break;
      }
      case ISSE:  // sizebits j
        if (cp[2]>=i) error("ISSE j >= i");
        cr.ht.resize(64, cp[1]);
        cr.cm.resize(512);
        for (int j=0; j<256; ++j) {
          cr.cm[j*2]=1<<15;
          cr.cm[j*2+1]=clamp512k(stretch(st.cminit(j)>>8)<<10);
        }
        break;
      case SSE: // sizebits j start limit
        if (cp[2]>=i) error("SSE j >= i");
        if (cp[3]>cp[4]*4) error("SSE start > limit*4");
        cr.cm.resize(32, cp[1]);
        cr.limit=cp[4]*4;
        for (int j=0; j<cr.cm.size(); ++j)
          cr.cm[j]=squash((j&31)*64-992)<<17|cp[3];
        break;
      default: error("unknown component type");
    }
    assert(compsize[*cp]>0);
    cp+=compsize[*cp];
    assert(cp>=&z.header[7] && cp<&z.header[z.cend]);
  }
}

int Predictor::predict0() {
  assert(c8>=1 && c8<=255);

#ifdef OPT
  error("no model");
  return 16384;
#else
  // Predict next bit
  int n=z.header[6];
  assert(n>0 && n<=255);
  const U8* cp=&z.header[7];
  assert(cp[-1]==n);
  for (int i=0; i<n; ++i) {
    assert(cp>&z.header[0] && cp<&z.header[z.header.size()-8]);
    Component& cr=comp[i];
    switch(cp[0]) {
      case CONST:  // c
        break;
      case CM:  // sizebits limit
        cr.cxt=z.H(i)^hmap4;
        p[i]=stretch(cr.cm(cr.cxt)>>17);
        break;
      case ICM: // sizebits
        assert((hmap4&15)>0);
        if (c8==1 || (c8&0xf0)==16) cr.c=find(cr.ht, cp[1]+2, z.H(i)+16*c8);
        cr.cxt=cr.ht[cr.c+(hmap4&15)];
        p[i]=stretch(cr.cm(cr.cxt)>>8);
        break;
      case MATCH: // sizebits bufbits: a=len, b=offset, c=bit, cxt=256/len,
                  //                   ht=buf, limit=8*pos+bp
        assert(cr.a>=0 && cr.a<=255);
        if (cr.a==0) p[i]=0;
        else {
          cr.c=cr.ht((cr.limit>>3)-cr.b)>>(7-(cr.limit&7))&1; // predicted bit
          p[i]=stretch(cr.cxt*(cr.c*-2+1)&32767);
        }
        break;
      case AVG: // j k wt
        p[i]=(p[cp[1]]*cp[3]+p[cp[2]]*(256-cp[3]))>>8;
        break;
      case MIX2: { // sizebits j k rate mask
                   // c=size cm=wt[size][m] cxt=input
        cr.cxt=((z.H(i)+(c8&cp[5]))&(cr.c-1));
        assert(int(cr.cxt)>=0 && int(cr.cxt)<cr.a16.size());
        int w=cr.a16[cr.cxt];
        assert(w>=0 && w<65536);
        p[i]=(w*p[cp[2]]+(65536-w)*p[cp[3]])>>16;
        assert(p[i]>=-2048 && p[i]<2048);
      }
        break;
      case MIX: {  // sizebits j m rate mask
                   // c=size cm=wt[size][m] cxt=index of wt in cm
        int m=cp[3];
        assert(m>=1 && m<=i);
        cr.cxt=z.H(i)+(c8&cp[5]);
        cr.cxt=(cr.cxt&(cr.c-1))*m; // pointer to row of weights
        assert(int(cr.cxt)>=0 && int(cr.cxt)<=cr.cm.size()-m);
        int* wt=(int*)&cr.cm[cr.cxt];
        p[i]=0;
        for (int j=0; j<m; ++j)
          p[i]+=(wt[j]>>8)*p[cp[2]+j];
        p[i]=clamp2k(p[i]>>8);
      }
        break;
      case ISSE: { // sizebits j -- c=hi, cxt=bh
        assert((hmap4&15)>0);
        if (c8==1 || (c8&0xf0)==16)
          cr.c=find(cr.ht, cp[1]+2, z.H(i)+16*c8);
        cr.cxt=cr.ht[cr.c+(hmap4&15)];  // bit history
        int *wt=(int*)&cr.cm[cr.cxt*2];
        p[i]=clamp2k((wt[0]*p[cp[2]]+wt[1]*64)>>16);
      }
        break;
      case SSE: { // sizebits j start limit
        cr.cxt=(z.H(i)+c8)*32;
        int pq=p[cp[2]]+992;
        if (pq<0) pq=0;
        if (pq>1983) pq=1983;
        int wt=pq&63;
        pq>>=6;
        assert(pq>=0 && pq<=30);
        cr.cxt+=pq;
        p[i]=stretch(((cr.cm(cr.cxt)>>10)*(64-wt)+(cr.cm(cr.cxt+1)>>10)*wt)>>13);
        cr.cxt+=wt>>5;
      }
        break;
      default:
        error("component predict not implemented");
    }
    cp+=compsize[cp[0]];
    assert(cp<&z.header[z.cend]);
    assert(p[i]>=-2048 && p[i]<2048);
  }
  assert(cp[0]==NONE);
  return squash(p[n-1]);
#endif
}

// Update model with decoded bit y (0...1)
void Predictor::update0(int y) {
#ifdef OPT
  error("no model");
#else
  assert(y==0 || y==1);
  assert(c8>=1 && c8<=255);
  assert(hmap4>=1 && hmap4<=511);

  // Update components
  const U8* cp=&z.header[7];
  int n=z.header[6];
  assert(n>=1 && n<=255);
  assert(cp[-1]==n);
  for (int i=0; i<n; ++i) {
    Component& cr=comp[i];
    switch(cp[0]) {
      case CONST:  // c
        break;
      case CM:  // sizebits limit
        train(cr, y);
        break;
      case ICM: { // sizebits: cxt=ht[b]=bh, ht[c][0..15]=bh row, cxt=bh
        cr.ht[cr.c+(hmap4&15)]=st.next(cr.ht[cr.c+(hmap4&15)], y);
        U32& pn=cr.cm(cr.cxt);
        pn+=int(y*32767-(pn>>8))>>2;
      }
        break;
      case MATCH: // sizebits bufbits:
                  //   a=len, b=offset, c=bit, cm=index, cxt=256/len
                  //   ht=buf, limit=8*pos+bp
      {
        assert(cr.a>=0 && cr.a<=255);
        assert(cr.c==0 || cr.c==1);
        if (cr.c!=y) cr.a=0;  // mismatch?
        cr.ht(cr.limit>>3)+=cr.ht(cr.limit>>3)+y;
        if ((++cr.limit&7)==0) {
          int pos=cr.limit>>3;
          if (cr.a==0) {  // look for a match
            cr.b=pos-cr.cm(z.H(i));
            if (cr.b&(cr.ht.size()-1))
              while (cr.a<255 && cr.ht(pos-cr.a-1)==cr.ht(pos-cr.a-cr.b-1))
                ++cr.a;
          }
          else cr.a+=cr.a<255;
          cr.cm(z.H(i))=pos;
          if (cr.a>0) cr.cxt=2048/cr.a;
        }
      }
        break;
      case AVG:  // j k wt
        break;
      case MIX2: { // sizebits j k rate mask
                   // cm=input[2],wt[size][2], cxt=weight row
        assert(cr.a16.size()==cr.c);
        assert(int(cr.cxt)>=0 && int(cr.cxt)<cr.a16.size());
        int err=(y*32767-squash(p[i]))*cp[4]>>5;
        int w=cr.a16[cr.cxt];
        w+=(err*(p[cp[2]]-p[cp[3]])+(1<<12))>>13;
        if (w<0) w=0;
        if (w>65535) w=65535;
        cr.a16[cr.cxt]=w;
      }
        break;
      case MIX: {   // sizebits j m rate mask
                    // cm=wt[size][m], cxt=input
        int m=cp[3];
        assert(m>0 && m<=i);
        assert(cr.cm.size()==m*cr.c);
        assert(int(cr.cxt)>=0 && int(cr.cxt)<=cr.cm.size()-m);
        int err=(y*32767-squash(p[i]))*cp[4]>>4;
        int* wt=(int*)&cr.cm[cr.cxt];
        for (int j=0; j<m; ++j)
          wt[j]=clamp512k(wt[j]+((err*p[cp[2]+j]+(1<<12))>>13));
      }
        break;
      case ISSE: { // sizebits j  -- c=hi, cxt=bh
        assert(cr.cxt==cr.ht[cr.c+(hmap4&15)]);
        int err=y*32767-squash(p[i]);
        int *wt=(int*)&cr.cm[cr.cxt*2];
        wt[0]=clamp512k(wt[0]+((err*p[cp[2]]+(1<<12))>>13));
        wt[1]=clamp512k(wt[1]+((err+16)>>5));
        cr.ht[cr.c+(hmap4&15)]=st.next(cr.cxt, y);
      }
        break;
      case SSE:  // sizebits j start limit
        train(cr, y);
        break;
      default:
        assert(0);
    }
    cp+=compsize[cp[0]];
    assert(cp>=&z.header[7] && cp<&z.header[z.cend] 
           && cp<&z.header[z.header.size()-8]);
  }
  assert(cp[0]==NONE);

  // Save bit y in c8, hmap4
  c8+=c8+y;
  if (c8>=256) {
    z.run(c8-256);
    hmap4=1;
    c8=1;
  }
  else if (c8>=16 && c8<32)
    hmap4=(hmap4&0xf)<<5|y<<4|1;
  else
    hmap4=(hmap4&0x1f0)|(((hmap4&0xf)*2+y)&0xf);
#endif
}

// Find cxt row in hash table ht. ht has rows of 16 indexed by the
// low sizebits of cxt with element 0 having the next higher 8 bits for
// collision detection. If not found after 3 adjacent tries, replace the
// row with lowest element 1 as priority. Return index of row.
int Predictor::find(Array<U8>& ht, int sizebits, U32 cxt) {
  assert(ht.size()==16<<sizebits);
  int chk=cxt>>sizebits&255;
  int h0=(cxt*16)&(ht.size()-16);
  if (ht[h0]==chk) return h0;
  int h1=h0^16;
  if (ht[h1]==chk) return h1;
  int h2=h0^32;
  if (ht[h2]==chk) return h2;
  if (ht[h0+1]<=ht[h1+1] && ht[h0+1]<=ht[h2+1])
    return memset(&ht[h0], 0, 16), ht[h0]=chk, h0;
  else if (ht[h1+1]<ht[h2+1])
    return memset(&ht[h1], 0, 16), ht[h1]=chk, h1;
  else
    return memset(&ht[h2], 0, 16), ht[h2]=chk, h2;
}

/////////////////////////// optimize ///////////////////////

// Default run(), predict(), update() ifndef OPT
#ifndef OPT
inline void ZPAQL::run(U32 input) {run0(input);}
inline int Predictor::predict() {return predict0();}
inline void Predictor::update(int y) {update0(y);}

// Generate one case of predict()
void opt_predict(FILE *out, ZPAQL& z) {
  int n=z.header[6];
  fprintf(out, "      // %d components\n", n);

  // PCOMP should not call predict()
  if (n==0) {
    fprintf(out, 
      "      assert(0);\n"
      "      return 16384;\n");
    return;
  }

  // Code each component
  const U8* cp=&z.header[7];
  assert(cp[-1]==n);
  for (int i=0; i<n; ++i) {
    assert(cp>&z.header[0] && cp<&z.header[z.header.size()-8]);
    switch(cp[0]) {
      case CONST:  // c
        fprintf(out, "\n      // %d CONST %d\n", i, cp[1]);
        break;
      case CM:  // sizebits limit
        fprintf(out, "\n      // %d CM %d %d\n", i, cp[1], cp[2]);
        fprintf(out,
          "      comp[%d].cxt=z.H(%d)^hmap4;\n"
          "      p[%d]=stretch(comp[%d].cm(comp[%d].cxt)>>17);\n",
          i, i, i, i, i);
        break;
      case ICM: // sizebits
        fprintf(out, "\n      // %d ICM %d\n", i, cp[1]);
        fprintf(out,
          "      if (c8==1 || (c8&0xf0)==16)\n"
          "        comp[%d].c=find(comp[%d].ht, %d+2, z.H(%d)+16*c8);\n"
          "      comp[%d].cxt=comp[%d].ht[comp[%d].c+(hmap4&15)];\n"
          "      p[%d]=stretch(comp[%d].cm(comp[%d].cxt)>>8);\n",
          i, i, cp[1], i, i, i, i, i, i, i);
        break;
      case MATCH: // sizebits bufbits: a=len, b=offset, c=bit, cxt=256/len,
                  //                   ht=buf, limit=8*pos+bp
        fprintf(out, "\n      // %d MATCH %d %d\n", i, cp[1], cp[2]);
        fprintf(out,
          "      if (comp[%d].a==0) p[%d]=0;\n"
          "      else {\n"
          "        comp[%d].c=comp[%d].ht((comp[%d].limit>>3)\n"
          "           -comp[%d].b)>>(7-(comp[%d].limit&7))&1;\n"
          "        p[%d]=stretch(comp[%d].cxt*(comp[%d].c*-2+1)&32767);\n"
          "      }\n",
          i, i, i, i, i, i, i, i, i, i);
        break;
      case AVG: // j k wt
          fprintf(out, "\n      // %d AVG %d %d %d\n", i, cp[1], cp[2], cp[3]);
          fprintf(out,
          "      p[%d]=(p[%d]*%d+p[%d]*(256-%d))>>8;\n",
          i, cp[1], cp[3], cp[2], cp[3]);
        break;
      case MIX2:   // sizebits j k rate mask
                   // c=size cm=wt[size][m] cxt=input
        fprintf(out, "\n      // %d MIX2 %d %d %d %d %d\n", 
                     i, cp[1], cp[2], cp[3], cp[4], cp[5]);
        fprintf(out,
          "      {\n"
          "        comp[%d].cxt=((z.H(%d)+(c8&%d))&(comp[%d].c-1));\n"
          "        int w=comp[%d].a16[comp[%d].cxt];\n"
          "        p[%d]=(w*p[%d]+(65536-w)*p[%d])>>16;\n"
          "      }\n",
          i, i, cp[5], i, i, i, i, cp[2], cp[3]);

        break;
      case MIX:    // sizebits j m rate mask
                   // c=size cm=wt[size][m] cxt=index of wt in cm
        fprintf(out, "\n      // %d MIX %d %d %d %d %d\n", 
                     i, cp[1], cp[2], cp[3], cp[4], cp[5]);
        fprintf(out,
          "      {\n"
          "        comp[%d].cxt=z.H(%d)+(c8&%d);\n"
          "        comp[%d].cxt=(comp[%d].cxt&(comp[%d].c-1))*%d;\n"
          "        int* wt=(int*)&comp[%d].cm[comp[%d].cxt];\n",
          i, i, cp[5], i, i, i, cp[3], i, i);
          for (int j=0; j<cp[3]; ++j)  // unrolled for-loop
            fprintf(out, 
              "        p[%d]%s=(wt[%d]>>8)*p[%d];\n", 
              i, j?"+":"", j, cp[2]+j);
        fprintf(out,
          "        p[%d]=clamp2k(p[%d]>>8);\n"
          "      }\n",
          i, i);
        break;
      case ISSE:   // sizebits j -- c=hi, cxt=bh
        fprintf(out, "\n      // %d ISSE %d %d\n", i, cp[1], cp[2]);
        fprintf(out,
          "      {\n"
          "        if (c8==1 || (c8&0xf0)==16)\n"
          "          comp[%d].c=find(comp[%d].ht, %d, z.H(%d)+16*c8);\n"
          "        comp[%d].cxt=comp[%d].ht[comp[%d].c+(hmap4&15)];\n"
          "        int *wt=(int*)&comp[%d].cm[comp[%d].cxt*2];\n"
          "        p[%d]=clamp2k((wt[0]*p[%d]+wt[1]*64)>>16);\n"
          "      }\n",
          i, i, cp[1]+2, i, i, i, i, i, i, i, cp[2]);
        break;
      case SSE:   // sizebits j start limit
        fprintf(out, "\n      // %d SSE %d %d %d %d\n", 
                     i, cp[1], cp[2], cp[3], cp[4]);
        fprintf(out,
          "      {\n"
          "        comp[%d].cxt=(z.H(%d)+c8)*32;\n"
          "        int pq=p[%d]+992;\n"
          "        if (pq<0) pq=0;\n"
          "        if (pq>1983) pq=1983;\n"
          "        int wt=pq&63;\n"
          "        pq>>=6;\n"
          "        comp[%d].cxt+=pq;\n"
          "        p[%d]=stretch(((comp[%d].cm(comp[%d].cxt)>>10)*(64-wt)\n"
          "           +(comp[%d].cm(comp[%d].cxt+1)>>10)*wt)>>13);\n"
          "        comp[%d].cxt+=wt>>5;\n"
          "      }\n",
          i, i, cp[2], i, i, i, i, i, i, i);
        break;
    }
    cp+=compsize[cp[0]];
    assert(cp<&z.header[z.cend]);
  }
  assert(cp[0]==NONE);
  fprintf(out,
    "      return squash(p[%d]);\n", n-1);
}

void opt_update(FILE *out, ZPAQL& z) {
  int n=z.header[6];
  fprintf(out, "      // %d components\n", n);

  // PCOMP should not call update()
  if (n==0) {
    fprintf(out, 
      "      assert(0);\n");
    return;
  }

  // Code each component
  const U8* cp=&z.header[7];
  assert(cp[-1]==n);
  for (int i=0; i<n; ++i) {
    assert(cp>&z.header[0] && cp<&z.header[z.header.size()-8]);
    switch(cp[0]) {
      case CONST:  // c
        fprintf(out, "\n      // %d CONST %d\n", i, cp[1]);
        break;
      case CM:  // sizebits limit
        fprintf(out, "\n      // %d CM %d %d\n", i, cp[1], cp[2]);
        fprintf(out,
          "      train(comp[%d], y);\n",
          i);
        break;
      case ICM:   // sizebits: cxt=ht[b]=bh, ht[c][0..15]=bh row, cxt=bh
        fprintf(out, "\n      // %d ICM %d\n", i, cp[1]);
        fprintf(out,
          "      {\n"
          "        comp[%d].ht[comp[%d].c+(hmap4&15)]=\n"
          "            st.next(comp[%d].ht[comp[%d].c+(hmap4&15)], y);\n"
          "        U32& pn=comp[%d].cm(comp[%d].cxt);\n"
          "        pn+=int(y*32767-(pn>>8))>>2;\n"
          "      }\n",
          i, i, i, i, i, i);
        break;
      case MATCH: // sizebits bufbits:
                  //   a=len, b=offset, c=bit, cm=index, cxt=256/len
                  //   ht=buf, limit=8*pos+bp
        fprintf(out, "\n      // %d MATCH %d %d\n", i, cp[1], cp[2]);
        fprintf(out,
          "      {\n"
          "        if (comp[%d].c!=y) comp[%d].a=0;\n"
          "        comp[%d].ht(comp[%d].limit>>3)+=comp[%d].ht(comp[%d].limit>>3)+y;\n"
          "        if ((++comp[%d].limit&7)==0) {\n"
          "          int pos=comp[%d].limit>>3;\n"
          "          if (comp[%d].a==0) {\n"
          "            comp[%d].b=pos-comp[%d].cm(z.H(%d));\n"
          "            if (comp[%d].b&(comp[%d].ht.size()-1))\n"
          "              while (comp[%d].a<255 && comp[%d].ht(pos-comp[%d].a-1)\n"
          "                     ==comp[%d].ht(pos-comp[%d].a-comp[%d].b-1))\n"
          "                ++comp[%d].a;\n"
          "          }\n"
          "          else comp[%d].a+=comp[%d].a<255;\n"
          "          comp[%d].cm(z.H(%d))=pos;\n"
          "          if (comp[%d].a>0) comp[%d].cxt=2048/comp[%d].a;\n"
          "        }\n"
          "      }\n",
          i, i, i, i, i, i, i, i, i, i, i, i, i, i,
          i, i, i, i, i, i, i, i, i, i, i, i, i, i);
        break;
      case AVG:  // j k wt
        fprintf(out, "\n      // %d AVG %d %d %d\n", i, cp[1], cp[2], cp[3]);
        break;
      case MIX2:   // sizebits j k rate mask
                   // cm=input[2],wt[size][2], cxt=weight row
        fprintf(out, "\n      // %d MIX2 %d %d %d %d %d\n", 
                     i, cp[1], cp[2], cp[3], cp[4], cp[5]);
        fprintf(out,
          "      {\n"
          "        int err=(y*32767-squash(p[%d]))*%d>>5;\n"
          "        int w=comp[%d].a16[comp[%d].cxt];\n"
          "        w+=(err*(p[%d]-p[%d])+(1<<12))>>13;\n"
          "        if (w<0) w=0;\n"
          "        if (w>65535) w=65535;\n"
          "        comp[%d].a16[comp[%d].cxt]=w;\n"
          "      }\n",
          i, cp[4], i, i, cp[2], cp[3], i, i);
        break;
      case MIX:     // sizebits j m rate mask
                    // cm=wt[size][m], cxt=input
        fprintf(out, "\n      // %d MIX %d %d %d %d %d\n", 
                     i, cp[1], cp[2], cp[3], cp[4], cp[5]);
        fprintf(out,
          "      {\n"
          "        int err=(y*32767-squash(p[%d]))*%d>>4;\n"
          "        int* wt=(int*)&comp[%d].cm[comp[%d].cxt];\n",
          i, cp[4], i, i);
        for (int j=0; j<cp[3]; ++j) // unrolled
          fprintf(out,
            "          wt[%d]=clamp512k(wt[%d]+((err*p[%d]+(1<<12))>>13));\n",
            j, j, cp[2]+j);
        fprintf(out,
          "      }\n");
        break;
      case ISSE:   // sizebits j  -- c=hi, cxt=bh
        fprintf(out, "\n      // %d ISSE %d %d\n", i, cp[1], cp[2]);
        fprintf(out,
          "      {\n"
          "        int err=y*32767-squash(p[%d]);\n"
          "        int *wt=(int*)&comp[%d].cm[comp[%d].cxt*2];\n"
          "        wt[0]=clamp512k(wt[0]+((err*p[%d]+(1<<12))>>13));\n"
          "        wt[1]=clamp512k(wt[1]+((err+16)>>5));\n"
          "        comp[%d].ht[comp[%d].c+(hmap4&15)]=st.next(comp[%d].cxt, y);\n"
          "      }\n",
          i, i, i, cp[2], i, i, i);
        break;
      case SSE:  // sizebits j start limit
        fprintf(out, "\n      // %d SSE %d %d %d %d\n", 
                     i, cp[1], cp[2], cp[3], cp[4]);
        fprintf(out,
          "      train(comp[%d], y);\n",
          i);
        break;
    }
    cp+=compsize[cp[0]];
    assert(cp<&z.header[z.cend]);
  }
  assert(cp[0]==NONE);
}

// Generate optimization code for the HCOMP section of z
void opt_hcomp(FILE *out, ZPAQL& z, int select) {

  /* Instruction translation table. It was generated from
  the body of ZPAQL::run0() with the following perl script,
  then hand editing JT, JF, JMP, and LJ.

  for ($i=0; $i<256; ++$i) {
    $a[$i]="    \"err();\",";
  }
  while (<>) {
    chomp;
    if (/case (\d+): (.*) break; *\/\/(.*)/) {
      $n=$1;
      $op=$2;
      $comment=$3;
      $op=~s/header\[pc\+\+\]/%d/;
      $op="\"".$op."\",";
      $a[$n]=sprintf("    %-26s // $n ".$comment, $op);
    }
  }
  for ($i=0; $i<256; ++$i) {
    print("$a[$i]\n");
  }
  */
  static const char* inst[256]={
    "err();",                  // 0  ERROR
    "++a;",                    // 1  A++
    "--a;",                    // 2  A--
    "a = ~a;",                 // 3  A!
    "a = 0;",                  // 4  A=0
    "err();",
    "err();",
    "a = r[%d];",              // 7  A=R N
    "swap(b);",                // 8  B<>A
    "++b;",                    // 9  B++
    "--b;",                    // 10  B--
    "b = ~b;",                 // 11  B!
    "b = 0;",                  // 12  B=0
    "err();",
    "err();",
    "b = r[%d];",              // 15  B=R N
    "swap(c);",                // 16  C<>A
    "++c;",                    // 17  C++
    "--c;",                    // 18  C--
    "c = ~c;",                 // 19  C!
    "c = 0;",                  // 20  C=0
    "err();",
    "err();",
    "c = r[%d];",              // 23  C=R N
    "swap(d);",                // 24  D<>A
    "++d;",                    // 25  D++
    "--d;",                    // 26  D--
    "d = ~d;",                 // 27  D!
    "d = 0;",                  // 28  D=0
    "err();",
    "err();",
    "d = r[%d];",              // 31  D=R N
    "swap(m(b));",             // 32  *B<>A
    "++m(b);",                 // 33  *B++
    "--m(b);",                 // 34  *B--
    "m(b) = ~m(b);",           // 35  *B!
    "m(b) = 0;",               // 36  *B=0
    "err();",
    "err();",
    "if (f) goto L%d;",        // 39  JT N
    "swap(m(c));",             // 40  *C<>A
    "++m(c);",                 // 41  *C++
    "--m(c);",                 // 42  *C--
    "m(c) = ~m(c);",           // 43  *C!
    "m(c) = 0;",               // 44  *C=0
    "err();",
    "err();",
    "if (!f) goto L%d;",       // 47  JF N
    "swap(h(d));",             // 48  *D<>A
    "++h(d);",                 // 49  *D++
    "--h(d);",                 // 50  *D--
    "h(d) = ~h(d);",           // 51  *D!
    "h(d) = 0;",               // 52  *D=0
    "err();",
    "err();",
    "r[%d] = a;",              // 55  R=A N
    "return;",                 // 56  HALT
    "if (output) putc(a, output); if (sha1) sha1->put(a);", // 57  OUT
    "err();",
    "a = (a+m(b)+512)*773;",   // 59  HASH
    "h(d) = (h(d)+a+512)*773;",// 60  HASHD
    "err();",
    "err();",
    "goto L%d;",               // 63  JMP N
    "a = a;",                  // 64  A=A
    "a = b;",                  // 65  A=B
    "a = c;",                  // 66  A=C
    "a = d;",                  // 67  A=D
    "a = m(b);",               // 68  A=*B
    "a = m(c);",               // 69  A=*C
    "a = h(d);",               // 70  A=*D
    "a = %d;",                 // 71  A= N
    "b = a;",                  // 72  B=A
    "b = b;",                  // 73  B=B
    "b = c;",                  // 74  B=C
    "b = d;",                  // 75  B=D
    "b = m(b);",               // 76  B=*B
    "b = m(c);",               // 77  B=*C
    "b = h(d);",               // 78  B=*D
    "b = %d;",                 // 79  B= N
    "c = a;",                  // 80  C=A
    "c = b;",                  // 81  C=B
    "c = c;",                  // 82  C=C
    "c = d;",                  // 83  C=D
    "c = m(b);",               // 84  C=*B
    "c = m(c);",               // 85  C=*C
    "c = h(d);",               // 86  C=*D
    "c = %d;",                 // 87  C= N
    "d = a;",                  // 88  D=A
    "d = b;",                  // 89  D=B
    "d = c;",                  // 90  D=C
    "d = d;",                  // 91  D=D
    "d = m(b);",               // 92  D=*B
    "d = m(c);",               // 93  D=*C
    "d = h(d);",               // 94  D=*D
    "d = %d;",                 // 95  D= N
    "m(b) = a;",               // 96  *B=A
    "m(b) = b;",               // 97  *B=B
    "m(b) = c;",               // 98  *B=C
    "m(b) = d;",               // 99  *B=D
    "m(b) = m(b);",            // 100  *B=*B
    "m(b) = m(c);",            // 101  *B=*C
    "m(b) = h(d);",            // 102  *B=*D
    "m(b) = %d;",              // 103  *B= N
    "m(c) = a;",               // 104  *C=A
    "m(c) = b;",               // 105  *C=B
    "m(c) = c;",               // 106  *C=C
    "m(c) = d;",               // 107  *C=D
    "m(c) = m(b);",            // 108  *C=*B
    "m(c) = m(c);",            // 109  *C=*C
    "m(c) = h(d);",            // 110  *C=*D
    "m(c) = %d;",              // 111  *C= N
    "h(d) = a;",               // 112  *D=A
    "h(d) = b;",               // 113  *D=B
    "h(d) = c;",               // 114  *D=C
    "h(d) = d;",               // 115  *D=D
    "h(d) = m(b);",            // 116  *D=*B
    "h(d) = m(c);",            // 117  *D=*C
    "h(d) = h(d);",            // 118  *D=*D
    "h(d) = %d;",              // 119  *D= N
    "err();",
    "err();",
    "err();",
    "err();",
    "err();",
    "err();",
    "err();",
    "err();",
    "a += a;",                 // 128  A+=A
    "a += b;",                 // 129  A+=B
    "a += c;",                 // 130  A+=C
    "a += d;",                 // 131  A+=D
    "a += m(b);",              // 132  A+=*B
    "a += m(c);",              // 133  A+=*C
    "a += h(d);",              // 134  A+=*D
    "a += %d;",                // 135  A+= N
    "a -= a;",                 // 136  A-=A
    "a -= b;",                 // 137  A-=B
    "a -= c;",                 // 138  A-=C
    "a -= d;",                 // 139  A-=D
    "a -= m(b);",              // 140  A-=*B
    "a -= m(c);",              // 141  A-=*C
    "a -= h(d);",              // 142  A-=*D
    "a -= %d;",                // 143  A-= N
    "a *= a;",                 // 144  A*=A
    "a *= b;",                 // 145  A*=B
    "a *= c;",                 // 146  A*=C
    "a *= d;",                 // 147  A*=D
    "a *= m(b);",              // 148  A*=*B
    "a *= m(c);",              // 149  A*=*C
    "a *= h(d);",              // 150  A*=*D
    "a *= %d;",                // 151  A*= N
    "div(a);",                 // 152  A/=A
    "div(b);",                 // 153  A/=B
    "div(c);",                 // 154  A/=C
    "div(d);",                 // 155  A/=D
    "div(m(b));",              // 156  A/=*B
    "div(m(c));",              // 157  A/=*C
    "div(h(d));",              // 158  A/=*D
    "div(%d);",                // 159  A/= N
    "mod(a);",                 // 160  A=A
    "mod(b);",                 // 161  A=B
    "mod(c);",                 // 162  A=C
    "mod(d);",                 // 163  A=D
    "mod(m(b));",              // 164  A=*B
    "mod(m(c));",              // 165  A=*C
    "mod(h(d));",              // 166  A=*D
    "mod(%d);",                // 167  A= N
    "a &= a;",                 // 168  A&=A
    "a &= b;",                 // 169  A&=B
    "a &= c;",                 // 170  A&=C
    "a &= d;",                 // 171  A&=D
    "a &= m(b);",              // 172  A&=*B
    "a &= m(c);",              // 173  A&=*C
    "a &= h(d);",              // 174  A&=*D
    "a &= %d;",                // 175  A&= N
    "a &= ~ a;",               // 176  A&~A
    "a &= ~ b;",               // 177  A&~B
    "a &= ~ c;",               // 178  A&~C
    "a &= ~ d;",               // 179  A&~D
    "a &= ~ m(b);",            // 180  A&~*B
    "a &= ~ m(c);",            // 181  A&~*C
    "a &= ~ h(d);",            // 182  A&~*D
    "a &= ~ %d;",              // 183  A&~ N
    "a |= a;",                 // 184  A|=A
    "a |= b;",                 // 185  A|=B
    "a |= c;",                 // 186  A|=C
    "a |= d;",                 // 187  A|=D
    "a |= m(b);",              // 188  A|=*B
    "a |= m(c);",              // 189  A|=*C
    "a |= h(d);",              // 190  A|=*D
    "a |= %d;",                // 191  A|= N
    "a ^= a;",                 // 192  A^=A
    "a ^= b;",                 // 193  A^=B
    "a ^= c;",                 // 194  A^=C
    "a ^= d;",                 // 195  A^=D
    "a ^= m(b);",              // 196  A^=*B
    "a ^= m(c);",              // 197  A^=*C
    "a ^= h(d);",              // 198  A^=*D
    "a ^= %d;",                // 199  A^= N
    "a <<= (a&31);",           // 200  A<<=A
    "a <<= (b&31);",           // 201  A<<=B
    "a <<= (c&31);",           // 202  A<<=C
    "a <<= (d&31);",           // 203  A<<=D
    "a <<= (m(b)&31);",        // 204  A<<=*B
    "a <<= (m(c)&31);",        // 205  A<<=*C
    "a <<= (h(d)&31);",        // 206  A<<=*D
    "a <<= (%d&31);",          // 207  A<<= N
    "a >>= (a&31);",           // 208  A>>=A
    "a >>= (b&31);",           // 209  A>>=B
    "a >>= (c&31);",           // 210  A>>=C
    "a >>= (d&31);",           // 211  A>>=D
    "a >>= (m(b)&31);",        // 212  A>>=*B
    "a >>= (m(c)&31);",        // 213  A>>=*C
    "a >>= (h(d)&31);",        // 214  A>>=*D
    "a >>= (%d&31);",          // 215  A>>= N    
    "f = (a == a);",           // 216  A==A
    "f = (a == b);",           // 217  A==B
    "f = (a == c);",           // 218  A==C
    "f = (a == d);",           // 219  A==D
    "f = (a == U32(m(b)));",   // 220  A==*B
    "f = (a == U32(m(c)));",   // 221  A==*C
    "f = (a == h(d));",        // 222  A==*D
    "f = (a == U32(%d));",     // 223  A== N
    "f = (a < a);",            // 224  A<A
    "f = (a < b);",            // 225  A<B
    "f = (a < c);",            // 226  A<C
    "f = (a < d);",            // 227  A<D
    "f = (a < U32(m(b)));",    // 228  A<*B
    "f = (a < U32(m(c)));",    // 229  A<*C
    "f = (a < h(d));",         // 230  A<*D
    "f = (a < U32(%d));",      // 231  A< N
    "f = (a > a);",            // 232  A>A
    "f = (a > b);",            // 233  A>B
    "f = (a > c);",            // 234  A>C
    "f = (a > d);",            // 235  A>D
    "f = (a > U32(m(b)));",    // 236  A>*B
    "f = (a > U32(m(c)));",    // 237  A>*C
    "f = (a > h(d));",         // 238  A>*D
    "f = (a > U32(%d));",      // 239  A> N
    "err();",
    "err();",
    "err();",
    "err();",
    "err();",
    "err();",
    "err();",
    "err();",
    "err();",
    "err();",
    "err();",
    "err();",
    "err();",
    "err();",
    "err();",
    "goto L%d;"};              // 255 LJ NN

  // Generate a map of jump targets
  if (z.hend<=z.hbegin) return;
  Array<char> targets(0x10000);
  for (int i=z.hbegin; i<z.hend-1; ++i) {
    int op=z.header[i];
    if (op==LJ) targets[z.header[i+1]+256*z.header[i+2]]=1, ++i;
    if (op==JT || op==JF || op==JMP) {
      int addr=i+2+(int(z.header[i+1])<<24>>24)-z.hbegin;
      if (addr>=0 && addr<0x10000) targets[addr]=1;
      else error("goto target out of range");
    }
    if (op%8==7) ++i;  // 2 byte instruction (LJ is 3)
  }

  // Generate instructions. The output code will not compile
  // if any ZPAQL instructions jump to the middle of a 2 or 3
  // byte instruction (legal) or out of range (legal if not executed).
  fprintf(out, "      a = input;\n");
  for (int i=z.hbegin; i<z.hend-1; ++i) {
    int op=z.header[i];
    if (targets[i-z.hbegin]) {
      fprintf(out, "L%d:\n", select*100000+(i-z.hbegin)); // goto label
      targets[i-z.hbegin]=0;
    }
    int operand=z.header[i+1];  // numeric operand
    if (op==JT || op==JF || op==JMP)  // label
      operand=select*100000+i+2+(int(z.header[i+1])<<24>>24)-z.hbegin;
    if (op==LJ)
      operand=select*100000+z.header[i+1]+256*z.header[i+2], ++i; // label
    if (op%8==7) ++i; // 2 byte instruction
    fprintf(out, "      ");
    fprintf(out, inst[op], operand);
    fprintf(out, "\n");
  }
}

// Write z.header as a C++ array of bytes, var
void dump(FILE *out, ZPAQL& z, const char *var) {
  int hsize=z.cend+z.hend-z.hbegin;
  assert(hsize==0 || hsize==z.header[0]+256*z.header[1]+2);
  if (hsize==0) {
    fprintf(out, 
      "const U8 %s_array[2]={0,0};\n", var);
  }
  else {
    fprintf(out, "const U8 %s_array[%d]={ // COMP=%d HCOMP=%d\n  ",
      var, hsize, z.cend, z.hend-z.hbegin);
    for (int i=0, j=0; i<hsize; ++i, ++j) {
      if (j==z.cend) {
        j=z.hbegin;
        fprintf(out, "\n  // HCOMP\n  ");
      }
      fprintf(out, "%d", z.header[j]);
      if (i<hsize-1) {
        fprintf(out, ",");
        if (i%16==15)
          fprintf(out, "\n  ");
      }
    }
    fprintf(out, "};\n");
  }
  fprintf(out, "const U8 *%s=%s_array;\n\n", var, var);
}

// Create filename.cpp with the following:
//   pre_cmd containing preprocessor command
//   zlist[zlist_size] containing z.header
//   pzlist[pzlist_size] containing pz header
//   ZPAQL::run() containing optimized code z, pz when select is 1, 2
//   Predictor::predict() for z when z.select is 1
//   Predictor::update() for z when z.select is 1
void optimize(ZPAQL& z, ZPAQL& pz, const char* filename, const char* pcomp_cmd) {

  // Open output
  FILE *out=fopen(filename, "w");
  if (!out) perror(filename), exit(1);

  // Write header
  fprintf(out, 
    "// %s generated by ZPAQ\n"
    "#include <zpaq.h>\n"
    "\n", filename);

  // Write pre_cmd
  fprintf(out,
    "const char *pre_cmd=\"%s\";\n",
    pcomp_cmd);

  // Write zlist, pzlist
  dump(out, z, "zlist");
  dump(out, pz, "pzlist");

  // Write Predictor::predict()
  fprintf(out,
    "int Predictor::predict() {\n"
    "  switch(z.select) {\n"
    "    case 1: {\n");
    opt_predict(out, z);
  fprintf(out,
    "    }\n"
    "    default: return predict0();\n"
    "  }\n"
    "}\n"
    "\n");

  // Write Predictor::update()
  fprintf(out,
    "void Predictor::update(int y) {\n"
    "  switch(z.select) {\n"
    "    case 1: {\n");
  opt_update(out, z);
  fprintf(out,
    "      break;\n"
    "    }\n"
    "    default: return update0(y);\n"
    "  }\n"
    "  c8+=c8+y;\n"
    "  if (c8>=256) {\n"
    "    z.run(c8-256);\n"
    "    hmap4=1;\n"
    "    c8=1;\n"
    "  }\n"
    "  else if (c8>=16 && c8<32)\n"
    "    hmap4=(hmap4&0xf)<<5|y<<4|1;\n"
    "  else\n"
    "    hmap4=(hmap4&0x1f0)|(((hmap4&0xf)*2+y)&0xf);\n"
    "}\n"
    "\n");

  // Write ZPAQL::run()
  fprintf(out,
    "void ZPAQL::run(U32 input) {\n"
    "  switch(select) {\n"
    "    case 1: {\n");
  opt_hcomp(out, z, 1);
  fprintf(out,
    "      break;\n"
    "    }\n"
    "    case 2: {\n");
  opt_hcomp(out, pz, 2);
  fprintf(out,
    "      break;\n"
    "    }\n"
    "    default: run0(input);\n"
    "  }\n"
    "}\n"
    "\n"
    "\n");

  // Close file
  fclose(out);
  if (!quiet) printf("Created %s\n", filename);
}
#endif // not OPT

////////////////////////////// Decoder ////////////////////////////

// Decoder decompresses using an arithmetic code
class Decoder {
  FILE* in;  // destination
  U32 low, high; // range
  U32 curr;  // last 4 bytes of archive
  Predictor pr;  // to get p
  int decode(int p); // return decoded bit (0..1) with probability p (0..8191)
public:
  Decoder(FILE* f, ZPAQL& z);
  int decompress();  // return a byte or EOF
  int skip();  // skip to the end of the segment, return next byte
};

Decoder::Decoder(FILE* f, ZPAQL& z):
  in(f), low(1), high(0xFFFFFFFF), curr(0), pr(z) {}

inline int Decoder::decode(int p) {
  assert(p>=0 && p<65536);
  assert(high>low && low>0);
  if (curr<low || curr>high) error("archive corrupted");
  assert(curr>=low && curr<=high);
  U32 mid=low+((high-low)>>16)*p+((((high-low)&0xffff)*p)>>16); // split range
  assert(high>mid && mid>=low);
  int y=curr<=mid;
  if (y) high=mid; else low=mid+1; // pick half
  while ((high^low)<0x1000000) { // shift out identical leading bytes
    high=high<<8|255;
    low=low<<8;
    low+=(low==0);
    int c=getc(in);
    if (c==EOF) error("unexpected end of file");
    curr=curr<<8|c;
  }
  return y;
}

int Decoder::decompress() {
  if (curr==0) {  // finish initialization
    for (int i=0; i<4; ++i)
      curr=curr<<8|getc(in);
  }
  if (decode(0)) {
    if (curr!=0) error("decoding end of stream");
    return EOF;
  }
  else {
    int c=1;
    while (c<256) {  // get 8 bits
      int p=pr.predict()*2+1;
      c+=c+decode(p);
      pr.update(c&1);
    }
    return c-256;
  }
}

// Find end of compressed data and return next byte
int Decoder::skip() {
  int c=0;
  while (curr==0)  // at start?
    curr=getc(in);
  while (curr && (c=getc(in))!=EOF)  // find 4 zeros
    curr=curr<<8|c;
  while ((c=getc(in))==0) ;  // might be more than 4
  return c;
}

/////////////////////////// PostProcessor ////////////////////

class PostProcessor {
  int state;   // input parse state
  int hsize;   // header size
  int ph, pm;  // sizes of H and M in z
public:
  ZPAQL z;     // holds PCOMP
  PostProcessor(ZPAQL& hz);
  void set(FILE* out, SHA1* p) {z.output=out; z.sha1=p;}  // Set output
  int write(int c);  // Input a byte, return state
};

// Copy ph, pm from block header. sel selects ZPAQL::run() version
PostProcessor::PostProcessor(ZPAQL& hz) {
  state=hsize=0;
  ph=hz.header[4];
  pm=hz.header[5];
}

// (PASS=0 | PROG=1 psize[0..1] pcomp[0..psize-1]) data... EOB=-1
// Return state: 1=PASS, 2..4=loading PROG, 5=PROG loaded
int PostProcessor::write(int c) {
  assert(c>=-1 && c<=255);
  switch (state) {
    case 0:  // initial state
      if (c<0) error("Unexpected EOS");
      state=c+1;  // 1=PASS, 2=PROG
      if (state>2) error("unknown post processing type");
      break;
    case 1:  // PASS
      if (z.output && c>=0) putc(c, z.output);  // data
      if (z.sha1 && c>=0) z.sha1->put(c);
      break;
    case 2: // PROG
      if (c<0) error("Unexpected EOS");
      hsize=c;  // low byte of size
      state=3;
      break;
    case 3:  // PROG psize[0]
      if (c<0) error("Unexpected EOS");
      hsize+=c*256;  // high byte of psize
      z.header.resize(hsize+300);
      z.cend=8;
      z.hbegin=z.hend=z.cend+128;
      z.header[4]=ph;
      z.header[5]=pm;
      state=4;
      break;
    case 4:  // PROG psize[0..1] pcomp[0...]
      if (c<0) error("Unexpected EOS");
      assert(z.hend<z.header.size());
      z.header[z.hend++]=c;  // one byte of pcomp
      if (z.hend-z.hbegin==hsize) {  // last byte of pcomp?
        hsize=z.cend-2+z.hend-z.hbegin;
        z.header[0]=hsize&255;  // header size with empty COMP
        z.header[1]=hsize>>8;
        z.initp();
        state=5;
      }
      break;
    case 5:  // PROG ... data
      z.run(c);
      break;
  }
  return state;
}

/////////////////////////// rerun ////////////////////////////

// Return "/" in Linux or "\\" in Windows or error if unknown
const char* slash() {

  // Guess by counting / and \ in PATH (or TEMP) and pick the most common
  static char result[2]={0};
  if (!result[0]) {
    int forward=0;
    const char *path=getenv("PATH");
    if (!path) path=getenv("TEMP");
    if (path) {
      for (; *path; ++path) {
        if (*path=='/') ++forward;
        if (*path=='\\') --forward;
      }
    }
    if (forward>0) result[0]='/';
    if (forward<0) result[0]='\\';
  }
  if (!result[0]) error("unknown operating system");
  return result;
}

// Put the name of a temporary directory in filename ending eith \ or /
void tempdir(Array<char>& filename) {
  const char *env=getenv("TEMP");
  if (env) append(filename, env);
  else append(filename, ".");
  int len=strlen(&filename[0]);
  if (len>0 && filename[len-1]!='/' && filename[len-1]!='\\')
    append(filename, slash());
}

#ifndef OPT

// Call the optimized ZPAQ with arguments argc, argv. The name of the
// program is TEMP/zpaq_SHA1(z.header, pz.header, pre_cmd).exe
// If it doesn't exist then create a .cpp file with the same name
// and call zpaqmake to compile it first. For compression,
// the optimize function needs to preprocess with pre_cmd.
// For decompression, append block to "x" if not 0 and
// ignore argv[3..skipped_files+2]
void rerun(int argc, char** argv, ZPAQL& z, ZPAQL& pz,
           const char* pre_cmd, int block=0, int skipped_files=0) {

  // Get filename from hash of z, pz, pre_cmd
  SHA1 sha1;
  for (int i=0; i<z.cend; ++i)
    sha1.put(z.header[i]);
  for (int i=z.hbegin; i<z.hend; ++i)
    sha1.put(z.header[i]);
  for (int i=pz.hbegin; i<pz.hend && i<pz.header.size(); ++i)
    sha1.put(pz.header[i]);
  if (pre_cmd)
    for (int i=0; pre_cmd[i]; ++i)
      sha1.put(pre_cmd[i]);
  Array<char> filename;
  tempdir(filename);
  append(filename, "zpaq_");
  for (int i=0; i<20; ++i) {
    char s[10];
    sprintf(s, "%02x", sha1.result(i));
    append(filename, s);
  }
  append(filename, ".exe");

  // Test if file exists. If not, create it
  FILE *in=fopen(&filename[0], "rb");
  if (!in) {

    // Generate optimized C++ code
    int len=strlen(&filename[0]);
    assert(len>40);
    filename[len-4]=0;  // chop .exe
    append(filename, ".cpp");
    optimize(z, pz, &filename[0], pre_cmd);

    // compile it
    filename[len-4]=0;  // chop .cpp
    Array<char> cmd;
    append(cmd, "zpaqmake ");
    append(cmd, &filename[0]);
    if (!quiet) printf("%s\n", &cmd[0]);
    system(&cmd[0]);

    // Test if compile worked
    append(filename, ".exe");
    in=fopen(&filename[0], "rb");
    if (!in) error("optimize: compile failed");
  }
  fclose(in);

  // Execute command filename.exe(argc, argv)
  Array<char> cmd;
  append(cmd, &filename[0]);
  for (int i=1; i<argc; ++i) {
    if (i<3 || i>=skipped_files+3) {  // skip files
      append(cmd, " ");
      append(cmd, argv[i]);
    }
    if (i==1 && block>0) {  // append block to command if not 0
      char s[20];
      sprintf(s, "%d", block);
      append(cmd, s);
    }
  }
  if (!quiet) printf("%s\n", &cmd[0]);
  system(&cmd[0]);
}
#endif

/////////////////////////// Decompress ///////////////////////

void usage();  // print help message and exit.

// Reject archive filenames that might cause problems
bool validate_filename(const char* filename) {
  int len=strlen(filename);
  if (len<1) return true;  // No name is OK
  if (len>511) return false;  // name too long
  if (strstr(filename, "../")) return false; // no backward paths
  if (strstr(filename, "..\\")) return false;
  if (filename[0]=='/' || filename[0]=='\\') return false; // no absolute path
  for (int i=0; i<len; ++i)  // no control chars, drive letters, or devices
    if ((filename[i]&255)<32 || filename[i]==':') return false;
  return true;
}

// Advance 'in' past "zPQ" at its current location. If something
// else is there, search for the following 16 byte string
// which ends with "zPQ":
// 37 6B 53 74  A0 31 83 D3  8C B2 28 B0  D3 7A 50 51 (hex)
// Return true if found, false at EOF.
bool find_start(FILE *in) {
  U32 h1=0x3D49B113, h2=0x29EB7F93, h3=0x2614BE13, h4=0x3828EB13;
  // Rolling hashes initialized to hash of first 13 bytes
  int c;
  while ((c=getc(in))!=EOF) {
    h1=h1*12+c;
    h2=h2*20+c;
    h3=h3*28+c;
    h4=h4*44+c;
    if (h1==0xB16B88F1 && h2==0xFF5376F1 && h3==0x72AC5BF1 && h4==0x2F909AF1)
      return true;  // hash of 16 byte string
  }
  return false;
}

// Advance in to start of next block. Return number of segments skipped.
int skip_block(FILE *in) {
  assert(in);
  int segments=0;

  // Find start of next block
  int c;
  if (!find_start(in)) return 0;  // EOF
  if ((c=getc(in))>LEVEL || c<1 || getc(in)!=1)
    error("not ZPAQ");

  // Skip block header
  int hsize=getc(in);
  hsize+=getc(in)*256;
  if (hsize<6 || hsize>65535) error("hsize missing");
  while (hsize-->0) getc(in);
  
  // Skip segments
  while ((c=getc(in))==1) {
    ++segments;
    while (getc(in)>0) ; // skip filename
    while (getc(in)>0) ; // skip comment
    if (getc(in)!=0) error("reserved 0 missing");

    // Skip to end of data
    U32 c4=0xFFFFFFFF;  // last 4 bytes will be all 0
    while ((c=getc(in))!=EOF && (c4=c4<<8|c)!=0) ;
    if (c==EOF) error("unexpected end of file");
    while ((c=getc(in))==0) ;
    if (c==253) {  // Skip SHA1
      for (int i=0; i<20; ++i)
        getc(in);
    }
    else if (c!=254) error("missing end of segment marker");
  }
  if (c!=255) error("missing end of block marker");
  return segments;
}

// Remove path from filename
const char* strip(const char* filename) {
  assert(filename);
  int len=strlen(filename);
  const char *result=filename;
  for (int i=0; i<len; ++i) {
    if (filename[i]=='/' || filename[i]=='\\' || (i==1 && filename[i]==':'))
      result=filename+i+1;
  }
  return result;
}

// Decompress: [opntq]xN archive [files...]
// o=optimize, p=paths, n=extract all to one file, t=no postprocessing,
// N=block to extract (default all), files...=new names.
void decompress(int argc, char** argv) {
  assert(argc>=3);

  // Get options
  bool ocmd=false, pcmd=false, ncmd=false, tcmd=false;
  int blocknum=0;
  const char* cmd=argv[1];
  assert(cmd);
  while (*cmd) {
    if (*cmd=='o') ocmd=true;
    else if (*cmd=='p') pcmd=true;
    else if (*cmd=='n') ncmd=true;
    else if (*cmd=='t') tcmd=true;
    else if (*cmd=='q') quiet=true;
    else if (*cmd=='x') break;
    else usage();
    ++cmd;
  }
  if (cmd[0]!='x') usage();
  if (cmd[1]) blocknum=atoi(cmd+1);
#ifdef OPT
  ocmd=false;
  if (blocknum<1) error("'x' command requires a block number");
#endif

  // Open archive
  FILE* in=fopen(argv[2], "rb");
  if (!in) perror(argv[2]), exit(1);

  // Skip to specified block
  int block=1;
  while (blocknum>block) {
    skip_block(in);
    ++block;
  }

  // Read the archive
  int filecount=0;  // number of files extracted
  int c;
  while (find_start(in)) {
    if (getc(in)!=LEVEL || getc(in)!=1)
      error("Not ZPAQ");

    // Read block header
    ZPAQL z;
    z.read(Reader(in));

    // PostProcessor and Decoder is created and and destroyed for each block
    PostProcessor pp(z);
    Decoder dec(in, z);

#ifdef OPT
    z.select=1;  // select optimized code
    z.verify();
    pp.z.select=2;
#else
    // clear output file for append
    if (ncmd && (block==1 || block==blocknum)) {
      if (argc!=4)
        error("'nx' requires one output filename");
      remove(argv[3]);
    }
#endif

    // Read segments
    bool first=true;  // first segment of block?
    while ((c=getc(in))==1) {

      // Read the filename
      char filename[512]={0};
      int i;
      for (i=0; (c=getc(in))>0; ++i)
        if (i<511) filename[i]=c;
      if (i>0 && i<512) filename[i]=0;
      if (!ocmd && !quiet) printf("%s ", filename);

#ifndef OPT
      // If the user named some but not all output files, then skip the rest
      if (!ncmd && argc>3 && filecount+3>=argc) {
        if (!quiet) printf("\nSkipping %s and remaining files\n", filename);
        goto end;
      }
#endif

      // Get comment
      char comment[20]={0};
      i=0;
      while ((c=getc(in))!=EOF && c!=0) {
        if (i<19) comment[i]=c;
        ++i;
      }
      if (!ocmd && !quiet) printf("%s -> ", comment);
      if (getc(in)) error("reserved");  // reserved 0

      // If not 'o', open output file
      FILE *out=0;
      if (!ocmd) {

        // If 'n', open as argv[3] for append.
        if (ncmd) {
          if (argc!=4)
            error("'nx' command requires one output filename");
          out=fopen(argv[3], "ab");
          if (!out) perror(argv[3]), exit(1);
          if (!quiet) printf("%s -> ", argv[3]);
        }

        // Else if the user gave an output file starting at argv[3], use it instead.
        else if (argc>3) {
          if (filecount+3>=argc) goto end;
          out=fopen(argv[filecount+3], "wb");
          if (!out) {
            perror(argv[filecount+3]);
            goto end;
          }
          else if (!quiet)
            printf("%s ", argv[filecount+3]);
        }

        // Otherwise, use the names in the archive, but don't clobber
        // or use suspicious filenames
        else {
          const char* newname=filename;
          if (!pcmd) newname=strip(filename);
          if (newname!=filename)
            printf("%s -> ", newname);
          if (!validate_filename(newname)) {
            printf("Error: bad filename\n");
            goto end;
          }
          out=fopen(newname, "rb");
          if (out) {
            fclose(out);
            printf("Error: won't overwrite\n");
            goto end;
          }
          else {
            out=fopen(newname, "wb");
            if (!out) {
              perror(newname);
              goto end;
            }
          }
        }
      }

      // Decompress
      SHA1 sha1;
      pp.set(out, &sha1);
#ifndef OPT
      // optimize: Decode PCOMP in first segment and skip the rest of
      // the block. Call rerun to use external optimized program to
      // extract the current block.
      if (ocmd) {
        if (first) {
          first=false;
          while ((c=dec.decompress())!=EOF) {
            c=pp.write(c);
            if (c==1 || c==5) {  // 1=no PCOMP, 5=PCOMP
              c=dec.skip();
              rerun(argc, argv, z, pp.z, "", blocknum?0:block, 
                    ncmd?0:filecount);
              break;
            }
          }
        }
        else
          c=dec.skip();
      }
      else 
#endif
      // Extract the current segment
      {
        time_t now=time(0);
        int len=0;
        while ((c=dec.decompress())!=EOF) {
          if (!ocmd && tcmd) { // don't preprocess
            if (out) putc(c, out);
            sha1.put(c);
          }
          else {
            if (pp.write(c)==5 && first) {
              pp.z.verify();
              first=false;
            }
          }
          if (!ocmd && !quiet && !(len++&0xfff) && time(0)!=now) {
            for (int i=printf("%1.0f ", sha1.size()); i>0; --i)
              putchar('\b');
            fflush(stdout);
            now=time(0);
          }
        }
        if (!tcmd)
          pp.write(-1);
        if (out) fclose(out);
      }
      ++filecount;

      // Check for end of segment and block markers
      int eos=c;
      if (!ocmd)
        eos=getc(in);  // 253=SHA1 follows, 254=EOS
      if (eos==253) {
        U8 hash[20];
        bool match=true;
        for (int i=0; i<20; ++i) {
          hash[i]=getc(in);
          if (hash[i]!=sha1.result(i))
            match=false;
        }
        if (!ocmd) {
          if (match) {
            if (!quiet) printf("Checksum OK      ");
          }
          else {
            fprintf(stderr, 
              "CHECKSUM FAILED: FILE IS NOT IDENTICAL\n  Archive SHA1: ");
            for (int i=0; i<20; ++i)
              fprintf(stderr, "%02x", hash[i]);
            fprintf(stderr, "\n  File SHA1:    ");
            for (int i=0; i<20; ++i)
              fprintf(stderr, "%02x", sha1.result(i));
            fprintf(stderr, "\n");
          }
        }
      }
      else if (eos!=254)
        error("missing end of segment marker");
      else
        if (!quiet) printf("OK, no checksum ");
      if (!ocmd && !quiet) printf("\n");
    }
    if (c!=255) error("missing end of block marker");
    if (blocknum) goto end;
    ++block;
  }

  // Close the archive
end:
  if (!quiet) printf("%d file(s) extracted\n", filecount);
  fclose(in);
}

//////////////////////////// Compressor ////////////////////////////

//////////////////////////// Encoder ///////////////////////////////

// Encoder compresses using an arithmetic code
class Encoder {
  FILE* out;  // destination
  U32 low, high; // range
  Predictor pr;  // to get p
  void encode(int y, int p); // encode bit y (0..1) with probability p (0..8191)
  U32 in_low, in_high; // number of input, output bytes (64 bits)
  U32 out_low, out_high;
public:
  Encoder(FILE* f, ZPAQL& z);
  void compress(int c);  // c is 0..255 or EOF
  void stat() {pr.stat();}  // print predictor statistics
  void setOutput(FILE* f) {out=f;}
  double in_size() const {return in_low+4294967296.0*in_high;}
  double out_size() const {return out_low+4294967296.0*out_high;}
  void reset() {in_low=in_high=out_low=out_high=0;} //  clear sizes
};

// Compress to file f using model z
Encoder::Encoder(FILE* f, ZPAQL& z): 
    out(f), low(1), high(0xFFFFFFFF), pr(z) {
  reset();
}

// compress bit y having probability p/64K
inline void Encoder::encode(int y, int p) {
  assert(out);
  assert(p>=0 && p<65536);
  assert(y==0 || y==1);
  assert(high>low && low>0);
  U32 mid=low+((high-low)>>16)*p+((((high-low)&0xffff)*p)>>16); // split range
  assert(high>mid && mid>=low);
  if (y) high=mid; else low=mid+1; // pick half
  while ((high^low)<0x1000000) { // write identical leading bytes
    putc(high>>24, out);  // same as low>>24
    high=high<<8|255;
    low=low<<8;
    low+=(low==0); // so we don't code 4 0 bytes in a row
    out_high+=(++out_low==0);
  }
}

// compress byte c (0..255 or -1=EOS)
void Encoder::compress(int c) {
  assert(out);
  if (c==-1)
    encode(1, 0);
  else {
    assert(c>=0 && c<=255);
    in_high+=(++in_low==0);
    encode(0, 0);
    for (int i=7; i>=0; --i) {
      int p=pr.predict()*2+1;
      assert(p>0 && p<65536);
      int y=c>>i&1;
      encode(y, p);
      pr.update(y);
    }
  }
}

//////////////////////////// Compress ////////////////////////////

#ifndef OPT
// Parse up to 9 comma separated numeric arguments appended to
// cmd and put in global args[0..8]. Replace commas with 0 in cmd.
void get_args(char *cmd) {
  if (cmd && cmd[0]) {
    int i=0;
    char *s=cmd, *sn;
    while (i<9 && (sn=strchr(s, ','))!=0) {
      args[i++]=atoi(sn+1);
      *sn=0;
      s=sn+1;
    }
  }
}
#endif

// Compress files: [pnsiqvo]c|a[F][,N...]] archive files...
void compress(int argc, char** argv) {
  assert(argc>=3);

  // Get command options
  bool pcmd=false, ncmd=false, scmd=false, icmd=false, // options
       tcmd=false, ocmd=false, acmd=false, ccmd=false;
  char *cmd=argv[1];
  while (cmd && cmd[0]) {
    if (cmd[0]=='p') pcmd=true, ncmd=false;
    else if (cmd[0]=='n') ncmd=true, pcmd=false;
    else if (cmd[0]=='s') scmd=true;
    else if (cmd[0]=='i') icmd=true;
    else if (cmd[0]=='q') quiet=true;
    else if (cmd[0]=='v') verbose=true;
    else if (cmd[0]=='t') tcmd=true;
    else if (cmd[0]=='o') ocmd=true;
    else if (cmd[0]=='a') {acmd=true; break;}
    else if (cmd[0]=='c') {ccmd=true; break;}
    else usage();
    ++cmd;
  }
  ++cmd;
  if (acmd==ccmd) usage();

#ifndef OPT
  // Parse comma separated arguments after config file (now in cmd)
  get_args(cmd);
#endif

  ZPAQL z, pz; // compression and postprocessing models
  Array<char> pcomp_cmd(64);  // name of external preprocessor

  // Initialize from optimization code
#ifdef OPT
  z.read(Reader(zlist, 0x10002));
  z.select=1;
  if (pzlist[0] || pzlist[1]) {  // hsize>0 ?
    pz.read(Reader(pzlist, 0x10002));
    pz.select=2;
  }
  append(pcomp_cmd, pre_cmd);

  // Initialize from config file or use default
#else
  if (cmd[0]) {  // config file name?
    FILE* cfg=fopen(cmd, "rb");
    if (!cfg) perror(cmd), exit(1);
    compile(cfg, z, pz, pcomp_cmd, args);
    fclose(cfg);
    if (!quiet) printf("%1.3f MB memory required.\n", z.memory()/1000000);
  }
  else {
    static U8 header[71]={ // COMP 34 bytes from mid.cfg
      69,0,3,3,0,0,8,3,5,8,13,0,8,17,1,8,
      18,2,8,18,3,8,19,4,4,22,24,7,16,0,7,24,
      255,0,
      // HCOMP 37 bytes
      17,104,74,4,95,1,59,112,10,25,59,112,10,25,59,112,
      10,25,59,112,10,25,59,112,10,25,59,10,59,112,25,69,
      207,8,112,56,0};
    z.read(Reader(header, 71));
  }
  if (ocmd) {
    rerun(argc, argv, z, pz, &pcomp_cmd[0]);
    return;
  }
#endif

  if (pz.hend>pz.hbegin)
    pz.initp();

  // Construct temporary file names from archive name
  Array<char> prefile, tempfile;
  tempdir(prefile);
  tempdir(tempfile);
  append(prefile, argv[2]);
  append(prefile, ".zpaq.pre");
  append(tempfile, argv[2]);
  append(tempfile, ".zpaq.tmp");

  // Initialize preprocessor
  remove(&tempfile[0]);

  // Compress files in argv[3...argc-1]
  FILE *out=0;  // archive opened when ready to compress first file
  Encoder enc(out, z);  // compressor
  double outsum=0;  // total output size
  for (int i=3; i<argc; ++i) {

    // Open input file
    FILE *in=fopen(argv[i], "rb");
    if (!in) {
      perror(argv[i]);
      continue;
    }

    // Get checksum and file size
    SHA1 check1;
    int c;
    while ((c=getc(in))!=EOF) check1.put(c);
    double insize=check1.size();  // input size of file
    double presize=insize;        // preprocessed size
    double outsize=(outsum==0);   // output size including header, EOB
    rewind(in);

    // Verify post(pre(in)) == in
    if (pz.hend>pz.hbegin) {  // PCOMP section?
      fclose(in);
      remove(&prefile[0]);

      // Run external preprocessor
      int len=strlen(&pcomp_cmd[0]);
      assert(len>=0 && len<pcomp_cmd.size());
      append(pcomp_cmd, " ");
      append(pcomp_cmd, argv[i]);
      append(pcomp_cmd, " ");
      append(pcomp_cmd, &prefile[0]);
      if (!quiet) {
        printf("%s ... ", &pcomp_cmd[0]);
        fflush(stdout);
      }
      system(&pcomp_cmd[0]);
      pcomp_cmd[len]=0; // chop last 2 arguments

      // Open preprocessor output
      in=fopen(&prefile[0], "rb");
      if (!in) {
        perror(&prefile[0]);
        continue;
      }

      // Run preprocessed data through postprocessor
      SHA1 check2;
      pz.sha1=&check2;
      presize=0;
      while ((c=getc(in))!=EOF) {
        pz.run(c);
        presize+=1;
      }
      pz.run(U32(-1));

      // Compare postprocessed output with unpreprocessed input
      bool match=true;
      for (int j=0; j<20; ++j)
        if (check1.result(j)!=check2.result(j))
          match=false;
      if (!match) {
        fprintf(stderr, "FAILED\n");
        fclose(in);
        continue;
      }
      if (!quiet) printf("OK\n");

      // If OK then use preprocessed input
      rewind(in);
      pz.sha1=0;
    } // end if PCOMP

    // Open archive for first file
    bool first=false;  // first file?
    if (!out) {
      out=fopen(argv[2], acmd?"ab":"wb");
      if (!out) perror(argv[2]), exit(1);

      // append locator tag
      if (tcmd)
        outsize+=fprintf(out, "%s", 
        "\x37\x6B\x53\x74\xA0\x31\x83\xD3\x8C\xB2\x28\xB0\xD3");

      // Write block header
      enc.setOutput(out);
      outsize+=fprintf(out, "%cPQ%c%c", 'z', LEVEL, 1);
      outsize+=z.write(out);
      first=true;
    }

    // Code segment header
    putc(1, out);  // start of segment
    if (!ncmd)
      outsize+=fprintf(out, "%s", pcmd?argv[i]:strip(argv[i]));  // filename
    putc(0, out);  // filename terminator
    if (!icmd)
      outsize+=fprintf(out, "%1.0f", insize);  // size as comment
    putc(0, out);  // comment terminator
    putc(0, out);  // reserved
    outsize+=4;
    enc.reset();   // size=0

    // Compress PCOMP or POST 0
    if (first) {
      const int psize=pz.hend-pz.hbegin;
      assert(psize>=0 && psize<0x10000);
      assert(pz.header.size()>=pz.hend);
      if (psize==0)
        enc.compress(0);  // PASS
      else {
        enc.compress(1);  // POST
        enc.compress(psize&255);     // size low byte
        enc.compress(psize>>8&255);  // size high byte
        for (int j=0; j<psize; ++j)  // PCOMP code
          enc.compress(pz.header[pz.hbegin+j]);
      }
    }

    // Compress 
    if (!quiet) {
      printf("%s %1.0f ", argv[i], insize);
      if (insize!=presize)
        printf("-> %1.0f ", presize);
    }
    int len=0;
    time_t now=time(0);
    while ((c=getc(in))!=EOF) {
      enc.compress(c);
      if (!quiet && !(len++&0xfff) && now!=time(0)) {
        for (int j=printf("%1.0f -> %1.0f ", 
                  enc.in_size(), outsize+enc.out_size()); j>0; --j)
          putchar('\b');
        fflush(stdout);
        now=time(0);
      }
    }
    enc.compress(-1);

    // Write segment trailer
    if (scmd)  // no SHA1
      outsize+=fprintf(out, "%c%c%c%c%c", 0, 0, 0, 0, 254);
    else {
      outsize+=20+fprintf(out, "%c%c%c%c%c", 0, 0, 0, 0, 253);
      for (int j=0; j<20; ++j)
        putc(check1.result(j), out);
    }
    fclose(in);
    in=0;
    remove(&prefile[0]);
    if (!quiet) 
      printf("-> %1.0f                        \n", outsize+enc.out_size());
    outsum+=outsize+enc.out_size();
  }

  // Code end of block and close archive
  if (out) {
    putc(255, out);  // block trailer
    if (!quiet) {
      printf("-> %1.0f\n", outsum);
      enc.stat();  // print statistics
    }
    fclose(out);

    // If no error then clean up temporary files
    remove(&tempfile[0]);
    remove(&prefile[0]);
  }
  else
    if (!quiet) printf("Archive %s not updated\n", argv[2]);
}

////////////////////////// list //////////////////////////

#ifndef OPT
// List archive contents: l archive
void list(int argc, char** argv) {
  assert(argc>2 && argv[2]);

  // Open archive
  FILE* in=fopen(argv[2], "rb");
  if (!in) perror(argv[2]), exit(1);

  // Read the file
  int c, blocks=0;
  while (find_start(in)) {

    // Read block header
    if (getc(in)!=LEVEL || getc(in)!=1)
      error("not ZPAQ");
    ZPAQL z;
    double size=6+z.read(in);  // compressed size
    printf("Block %d: requires %1.3f MB memory\n",
     ++blocks, z.memory()/1000000);

    // Read segments
    while ((c=getc(in))==1) {

      // Print filename and comments
      printf("  ");
      while ((c=getc(in))!=EOF && c) putchar(c), size+=1;
      printf("  ");
      while ((c=getc(in))!=EOF && c) putchar(c), size+=1;
      if (getc(in)!=0) error("reserved data");
      size+=6;

      // Skip to end of data
      U32 c4=0xFFFFFFFF;  // last 4 bytes will be all 0
      while ((c=getc(in))!=EOF && (c4=c4<<8|c)!=0)
        size+=1;
      if (c==EOF) error("unexpected end of file");
      while ((c=getc(in))==0)
        size+=1;
      if (c==253) {  // print SHA1 in verbose mode
        printf(" SHA1=");
        size+=20;
        for (int i=0; i<20; ++i) {
          int c=getc(in);
          if (i<4) printf("%02x", c);
        }
        printf("...");
      }
      else if (c!=254) error("missing end of segment marker");
      printf(" -> %1.0f\n", size);
      size=0;
    }
    if (c!=255) error("missing end of block marker");
  }
}
#endif

//////////////////////////// run ///////////////////////////

#ifndef OPT
// Debug config file: [pvth]rF[,N...] [args...]
// p=run PCOMP, v=verbose, t=trace once per numeric arg
// otherwise args are output, input (default stdout, stdin),
// h=trace in hexadecimal, o=generate zpaqopt.h.
void run(int argc, char** argv) {
  assert(argc>=2);

  // Get options
  bool pcmd=false, tcmd=false, hcmd=false;
  char *cmd=argv[1];
  assert(cmd);
  while (cmd[0]) {
    if (cmd[0]=='p') pcmd=true;
    else if (cmd[0]=='v') verbose=true;
    else if (cmd[0]=='t') tcmd=true;
    else if (cmd[0]=='h') hcmd=true;
    else if (cmd[0]=='r') break;
    else usage();
    ++cmd;
  }
  ++cmd; // now points config file name
  if (!cmd[0]) usage();

  // Parse comma separated arguments after config file (now in cmd)
  get_args(cmd);

  // Initialze virtual machine
  ZPAQL hz, pz;  // HCOMP, PCOMP virtual machines
  Array<char> pcomp_cmd;  // PCOMP command (not used)
  FILE* in=fopen(cmd, "r");
  if (!in) perror(cmd), exit(1);
  compile(in, hz, pz, pcomp_cmd, args);
  ZPAQL& z=pcmd?pz:hz;  // the machine to be run
  if (z.hend<=z.hbegin) error("no program to run");
  if (pcmd) z.initp();
  else z.inith();

  // Run the program
  if (tcmd) {  // trace with numeric args
    for (int i=2; i<argc; ++i)
      z.step(atoi(argv[i]), hcmd);
  }
  else {  // run F input output
    FILE *in=stdin;
    z.output=stdout;
    if (argc>2) {
      in=fopen(argv[2], "rb");
      if (!in) perror(argv[2]), exit(1);
    }
    if (argc>3) {
      z.output=fopen(argv[3], "wb");
      if (!z.output) perror(argv[3]), exit(1);
    }
    int c;
    while ((c=getc(in))!=EOF)
      z.run(c);
    z.run(U32(-1));
  }
}
#endif

///////////////////////////// Main ///////////////////////////

// Print help message and exit
void usage() {
  printf("ZPAQ v1.10 archiver, (C) 2009, Ocarina Networks Inc.\n"
    "Written by Matt Mahoney, " __DATE__ ".\n"
    "This is free software under GPL v3, http://www.gnu.org/copyleft/gpl.html\n"
    "\n"
    "To compress to new archive: zpaq [opnsitqv]c[F[,N...]] archive files...\n"
    "To append to archive:       zpaq [opnsitqv]a[F[,N...]] archive files...\n"
    "Optional modifiers:\n"
#ifndef OPT
    "  o = compress faster (requires C++ compiler)\n"
#endif
    "  p = store filename paths in archive\n"
    "  n = don't store filenames (names will be needed to decompress)\n"
    "  s = don't store SHA1 checksums (saves 20 bytes)\n"
    "  i = don't store file sizes as comments (saves a few bytes)\n"
    "  t = append locator tag to non-ZPAQ data\n"
    "  q = quiet\n"
#ifndef OPT
    "  v = verbose (show F as it compiles)\n"
    "  F = use options in configuration file F (min.cfg, max.cfg)\n"
    "  ,N = pass numeric arguments to F\n"
    "To list contents: zpaq l archive\n"
#endif
    "To extract: zpaq [opntq]x[N] archive [files...]\n"
#ifndef OPT
    "  o = extract faster (requires C++ compiler)\n"
#endif
    "  p = extract to stored paths instead of current directory\n"
    "  n = decompress all to one file\n"
    "  t = don't post-process (for debugging)\n"
    "  q = quiet\n"
    "  N = extract only block N (1, 2, 3...)\n"
    "  files... = rename extracted files (clobbers)\n"
    "      otherwise use stored names (does not clobber)\n"
#ifndef OPT
    "To debug configuration file F: zpaq [pthv]rF[,N...] [args...]\n"
    "  p = run PCOMP (default is to run HCOMP)\n"
    "  t = trace (single step), args are numeric inputs\n"
    "      otherwise args are input, output (default stdin, stdout)\n"
    "  h = trace display in hexadecimal\n"
    "  v = verbose compile\n"
    "  ,N = pass numeric arguments to F\n"
#endif
    );
  exit(0);
}

// Command syntax as in usage()
int main(int argc, char** argv) {
  time_t start=time(0);

  // Check usage
  if (argc<2) 
    usage();

  // Find the command c, a, x, l, r
  char cmd=0;
  for (int i=0; (cmd=argv[1][i])!=0; ++i)
    if (strchr("caxlr", cmd))
      break;

  // Do the command
  if (argc>=3 && (cmd=='a' || cmd=='c'))
    compress(argc, argv);
  else if (argc>=3 && cmd=='x')
    decompress(argc, argv);
#ifndef OPT
  else if (argc>=3 && cmd=='l')
    list(argc, argv);
  else if (cmd=='r')
    run(argc, argv);
#endif
  else
    usage();

  // Print time used
  if (!quiet) {
    printf("Process time %1.2f sec. Wall time %1.0f sec.\n", 
      double(clock())/CLOCKS_PER_SEC, difftime(time(0), start));
  }
  return 0;
}
