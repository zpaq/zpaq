README for libzpaq v3.00 - July 28, 2011.
Matt Mahoney, matmahoney@yahoo.com

This package contains the libzpaq API for developing applications that read
or write in the ZPAQ level 1 standard format for compressed data.
All versions of this software and supporting code and documents
can be found at http://mattmahoney.net/dc/zpaq.html
A copy of this software is included in the zpaq v3.01 source distribution.

Contents:

  libzpaq.h        API header file to include in your C++ application.
  libzpaq.cpp      Source code to link to your application.
  libzpaqo.cpp     Default models fast, mid, max, to link.
  libzpaq.3.pod    pod2html, pod2man source for documentation.
  readme.txt       This file.

libzpaq is needed to compile most ZPAQ compatible applications such
as zpaq, zpipe, and zpaqsfx. It is not needed to compile the reference
decoder, unzpaq.

libzpaq is an API that allows you to develop applications that compress
or decompress byte streams such as files, arrays, or strings. It will
decompress any stream that conforms to the ZPAQ level 1 standard.
It will compress in the fast, mid, and max formats as described in the
configuration files at the above website, and has speed optimizations
to decompress these same configurations when they are recognized in
the input. If you want to compress and/or optimize in other formats, then
create an archive using zpaq config files for each format you want to
support, extract it with zpaq -j2, and find the source code replacing
libzpaqo.cpp in the temporary directory, either %TEMP%, $TMPDIR, or /tmp.

LICENSE

  Copyright (C) 2011, Dell Inc. Written by Matt Mahoney.

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so without restriction.
  This Software is provided "as is" without warranty.


