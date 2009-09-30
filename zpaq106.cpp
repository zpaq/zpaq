/*  zpaq v1.06 archiver and file compressor.

(C) 2009, Ocarina Networks, Inc.
    Written by Matt Mahoney, matmahoney@yahoo.com, Sept. 29, 2009.

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

Compressed archives created with this program are readable by this and
any other ZPAQ level 1 compliant decompressor including the unzpaq
reference decoder found at http://mattmahoney.net/dc/
The archive format is described in the ZPAQ specification also found
at this website.


Command summary
---------------

To compress to new archive: zpaq [pnsivt]c[F] archive files...
To append to archive:       zpaq [pnsivt]a[F] archive files...
Optional modifiers:
  p = store filename paths in archive
  n = don't store filenames (extractor will append to last named file)
  s = don't store SHA1 checksums (saves 20 bytes)
  i = don't store file sizes as comments (saves a few bytes)
  v = verbose
  t = append locator tag to non-ZPAQ data
  F = use options in configuration file F (min.cfg, max.cfg)
To list contents: zpaq [v]l archive
  v = verbose
To extract: zpaq [pnt]x[N] archive [files...]
  p = extract to stored paths instead of current directory
  n = extract unnamed segments as separate files (for debugging)
  t = don't post-process (for debugging)
  N = extract only block N (1, 2, 3...)
  files... = rename extracted files (clobbers)
      otherwise use stored names (does not clobber)
To debug configuration file F: zpaq [pvth]rF [args...]
  p = run PCOMP (default is to run HCOMP)
  v = verbose compile and show initialization lists
  t = trace (single step), args are numeric inputs
      otherwise args are input, output (default stdin, stdout)
  h = trace display in hexadecimal
To make self extracting archive: append to a copy of zpaqsfx.exe


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

List the archive contents.

  zpaq x archive

Extract the contents of the archive. New files are created and named
according to the stored filenames. Does not clobber existing files.
Extracts to current directory.

  zpaq x archive files...

Extract files and renames in the order they were added to the
archive. Clobbers any already existing output files. The number
of files extracted is the smaller of the number of filenames
on the command line or the number of files in the archive.
No restrictions on file names.


Archive format
--------------

A ZPAQ archive consists of a sequence of blocks that can be
decompressed independently. Each block contains one or more
segments that must be decompressed in sequence from the start
of the block. A block header describes the compression algorithm.
A segment contains an optional filename field, an optional comment
string, the compressed file, and optionally the SHA1 checksum of the
data prior to compression. The "c" and "a" commands create or append
one block with each file in a separate segment. The "l" and "x"
commands list or extract all of the blocks in the order they appear.

An archive may have other data before the first block, such as a
stub for a self extracting archive. All ZPAQ commands will work
with these.


Self extracting archives
------------------------

The program zpaqsfx.exe will read itself and list or extract
an archive appended to it. It has a size of about 15 KB.

To create a self extracting archive.exe:

  zpaq c archive files...
  copy/b zpaqsfx.exe+archive archive.exe
or
  copy zpaqsfx.exe archive.exe
  zpaq a archive.exe files...

To list contents:

  zpaq l archive.exe
or
  archive.exe

To extract (does not clobber):

  zpaq x archive.exe
or
  archive.exe x

To extract and rename (clobbers):

  zpaq x archive.exe files...
or
  archive.exe x files...

Self extracting archives don't compress, don't support
any other options, and don't check for absolute paths
in file names.


Compression options
-------------------

  zpaq {p|n|s|i|v|t}*{c|a}[F] archive files...

Create or append archive with optional modifiers and an optional
configuration file F. Three files are included:

  min.cfg - for fast but poor compression.
  max.cfg - for slow but good compression.
  mid.cfg - for moderate speed and compression (default).

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

Do not store filenames. The effect is that the decompressor
will append each unnamed segment to the previous file, which
might be extracted from an earlier block. If the first segment
of the first block is unnamed, then decompression will fail
unless a file name is given.

  s

Do not store SHA1 checksums. This saves 20 bytes. The decompressor
will not check the output for errors.

  i

Do not store anything in the comment field. Normally the input
file size is stored as a decimal string, taking a few bytes.
The comment field has no effect on the program except that
it is displayed by the "l" and "x" commands.

  v

Verbose. Show config file F (if present) as it is compiled.
This is useful for error checking.

  t

Append a locator tag to non-ZPAQ data. The tag is a string of
13 bytes that allows ZPAQ and UNZPAQ to find the start
of a sequence of ZPAQ blocks embedded in other data.
zpaqsfx.exe already has this tag at the end. However, if a
new stub is compiled from the source then the t command should
be used when appending the first file.


List options
------------

  zpaq vl archive

Verbose. Show detailed contents of archive.


Extraction options
------------------

  zpaq {p|n|t}*x[N] archive [files...]

p means extract using stored paths if present. The default is
to extract to the current directory regardless of how the file
names are stored. [files...] overrides.

n means to ignore stored filenames and extract each segment
as a different file even if it already is unnamed. Normally
an unnamed segment is appended to the last named segment.
If the number of segments is different from the number of files
named, then the smaller number of files is extracted.

t means extract without postprocessing (for debugging).

N means to extract only from block number N, where 1 is the
first block. Otherwise all blocks are extracted. 

For example:

  zpaq c arc.zpaq f1 f2
  zpaq na arc.zpaq f3 f4

creates an archive with two named files f1 and f2 in block 1
and two unnamed segments in block 2.

  zpaq x arc.zpaq

would extract file f1 and f2+f3+f4 to f2.

  zpaq x arc.zpaq f5 f6 f7 f8

would extract f1 to f5, and f2+f3+f4 to f6.

  zpaq nx arc.zpaq f5 f6 f7 f8

would extract f1 to f5, f2 to f6, f3 to f7, f4 to f8.

  zpaq x2 arc.zpaq

would fail because the first file in block 2 is unnamed.

  zpaq x2 arc.zpaq f3 f4

would extract f3+f4 to f3 and not create f4.

  zpaq nx2 arc.zpaq f3 f4

would extract f3 and f4.


Development options
-------------------

  zpaq {v|p|t|h}*rF [args...]

Run the ZPAQL program in HCOMP section of configuration file F.
The program is run once for each byte of input from the file
named in the second argument and once at EOF with the input byte
(or -1) in the A register. Output is to the file named in the first
argument. If run with no arguments then take input from stdin
and output to stdout. Modifiers:

  v

Compile in verbose mode. Display contents of program to show jump
addresses and a C style array initializations of the two
headers. Also useful for locating compile errors.

  p

Run the PCOMP section rather than HCOMP.

  t

Trace (single step) the program. The arguments should be numbers
rather than file names. The program is run once for each argument
with the value in the A register. As each instruction is executed
the register contents are shown. At HALT, memory contents are
displayed.

  h

When tracing, display register contents in hexadecimal (instead
of decimal).


Configuration files
-------------------

ZPAQ uses a configurable compression algorithm based on
bitwise prediction and arithmetic coding, and optional
pre- and post-processing. The algorithm is described
precisely in http://mattmahoney.net/dc/zpaq.pdf

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
leads to better compression). Bits are coded in MSB
to LSB order.

HCOMP - a program that is called once for each byte of
uncompressed data with that byte as input, and outputs
an array of 32-bit contexts, one for each component
in the COMP section. The program is written in ZPAQL,
a sandboxed assembler-like language designed for small size
and fast interpretation.

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
output with the input. If they don't match, an error occurs.
If they do, then the input files are preprocessed and compressed,
along with the postprocessing program that will be used to
invert the preprocessing transform. The compression model
described in the COMP and HCOMP sections sees a 1 as the
first byte to indicate that the decoded data should be
postprocessed before output. This is followed by a 2 byte
program length (LSB first), the ZPAQL postprocessor code, and a
concatenation of the preprocessed input files.

The preprocessor is an external program. It is not needed for
decompression so it is not saved in the archive. It expects to be
called with one input filename as its last argument, and to write
its output to a temporary file named "$zpaq.pre". ZPAQ will delete
this file when done with it. The preprocessor is called once
for each input filename. If it needs to save any state information
between calls, it is expected to save it in a file named
"$zpaq.tmp". ZPAQ will delete this file before calling the
preprocessor for the first time, and again after the last time
if there are no errors.

The postprocessor is a ZPAQL program that is called once for each
input byte and once with input EOS (-1) at the end of each segment.
The program is initialized at the beginning of a block but
maintains state information between segments within the same
block. Its input is from $zpaq.pre during compression testing
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
  PCOMP preprocessor-command (on a line by itself with no comments)
    (postprocessor program, memory size = ph, pm)
  END

Configuration files mostly are free format (all white space is
the same) and not case sensitive. They may contain comments in
((nested) parenthesis). The exception is the preprocessor
command which has to be on a line by itself with no comments.

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
form ln(p(1)/p(0)). where p(0) and p(1) are the model's estimated
probability that the next bit will be a 0 or 1, respectively.
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
and predicting whatever bit came next with a confidence
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

Averages the predictions of components j and k (must precede i)
adaptively. The weight is selected from a table of size 2^s
by the low s bits of H[i] added to the masked previously coded
bits of the current byte (an 8 bit value). A mask of 255 includes
the current byte, and a mask of 0 excludes it. (Other masks
are rarely useful). The adaptation rate is selectable. Typical
values are around 8 to 32. Lower values are best for stationary
sources. Higher rates are more adaptive. A MIX2 generally gives
better compression than AVG but at a cost in speed and memory.
Uses 2^(s+2) bytes of memory.

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
however. The input from component j and the context are
mapped to a 2^s by 64 CM table by quantizing the prediction and
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
  MIX2 s j k rate mask   s               2^(s+2)
  MIX s j m rate mask    s               m*2^(s+2)
  ISSE s j               s+10            2^(s+6)
  SSE s j start limit    s               2^(s+8)


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
- LJ N (long jump)
  - where N is in (0...65535).
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
which assigns A to B through R3. These registers can only be
assigned from A or to A, B, C, or D.

ERROR causes an error like an undefined instruction, but is
not reserved for future use (ZPAQ level 2) like other undefined
instructions.

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


Pre/Post processing
-------------------

The PCOMP/POST section has the form:

  POST 0 END

to indicate no preprocessing or postprocessing, or

  PCOMP preprocessor-command
    (postprocessing code)
  END

to preprocess with an external program and to invert the transform
with postprocessing code written in ZPAQL. The preprocessing
command must be on a line by itself after PCOMP with no comments.
The command may contain spaces or options. The program is expected
to take as two additional arguments an input file and an output
file. ZPAQ will call the program by appending the input file and
a temporary file "archive.$zpaq.pre" formed by appending the
extension to the archive name. If the program needs to save
any state information then it should do so in a file named
"archive.$zpaq.tmp" (i.e. replace ".pre" with ".tmp" in the output filename).
ZPAQ will delete this file before compressing the first file to
initialize the state of the preprocessor, and again after compressing
the last file to clean up. It will also delete archive.$zpaq.pre
before and after compressing each file.

Before each file is compressed, ZPAQ will verify that the transformed
data in archive.$zpaq.pre will be converted back to the original input file
by inputting archive.$zpaq.pre to the ZPAQL program in PCOMP and comparing
its output to the original input. If the output is verified then 
the file is compressed. Otherwise it is skipped. The algorithm is:

  Command: zpaq {pnsiv}*{ca}F archive inputfiles...

  if F then compile header Z from F else use default header Z
  If "c" then open archive for write
  If "a" then open archive for append
  Delete archive.$zpaq.tmp
  FIRST = true
  For each inputfile FILENAME loop
    Open FILENAME as IN
    If open fails then continue
    CHECK1 = SHA1(IN)
    SIZE = |IN|
    if Z.PCOMP then
      Close IN
      Delete archive.$zpaq.pre
      Run Z.preprocessor-command FILENAME
      CHECK2 = SHA1(Z.PCOMP(archive.$zpaq.pre, EOS))
      if CHECK1 != CHECK2 then continue
      Open archive.$zpaq.pre as IN
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
  Delete archive.$zpaq.tmp, archive.$zpaq.pre

The preprocessor-command must appear on a line by itself in the
config file. The command may not exceed 511 bytes. It is not possible to
compress a file name archive.$zpaq.pre or archive.$zpaq.pre.tmp
(depending on the archive name) because these files
might be overwritten or deleted.

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
  PCOMP caesar 5
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
To turn off assertion checking (faster), compile with -DNDEBUG

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <math.h>
#include <assert.h>

const int LEVEL=1;  // ZPAQ level 0=experimental 1=final

// 1, 2, 4 byte unsigned integers
typedef unsigned char U8;
typedef unsigned short U16;
typedef unsigned int U32;

// Print an error message and exit
void error(const char* msg="") {
  fprintf(stderr, "\nError: %s\n", msg);
  exit(1);
}

// An Array of T is cleared and aligned on a 64 byte address
//   with no constructors called. No copy or assignment.
// Array<T> a(n, ex=0);  - creates n<<ex elements of type T
// a[i] - index
// a(i) - index mod n, n must be a power of 2
// a.size() - gets n
template <class T>
class Array {
private:
  T *data; // user location of [0] on a 64 byte boundary
  int n;   // user size-1
  int offset;  // distance back in bytes to start of actual allocation
  void operator=(const Array&);  // no assignment
public:
  Array(int sz=0, int ex=0): data(0), n(-1), offset(0) {
    resize(sz, ex);} // [0..sz-1] = 0
  Array(const Array&);  // copy
  void resize(int sz, int ex=0); // change size, erase content to zeros
  ~Array() {resize(0);}  // free memory
  int size() const {return n+1;}  // get size
  T& operator[](int i) {assert(n>=0 && i>=0 && U32(i)<=U32(n)); return data[i];}
  T& operator()(int i) {assert(n>=0 && (n&(n+1))==0); return data[i&n];}
};

// Copy
template<class T>
Array<T>::Array(const Array<T>& a): data(0), n(-1), offset(0) {
  resize(a.size());
  if (size()>0)
    memcpy(data, a.data, size()*sizeof(T));
}

// Change size to sz<<ex elements of 0
template<class T>
void Array<T>::resize(int sz, int ex) {
  while (ex>0) {
    if (sz<0 || sz>=(1<<30)) fprintf(stderr, "Array too big\n"), exit(1);
    sz*=2, --ex;
  }
  if (sz<0) fprintf(stderr, "Array too big\n"), exit(1);
  if (n>-1) {
    assert(offset>0 && offset<=64);
    assert((char*)data-offset);
    free((char*)data-offset);
  }
  n=-1;
  if (sz<=0) return;
  n=sz-1;
  data=(T*)calloc(64+(n+1)*sizeof(T), 1);
  if (!data) fprintf(stderr, "Out of memory\n"), exit(1);
  offset=64-int((long)data&63);
  assert(offset>0 && offset<=64);
  data=(T*)((char*)data+offset);
}
/*
// Concatenate string s to a, error if not enough room
void cat(Array<char>& a, const char* s) {
  int len=strlen(&a[0]);
  if (len+int(strlen(s))>=a.size()) error("string full");
  strcpy(&a[len], s);
}
*/
// a=s1+s2+s3+s4+s5+s6 (concatenate 1 to 6 strings to a)
const char* cat(Array<char>& a, const char* s1, const char* s2="",
                const char* s3="", const char* s4="",
                const char* s5="", const char* s6="") {
  int len1=strlen(s1);
  int len2=strlen(s2);
  int len3=strlen(s3);
  int len4=strlen(s4);
  int len5=strlen(s5);
  int len6=strlen(s6);
  a.resize(len1+len2+len3+len4+len5+len6+1);
  strcpy(&a[0], s1);
  strcpy(&a[len1], s2);
  strcpy(&a[len1+len2], s3);
  strcpy(&a[len1+len2+len3], s4);
  strcpy(&a[len1+len2+len3+len4], s5);
  strcpy(&a[len1+len2+len3+len4+len5], s6);
  return &a[0];
}

