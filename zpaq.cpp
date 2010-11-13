/*  zpaq v2.02 archiver and file compressor.

(C) 2009, Dell Inc.
    Written by Matt Mahoney, matmahoney@yahoo.com, Nov. 13, 2010

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
programs. See http://mattmahoney.net/dc/ for the latest version of this
program. zpaq is also a development environment for creating and
debugging new compression algorithms using configuration (.cfg) files
and external preprocessors.


Installation (Windows)
----------------------

Create a folder c:\zpaq and put all files here.

zpaq is a command line program. To run it, use the command

  c:\zpaq\zpaq

or put c:\zpaq in your PATH and simply use the command "zpaq".

zpaq has 3 built in compression levels (1=fast, 2=mid, 3=max)
and the ability to use custom compression algorithms described
in configuration (.cfg) files and external preprocessors, which
should be in the same folder as zpaq.exe.

If you want to use custom algorithms, then you can speed up compression
and decompression (typically twice as fast) with a C++ compiler,
preferably MinGW g++. To install run "compile.bat" to create
the necessary object files.

If you want to use a different install folder than c:\zpaq
or different compiler than g++ then edit compile.bat and makezpaq.bat
and make appropriate changes. You can also change compiler options
as appropriate for your computer.


Command summary
---------------

To compress: zpaq [nsitokv][ca][N][F[,N...]] archive [folder/] files...
  n = don't store filenames (extraction will concatenate)
  s = don't store SHA1 checksums (saves 20 bytes)
  i = don't store file sizes as comments (saves a few bytes)
  t = append locator tag to non-ZPAQ data such as zpsfx.exe
  c = create new archive.zpaq with 1 block
  a = or append 1 block to existing archive or archive.zpaq
  N = compression level 1=fast, 2=mid, 3=max
  F = or use configuration file F.cfg
  ,N = pass numeric arguments to F.cfg
  folder/ = store path for extraction (default = filename only)
To list contents: zpaq [v]l archive
To extract: zpaq [ok]x[N] archive [folder/] [files...]
  N = extract only block N (1, 2, 3...)
  folder/ = extract to folder (default = stored paths)
  files... = rename extracted files (clobbers)
      otherwise use stored names (does not clobber)
To make self extracting archive.exe: zpaq [ok]e archive
To debug configuration file F.cfg: zpaq [ptokv]rF[,N...] [args...]
  p = run PCOMP (default is to run HCOMP)
  t = trace (single step), args are numeric inputs
      otherwise args are input, output (default stdin, stdout)
  ,N = pass numeric arguments to F
For all commands:
  o = compress or decompress faster (requires C++ compiler)
  k = with o, keep zpaqopt.cpp, zpaqopt.exe
  v = verbose (echo F.cfg)

Prefixes, commands, and suffixes should be written without spaces,
for example:

  zpaq otsamax,2,1 books.zpaq calgary/ \data\calgary\book1

appends (a) to archive books.zpaq with prefixes o (compress faster),
t (append locator tag), and s (don't store checksum). The suffix
max,2,1 says to compress with the model described in max.cfg with
arguments 2 and 1 passed to that model. The file \data\calgary\book1
is stored with a filename of calgary/book1.

  zpaq x books /tmp/

will extract the contents of books.zpaq and create /tmp/book1
(instead of calgary/book1).

The ZPAQ archive format is described in the level 1, revision
1 version of the specification which can be found in this
distribution and from http://mattmahoney.net/dc/
Briefly, an archive consists of a sequence of blocks that can
be extracted independently in any order. A block contains a header
which describes the decompression algorithm, followed by a sequence
of segments which must be decompressed in order from the beginning.
Each segment has an optional filename, an optional checksum, compressed
data, and an optional SHA-1 checksum.


Compression
-----------

The "c" command creates a new archive. The archive must have a .zpaq
extension. If the filename does not end with ".zpaq", it will be
added automatically.

The "a" command appends to an existing archive. It does not have
to have a .zpaq extension if the file already exists. If the named
archive does not exist, the program will try adding a .zpaq extension.
If that file still does not exist, it will be created.

For both "c" and "a", the files to be compressed will be placed
in separate segments in a single block. The filename will be stored
without a path unless you supply a "folder/" argument with a trailing
slash or backslash after the archive name. In that case, each segment
filename will be stored as "folder/file".

The "n" prefix means don't store filenames. When an unnamed segment
is extracted, it is appended to the previous segment. The effect is
that all of the files will be concatenated. If the first segment in
the archive is not named, then a filename must be given during extraction.
This option is useful if you want to split a file into parts that
use different compression algorithms in different blocks with one
segment each. In this case, only the first part should be named.

A ZPAQ archive can be mixed with non-ZPAQ data between blocks. This
extra data will be ignored. However, any ZPAQ blocks following the
non-ZPAQ data will only be found if preceded by a special 13 byte
marker. The "t" prefix appends this marker at the start of the block.

The "i" prefix prevents a comment from being stored, which saves
a few bytes. Normally the prefix contains the original size of the
file as a decimal string. A comment is displayed when the archive
is listed or extracted but has no effect on the output file.

The "s" prefix prevents the 20 byte SHA-1 checksum from being
stored. The effect is that the extracted data cannot be verified
as identical.

The compression algorithm is specified by the suffix to the
"c" or "a" command, either as a number (1, 2, or 3) or the name of a
configuration file without the .cfg extension, such as "a2" or
"cmax" for config file max.cfg. Some config files take numeric
arguments, which should be preceded by commas without spaces,
for example "cmid,-1" or "amax,2,3". The meaning of the arguments
depends on the config file. If the suffix starts with a digit
then it has the following meaning:

  1 = fast (fastest and least memory but least compression)
  2 = mid  (moderate speed, memory, and compression)
  3 = max  (slowest and most memory but best compression)

The config files fast.cfg, mid.cfg, and max.cfg are included but
are also built into zpaq so they need not be present. If a config
file is specified without a leading path, then it must either
be in the current directory or in the ZPAQ install directory
where zpaq.exe is stored (normally c:\zpaq).

Some configuration files specify an external preprocessor and matching
postprocessing code which is embedded in the block header. If so, then
the preprocessor must be either in the current directory or in the
ZPAQ install directory where zpaq.exe is located (usually c:\zpaq).
The preprocessor is run to create a temporary file archive.zpaq.pre
in the current directory and then run through the specified
postprocessing code to verify that the original will be restored during
decompression. If the output SHA-1 checksum does not match, then the file
is skipped.

The "o" prefix tells zpaq to compress faster when using a config
file. zpaq will translate the ZPAQL code in the config file into
zpaqopt.cpp in the current directory, call makezpaq.bat to compile it
with an external C++ compiler to zpaqopt.exe in the current directory,
then run it with the same arguments that were passed to zpaq. "o" has no
effect on speed for built in models (1..3) because these models have
already been compiled for better speed.

The "k" prefix tells zpaq not to delete zpaqopt.cpp and zpaqopt.exe
after using "o". These programs work just like zpaq except that "o"
is ignored, and the model number (c1, a2, etc) selects an optimized
model rather than the 3 standard models. Other models can be
used but won't be as fast.

The "v" (verbose) prefix displays the config file as it is
read, which makes debugging easier. How to write config files
is described later. It also prevents temporary file archive.zpaq.pre,
from being deleted.


List
----

The "l" command lists the contents of an archive. It first searches
for the named archive, and if not found, tries appending a .zpaq
extension if there is not already one. The listing displays
each block (numbered starting at 1) and the amount of memory needed
to extract it. Decompression requires about the same amount of memory
and time as compression. For each segment, the filename, comment
(uncompressed size) and compressed size is shown.

The "v" prefix ("vl") displays in addition the compression algorithm
in a format suitable for use as a config file. If the original
config file specified an external preprocessing program, then that
information will be missing because it is not needed for extraction.
The SHA-1 checksum for each segment is also shown.


Extraction
----------

The "x" command extracts from an archive. It first searches
for the named archive, and if not found, tries appending a .zpaq
extension if there is not already one.

A numeric suffix N like "x1", "x2" means to extract only from
block N. The first block is 1. By default, the entire archive
is extracted.

Files are extracted using the filenames stored for each segment.
If a filename contains a path like "dir1/dir2/file" then zpaq
will try to create the needed folders (dir1/ and dir1/dir2/) if they
do not already exist.

If the output file already exists, then zpaq will exit rather than
clobber the file. If you specify one or more filenames on the
command line, then each named segment will be extracted by
creating the newly named file. In this case, existing files
will be overwritten. If you specify more file names than there
are named segments, the extra will be ignored. If you specify fewer,
then the extra segments will not be extracted. Unnamed segments
are always appended to the last named segment regardless of
renaming, except that if the first segment is not named, then
a filename argument is required to name it.

If you specify a folder (with a trailing / or \ ) then all files
will be extracted to the named folder regardless of any stored
paths. To extract all files to the current folder, you can use "./"
as in "zpaq x archive ./". zpaq treats / and \ equivalently and
converts to the approprate form as needed (/ for Linux, \ for Windows).

The "o" prefix makes decompression faster. It first scans all
the block headers to extract the decompression algorithms, translates
the embedded ZPAQL code into C++, compiles the program zpaqopt.exe
in the current directory and runs it with the same command line
arguments. This only helps if the archive was compressed
with a config file different from fast, mid, or max with no arguments,
because those 3 models are built in and automatically detected.

The "k" prefix keeps zpaqopt.cpp and zpaqopt.exe.


Self extracting archives
------------------------

The "e" command tells zpaq to convert archive.zpaq to archive.exe,
which can be run to extract the files it contains. archive.zpaq
is not deleted.

To run archive.exe, enter the file name including the .exe extension.
If it is in a different folder, you must include the path to
it even if that folder is in the PATH environment variable.
When run, it will extract its contents using the file names as stored.
If the named files exist, they will be overwritten. Command line
arguments to archive.exe are ignored.

You can also use zpaq to list or extract from archive.exe like a
regular archive. If you append to it with "a" then those files
will also self-extract when run.

The "e" command is equivalent to:

  copy/b c:\zpaq\zpsfx.exe+c:\zpaq\zpsfx.tag+archive.zpaq archive.exe

zpsfx.exe reads itself to find the start of the archive marked by
the 13 byte tag in zpsfx.tag. This is the same tag appended by "ta".

The "o" prefix builds an optimized archive containing code which
decompresses faster. It generates zpaqopt.cpp (as with "ox") and
uses makezpsfx.bat to link zpsfx.o to create zpaqopt.exe.
Then the tag and archive are appended as before.

The "k" prefix keeps zpaqopt.cpp and zpaqopt.exe.


Debugging
---------

The "r" command is used to debug config files for development.
No archive is given. Instead, there are two optional command line
arguments specifying the input and output files, which default to
stdin and stdout. The program is run by calling the HCOMP section
of the named config file (with optional arguments) once for each
input byte with that input in the A register. Output is by the
OUT instruction. The HCOMP code normally computes contexts.

The "p" prefix says to execute the PCOMP section rather than HCOMP.
The PCOMP code normally post processes. It is run once per input
byte and once more with EOF (-1) as input.

The "t" prefix says to trace exection. The command line arguments are
numbers rather than files. The program is run once per argument
with that value in the A register. After each instruction is
executed, the register contents are displayed. After HALT is executed,
the memory contents are displayed. The input arguments are either
decimal numbers like "255" or hexadecimal like "xFF". Contents are
displayed in the same base.

The "o", "k", and "v" prefixes works like with compression.


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
exit without compressing the file. If they do match, then the
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
block. Its input is from archive.zpaq.tmp in the current folder
during compression testing and from the decoder during decompression.

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
a temporary file archive.zpaq.tmp formed by appending the
extension ".tmp" to the archive name.

Before each file is compressed, ZPAQ will verify that the transformed
data in archive.zpaq.tmp will be converted back to the original input file
by inputting archive.zpaq.tmp to the ZPAQL program in PCOMP and comparing
its output to the original input. If the output is verified then 
the file is compressed, and the temporary file is deleted. Otherwise
zpaq exits with an error and archive.zpaq.tmp is retained. Furthermore,
if the "v" (verbose) prefix is used, then the postprocessed temporary
file archive.zpaq.tmp.out is created. This file will be different from
the input, although it should normally be identical.

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
    a> 255 if halt endif (ignore EOS)
    a-= 5 out halt (subtract 5 from each byte)
  END

The ZPAQL code inverts the transform by subtracting 5 from each
byte. During decompression, the code is called once for
each (transformed) decompressed byte in the A register, and once
with EOS (0xFFFFFFFF) at the end of file, which is ignored.

=======================================================================

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <time.h>
#include <string>
using std::string;

#ifdef unix
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#include "libzpaq.h"

// Print an error message and exit
namespace libzpaq {
  void error(const char* msg="") {
    fprintf(stderr, "\nError: %s\n", msg);
    exit(1);
  }
}
using libzpaq::error;

// FILE type with a byte counter
struct File: public libzpaq::Reader, public libzpaq::Writer {
  FILE* f;
  double count;  // number of bytes get or put to f
  File(FILE* f_=0): f(f_), count(0) {}

  // Read and count a byte
  int get() {
    int c=getc(f);
    if (c!=EOF) count+=1;
    return c;
  }

  // Write and count a byte
  void put(int c) {
    count+=1;
    putc(c, f);
  }
};

// Improved std:string with output by appending to it
struct String: public libzpaq::Writer, public libzpaq::Reader, public string {
  String(const string& s): string(s) {} // base to derived conversions
  String(const char* s=""): string(s) {}
  void put(int c) {*this+=char(c);}     // append 1 byte
  int get();                            // read and remove first byte or EOF
  int len() const {return int(size());} // size as a signed int
  int operator()(unsigned int i) const; // i'th byte, bounds checked
  string sub(int i, int n) const;       // clipped substr(i, n)
  string sub(int i) const;              // clipped substr(i)
};

int String::get() {
  if (len()==0) return -1;
  int c=(*this)(0);
  *this=sub(1);
  return c;
}

int String::operator()(unsigned int i) const {
  assert(i<size());
  if (i>=size()) return 0;
  return (*this)[i]&255;
}

string String::sub(int i, int n) const {
  if (i<0) n+=i, i=0;
  if (i+n>len()) n=len()-i;
  if (n<=0) return "";
  return substr(i, n);
}

string String::sub(int i) const {
  return sub(i, len()-i);
}

//////////////////////////////// compile ///////////////////////////

// This code is to read configuration files containing custom
// compression algorithms written in ZPAQL.

// Globals
bool verbose=false;  // display config file as it compiles?
int args[9]={0};     // configuration file arguments
bool keep_option=false;  // keep temporary files?

// Symbolic constants
typedef enum {NONE,CONST,CM,ICM,MATCH,AVG,MIX2,MIX,ISSE,SSE,
  JT=39,JF=47,JMP=63,LJ=255,
  POST=256,PCOMP,END,IF,IFNOT,ELSE,ENDIF,DO,
  WHILE,UNTIL,FOREVER,IFL,IFNOTL,ELSEL,SEMICOLON} CompType;

// Component names
static const char* compname[256]=
  {"","const","cm","icm","match","avg","mix2","mix","isse","sse",0};

// Opcodes
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
  if (!tok)
    fprintf(stderr, "\nUnexpected end of configuration file\n"), exit(1);
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
  if (strcmp(s, t))
    fprintf(stderr, "\nExpected %s, found %s\n", s, t), exit(1);
}

// Read a number in (low...high) or exit with an error
int rtoken(FILE* in, int low, int high) {
  const char* tok=token(in);
  if (!tok)
    fprintf(stderr, "\nUnexpected end of configuration file\n"), exit(1);
  int n=0;
  const char* p=tok;
  int sign=1;
  if (*p=='-') sign=-1, ++p;
  while (*p) {
    if (isdigit(*p))
      n=n*10+*p-'0';
    else
      fprintf(stderr,
        "\nConfiguration file error at %s: expected a number\n", tok),
      exit(1);
    ++p;
  }
  n*=sign;
  if (n>=low && n<=high)
    return n;
  fprintf(stderr,
    "\nConfiguration file error: expected (%d...%d), found %d\n",
    low, high, n);
  exit(1);
  return 0;
}

// Stack of n elements of type T
template<class T>
class Stack {
  libzpaq::Array<T> s;
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
CompType compile_comp(FILE* in, String& comp) {
  int op=0;
  const int comp_begin=comp.len();
  Stack<unsigned short> if_stack(1000), do_stack(1000);  // IF, DO addresses
  if (verbose) printf("\n");
  int indent=0;  // program listing indentation
  while (comp.len()<0x10000) {
    if (verbose) {
      printf("(%4d) ", comp.len()-comp_begin);
      for (int i=0; i<indent; ++i) printf("  ");
    }
    op=rtoken(in, opcodelist);
    if (op==POST || op==PCOMP || op==END) break;
    int operand=-1; // 0...255 if 2 bytes
    int operand2=-1;  // 0...255 if 3 bytes
    if (op==IF) {
      op=JF;
      operand=0; // set later
      if_stack.push(comp.len()+1); // save jump target location
      ++indent;
    }
    else if (op==IFNOT) {
      op=JT;
      operand=0;
      if_stack.push(comp.len()+1); // save jump target location
      ++indent;
    }
    else if (op==IFL || op==IFNOTL) {  // long if
      if (op==IFL) comp.put(JT);
      if (op==IFNOTL) comp.put(JF);
      comp.put(3);
      op=LJ;
      operand=operand2=0;
      if_stack.push(comp.len()+1);
      if (verbose)
        printf("(%s 3 (%d 3) lj 0 0)",
          opcodelist[comp(comp.len()-2)], comp(comp.len()-2));
      ++indent;
    }
    else if (op==ELSE || op==ELSEL) {
      if (op==ELSE) op=JMP, operand=0;
      if (op==ELSEL) op=LJ, operand=operand2=0;
      int a=if_stack.pop();  // conditional jump target location
      assert(a>comp_begin && a<comp.len());
      if (comp(a-1)!=LJ) {  // IF, IFNOT
        assert(comp(a-1)==JT || comp(a-1)==JF || comp(a-1)==JMP);
        int j=comp.len()-a+1+(op==LJ); // offset at IF
        assert(j>=0);
        if (j>127) error("IF too big, try IFL, IFNOTL");
        comp[a]=j;
        if (verbose) printf("((%d) %s %d (to %d)) ",
          a-comp_begin-1, opcodelist[comp(a-1)], j, comp.len()-comp_begin+2);
      }
      else {  // IFL, IFNOTL
        int j=comp.len()-comp_begin+2+(op==LJ);
        assert(j>=0);
        comp[a]=j&255;
        comp[a+1]=(j>>8)&255;
        if (verbose) printf("((%d) lj %d) ", a-comp_begin-1, j);
      }
      if_stack.push(comp.len()+1);  // save JMP target location
    }
    else if (op==ENDIF) {
      int a=if_stack.pop();  // jump target address
      assert(a>comp_begin && a<comp.len());
      int j=comp.len()-a-1;  // jump offset
      assert(j>=0);
      if (comp(a-1)!=LJ) {
        assert(comp(a-1)==JT || comp(a-1)==JF || comp(a-1)==JMP);
        if (j>127) error("IF too big, try IFL, IFNOTL, ELSEL\n");
        comp[a]=j;
        if (verbose) printf("((%d) %s %d (to %d))\n",
          a-comp_begin-1, opcodelist[comp(a-1)], j, comp.len()-comp_begin);
      }
      else {
        assert(a+1<comp.len());
        j=comp.len()-comp_begin;
        comp[a]=j&255;
        comp[a+1]=(j>>8)&255;
        if (verbose) printf("((%d) lj %d)\n", a-1, j);
      }
      --indent;
    }
    else if (op==DO) {
      do_stack.push(comp.len());
      if (verbose) printf("\n");
      ++indent;
    }
    else if (op==WHILE || op==UNTIL || op==FOREVER) {
      int a=do_stack.pop();
      assert(a>=comp_begin && a<comp.len());
      int j=a-comp.len()-2;
      assert(j<=-2);
      if (j>=-127) {  // backward short jump
        if (op==WHILE) op=JT;
        if (op==UNTIL) op=JF;
        if (op==FOREVER) op=JMP;
        operand=j&255;
        if (verbose)
          printf("(%s %d (to %d)) ", opcodelist[op], j,
                 comp.len()-comp_begin+2+j);
      }
      else {  // backward long jump
        j=a-comp_begin;
        assert(j>=0 && j<comp.len()-comp_begin);
        if (op==WHILE) {
          comp.put(JF);
          comp.put(3);
          if (verbose) printf("(jf 3) ");
        }
        if (op==UNTIL) {
          comp.put(JT);
          comp.put(3);
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
        if (verbose) printf("(to %d) ", comp.len()-comp_begin+2+operand);
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
      comp.put(op);
    if (operand>=0)
      comp.put(operand);
    if (operand2>=0)
      comp.put(operand2);
    if (comp.len()>=0x10000)
      error("program too big");
  }
  comp.put(0); // END
  return CompType(op);
}

// Compile a configuration file. Store COMP/HCOMP section in hcomp.
// If there is a PCOMP section, store it in pcomp and store the PCOMP
// command in pcomp_cmd. Replace "$1..$9+n" with args[0..8]+n
void compile(FILE* in, String& hcomp, String& pcomp, String& pcomp_cmd) {

  // Allocate header
  hcomp="";
  pcomp="";
  pcomp_cmd="";
 
  // Compile the COMP section of header
  rtoken(in, "comp");
  hcomp.put(0);  // size low byte to fill in later
  hcomp.put(0);  // size high byte
  hcomp.put(rtoken(in, 0, 255)); // hh
  hcomp.put(rtoken(in, 0, 255)); // hm
  hcomp.put(rtoken(in, 0, 255)); // ph
  hcomp.put(rtoken(in, 0, 255)); // pm
  int n=rtoken(in, 0, 255); // number of components
  hcomp.put(n);
  if (verbose) printf("\n");
  for (int i=0; i<n; ++i) {
    if (verbose) printf("  ");
    rtoken(in, i, i);
    CompType type=CompType(rtoken(in, compname));
    hcomp.put(type);
    int clen=libzpaq::compsize[type];
    assert(clen>0 && clen<10);
    for (int j=1; j<clen; ++j)
      hcomp.put(rtoken(in, 0, 255));
    if (verbose) printf("\n");
  }
  hcomp.put(0); // END

  // Compile HCOMP
  rtoken(in, "hcomp");
  CompType op=compile_comp(in, hcomp);
  if (verbose) printf("\n");

  // Compute header size
  int hsize=hcomp.len()-2;
  hcomp[0]=hsize&255;
  hcomp[1]=hsize>>8;

  // Compile POST 0 END
  if (op==POST) {
    rtoken(in, 0, 0);
    rtoken(in, "end");
  }

  // Compile PCOMP pcomp_cmd ; program... END
  else if (op==PCOMP) {
    pcomp.put(0);  // fill in size later
    pcomp.put(0);

    // get pcomp_cmd ending with ";" (case sensitive)
    const char *tok;
    while ((tok=token(in, false))!=0 && strcmp(tok, ";")) {
      if (pcomp_cmd.len()>0) pcomp_cmd+=" ";
      pcomp_cmd+=tok;
    }
    op=compile_comp(in, pcomp);
    if (op!=END)
      error("Expected END in configuation file");

    // Compute header size
    int hsize=pcomp.len()-2;
    pcomp[0]=hsize&255;
    pcomp[1]=hsize>>8;
  }
}

// Compile config file in cmd, like "3" or "min,2,1". If it starts
// with a nonzero digit then return the number and leave the strings empty.
// Otherwise, fill hcomp, pcomp, and pcomp_cmd from the config file
// with or without a .cfg extension (min or min.cfg) and put the
// numeric arguments in args[9] (args[0]=2, args[1]=1), and return 0.
int compile_cmd(const char* cmd, String& hcomp,
                String& pcomp, String& pcomp_cmd, const String& root) {
  int level=0;
  if (isdigit(cmd[0]))
    level=atoi(cmd);
  if (level==0) {

    // parse args
    int argnum=0;
    String filename;
    for (const char* p=cmd; *p && argnum<9; ++p) {
      if (*p==',')
        args[argnum++]=atoi(p+1);
      else if (argnum==0)
        filename+=*p;
    }

    // Add .cfg extension
    if (filename.sub(filename.len()-4)!=".cfg")
      filename+=".cfg";

    // Compile F or F.cfg
    FILE* in=fopen(filename.c_str(), "r");
    if (!in) {
      filename=root+filename;
      in=fopen(filename.c_str(), "r");
    }
    if (!in) perror(filename.c_str()), exit(1);
    fprintf(stderr, "Using model %s", filename.c_str());
    for (int i=0; i<argnum; ++i)
      fprintf(stderr, ",%d", args[i]);
    fprintf(stderr, "\n");
    compile(in, hcomp, pcomp, pcomp_cmd);
    fclose(in);
  }
  return level;
}

/////////////////////////// optimize ///////////////////////

// This code is to convert ZPAQL to C++.

// Pad pcomp string with an empty COMP header with ph,pm from hcomp
void fix_pcomp(const String& hcomp, String& pcomp) {
  if (hcomp.len()>=8 && pcomp.len()>=2) {
    pcomp=hcomp.sub(0, 8)+pcomp.sub(2);
    pcomp[0]=(pcomp.len()-2)&255;  // new length of PCOMP
    pcomp[1]=(pcomp.len()-2)>>8;
    pcomp[6]=pcomp[7]=0;  // n=0 components
  }
}

// Test if filename is readable
bool exists(const char* filename) {
  FILE* in=fopen(filename, "rb");
  if (in) {
    fclose(in);
    return true;
  }
  else
    return false;
}

// Test if a file exists and exit with error if not
void testfile(const char* filename) {
  if (!exists(filename)) {
    fprintf(stderr, "File not found: %s\n", filename);
    exit(1);
  }
}

// Print and run a command
void run_cmd(const string& cmd) {
  fprintf(stderr, "%s\n", cmd.c_str());
  system(cmd.c_str());
}

// ZPAQ install directory, defined in zpaqopt.cpp
extern const char* zpaqdir;
#ifndef OPT
const char* zpaqdir=0;
#endif

// Return '/' in Linux or '\' in Windows or 0 if unknown
char slash() {
  const char* path=getenv("PATH");  // guess by counting slashes in PATH
  if (!path) return 0;
  int forward=0;
  for (; *path; ++path) {
    if (*path=='/') ++forward;
    if (*path=='\\') --forward;
  }
  if (forward>0) return '/';
  else if (forward<0) return '\\';
  else return 0;
}

// Return the path to the install directory, e.g. "c:\zpaq\"
// for finding config files and preprocessors
String root(int argc, char** argv) {

  // If zpaqdir is set then use it
  if (zpaqdir)
    return zpaqdir;

  // If there is a command line path in argv[0], then use it
  String self=argv[0];
  for (int i=self.len()-1; i>=0; --i)  // find last slash
    if (self(i)=='/' || self(i)=='\\' || (i==1 && self(i)==':'))
      return self.sub(0, i+1);

  // Look in current directory
  if (exists(argv[0])) return "";
  if (exists((String(argv[0])+".exe").c_str())) return "";

  // Otherwise search PATH for this program with or without .exe extension
  const char* path=getenv("PATH");
  if (!path) error("no PATH");

  // Guess OS to determine path separator : or ;
  char slashchar=slash();
  char sep=slashchar=='/' ? ':' : ';';

  // Parse PATH. Look for both zpaq and zpaq.exe
  while (*path) {  // for each dir
    int i;
    for (i=0; path[i] && path[i]!=sep; ++i) ;  // find end of dir
    if (i>0) {
      String dir=String(path).sub(0, i)+slashchar;
      String file=dir+argv[0];
      if (exists(file.c_str())) return dir;
      String ext=file.sub(file.len()-4);
      if (ext!=".exe" && exists((file+".exe").c_str())) return dir;
    }
    path+=i+(path[i]!=0);
  }
  error("ZPAQ install directory not found");
  return "";
}

#ifndef OPT

// Generate one case of predict()
void opt_predict(FILE *out, const String& models, int p, int select) {
  assert(models.len()>p+7);
  int n=models(p+6);
  fprintf(out,
    "    case %d: {\n"
    "      // %d components\n", select, n);

  // Code each component
  p+=7;
  for (int i=0; i<n; ++i) {
    int cp[10]={0};
    for (int j=0; j<10 && p+j<models.len(); ++j)
      cp[j]=models(p+j);
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
      default:
        error("unknown component type");
    }
    assert(libzpaq::compsize[cp[0]]>0);
    p+=libzpaq::compsize[cp[0]];
    assert(p<models.len());
  }
  assert(models(p)==NONE);
  if (n<1)
    fprintf(out,
      "      return predict0();\n"
      "    }\n");
  else
    fprintf(out,
      "      return squash(p[%d]);\n"
      "    }\n", n-1);
}

void opt_update(FILE *out, const String& models, int p, int select) {
  assert(models.len()>p+7);
  int n=models(p+6);
  fprintf(out,
    "    case %d: {\n"
    "      // %d components\n", select, n);

  // Code each component
  p+=7;
  for (int i=0; i<n; ++i) {
    int cp[10]={0};
    for (int j=0; j<10 && p+j<models.len(); ++j)
      cp[j]=models(p+j);
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
      default:
        error("unknown component type");
    }
    assert(libzpaq::compsize[cp[0]]>0);
    p+=libzpaq::compsize[cp[0]];
    assert(p<models.len());
  }
  assert(models(p)==NONE);
  fprintf(out,
    "      break;\n"
    "    }\n");
}

// Generate optimization code for the HCOMP section of z
void opt_hcomp(FILE *out, const String& models, int p, int select) {

  /* Instruction translation table. It was generated from
  the body of ZPAQL::run0() with the following perl script,
  then hand editing OUT, JT, JF, JMP, and LJ.

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
    "if (output) output->put(a); if (sha1) sha1->put(a);", // 57  OUT
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

  // Find start and end of code
  assert(models.len()>p+8);
  const int end=p+models(p)+256*models(p+1)+2;
  assert(models.len()>=end+2);
  int n=models(p+6);
  p+=7;
  for (int i=0; i<n; ++i) {
    assert(models(p)>0 && libzpaq::compsize[models(p)]>0);
    p+=libzpaq::compsize[models(p)];
    assert(p<models.len()-1 && p<end);
  }
  assert(models(p)==0);
  ++p;
  assert(p<=end);

  // Generate a map of jump targets
  if (p==end) return;
  libzpaq::Array<char> targets(0x10000);
  for (int i=p; i<end-1; ++i) {
    int op=models(i);
    if (op==LJ && p<end-2)
      targets[models(i+1)+256*models(i+2)]=1, ++i;
    if (op==JT || op==JF || op==JMP) {
      int addr=i+2+(models(i+1)<<24>>24)-p;
      if (addr>=0 && addr<0x10000) targets[addr]=1;
      else error("goto target out of range");
    }
    if (op%8==7) ++i;  // 2 byte instruction (LJ is 3)
  }

  // Generate instructions. The output code will not compile
  // if any ZPAQL instructions jump to the middle of a 2 or 3
  // byte instruction (legal) or out of range (legal if not executed).
  fprintf(out, "      a = input;\n");
  for (int i=p; i<end-1; ++i) {
    int op=models(i);
    assert(i-p<0x10000);
    if (targets[i-p]) {
      fprintf(out, "L%d:\n", select*100000+(i-p)); // goto label
      targets[i-p]=0;
    }
    int operand=0;
    operand=models(i+1);  // numeric operand
    if (op==JT || op==JF || op==JMP)  // label
      operand=select*100000+i+2+(operand<<24>>24)-p;
    if (op==LJ) {
      if (i<end-2)
        operand=select*100000+models(i+1)+256*models(i+2);  // label
      ++i;
    }
    if (op%8==7) ++i; // 2 byte instruction
    fprintf(out, "      ");
    fprintf(out, inst[op], operand);
    fprintf(out, "\n");
  }
}

// Search list of models for comp, return true if a match is found
bool findModel(const String& models, const String& comp) {
  if (comp.len()<8) return false;
  for (int p=0; p<models.len()-1; p+=models(p)+models(p+1)*256+2) {
    bool mismatch=false;
    for (int i=0; !mismatch && i<comp.len(); ++i)
      mismatch=i+p>=models.len() || models(i+p)!=comp(i);
    if (!mismatch) return true;
  }
  return false;
}

// Read model string from archive in format suitable for libzpaq::models[]
// If oneblock is true then read only one block
String getModels(libzpaq::Decompresser& d, bool oneblock) {
  String result;
  while (d.findBlock()) {
    String hcomp, pcomp;
    d.hcomp(&hcomp);
    if (!findModel(result, hcomp)) result+=hcomp;
    bool firstSegment=true;
    while (d.findFilename()) {
      d.readComment();
      if (firstSegment) {
        d.decompress(0);
        if (d.pcomp(&pcomp)) {
          fix_pcomp(hcomp, pcomp);
          if (!findModel(result, pcomp)) result+=pcomp;
        }
        firstSegment=false;
      }
      d.readSegmentEnd();
      if (oneblock) break;
    }
  }
  result.put(0);
  result.put(0);
  return result;
}

// Combine hcomp and pcomp into 1 or 2 models suitable for libzpaq::models[]
String combine(String hcomp, String pcomp) {
  if (pcomp!="") {
    fix_pcomp(hcomp, pcomp);
    hcomp+=pcomp;
  }
  hcomp.put(0);
  hcomp.put(0);
  return hcomp;
}

// Print models[p..] for model i
void dump(FILE* out, const String& models, int p, int n) {
  assert(models.len()>p+1);
  const int len=models(p)+models(p+1)*256+2;
  assert(models.len()>=p+len);
  fprintf(out,
  "\n"
  "  // Model %d\n  ", n);
  for (int i=0; i<len; ++i) {
    fprintf(out, "%d,", char(models(p+i)));
    if (i%16==15) fprintf(out, "\n  ");
  }
  fprintf(out, "\n");
}

// Generate C++ source code from a list of models
// Then compile and run it with argc, argv
void optimize(const String& models, int argc, char** argv) {

  // Find the command c, a, x, l, r, e
  char cmd=0;
  for (int i=0; (cmd=argv[1][i])!=0; ++i)
    if (strchr("caxlre", cmd))
      break;

  // Open output file
  String rootdir=root(argc, argv);
  String filename="zpaqopt.cpp";
  FILE* out=fopen(filename.c_str(), "w");
  if (!out) perror(filename.c_str()), exit(1);

  // Print models[]
  fprintf(out,
  "// zpaqopt.cpp generated by zpaq\n"
  "\n"
  "#include \"libzpaq.h\"\n"
  "namespace libzpaq {\n"
  "\n"
  "const char models[]={\n");
  int p, i;
  for (p=0, i=1; p<models.len()-2; p+=models(p)+models(p+1)*256+2, ++i)
    dump(out, models, p, i);
  assert(p==models.len()-2);
  assert(models(p)==0 && models(p+1)==0);
  fprintf(out, "\n  0,0};\n");  // end of list

  // Print predict()
  // Write Predictor::predict()
  fprintf(out,
    "\n"
    "int Predictor::predict() {\n"
    "  switch(z.select) {\n");
  for (p=0, i=1; p<models.len()-2; p+=models(p)+models(p+1)*256+2, ++i)
    opt_predict(out, models, p, i);
  fprintf(out,
    "    default: return %s;\n"
    "  }\n"
    "}\n"
    "\n",
    cmd!='e' ? "predict0()" : "(error(\"model not implemented\"),0)");

  // Write Predictor::update()
  fprintf(out,
    "void Predictor::update(int y) {\n"
    "  switch(z.select) {\n");
  for (p=0, i=1; p<models.len()-2; p+=models(p)+models(p+1)*256+2, ++i)
    opt_update(out, models, p, i);
  fprintf(out,
    "    default: return %s;\n"
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
    "\n",
    cmd!='e' ? "update0(y)" : "error(\"model not implemented\")");

  // Write ZPAQL::run()
  fprintf(out,
    "void ZPAQL::run(U32 input) {\n"
    "  switch(select) {\n");
  for (p=0, i=1; p<models.len()-2; p+=models(p)+models(p+1)*256+2, ++i) {
    fprintf(out, "    case %d: {\n", i);
    opt_hcomp(out, models, p, i);
    fprintf(out,
      "      break;\n"
      "    }\n");
  }
  fprintf(out,
    "    default: %s;\n"
    "  }\n"
    "}\n"
    "}\n"
    "\n",
    cmd!='e' ? "run0(input)" : "err()");


  // Set global zpaqdir
  fprintf(out,
    "const char* zpaqdir=\"");
  for (int i=0; i<rootdir.len(); ++i) {
    int c=rootdir(i);
    if (c=='\\' || c=='\"' || c=='\?' || c=='\'')
      fprintf(out, "\\%c", c);
    else if (c<32 || c>126)
      fprintf(out, "x%02X", c);
    else
      fprintf(out, "%c", c);
  }
  fprintf(out, "\";\n");

  // Close output and make sure it exists
  fclose(out);
  testfile(filename.c_str());
  fprintf(stderr, "Created %s\n", filename.c_str());

  // Make optimized self extractor
  if (cmd=='e') {
    String cmd=rootdir+"makezpsfx.bat";
    run_cmd(cmd);
    testfile("zpsfxopt.exe");
    return;
  }

  // Run makefile.bat with the same path as this program
  // to compile zpaqopt.cpp to zpaqopt.exe
  unlink("zpaqopt.exe");
  String command=rootdir+"makezpaq.bat";
  run_cmd(command);

  // Run it
  testfile("zpaqopt.exe");
  command=String(".")+slash()+"zpaqopt.exe";
  for (int i=1; i<argc; ++i) {
    command+=" ";
    command+=argv[i];
  }
  run_cmd(command);
  if (!keep_option) {
    unlink("zpaqopt.exe");
    unlink("zpaqopt.cpp");
    fprintf(stderr, "zpaqopt.cpp and zpaqopt.exe deleted\n");
  }
  exit(0);
}

#endif // ifndef OPT

/////////////////////////// Decompress ///////////////////////

// Print help message and exit
void usage() {
  fprintf(stderr, "ZPAQ v2.02 archiver, (C) 2010, Dell Inc.\n"
    "Written by Matt Mahoney, " __DATE__ ".\n"
    "This is free software under GPL v3, http://www.gnu.org/copyleft/gpl.html\n"
    "\n"
    "To compress: zpaq [nsitokv][ca][N][F[,N...]] archive [folder/] files...\n"
    "  n = don't store filenames (extraction will concatenate)\n"
    "  s = don't store SHA1 checksums (saves 20 bytes)\n"
    "  i = don't store file sizes as comments (saves a few bytes)\n"
    "  t = append locator tag to non-ZPAQ data such as zpsfx.exe\n"
    "  c = create new archive.zpaq with 1 block\n"
    "  a = or append 1 block to existing archive or archive.zpaq\n"
    "  N = compression level 1=fast, 2=mid, 3=max\n"
    "  F = or use configuration file F.cfg\n"
    "  ,N = pass numeric arguments to F.cfg\n"
    "  folder/ = store path for extraction (default = filename only)\n"
    "To list contents: zpaq [v]l archive\n"
    "To extract: zpaq [ok]x[N] archive [folder/] [files...]\n"
    "  N = extract only block N (1, 2, 3...)\n"
    "  folder/ = extract to folder (default = stored paths)\n"
    "  files... = rename extracted files (clobbers)\n"
    "      otherwise use stored names (does not clobber)\n"
    "To make self extracting archive.exe: zpaq [ok]e archive\n"
    "To debug configuration file F.cfg: zpaq [ptokv]rF[,N...] [args...]\n"
    "  p = run PCOMP (default is to run HCOMP)\n"
    "  t = trace (single step), args are numeric inputs\n"
    "      otherwise args are input, output (default stdin, stdout)\n"
    "  ,N = pass numeric arguments to F\n"
    "For all commands:\n"
    "  o = compress or decompress faster (requires C++ compiler)\n"
    "  k = with o, keep zpaqopt.cpp, zpaqopt.exe\n"
    "  v = verbose (echo F.cfg)\n"
    );
  exit(0);
}

// Open archive. filename and mode are as in fopen().
// In read or append mode check if filename exists, and if not
// then try filename.zpaq. In write mode, always add .zpaq extension
// if there is not already one.
FILE *open_archive(const char *filename, const char *mode) {
  assert(filename);
  assert(mode);
  String newname=filename;
  if (mode[0]=='w' || !exists(filename)) {
    if (newname.sub(newname.len()-5)!=".zpaq")
      newname+=".zpaq";
  }
  FILE*  f=fopen(newname.c_str(), mode);
  if (!f) perror(newname.c_str()), libzpaq::error("cannot open archive");
  switch(mode[0]) {
    case 'r': fprintf(stderr, 
      "Reading from archive %s\n", newname.c_str()); break;
    case 'w': fprintf(stderr, 
       "Created archive %s\n", newname.c_str()); break;
    case 'a': fprintf(stderr, 
       "Appending to archive %s\n", newname.c_str()); break;
  }
  return f;
}

// Reject archive filenames with absolute paths, drive letters
// or control characters or that are too long.
bool validate_filename(const char* filename) {
  int len=strlen(filename);
  if (len<1) return true;  // No name is OK
  if (len>511) return false;  // name too long
  if (strstr(filename, "../")) return false; // no backward paths
  if (strstr(filename, "..\\")) return false;
  if (filename[0]=='/' || filename[0]=='\\') return false; // no absolute path
  for (int i=0; i<len; ++i)  // no control chars or drive letters
    if ((filename[i]&255)<32 || (i==1 && filename[i]==':')) return false;
  return true;
}

// Skip n blocks
void skip_block(libzpaq::Decompresser& d, int n) {
  for (; n>0 && d.findBlock(); --n) {
    while (d.findFilename()) {
      d.readComment();
      d.readSegmentEnd();
    }
  }
}

// Remove path from filename
string strip(const string& filename) {
  for (int i=int(filename.size())-1; i>=0; --i) {
    if (filename[i]=='/' || filename[i]=='\\' || (i==1 && filename[i]==':'))
      return filename.substr(i+1);
  }
  return filename;
}

// Open filename. Depending on OS, change slashes to / or \.
// If this fails then try creating directories in its path.
// If it fails again, return 0, else return FILE*.
FILE* create(String filename) {

  // Find last slash in filename
  int slashpos=-1;
  for (int i=0; i<filename.len(); ++i)
    if (filename(i)=='/' || filename(i)=='\\')
      slashpos=i;

  // If there is no path, then open file and return
  if (slashpos<0)
    return fopen(filename.c_str(), "wb");

  // Guess the OS by counting / (Linux) or \ (Windows) in PATH
  char slashchar=slash();  // 0 if unknown

  // Change slashes in filename per OS if known.
  for (int i=0; i<filename.len(); ++i) {
    if (slashchar=='/' && filename[i]=='\\') filename[i]='/';
    if (slashchar=='\\' && filename[i]=='/') filename[i]='\\';
  }

  // Try opening file
  FILE *f=fopen(filename.c_str(), "wb");
  if (f) return f;

  // If this doesn't work, try creating a directory for it using "mkdir"
  if (slashchar) {
    string cmd = slashchar=='\\' ? "mkdir " : "mkdir -p ";
    cmd+=filename.sub(0, slashpos);
    fprintf(stderr, "\n");
    run_cmd(cmd);

    // Last try
    return fopen(filename.c_str(), "wb");
  }
  return 0;
}

// Decompress: [ovk]x[N] archive [path/] [files...]
void decompress(int argc, char** argv) {
  assert(argc>=3);

  // Get options
  bool ocmd=false;
  int blocknum=0;
  const char* cmd=argv[1];
  assert(cmd);
  while (*cmd) {
    if (*cmd=='o') ocmd=true;
    else if (*cmd=='v') verbose=true;
    else if (*cmd=='k') keep_option=true;
    else if (*cmd=='x') break;
    else usage();
    ++cmd;
  }
  if (cmd[0]!='x') usage();
  if (cmd[1]) blocknum=atoi(cmd+1);

  // Get output path if any
  const char* path=0;
  if (argc>3 && argv[3][0]) {
    const char slash=argv[3][strlen(argv[3])-1];
    if (slash=='/' || slash=='\\') {
      path=argv[3];
      fprintf(stderr, "Output folder is %s\n", path);
    }
  }

  // Open input archive
  File in(open_archive(argv[2], "rb"));
  libzpaq::Decompresser d;
  d.setInput(&in);

  // If user specifies N then skip N-1 blocks
  int block=atoi(argv[1]+1);
  if (block>0)
    skip_block(d, block-1);

  // Optimize one block or entire archive
#ifndef OPT
  if (ocmd)
    optimize(getModels(d, block!=0), argc, argv);
#endif

  // Read the archive
  File out(0);  // output file
  int filecount=0;  // number of files extracted
  libzpaq::SHA1 sha1;
  d.setSHA1(&sha1);
  while (d.findBlock()) {
    for (String filename; d.findFilename(&filename); filename="") {
      String comment;
      d.readComment(&comment);
      fprintf(stderr, "%s %s ", filename.c_str(), comment.c_str());

      // open output file.
      // If filename is empty, use the previously opened file, or if none
      // then get the filename from the command line.
      if (filename!="" || !out.f) {

        // close last file
        if (out.f) {
          fclose(out.f);
          out.f=0;
          ++filecount;
        }

        // if the user gave an output file starting at argv[3], use it instead.
        if (argc>3+(path!=0)) {
          if (filecount+3+(path!=0)>=argc) {
            fprintf(stderr, "and remaining files not extracted\n");
            goto end;
          }
          String name;
          if (path) name+=path;
          name+=argv[filecount+3+(path!=0)];
          out.f=create(name.c_str());
          if (!out.f) {
            perror(name.c_str());
            goto end;
          }
          else
            fprintf(stderr, "-> %s ", name.c_str());
        }

        // Otherwise, use the names in the archive, but don't clobber
        // or use suspicious filenames
        else {
          String newname=filename;
          if (path) newname=path+strip(filename);
          if (newname!=filename)
            fprintf(stderr, "-> %s ", newname.c_str());
          if (!path && !validate_filename(newname.c_str())) {
            fprintf(stderr, "Error: bad filename\n");
            goto end;
          }
          if (exists(newname.c_str())) {
            fprintf(stderr, "Error: won't overwrite\n");
            goto end;
          }
          else {
            out.f=create(newname.c_str());
            if (!out.f) {
              perror(newname.c_str());
              goto end;
            }
          }
        }
      }
      if (!out.f) {
        fprintf(stderr, "Output filename not specified\n");
        goto end;
      }

      // Decompress and report progress every 100 KB
      d.setOutput(&out);
      fprintf(stderr, "-> ");
      while (d.decompress(100000)) {
        for (int i=fprintf(stderr, "%1.0f ", sha1.size()); i>0; --i)
          putc('\b', stderr);
        fflush(stderr);
      }

      // Verify checksum
      char sha1string[21];
      d.readSegmentEnd(sha1string);
      bool sha1result=memcmp(sha1string+1, sha1.result(), 20);
      if (sha1string[0]) {
        if (sha1result) fprintf(stderr, "WARNING: CHECKSUM MISMATCH\n");
        else fprintf(stderr, "OK, checksum verified\n");
      }
      else fprintf(stderr, "OK, no checksum   \n");
    }
    if (block) break;
  }

  // Close files
end:
  if (out.f) fclose(out.f), ++filecount;
  fclose(in.f);
  fprintf(stderr, "%d file(s) extracted\n", filecount);
}

//////////////////////////// Compress ////////////////////////////

// Test for regular file (Linux)
static bool is_file(const char* filename) {
#ifdef unix
  struct stat st;
  return stat(filename, &st)==0 && (st.st_mode & S_IFREG);
#endif
  return true;
}

// Compress files: [onsitvk][ca][N][F[,N]...] archive [folder/] files...
static void compress(int argc, char** argv) {
  assert(argc>=3);

  // Get command options
  bool ncmd=false, scmd=false, icmd=false, // options
       tcmd=false, ocmd=false, acmd=false, ccmd=false;
  char *cmd=argv[1];
  while (cmd && cmd[0]) {
    if (cmd[0]=='v') verbose=true;
    else if (cmd[0]=='n') ncmd=true;
    else if (cmd[0]=='i') icmd=true;
    else if (cmd[0]=='s') scmd=true;
    else if (cmd[0]=='t') tcmd=true;
    else if (cmd[0]=='o') ocmd=true;
    else if (cmd[0]=='k') keep_option=true;
    else if (cmd[0]=='a') {acmd=true; break;}
    else if (cmd[0]=='c') {ccmd=true; break;}
    else usage();
    ++cmd;
  }
  ++cmd;
  if (acmd==ccmd) usage();

  // Compile config file
  String hcomp, pcomp, pcomp_cmd;
  int level=compile_cmd(cmd, hcomp, pcomp, pcomp_cmd, root(argc, argv));

#ifndef OPT
  if (ocmd && level==0)
    optimize(combine(hcomp, pcomp), argc, argv);
#endif

  // Get output path (folder) if any
  const char* path=0;
  if (argc>3 && argv[3][0]) {
    const char slash=argv[3][strlen(argv[3])-1];
    if (slash=='/' || slash=='\\') {
      path=argv[3];
      fprintf(stderr, "Folder for extraction is %s\n", path);
    }
  }

  // Compress
  libzpaq::Compressor c;
  libzpaq::SHA1 sha1, sha2;  // input, postprocessor output
  libzpaq::PostProcessor pp; // for testing pre/post equivalence
  pp.setSHA1(&sha2);
  if (hcomp.len()>5)
    pp.init(hcomp(4), hcomp(5));  // ph, pm array sizes
  String tmp=string(argv[2])+".zpaq.pre";  // preprocessed input filename

  // Compress files in argv[3...argc-1]
  int filecount=0;  // number of files compressed
  File out(0);  // output file
  double start=0;  // output byte count at start of each file
  for (int i=3+(path!=0); i<argc; ++i) {

    // Ignore directories
    if (!is_file(argv[i])) {
      fprintf(stderr, "%s: not a regular file\n", argv[i]);
      continue;
    }

    // Open input file
    File in(fopen(argv[i], "rb"));
    if (!in.f) {
      perror(argv[i]);
      continue;
    }

    // Get checksum and file size
    char comment[20]={0};  // file size
    int ch;
    while ((ch=getc(in.f))!=EOF)
      sha1.put(ch);
    rewind(in.f);
    sprintf(comment, "%1.0f", sha1.size());
    const char* sha1result=sha1.result();

    // Preprocess to a temporary file archive.zpaq.pre
    // Test by running through a PostProcessor and comparing checksums.
    // If OK, then compress the temporary file, else skip.
    // Look for preprocessor in the current directory first, then
    // in the install directory.
    if (pcomp!="") {
      fclose(in.f);
      in.count=0;
      String cmd=root(argc, argv)+pcomp_cmd+" "+argv[i]+" "+tmp;
      run_cmd(cmd);

      // Test whether post(pre(in)) == in
      in.f=fopen(tmp.c_str(), "rb");
      if (!in.f) perror(tmp.c_str()), exit(1);
      if (filecount==0) {
        pp.write(1);
        for (int i=0; i<pcomp.len(); ++i)
          pp.write(pcomp(i));
      }
      int ch;
      while ((ch=in.get())!=EOF)
        pp.write(ch);
      pp.write(-1);
      fprintf(stderr, 
        "%s -> %1.0f -> %1.0f\n", comment, in.count, sha2.size());
      if (memcmp(sha1result, sha2.result(), 20)) {
        fprintf(stderr, "pre/post check failed, skipping...\n");
        fclose(in.f);
        continue;
      }
      rewind(in.f);
      in.count=0;
    }
      
    // Open archive for first file
    if (filecount==0) {

      // Create or append archive
      out.f=open_archive(argv[2], acmd?"ab":"wb");
      c.setOutput(&out);

      // Write block header
      if (tcmd) c.writeTag();
      if (level)
        c.startBlock(level);
      else
        c.startBlock(hcomp.c_str());
    }

    // Write segment header
    string filename=strip(argv[i]);
    if (path) filename=path+filename;
    c.startSegment(ncmd?0:filename.c_str(), icmd?0:comment);
    if (filecount==0)
      c.postProcess(pcomp=="" ? 0 : pcomp.c_str());

    // Compress and report progress every 100K
    fprintf(stderr, "%s %s ", argv[i], comment);
    c.setInput(&in);
    while (c.compress(100000)) {
      for (int j=fprintf(stderr,
          "%1.0f -> %1.0f ", in.count, out.count-start); j>0; --j)
        putc('\b', stderr);
      fflush(stderr);
    }
    fprintf(stderr, "-> %1.0f               \n", out.count-start);
    start=out.count;
    fclose(in.f);
    if (pcomp!="") unlink(tmp.c_str());

    // Append SHA-1 checksum to end of segment
    c.endSegment(scmd?0:sha1result);
    ++filecount;
  }

  // End block
  if (filecount>0) {
    c.endBlock();
    fprintf(stderr, "%d file(s) compressed to %s -> %1.0f\n",
      filecount, argv[2], out.count);
    c.stat(0);
    fclose(out.f);
  }
  else
    fprintf(stderr, "Archive %s not updated\n", argv[2]);
}

// Print component statistics
int libzpaq::Predictor::stat(int) {
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
  return 0;
}     

////////////////////////// list //////////////////////////

// Decompile ZPAQL starting at s[i]
void printCode(const String& s, int i) {
  int start=i;
  for (; i<s.len()-1; ++i) {
    int op=s(i);
    assert(op>=0 && op<256);
    printf("  (%d) %s", i-start, opcodelist[op]);
    if (op==LJ) printf(" %d", s(i+1)+256*s(i+2)), i+=2;
    else if (op%8==7) {
      int n=s(++i);
      if ((op==JT || op==JF || op==JMP) && n>=128) n-=256;
      printf(" %d", n);
      if (op==JT || op==JF || op==JMP) printf(" (to %d)", i-start+n+1);
    }
    printf("\n");
  }
}

// Archive listing: [v]l archive
// v = verbose. If not verbose, show for each block the filename
// and comment. If verbose show also the SHA-1 checksum for each segment
// and show the hcomp and pcomp code for each block in a format that
// can be read back as a config file.
static void list(int argc, char** argv) {

  // Verbose?
  if (argv[1][0]=='v') verbose=true;

  // Some variables to hold values read from the archive. An archive
  // is a sequence of independent blocks. Each block describes the
  // decompression algorithm in two strings called hcomp and pcomp.
  // Each block holds one or more segments that must be decompressed
  // in order from the start of the block. Each segment has an optional
  // filename string, and optional comment string, some compressed data,
  // and an optional 20 byte SHA-1 checksum of the data before compression.
  double memory;  // required by block
  String filename, comment;  // from segment header
  char sha1string[21];  // from segment trailer
  double start=-1;  // file offset of last segment

  // Create object d to decompress ZPAQ archives.
  libzpaq::Decompresser d;

  // Set the input to the archive
  File in(open_archive(argv[2], "rb"));
  d.setInput(&in);

  // Search for the next block and return false when done.
  // If true, calculate memory required for decompression.
  for (int i=1; d.findBlock(&memory); ++i) {
    if (verbose) printf("\n");
    printf("Block %d needs %1.3f MB memory\n", i, memory/1e6);
    bool firstSegment=true;  // first segment in block?

    // Find the next segment in the block. If found, read the file
    // name from the segment header and write it to filename.
    // The argument can be any type derived from libzpaq::Writer.
    // If there are no more segments in the block, return false.
    while (d.findFilename(&filename)) {

      // Read the comment from the segment header (like filename).
      // There is no limit on how long the filename or comment might be.
      d.readComment(&comment);

      // If this is the first segment in the block, then print the
      // hcomp string. This string contains ZPAQL code which describes
      // the decompression algorithm. The argument can be any type
      // derived from Writer. The format is suitable
      // for passing to Compressor::startBlock(hcomp). The first 2
      // bytes is the length of the rest of the string in little-endian
      // (LSB, MSB) format. The maximum possible size is 65537.
      // Here we decompile the string to config file format.
      if (firstSegment) {
        if (verbose) {
          String hcomp;
          d.hcomp(&hcomp);
          if (hcomp.len()<7) error("hcomp too small");

          // Print COMP section
          printf("comp %d %d %d %d %d (hh hm ph pm n)\n",
            hcomp[2], hcomp[3], hcomp[4], hcomp[5], hcomp[6]);
          int op=7;  // pointer to hcomp
          for (int i=0; i<hcomp(6); ++i) {
            if (!compname[hcomp(op)]) error("bad component");
            printf("  %d %s", i, compname[hcomp(op)]);
            int len=libzpaq::compsize[hcomp(op)];
            if (len<1) error("bad component");
            for (int j=1; j<len; ++j) {
              if (op+j>=hcomp.len()) error("end of hcomp");
              printf(" %d", hcomp(op+j));
            }
            printf("\n");
            op+=len;
          }
          if (hcomp(op)!=0) error("missing 0 at end of hcomp");

          // Print HCOMP section
          printf("hcomp\n");
          printCode(hcomp, op+1);

          // Decompress 0 bytes. We need to do this before getting the
          // pcomp string because it is compressed prior to the data in
          // the beginning of the first segment using the compression
          // algorithm described in hcomp. decompress(0) has the effect
          // of decompressing this string but stopping before decompressing
          // any data. On the other hand, decompress(n) would decompress
          // up to n bytes and return true if there is more data remaining.
          // d.decompress(-1) or d.decompress() would decompress the whole
          // setment and return false. The decompressed output would go
          // to the destination set by d.setOutput().
          d.decompress(0);

          // Print the pcomp string if present. pcomp describes a
          // postprocessing algorithm in ZPAQL code. It is optional.
          // If omitted, then the decompressed data is output directly.
          // pcomp(w) returns true if a string is present and writes
          // it to w, where w is any type derived from Writer.
          // If no pcomp string is present, then it returns
          // false without writing anything. If present, the output
          // format is suitable for passing to Compressor::postProcess(pcomp).
          // The first 2 bytes of the string is the length of the rest
          // of the string in little-endian format. The maximum output
          // is 65537 bytes. Here we decompile it to config file format.
          String pcomp;
          if (!d.pcomp(&pcomp))
            printf("post\n  0\nend\n\n");
          else {
            printf("pcomp (?) ;\n");
            printCode(pcomp, 2);
            printf("end\n\n");
          }

          // Display what built in optimized model was detected
          printf("Compression model %d, postprocessing model %d\n",
            d.getModel(), d.getPostModel());
        }
        firstSegment=false;
      }

      // Read the SHA-1 checksum at the end of the segment, skipping
      // any remaining compressed data. The checksum is optional.
      // If present, then a 1 will be written to sha1string[0]
      // and the 20 byte checksum will be written to sha1string[1] through
      // sha1string[20]. If there is no checksum, then a 0 will be
      // written to sha1string[0].
      d.readSegmentEnd(sha1string);
      if (verbose) {
        printf("  ");
        if (sha1string[0]) {
          for (int i=1; i<21; ++i)
            printf("%02x", sha1string[i]&255);
        }
      }

      // Write the filename and comment. We have to clear the
      // strings afterward because we defined put() to append bytes.
      printf("  %s %s -> %1.0f\n", filename.c_str(),
         comment.c_str(), in.count-start);
      start=in.count;
      filename=comment="";
    }
  }
  fclose(in.f);
  printf("\n");
}


//////////////////////////// run ///////////////////////////

// Execute program input and show progress
namespace libzpaq {
int ZPAQL::step(U32 input, int ishex) {
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
  return 0;
}
}  // end namespace libzpaq

// Convert decimal or hex string to int
int ntoi(const char* s) {
  int n=0, base=10, sign=1;
  for (; *s; ++s) {
    int c=*s;
    if (isupper(c)) c=tolower(c);
    if (!n && c=='x') base=16;
    else if (!n && c=='-') sign=-1;
    else if (c>='0' && c<='9') n=n*base+c-'0';
    else if (base==16 && c>='a' && c<='f') n=n*base+c-'a'+10;
    else break;
  }
  return n*sign;
}

// Debug config file: [opvtk]rF[,N...] [args...]
// p=run PCOMP, v=verbose, t=trace once per numeric arg
// otherwise args are output, input (default stdout, stdin),
// h=trace in hexadecimal, o=generate zpaqopt.h.
void run(int argc, char** argv) {
  assert(argc>=2);

  // Get options
  bool ocmd=false, pcmd=false, tcmd=false;
  char *cmd=argv[1];
  assert(cmd);
  while (cmd[0]) {
    if (cmd[0]=='p') pcmd=true;
    else if (cmd[0]=='o') ocmd=true;
    else if (cmd[0]=='v') verbose=true;
    else if (cmd[0]=='t') tcmd=true;
    else if (cmd[0]=='k') keep_option=true;
    else if (cmd[0]=='r') break;
    else usage();
    ++cmd;
  }
  ++cmd; // now points config file name
  if (!cmd[0]) usage();

  // Parse comma separated arguments after config file (now in cmd)
  String hcomp, pcomp, pcomp_cmd;
  if (compile_cmd(cmd, hcomp, pcomp, pcomp_cmd, root(argc, argv)))
    error("no config file");

#ifndef OPT
  if (ocmd)
    optimize(combine(hcomp, pcomp), argc, argv);
#endif

  // Initialze virtual machine
  libzpaq::ZPAQL z;
  if (pcmd) {
    if (pcomp.len()<2) error("no PCOMP section");
    fix_pcomp(hcomp, pcomp);
    z.read(&pcomp);
    z.initp();
  }
  else {
    z.read(&hcomp);
    z.inith();
  }

  // Run the program
  if (tcmd) {  // trace with numeric args
    for (int i=2; i<argc; ++i) 
      z.step(ntoi(argv[i]), tolower(argv[i][0])=='x');
  }
  else {  // run F input output
    FILE *in=stdin;
    File out(stdout);
    z.output=&out;
    if (argc>2) {
      in=fopen(argv[2], "rb");
      if (!in) perror(argv[2]), exit(1);
    }
    if (argc>3) {
      out.f=fopen(argv[3], "wb");
      if (!out.f) perror(argv[3]), exit(1);
    }
    int c;
    while ((c=getc(in))!=EOF)
      z.run(c);
    if (pcmd) z.run(-1);
  }
}

////////////////////////////// sfx /////////////////////////

// This code is for making self extracting archives

// Append a file
void copy(String from, String to) {
  fprintf(stderr, "Appending from %s to %s\n", from.c_str(), to.c_str());

  // Open files
  FILE* in=fopen(from.c_str(), "rb");
  if (!in) perror(from.c_str()), exit(1);
  FILE* out=fopen(to.c_str(), "ab");
  if (!out) perror(to.c_str()), exit(1);

  // Copy
  int c;
  while ((c=getc(in))!=EOF)
    putc(c, out);

  // Close files
  fclose(out);
  fclose(in);
}

// Create self extracting archive.exe: [ok]e archive
void sfx(int argc, char** argv) {

  // Get command options
  bool ocmd=false;
  char *cmd=argv[1];
  while (cmd && cmd[0]) {
    if (cmd[0]=='o') ocmd=true;
    else if (cmd[0]=='k') keep_option=true;
    else if (cmd[0]=='e') break;
    else usage();
    ++cmd;
  }

  // Get file names
  String rootdir=root(argc, argv);
  String sfx=rootdir+"zpsfx.exe";
  String input=argv[2];
  if (!exists(input.c_str()))
    input+=".zpaq";
  testfile(input.c_str());
  String output=input;
  if (output.sub(output.len()-5)==".zpaq")
    output=output.sub(0, output.len()-5);
  output+=".exe";

  // Optimize archive to zpsfxopt.exe
#ifndef OPT
  if (ocmd) {
    libzpaq::Decompresser d;
    File in(open_archive(argv[2], "rb"));
    d.setInput(&in);
    optimize(getModels(d, false), argc, argv);
    fclose(in.f);
    sfx="zpsfxopt.exe";
  }
#endif

  // Make self extracting archive
  unlink(output.c_str());
  copy(sfx, output);
  copy(rootdir+"zpsfx.tag", output);
  copy(input, output);
  testfile(output.c_str());
}

///////////////////////////// Main ///////////////////////////

// Command syntax as in usage()
int main(int argc, char** argv) {

  // Check usage
  if (argc<2) 
    usage();

  // Find the command c, a, x, l, r, e
  char cmd=0;
  for (int i=0; (cmd=argv[1][i])!=0; ++i)
    if (strchr("caxlre", cmd))
      break;

  // Do the command
  if (argc>=3 && (cmd=='a' || cmd=='c'))
    compress(argc, argv);
  else if (argc>=3 && cmd=='x')
    decompress(argc, argv);
  else if (argc>=3 && cmd=='l')
    list(argc, argv);
  else if (cmd=='e')
    sfx(argc, argv);
  else if (cmd=='r')
    run(argc, argv);
  else
    usage();

  // Print time used
  fprintf(stderr, "Time %1.2f sec.\n", double(clock())/CLOCKS_PER_SEC);
  return 0;
}
