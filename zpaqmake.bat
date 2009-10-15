: This script is called by ZPAQ to compile optimized versions. It is
: expected to compile %1.cpp to %1.exe with -DOPT -DNDEBUG,
: #include <zpaq.h> and link it to zpaq.cpp or zpaq.o. These files can
: be anywhere, but this script is expected to find them.
: The following assumes zpaq.cpp and zpaq.h are in C:\bin\src
: and that %TMPDIR% is not set and %TEMP% is.
: Adjust accordingly.

: MinGW 4.4.0 (recommended)
g++ -O2 -s -fomit-frame-pointer -march=pentiumpro -DNDEBUG -DOPT %1.cpp -IC:\bin\src C:\bin\src\zpaq.cpp -o %1.exe

: Borland compiler
:cd %temp%
:bcc32 -O -6 -DNDEBUG -DOPT -IC:\bin\src %1.cpp C:\bin\src\zpaq.cpp -e%1.exe
:del zpaq_*.obj zpaq_*.map zpaq_*.tds

: Digital Mars
:cd %temp%
:\dm\bin\dmc -o -6 -IC:\bin\src %1.cpp -DNDEBUG -DOPT C:\bin\src\zpaq.cpp
:del zpaq_*.obj zpaq_*.map

: Optionally compress the output .exe
upx -qqq %1.exe