//////////////////////////// SHA-1 //////////////////////////////

// The SHA1 class is used to compute segment checksums.
// SHA-1 code modified from RFC 3174.
// http://www.faqs.org/rfcs/rfc3174.html

enum
{
    shaSuccess = 0,
    shaNull,            /* Null pointer parameter */
    shaInputTooLong,    /* input data too long */
    shaStateError       /* called Input after Result */
};
const int SHA1HashSize=20;

class SHA1 {
  U32 Intermediate_Hash[SHA1HashSize/4]; /* Message Digest  */
  U32 Length_Low;            /* Message length in bits      */
  U32 Length_High;           /* Message length in bits      */
  int Message_Block_Index;   /* Index into message block array */
  U8 Message_Block[64];      /* 512-bit message blocks      */
  int Computed;              /* Is the digest computed?         */
  int Corrupted;             /* Is the message digest corrupted? */
  U8 result_buf[20];         // Place to put result
  void SHA1PadMessage();
  void SHA1ProcessMessageBlock();
  U32 SHA1CircularShift(int bits, U32 word) {
     return (((word) << (bits)) | ((word) >> (32-(bits))));
  }
  int SHA1Reset();   // Initalize
  int SHA1Input(const U8 *, unsigned int n);  // Hash n bytes
  int SHA1Result(U8 Message_Digest[SHA1HashSize]);  // Store result
public:
  SHA1() {SHA1Reset();}  // Begin hash
  void put(int c) {  // Hash 1 byte
    U8 ch=c;
    SHA1Input(&ch, 1);
  }
  int result(int i);  // Finish and return byte i (0..19) of SHA1 hash
};

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
        Length_Low = 0;    /* and clear length */
        Length_High = 0;
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
  POST=256,PCOMP,END,
  IF,IFNOT,ELSE,ENDIF,DO,WHILE,UNTIL,FOREVER,
  IFL,IFNOTL,ELSEL} CompType;
