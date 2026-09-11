CXX=g++
CC=gcc
CFLAGS+= -O3 -DZ7_ST
CPPFLAGS+=-Dunix
# CPPFLAGS+=NOJIT
CXXFLAGS=-O3 -march=native
PREFIX=/usr/local
BINDIR=$(PREFIX)/bin
MANDIR=$(PREFIX)/share/man

all: zpaqf zpaqf.1

Alloc.o: ./liblzma/Alloc.c
	$(CC) -c ./liblzma/Alloc.c -o Alloc.o $(CFLAGS)

CpuArch.o: ./liblzma/CpuArch.c
	$(CC) -c ./liblzma/CpuArch.c -o CpuArch.o $(CFLAGS)

LzFind.o: ./liblzma/LzFind.c
	$(CC) -c ./liblzma/LzFind.c -o LzFind.o $(CFLAGS)

LzFindOpt.o: ./liblzma/LzFindOpt.c
	$(CC) -c ./liblzma/LzFindOpt.c -o LzFindOpt.o $(CFLAGS)

LzmaEnc.o: ./liblzma/LzmaEnc.c
	$(CC) -c ./liblzma/LzmaEnc.c -o LzmaEnc.o $(CFLAGS)

LzmaLib.o: ./liblzma/LzmaLib.c
	$(CC) -c ./liblzma/LzmaLib.c -o LzmaLib.o $(CFLAGS)

libzpaq.o: libzpaq.cpp libzpaq.h
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -o $@ -c libzpaq.cpp

zpaq.o: zpaq.cpp libzpaq.h
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -o $@ -c zpaq.cpp -pthread

zpaqf: zpaq.o libzpaq.o Alloc.o CpuArch.o LzFind.o LzFindOpt.o LzmaEnc.o LzmaLib.o
	$(CXX) $(LDFLAGS) -o $@ zpaq.o libzpaq.o Alloc.o CpuArch.o LzFind.o LzFindOpt.o LzmaEnc.o LzmaLib.o -pthread -static -s

zpaqf.1: zpaqf.pod
	pod2man $< >$@

install: zpaqf zpaqf.1
	install -m 0755 -d $(DESTDIR)$(BINDIR)
	install -m 0755 zpaqf $(DESTDIR)$(BINDIR)
	install -m 0755 -d $(DESTDIR)$(MANDIR)/man1
	install -m 0644 zpaqf.1 $(DESTDIR)$(MANDIR)/man1

clean:
	rm -f zpaq.o libzpaq.o zpaqf zpaqf.1 archive.zpaq zpaq.new Alloc.o CpuArch.o LzFind.o LzFindOpt.o LzmaEnc.o LzmaLib.o

check: zpaqf
	./zpaqf add archive.zpaq zpaqf
	./zpaqf extract archive.zpaq zpaqf -to zpaq.new
	cmp zpaqf zpaq.new
	rm archive.zpaq zpaq.new