static const int compsize[256]={0,2,3,2,3,4,6,6,3,5};
static const char* compname[]=
  {"","const","cm","icm","match","avg","mix2","mix","isse","sse",0};

// Opcodes from ZPAQ spec, table 1, without operands (N, M)".
static const char* opcodelist[271]={
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
"while","until","forever","ifl","ifnotl","elsel",0};

// A ZPAQL machine HCOMP or PCOMP.
class ZPAQL {
public:
  ZPAQL();
  void load(int cn, int hn, const U8* data); // init from data[cn+hn]
  void read(FILE* in);    // Read header from archive
  void write(FILE* out);  // Write header to archive
  U32 compile(FILE* in);  // Create header from config file
  void list();            // Display header contents
  void inith();           // Initialize as HCOMP
  void initp();           // Initialize as PCOMP
  void run(U32 input);    // Execute with input
  void step(U32 input, bool ishex); // Execute while displaying progress
  void prints();          // Print header as an array initialization
  double memory();        // Return memory requirement in bytes
  int ph() {return header[4];}  // ph
  int pm() {return header[5];}  // pm
  FILE* output;           // Destination for OUT instruction, or 0 to suppress
  SHA1* sha1;             // Points to checksum computer
  bool verbose;           // Show config file during compile?
  Array<char> pcomp_cmd;  // Command string after compiling PCOMP
  friend class Predictor;
  friend class PostProcessor;
  friend void compress(int argc, char** argv);
private:

  // ZPAQ1 block header
  int hsize;          // Header size
  Array<U8> header;   // hsize[2] hh hm ph pm n COMP (guard) HCOMP (guard)
  int cend;           // COMP in header[7...cend-1] (empty for PCOMP)
  int hbegin, hend;   // HCOMP in header[hbegin...hend-1]
  int pbegin, pend;   // PCOMP in header[pbegin...pend-1]

  // Machine state for executing HCOMP
  Array<U8> m;        // memory array M for HCOMP
  Array<U32> h;       // hash array H for HCOMP
  Array<U32> r;       // 256 element register array
  U32 a, b, c, d;     // machine registers
  int f;              // condition flag
  int pc;             // program counter
  int pc_start;       // execution start, hbegin or pbegin
  int pc_end;         // hend or pend

  // Support code
  CompType compile_comp(FILE* in, int& begin, int& end);
    // compile HCOMP or PCOMP section of config file
  const char* token(FILE* in);  // read and print a token or 0 at EOF
  int rtoken(FILE* in, const char* list[]=0);  // read token in list -> index
  void rtoken(FILE* in, const char* s);  // read a token that must be s
  int rtoken(FILE* in, int low, int high);  // read token in low...high
  void init(int hbits, int mbits);  // initialize H and M sizes
  int execute();  // execute 1 instruction, return 0 after HALT, else 1
  void div(U32 x) {if (x) a/=x; else a=0;}
  void mod(U32 x) {if (x) a%=x; else a=0;}
  void swap(U32& x) {a^=x; x^=a; a^=x;}
  void swap(U8& x)  {a^=x; x^=a; a^=x;}
  void err();  // exit with run time error
};

// Constructor
ZPAQL::ZPAQL() {
  hsize=cend=hbegin=hend=pbegin=pend=0;
  pc_start=pc_end=0;
  a=b=c=d=f=pc=0;
  verbose=true;
  output=0;
  sha1=0;
}

// Copy cn bytes of COMP and hn bytes of HCOMP from data to header
void ZPAQL::load(int cn, int hn, const U8* data) {
  assert(header.size()==0);
  assert(cn>=7);
  assert(hn>=1);
  assert(data);
  cend=cn;
  hbegin=cend+128;
  hend=hbegin+hn;
  header.resize(hend+144);
  for (int i=0; i<cn; ++i)
    header[i]=data[i];
  for (int i=0; i<hn; ++i)
    header[hbegin+i]=data[cn+i];
  hsize=cn+hn-2;
  assert(header[0]+256*header[1]==hsize);
  assert(header[cend-1]==0);
  assert(header[hend-1]==0);
}

// Read header
void ZPAQL::read(FILE* in) {
  assert(in);

  // Get header size and allocate
  hsize=getc(in);
  hsize+=getc(in)*256;
  header.resize(hsize+300);
  cend=hbegin=hend=0;
  header[cend++]=hsize&255;
  header[cend++]=hsize>>8;
  while (cend<7) header[cend++]=getc(in); // hh hm ph pm n

  // Read COMP
  int n=header[cend-1];
  for (int i=0; i<n; ++i) {
    int type=getc(in);  // component type
    if (type==EOF) error("unexpected end of file");
    header[cend++]=type;  // component type
    int size=compsize[type];
    if (size<1) error("Invalid component type");
    if (cend+size>header.size()-8) error("COMP list too big");
    for (int j=1; j<size; ++j)
      header[cend++]=getc(in);
  }
  if ((header[cend++]=getc(in))!=0) error("missing COMP END");

  // Insert a guard gap and read HCOMP
  hbegin=hend=cend+128;
  while (hend<hsize+129) {
    assert(hend<header.size()-8);
    int op=getc(in);
    if (op==EOF) error("unexpected end of file");
    header[hend++]=op;
  }
  if ((header[hend++]=getc(in))!=0) error("missing HCOMP END");

  assert(cend>=7 && cend<header.size());
  assert(hbegin==cend+128 && hbegin<header.size());
  assert(hend>hbegin && hend<header.size());
  assert(hsize==header[0]+256*header[1]);
  assert(hsize==cend-2+hend-hbegin);
}

// Write header
void ZPAQL::write(FILE* out) {
  assert(out);
  assert(cend>=7 && cend<header.size());
  assert(hbegin==cend+128 && hbegin<header.size());
  assert(hend>hbegin && hend<header.size());
  assert(hsize==header[0]+256*header[1]);
  assert(hsize==cend-2+hend-hbegin);
  fwrite(&header[0], 1, cend, out);
  fwrite(&header[hbegin], 1, hend-hbegin, out);
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
CompType ZPAQL::compile_comp(FILE *in, int& begin, int& end) {
  int op=0;
  Stack<U16> if_stack(1000), do_stack(1000);  // IF, DO saved addresses
  if (verbose) printf("\n");
  int indent=0;  // program listing indentation
  while (end<0x10000) {
    if (verbose) {
      printf("(%4d) ", end-begin);
      for (int i=0; i<indent; ++i) printf("  ");
    }
    op=rtoken(in, opcodelist);
    if (op==POST || op==PCOMP || op==END) break;
    int operand=-1; // 0...255 if 2 bytes
    int operand2=-1;  // 0...255 if 3 bytes
    if (op==IF) {
      op=JF;
      operand=0; // set later
      if_stack.push(end+1); // save jump target location
      ++indent;
    }
    else if (op==IFNOT) {
      op=JT;
      operand=0;
      if_stack.push(end+1); // save jump target location
      ++indent;
    }
    else if (op==IFL || op==IFNOTL) {  // long if
      if (op==IFL) header[end++]=JT;
      if (op==IFNOTL) header[end++]=JF;
      header[end++]=3;
      op=LJ;
      operand=operand2=0;
      if_stack.push(end+1);
      if (verbose)
        printf("(%s 3 (%d 3) lj 0 0)",
          opcodelist[header[end-2]], header[end-2]);
      ++indent;
    }
    else if (op==ELSE || op==ELSEL) {
      if (op==ELSE) op=JMP, operand=0;
      if (op==ELSEL) op=LJ, operand=operand2=0;
      int a=if_stack.pop();  // conditional jump target location
      assert(a>begin && a<end);
      if (header[a-1]!=LJ) {  // IF, IFNOT
        assert(header[a-1]==JT || header[a-1]==JF || header[a-1]==JMP);
        int j=end-a+1+(op==LJ); // offset at IF
        assert(j>=0);
        if (j>127) error("IF too big, try IFL, IFNOTL");
        header[a]=j;
        if (verbose) printf("((%d) %s %d (to %d)) ",
          a-begin-1, opcodelist[header[a-1]], j, end-begin+2);
      }
      else {  // IFL, IFNOTL
        int j=end-begin+2+(op==LJ);
        assert(j>=0);
        header[a]=j&255;
        header[a+1]=(j>>8)&255;
        if (verbose) printf("((%d) lj %d) ", a-begin-1, j);
      }
      if_stack.push(end+1);  // save JMP target location
    }
    else if (op==ENDIF) {
      int a=if_stack.pop();  // jump target address
      assert(a>begin && a<end);
      int j=end-a-1;  // jump offset
      assert(j>=0);
      if (header[a-1]!=LJ) {
        assert(header[a-1]==JT || header[a-1]==JF || header[a-1]==JMP);
        if (j>127) error("IF too big, try IFL, IFNOTL, ELSEL\n");
        header[a]=j;
        if (verbose) printf("((%d) %s %d (to %d))\n",
          a-begin-1, opcodelist[header[a-1]], j, end-begin);
      }
      else {
        j=end-begin;
        header[a]=j&255;
        header[a+1]=(j>>8)&255;
        if (verbose) printf("((%d) lj %d)\n", a-1, j);
      }
      --indent;
    }
    else if (op==DO) {
      do_stack.push(end);
      if (verbose) printf("\n");
      ++indent;
    }
    else if (op==WHILE || op==UNTIL || op==FOREVER) {
      int a=do_stack.pop();
      assert(a>=begin && a<end);
      int j=a-end-2;
      assert(j<=-2);
      if (j>=-127) {  // backward short jump
        if (op==WHILE) op=JT;
        if (op==UNTIL) op=JF;
        if (op==FOREVER) op=JMP;
        operand=j&255;
        if (verbose)
          printf("(%s %d) (to %d)) ", opcodelist[op], j, end-begin+2+j);
      }
      else {  // backward long jump
        j=a-begin;
        assert(j>=0 && j<end-begin);
        if (op==WHILE) {
          header[end++]=JF;
          header[end++]=3;
          if (verbose) printf("(jf 3) ");
        }
        if (op==UNTIL) {
          header[end++]=JT;
          header[end++]=3;
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
        if (verbose) printf("(to %d) ", end-begin+2+operand);
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
      header[end++]=op;
    if (operand>=0)
      header[end++]=operand;
    if (operand2>=0)
      header[end++]=operand2;
    if (end-begin>=0x10000 || end>header.size()-144)
      error("program too big");
  }
  header[end++]=0; // END
  return CompType(op);
}

// Compile a configuration file and store the result in header.
// Return the POST command as a 32 bit value with a single
// letter in the low byte and up to 3 numeric parameters packed
// into the higher bytes. For example: "POST A 100" returns
// 'A' + 100*256. If there is PCOMP instead of POST then
// store the program in header[pbegin...pend-1] and return 0.
U32 ZPAQL::compile(FILE* in) {

  // Allocate header
  header.resize(0x21000); // includes hsize
 
  // Compile the COMP section of header
  cend=hbegin=hend=2;
  rtoken(in, "comp");
  header[cend++]=rtoken(in, 0, 255); // hh
  header[cend++]=rtoken(in, 0, 255); // hm
  header[cend++]=rtoken(in, 0, 255); // ph
  header[cend++]=rtoken(in, 0, 255); // pm
  int n=header[cend++]=rtoken(in, 0, 255); // n
  if (verbose) printf("\n");
  for (int i=0; i<n; ++i) {
    if (verbose) printf("  ");
    rtoken(in, i, i);
    CompType type=CompType(header[cend++]=rtoken(in, compname));
    int clen=compsize[type];
    assert(clen>0 && clen<10);
    for (int j=1; j<clen; ++j)
      header[cend++]=rtoken(in, 0, 255);
    if (verbose) printf("\n");
  }
  header[cend++]=0; // END

  // Compile HCOMP
  hbegin=hend=cend+128;  // leave a guard gap to catch backwards jumps
  rtoken(in, "hcomp");
  CompType op=compile_comp(in, hbegin, hend);
  if (verbose) printf("\n");
  if (hend>=0x10000) printf("\nProgram too big\n"), exit(1);

  // Compute header size
  hsize=hend-hbegin+cend-2;
  header[0]=hsize&255;
  header[1]=hsize>>8;
  if (verbose) {
    printf("(cend=%d hbegin=%d hend=%d hsize=%d Memory=%1.3f MB)\n\n", 
      cend, hbegin, hend, hsize, memory()/1000000);
  }

  // Compile POST section: cmd (0...255)[0..3] END
  U32 result=0;
  if (op==POST) {
    const char* tok=token(in);
    if (tok && strcmp(tok, "end")) result=tok[0];
    for (int i=1; i<4 && ((tok=token(in)))!=0 && strcmp(tok, "end"); ++i)
      result+=atoi(tok)<<i*8;
  }

  // Compile PCOMP pcomp_cmd\n program... END
  else if (op==PCOMP) {
    pcomp_cmd.resize(512);
    int c, len=0;
    while (len<pcomp_cmd.size()-1 && (c=getc(in))>=' ')
      pcomp_cmd[len++]=c;
    if (verbose) printf("%s\n", &pcomp_cmd[0]);
    pbegin=pend=hend+144;
    op=compile_comp(in, pbegin, pend);
    if (op!=END)
      error("Expected END in configuation file");
    if (verbose)
      printf("(pbegin=%d pend=%d pcomp size=%d)\n",
        pbegin, pend, pend-pbegin);
  }
  return result;
}

// Display header contents. Assume it is constructed correctly.
void ZPAQL::list() {
  assert(cend>=7 && cend<header.size());
  assert(hbegin==cend+128 && hbegin<header.size());
  assert(hend>hbegin && hend<header.size());
  assert(hsize==header[0]+256*header[1]);

  // Display COMP section
  printf("comp %d %d %d %d %d (hh hm ph pm n, header size=%d)\n",
    header[2], header[3], header[4], header[5], header[6], hsize);
  printf("  (Memory requirement: %1.3f MB)\n", memory()/1000000);
  int h=7;
  for (int i=0; i<header[6]; ++i) {
    int size=compsize[header[h]];
    assert(size>0);
    assert(h+size<header.size());
    printf("  %d %s", i, compname[header[h]]);
    for (int j=1; j<size; ++j)
      printf(" %d", header[h+j]);
    printf("\n");
    h+=size;
  }
  assert(h<header.size() && header[h]==0);
  ++h;
  assert(h==cend);

  // Display HCOMP section
  h+=128;  // skip guard
  assert(h==hbegin);
  printf("hcomp\n");
  while (h<hend-1) {
    assert(h<header.size()-2);
    int op=header[h];
    printf("(%4d) %s", h++-hbegin, opcodelist[op]);
    if (op==255) { // LJ
      printf(" %d %d (to %d)", header[h], header[h+1],
          header[h]+256*header[h+1]);
      h+=2;
    }
    else if ((op&7)==7) {
      printf(" %d", header[h++]);
      if (op==39 || op==47 || op==63) // JT, JF, JMP
        printf(" (to %d) ", h-hbegin+(int(header[h-1])<<24>>24));
    }
    printf("\n");
  }
  assert(header[h]==0);
  assert(h+1==hend);
  printf("post\nend\n");
}

// Initialize machine state as HCOMP
void ZPAQL::inith() {
  pc_start=hbegin;
  pc_end=hend;
  init(header[2], header[3]); // hh, hm
}

// Initialize machine state as PCOMP
void ZPAQL::initp() {
  if (pbegin) {
    assert(pbegin>=hend+128);
    assert(pend>=pbegin);
    pc_start=pbegin;
    pc_end=pend;
  }
  else {
    pc_start=hbegin;
    pc_end=hend;
  }
  init(header[4], header[5]);  // ph, pm
}

// Initialize machine state
void ZPAQL::init(int hbits, int mbits) {
  assert(pc_end>=pc_start);
  assert(pc_end==0 || pc_start>=hbegin);
  assert(h.size()==0);
  assert(m.size()==0);
  assert(hbegin>=cend+128);
  assert(cend>=7);
  h.resize(1, hbits);
  m.resize(1, mbits);
  r.resize(256);
  a=b=c=d=pc=f=0;
}

// Run program on input
void ZPAQL::run(U32 input) {
  assert(cend>6);
  assert(hbegin==cend+128);
  assert(hend>hbegin);
  assert(hend<header.size()-130);
  assert(m.size()>0);
  assert(h.size()>0);
  assert(pc_start==hbegin || pc_start==pbegin);
  pc=pc_start;
  a=input;
  while (execute()) ;
}

// Execute program input and show progress
void ZPAQL::step(U32 input, bool ishex) {
  assert(cend>6);
  assert(hbegin==cend+128);
  assert(hend>hbegin);
  assert(hend<header.size()-130);
  assert(m.size()>0);
  assert(h.size()>0);
  assert(pc_start==hbegin || pc_start==pbegin);
  pc=pc_start;
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

  // Dump memory
  printf("\n\nH (size %d) =", h.size());
  for (int i=0; i<h.size(); ++i) {
    if (i%4==0) printf("\n%8X %8d:", i, i);
    printf(ishex ? " %08X" : " %10u", h[i]);
  }
  printf("\n\nM (size %d) =", m.size());
  for (int i=0; i<m.size(); ++i) {
    if (i%8==0) printf("\n%8X %8d:", i, i);
    printf(ishex ? " %02X" : " %3d", m[i]);
  }
  int rsize=r.size(); // don't print trailing zeros
  while (rsize>4 && r[rsize-1]==0) --rsize;
  printf("\n\nR (size %d) =", r.size());
  for (int i=0; i<rsize; ++i) {
    if (i%4==0) printf("\n%02X %3d:", i, i);
    printf(ishex ? " %08X" : " %10u", r[i]);
  }
  printf("\n\n");
}

// Print header as an array initialization in C
void ZPAQL::prints() {
  assert(header.size()>hend);
  assert(hend>=hbegin);
  assert(hbegin>=0);
  printf("\n\n  header=[%d]={ // COMP %d bytes\n    ",
      cend+hend-hbegin, cend);
  for (int i=0; i<cend; ++i) {
    printf("%d,", header[i]);
    if (i%16==15) printf("\n    ");
  }
  printf("\n    // HCOMP %d bytes\n    ", hend-hbegin);
  for (int i=hbegin; i<hend; ++i) {
    printf("%d", header[i]);
    if (i<hend-1) printf(",");
    if ((i-hbegin)%16==15) printf("\n    ");
  }
  printf("};\n");
  if (pend>pbegin) {
    int psize=pend-pbegin;
    printf("  pcomp[%d]={%d,%d,%d, // PCOMP\n    ",
      psize+3, 1, psize&255, (psize>>8)&255);
    for (int i=pbegin; i<pend; ++i) {
      printf("%d", header[i]);
      if (i<pend-1) printf(",");
      if ((i-pbegin)%16==15) printf("\n    ");
    }
    printf("};\n");
  }
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

// Read a token and return it, or return 0 at EOF. Skip (comments).
// Convert to lower case. Tokens are separated by white space.
// In verbose mode, print the token.
const char* ZPAQL::token(FILE* in) {
  static char s[16];  // result
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
    if (isupper(c)) c=tolower(c);
    s[len++]=c;
  }
  while (len<15 && (c=getc(in))!=EOF && c>' ');
  s[len++]=0;
  if (verbose) printf("%s ", s);
  return s;
}

// Read a token, which must be in the NULL terminated list or else
// exit with an error. If found, return its index.
int ZPAQL::rtoken(FILE* in, const char* list[]) {
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
void ZPAQL::rtoken(FILE* in, const char* s) {
  assert(s);
  const char* t=token(in);
  if (!t) fprintf(stderr, "\nExpected %s, found EOF\n", s), exit(1);
  if (strcmp(s, t)) fprintf(stderr, "\nExpected %s, found %s\n", s, t), exit(1);
}

// Read a number in (low...high) or exit with an error
int ZPAQL::rtoken(FILE* in, int low, int high) {
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
    case 200: a <<= a; break; // A<<=A
    case 201: a <<= b; break; // A<<=B
    case 202: a <<= c; break; // A<<=C
    case 203: a <<= d; break; // A<<=D
    case 204: a <<= m(b); break; // A<<=*B
    case 205: a <<= m(c); break; // A<<=*C
    case 206: a <<= h(d); break; // A<<=*D
    case 207: a <<= header[pc++]; break; // A<<= N
    case 208: a >>= a; break; // A>>=A
    case 209: a >>= b; break; // A>>=B
    case 210: a >>= c; break; // A>>=C
    case 211: a >>= d; break; // A>>=D
    case 212: a >>= m(b); break; // A>>=*B
    case 213: a >>= m(c); break; // A>>=*C
    case 214: a >>= h(d); break; // A>>=*D
    case 215: a >>= header[pc++]; break; // A>>= N
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
    case 255: if((pc=pc_start+header[pc]+256*header[pc+1])>=pc_end)err();break;//LJ
    default: err();
  }
  return 1;
}

// Print illegal instruction error message and exit
void ZPAQL::err() {
  fprintf(stderr, "\nExecution aborted: pc=%d a=%d b=%d->%d c=%d->%d d=%d->%d\n",
    pc-hbegin, a, b, m(b), c, m(c), d, h(d));
  if (pc>=pc_start && pc<pc_end) fprintf(stderr, "opcode = %d %s\n",
    header[pc-pc_start], opcodelist[header[pc-pc_start]]);
  else
    fprintf(stderr, "pc out of range. Program size is %d\n", pc_end-pc_start);
  exit(1);
}

///////////////////////////// Predictor ///////////////////////////

// A Component is a context model, indirect context model, match model,
// fixed weight mixer, adaptive 2 input mixer without or with current
// partial byte as context, adaptive m input mixer (without or with),
// or SSE (without or with).

struct Component {
  int limit;      // max count for cm
  U32 cxt;        // saved context
  int a, b, c;    // multi-purpose variables
  Array<U32> cm;  // cm[cxt] -> p in bits 31..10, n in 9..0; MATCH index
  Array<U8> ht;   // ICM hash table[0..size1][0..15] of bit histories; MATCH buf
  Array<U16> a16; // multi-use
  Component();    // initialize to all 0
};

Component::Component(): limit(0), cxt(0), a(0), b(0), c(0) {}

// Next state table generator
class StateTable {
  enum {B=6, N=64}; // sizes of b, t
  static U8 ns[1024]; // state*4 -> next state if 0, if 1, n0, n1
  static const int bound[B];  // n0 -> max n1, n1 -> max n0
  int num_states(int n0, int n1);  // compute t[n0][n1][1]
  void discount(int& n0);  // set new value of n0 after 1 or n1 after 0
  void next_state(int& n0, int& n1, int y);  // new (n0,n1) after bit y
public:
  int next(int state, int y) {  // next state for bit y
    assert(state>=0 && state<256);
    assert(y>=0 && y<4);
    return ns[state*4+y];
  }
  int cminit(int state) {  // initial probability of 1 * 2^23
    assert(state>=0 && state<256);
    return ((ns[state*4+3]*2+1)<<22)/(ns[state*4+2]+ns[state*4+3]+1);
  }
  StateTable();
};

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

// A predictor guesses the next bit
class Predictor {
public:
  Predictor(ZPAQL&);    // build model
  int predict();        // probability that next bit is a 1 (0..4095)
  void update(int y);   // train on bit y (0..1)
  void stat();          // print statistics
private:

  // Predictor state
  int c8;               // last 0...7 bits.
  int hmap4;            // c8 split into nibbles
  int p[256];           // predictions
  ZPAQL& z;             // VM to compute context hashes, includes H, n
  Component comp[256];  // the model, includes P

  // Modeling support functions
  void train(Component& cr, int y);  // reduce prediction error in cr.cm
  int dt[1024];         // division table for cm: dt[i] = 2^16/(i+1.5)
  U16 squasht[4096];    // squash() lookup table
  short stretcht[32768];// stretch() lookup table
  StateTable st;        // next, cminit functions

  // x -> floor(32768/(1+exp(-x/64)))
  int squash(int x) {
    assert(x>=-2048 && x<=2047);
    return squasht[x+2048];
  }

  // x -> round(64*log((x+0.5)/(32767.5-x))), approx inverse of squash
  int stretch(int x) {
    assert(x>=0 && x<=32767);
    return stretcht[x];
  }

  // bound x to a 12 bit signed int
  int clamp2k(int x) {
    if (x<-2048) return -2048;
    else if (x>2047) return 2047;
    else return x;
  }

  // bound x to a 20 bit signed int
  int clamp512k(int x) {
    if (x<-(1<<19)) return -(1<<19);
    else if (x>=(1<<19)) return (1<<19)-1;
    else return x;
  }

  // Get cxt in ht, creating a new row if needed
  int find(Array<U8>& ht, int sizebits, U32 cxt);
};

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

int Predictor::predict() {
  assert(c8>=1 && c8<=255);

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
        cr.cxt=z.h(i)^hmap4;
        p[i]=stretch(cr.cm(cr.cxt)>>17);
        break;
      case ICM: // sizebits
        assert((hmap4&15)>0);
        if (c8==1 || (c8&0xf0)==16) cr.c=find(cr.ht, cp[1]+2, z.h(i)+16*c8);
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
        cr.cxt=((z.h(i)+(c8&cp[5]))&(cr.c-1));
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
        cr.cxt=z.h(i)+(c8&cp[5]);
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
          cr.c=find(cr.ht, cp[1]+2, z.h(i)+16*c8);
        cr.cxt=cr.ht[cr.c+(hmap4&15)];  // bit history
        int *wt=(int*)&cr.cm[cr.cxt*2];
        p[i]=clamp2k((wt[0]*p[cp[2]]+wt[1]*64)>>16);
      }
        break;
      case SSE: { // sizebits j start limit
        cr.cxt=(z.h(i)+c8)*32;
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
}

// Update model with decoded bit y (0...1)
void Predictor::update(int y) {
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
            cr.b=pos-cr.cm(z.h(i));
            if (cr.b&(cr.ht.size()-1))
              while (cr.a<255 && cr.ht(pos-cr.a-1)==cr.ht(pos-cr.a-cr.b-1))
                ++cr.a;
          }
          else cr.a+=cr.a<255;
          cr.cm(z.h(i))=pos;
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
}

// cr.cm(cr.cxt) has a prediction in the high 22 bits and a count in the
// low 10 bits.  Reduce the prediction error by error/(count+1.5) and
// count up to cr.limit. cm.size() must be a power of 2.
inline void Predictor::train(Component& cr, int y) {
  assert(y==0 || y==1);
  U32& pn=cr.cm(cr.cxt);
  int count=pn&0x3ff;
  int error=y*32767-(cr.cm(cr.cxt)>>17);
  pn+=(error*dt[count]&-1024)+(count<cr.limit);
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
};

Decoder::Decoder(FILE* f, ZPAQL& z):
  in(f), low(1), high(0xFFFFFFFF), curr(0), pr(z) {}

inline int Decoder::decode(int p) {
  assert(p>=0 && p<65536);
  assert(high>low && low>0);
  if (curr<low || curr>high) {
    printf("low=%08X curr=%08X high=%08X at %ld\n",
     low, curr, high, ftell(in));
    error("archive corrupted");
  }
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

/////////////////////////// PostProcessor ////////////////////

class PostProcessor {
  int state;   // input parse state
  int ph, pm;  // sizes of H and M in z
  ZPAQL z;     // holds PCOMP
public:
  PostProcessor(ZPAQL& hz);
  void set(FILE* out, SHA1* p) {z.output=out; z.sha1=p;}  // Set output
  void write(int c);  // Input a byte
};

// Copy ph, pm from block header
PostProcessor::PostProcessor(ZPAQL& hz) {
  state=0;
  ph=hz.header[4];
  pm=hz.header[5];
}

// (PASS=0 | PROG=1 psize[0..1] pcomp[0..psize-1]) data... EOB=-1
void PostProcessor::write(int c) {
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
      z.hsize=c;  // low byte of psize
      state=3;
      break;
    case 3:  // PROG psize[0]
      if (c<0) error("Unexpected EOS");
      z.hsize+=c*256+1;  // high byte of psize
      z.header.resize(z.hsize+300);
      z.cend=8;
      z.hbegin=z.hend=136;
      z.header[0]=z.hsize&255;
      z.header[1]=z.hsize>>8;
      z.header[4]=ph;
      z.header[5]=pm;
      state=4;
      break;
    case 4:  // PROG psize[0..1] pcomp[0...]
      if (c<0) error("Unexpected EOS");
      assert(z.hend<z.header.size());
      z.header[z.hend++]=c;  // one byte of pcomp
      if (z.hend-z.hbegin==z.hsize-1) {  // last byte of pcomp?
        z.header[z.hend++]=0;
        z.initp();
        state=5;
      }
      break;
    case 5:  // PROG ... data
      z.run(c);
      break;
  }
}

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

// Advance in to start of next block
void skip_block(FILE *in) {

  // Find start of next block
  int c;
  if (!find_start(in)) return;  // EOF
  if ((c=getc(in))>LEVEL || c<1 || getc(in)!=1)
    error("not ZPAQ");

  // Skip block header
  int hsize=getc(in);
  hsize+=getc(in)*256;
  if (hsize<6 || hsize>65535) error("hsize missing");
  while (hsize-->0) getc(in);
  
  // Skip segments
  while ((c=getc(in))==1) {
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

// Decompress: [pnt]xN archive [files...]
// p=paths, n=unnamed segments as separate files, t=no postprocessing,
// N=block to extract (default all), files...=new names.
void decompress(int argc, char** argv) {
  assert(argc>=3);

  // Get options
  bool pcmd=false, ncmd=false, tcmd=false;
  int blocknum=0;
  const char* cmd=argv[1];
  assert(cmd);
  while (*cmd) {
    if (*cmd=='p') pcmd=true;
    else if (*cmd=='n') ncmd=true;
    else if (*cmd=='t') tcmd=true;
    else if (*cmd=='x') break;
    else usage();
    ++cmd;
  }
  if (cmd[0]!='x') usage();
  if (cmd[1]) blocknum=atoi(cmd+1);

  // Open archive
  FILE* in=fopen(argv[2], "rb");
  if (!in) perror(argv[2]), exit(1);

  // Skip to specified block
  while (blocknum>1) {
    skip_block(in);
    --blocknum;
  }

  // Read the archive
  int filecount=0;  // number of files extracted
  FILE *out=0;  // file to extract
  int c;
  while (find_start(in)) {
    if (getc(in)!=LEVEL || getc(in)!=1)
      error("Not ZPAQ");

    // Read block header
    ZPAQL z;
    z.read(in);

    // PostProcessor and Decoder is created and and destroyed for each block
    PostProcessor pp(z);
    Decoder dec(in, z);

    // Read segments
    while ((c=getc(in))==1) {

      // Read the filename
      char filename[512]={0};
      int i;
      for (i=0; (c=getc(in))>0; ++i)
        if (i<511) filename[i]=c;
      if (i>0 && i<512) filename[i]=0;
      printf("%s ", filename);

      // Get comment
      char comment[20]={0};
      i=0;
      while ((c=getc(in))!=EOF && c!=0) {
        if (i<19) comment[i]=c;
        ++i;
      }
      printf("%s -> ", comment);
      if (getc(in)) error("reserved");  // reserved 0

      // If a segment is named, or no output file is open, or n option, then
      // create a new output file.
      if (ncmd || filename[0] || !out) {
        if (out) fclose(out), out=0;

        // If the user gave an output file starting at argv[3], use it instead.
        // If the user doesn't name all the files, then stop after the last
        // named file.
        if (argc>3) {
          if (filecount+3 < argc) {
            out=fopen(argv[filecount+3], "wb");
            if (!out) {
              perror(argv[filecount+3]);
              goto end;
            }
            else
              printf("%s ", argv[filecount+3]);
          }
          else {
            printf("\nSkipping %s and remaining files\n", filename);
            goto end;
          }
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
        ++filecount;
      }

      // Decompress
      SHA1 sha1;
      long len=0;
      if (tcmd) { // don't postprocess
        while ((c=dec.decompress())!=EOF) {
          if (out) putc(c, out);
          sha1.put(c);
          if (!(len&0xffff))
            printf("%-12ld\b\b\b\b\b\b\b\b\b\b\b\b", len);
          ++len;
        }
      }
      else {
        pp.set(out, &sha1);
        while ((c=dec.decompress())!=EOF) {
          pp.write(c);
          if (!(len&0xffff))
            printf("%-12ld\b\b\b\b\b\b\b\b\b\b\b\b", len);
          ++len;
        }
        pp.write(-1);
      }

      // Check for end of segment and block markers
      int eos=getc(in);  // 253=SHA1 follows, 254=EOS
      if (eos==253) {
        U8 hash[20];
        bool match=true;
        for (int i=0; i<20; ++i) {
          hash[i]=getc(in);
          if (hash[i]!=sha1.result(i))
            match=false;
        }
        if (match)
          printf("Checksum OK ");
        else {
          printf("CHECKSUM FAILED: FILE IS NOT IDENTICAL\n  Archive SHA1: ");
          for (int i=0; i<20; ++i)
            printf("%02x", hash[i]);
          printf("\n  File SHA1:    ");
          for (int i=0; i<20; ++i)
            printf("%02x", sha1.result(i));
        }
      }
      else if (eos!=254)
        error("missing end of segment marker");
      else
        printf("OK, no checksum");
      printf("\n");
    }
    if (c!=255) error("missing end of block marker");
    if (blocknum) goto end;
  }

  // Close the archive
end:
  if (out) fclose(out);
  printf("%d file(s) extracted\n", filecount);
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
public:
  Encoder(FILE* f, ZPAQL& z);
  void compress(int c);  // c is 0..255 or EOF
  void stat() {pr.stat();}  // print predictor statistics
  void setOutput(FILE* f) {out=f;}
};

Encoder::Encoder(FILE* f, ZPAQL& z): 
  out(f), low(1), high(0xFFFFFFFF), pr(z) {}

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
  }
}

void Encoder::compress(int c) {
  assert(out);
  if (c==-1)
    encode(1, 0);
  else {
    assert(c>=0 && c<=255);
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

// Compress files: [pnsiv]c|a[F] archive files...
void compress(int argc, char** argv) {
  assert(argc>=3);

  // Get command options
  bool pcmd=false, ncmd=false, scmd=false, icmd=false, vcmd=false, // options
       tcmd=false, acmd=false, ccmd=false;
  const char *cmd=argv[1];
  while (cmd && cmd[0]) {
    if (cmd[0]=='p') pcmd=true, ncmd=false;
    else if (cmd[0]=='n') ncmd=true, pcmd=false;
    else if (cmd[0]=='s') scmd=true;
    else if (cmd[0]=='i') icmd=true;
    else if (cmd[0]=='v') vcmd=true;
    else if (cmd[0]=='t') tcmd=true;
    else if (cmd[0]=='a') {acmd=true; break;}
    else if (cmd[0]=='c') {ccmd=true; break;}
    else usage();
    ++cmd;
  }
  ++cmd;
  if (acmd==ccmd) usage();

  // Compile config file F now in cmd
  ZPAQL z; // compression model
  if (cmd[0]) {  // config file name?
    FILE* cfg=fopen(cmd, "rb");
    if (!cfg) perror(cmd), exit(1);
    z.verbose=vcmd;
    z.compile(cfg);
    fclose(cfg);
    printf("%1.3f MB memory required.\n", z.memory()/1000000);
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
    z.load(34, 37, header);
  }
  ZPAQL zp=z; // for testing preprocessor
  zp.initp();

  // Construct temporary file names from archive name
  Array<char> prefile, tempfile;
  cat(prefile,  argv[2], ".$zpaq.pre");
  cat(tempfile, argv[2], ".$zpaq.tmp");

  // Initialize preprocessor
  remove(&tempfile[0]);

  // Compress files in argv[3...argc-1]
  FILE *out=0;  // archive opened when ready to compress first file
  long mark=0;  // archive size (for results display)
  Encoder enc(out, z);  // compressor
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
    long size=ftell(in);
    rewind(in);

    // Verify post(pre(in)) == in
    if (z.pend) {  // PCOMP section?
      fclose(in);
      remove(&prefile[0]);

      // Run external preprocessor
      Array<char> syscmd;
      cat(syscmd, &z.pcomp_cmd[0], " ", argv[i], " ", &prefile[0]);
      printf("%s ... ", &syscmd[0]);
      system(&syscmd[0]);

      // Open preprocessor output
      FILE *in=fopen(&prefile[0], "rb");
      if (!in) {
        perror(&prefile[0]);
        continue;
      }

      // Run preprocessed data through postprocessor
      SHA1 check2;
      zp.sha1=&check2;
      while ((c=getc(in))!=EOF)
        zp.run(c);
      zp.run(-1);

      // Compare postprocessed output with unpreprocessed input
      bool match=true;
      for (int j=0; j<20; ++j)
        if (check1.result(j)!=check2.result(j))
          match=false;
      if (!match) {
        printf("FAILED\n");
        fclose(in);
        continue;
      }
      printf("OK\n");

      // If OK then use preprocessed input
      rewind(in);
      zp.sha1=0;
    } // end if PCOMP

    // Open archive for first file
    bool first=false;  // first file?
    if (!out) {
      out=fopen(argv[2], acmd?"ab":"wb");
      if (!out) perror(argv[2]), exit(1);

      // append locator tag
      if (tcmd)
        fprintf(out, "%s", 
        "\x37\x6B\x53\x74\xA0\x31\x83\xD3\x8C\xB2\x28\xB0\xD3");

      // Write block header
      enc.setOutput(out);
      fprintf(out, "zPQ%c%c", LEVEL, 1);
      mark=ftell(out)-6;  // last reported size (adjusted for header/trailer)
      z.write(out);
      first=true;
    }

    // Code segment header
    putc(1, out);  // start of segment
    if (!ncmd)
      fprintf(out, "%s", pcmd?argv[i]:strip(argv[i]));  // filename
    putc(0, out);  // filename terminator
    if (!icmd)
      fprintf(out, "%ld", size);  // size as comment
    putc(0, out);  // comment terminator
    putc(0, out);  // reserved

    // Compress PCOMP or POST 0
    if (first) {
      const int psize=z.pend-z.pbegin;
      assert(psize>=0 && psize<0x10000);
      assert(z.header.size()>=z.pend);
      if (psize==0)
        enc.compress(0);  // PASS
      else {
        enc.compress(1);  // POST
        enc.compress(psize&255);     // size low byte
        enc.compress(psize>>8&255);  // size high byte
        for (int j=0; j<psize; ++j)  // PCOMP code
          enc.compress(z.header[z.pbegin+j]);
      }
    }

    // Compress 
    printf("%s %ld ", argv[i], size);
    long j=0;
    while ((c=getc(in))!=EOF) {
      enc.compress(c);
      if (!(++j&0xffff)) {
        printf("%12ld -> %-12ld"
          "\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b",
          j, ftell(out)-mark);
      }
    }
    enc.compress(-1);

    // Write segment trailer
    if (scmd)  // no SHA1
      fprintf(out, "%c%c%c%c%c", 0, 0, 0, 0, 254);
    else {
      fprintf(out, "%c%c%c%c%c", 0, 0, 0, 0, 253);
      for (int j=0; j<20; ++j)
        putc(check1.result(j), out);
    }
    fclose(in);
    in=0;
    remove(&prefile[0]);
    printf("-> %ld                        \n", ftell(out)-mark);
    mark=ftell(out);
  }

  // Code end of block and close archive
  if (out) {
    putc(255, out);  // block trailer
    printf("-> %ld\n", ftell(out));
    fclose(out);
    enc.stat();  // print statistics

    // If no error then clean up temporary files
    remove(&tempfile[0]);
    remove(&prefile[0]);
  }
  else
    printf("Archive %s not updated\n", argv[2]);
}

////////////////////////// list //////////////////////////

// List archive contents: [v]l archive
void list(int argc, char** argv) {
  assert(argc>2 && argv[2]);

  // Verbose?
  bool verbose=strchr(argv[1], 'v')!=0;

  // Open archive
  FILE* in=fopen(argv[2], "rb");
  if (!in) perror(argv[2]), exit(1);

  // File offsets to get compressed sizes
  long mark=0;

  // Read the file
  int c, blocks=0;
  while (find_start(in)) {

    // Read block header
    if (getc(in)!=LEVEL || getc(in)!=1)
      error("not ZPAQ");
    ZPAQL z;
    z.read(in);
    printf("Block %d: requires %1.3f MB memory\n",
     ++blocks, z.memory()/1000000);
    if (verbose)
      z.list();

    // Read segments
    while ((c=getc(in))==1) {

      // Print filename and comments
      printf("  ");
      while ((c=getc(in))!=EOF && c) putchar(c);
      printf("  ");
      while ((c=getc(in))!=EOF && c) putchar(c);
      if (getc(in)!=0) error("reserved data");

      // Skip to end of data
      U32 c4=0xFFFFFFFF;  // last 4 bytes will be all 0
      while ((c=getc(in))!=EOF && (c4=c4<<8|c)!=0) ;
      if (c==EOF) error("unexpected end of file");
      while ((c=getc(in))==0) ;
      if (c==253) {  // print SHA1 in verbose mode
        if (argv[1][0]=='v') {
          printf(" SHA1=");
          for (int i=0; i<20; ++i)
            printf("%02x", getc(in));
        }
        else {
          for (int i=0; i<20; ++i)  // skip SHA1
            getc(in);
        }
      }
      else if (c!=254) error("missing end of segment marker");
      printf(" -> %ld\n", 1+ftell(in)-mark);
      mark=1+ftell(in);
    }
    if (c!=255) error("missing end of block marker");
  }
}

//////////////////////////// run ///////////////////////////

// Debug config file: [pvt]rF [args...]
// p=run PCOMP, v=verbose, t=trace once per numeric arg
// otherwise args are output, input (default stdout, stdin),
// h=trace in hexadecimal
void run(int argc, char** argv) {
  assert(argc>=2);

  // Get options
  bool pcmd=false, vcmd=false, tcmd=false, hcmd=false;
  const char *cmd=argv[1];
  assert(cmd);
  while (cmd[0]) {
    if (cmd[0]=='p') pcmd=true;
    else if (cmd[0]=='v') vcmd=true;
    else if (cmd[0]=='t') tcmd=true;
    else if (cmd[0]=='h') hcmd=true;
    else if (cmd[0]=='r') break;
    else usage();
    ++cmd;
  }
  ++cmd; // now points config file name
  if (!cmd[0]) usage();

  // Initialze virtual machine
  ZPAQL z;
  z.verbose=vcmd;
  FILE* in=fopen(cmd, "r");
  if (!in) perror(cmd), exit(1);
  z.compile(in);
  if (pcmd) z.initp();
  else z.inith();
  if (vcmd) z.prints();

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

///////////////////////////// Main ///////////////////////////

// Print help message and exit
void usage() {
  printf("ZPAQ v1.06 archiver, (C) 2009, Ocarina Networks Inc.\n"
    "Written by Matt Mahoney, " __DATE__ ".\n"
    "This is free software under GPL v3, http://www.gnu.org/copyleft/gpl.html\n"
    "\n"
    "To compress to new archive: zpaq [pnsivt]c[F] archive files...\n"
    "To append to archive:       zpaq [pnsivt]a[F] archive files...\n"
    "Optional modifiers:\n"
    "  p = store filename paths in archive\n"
    "  n = don't store filenames (extractor will append to last named file)\n"
    "  s = don't store SHA1 checksums (saves 20 bytes)\n"
    "  i = don't store file sizes as comments (saves a few bytes)\n"
    "  v = verbose\n"
    "  t = append locator tag to non-ZPAQ data\n"
    "  F = use options in configuration file F (min.cfg, max.cfg)\n"
    "To list contents: zpaq [v]l archive\n"
    "  v = verbose\n"
    "To extract: zpaq [pnt]x[N] archive [files...]\n"
    "  p = extract to stored paths instead of current directory\n"
    "  n = extract unnamed segments as separate files (for debugging)\n"
    "  t = don't post-process (for debugging)\n"
    "  N = extract only block N (1, 2, 3...)\n"
    "  files... = rename extracted files (clobbers)\n"
    "      otherwise use stored names (does not clobber)\n"
    "To debug configuration file F: zpaq [pvt]rF [args...]\n"
    "  p = run PCOMP (default is to run HCOMP)\n"
    "  v = verbose compile and show initialization lists\n"
    "  t = trace (single step), args are numeric inputs\n"
    "      otherwise args are input, output (default stdin, stdout)\n"
    "  h = trace display in hexadecimal\n"
    "To make self extracting archive: append to a copy of zpaqsfx.exe\n");
  exit(0);
}

// Command syntax as in usage()
int main(int argc, char** argv) {

  // Check usage
  if (argc<2) 
    usage();

  // Find the command c, a, x, l, r
  char cmd=0;
  for (int i=0; (cmd=argv[1][i])!=0; ++i)
    if (strchr("caxlr", cmd))
      break;

  // Do the command
  if (argc>=3 && (cmd=='a' || cmd=='c')) {
    compress(argc, argv);
    printf("Used %1.2f seconds\n", clock()/double(CLOCKS_PER_SEC));
  }
  else if (argc>=3 && cmd=='x') {
    decompress(argc, argv);
    printf("Used %1.2f seconds\n", clock()/double(CLOCKS_PER_SEC));
  }
  else if (argc>=3 && cmd=='l')
    list(argc, argv);
  else if (cmd=='r')
    run(argc, argv);
  else
    usage();
  return 0;
}
