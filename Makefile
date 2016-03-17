CXX=g++
CPPFLAGS+=-Dunix
# CPPFLAGS+=NOJIT
CXXFLAGS=-O3 -march=native
PREFIX=/usr/local
LIBDIR=$(PREFIX)/lib
INCLUDEDIR=$(PREFIX)/include
BINDIR=$(PREFIX)/bin
MANDIR=$(PREFIX)/share/man
SONAME=libzpaq.so.0.1

all: $(SONAME) zpaq zpaq.1

libzpaq.o: libzpaq.cpp libzpaq.h
	$(CXX) -fPIC -DPIC $(CPPFLAGS) $(CXXFLAGS) -o $@ -c libzpaq.cpp

$(SONAME): libzpaq.o
	$(CXX) $(LDFLAGS) -shared -Wl,-soname,$(SONAME) -o $@ $<

libzpaq.so: $(SONAME)
	ln -s $(SONAME) libzpaq.so

zpaq.o: zpaq.cpp libzpaq.h
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -o $@ -c zpaq.cpp -pthread

zpaq: zpaq.o libzpaq.so
	$(CXX) $(LDFLAGS) -o $@ zpaq.o -L. -lzpaq -pthread

zpaq.1: zpaq.pod
	pod2man $< >$@

install: libzpaq.so libzpaq.h zpaq zpaq.1
	install -m 0755 -d $(DESTDIR)$(LIBDIR)
	install -m 0755 -t $(DESTDIR)$(LIBDIR) $(SONAME)
	rm -f $(DESTDIR)$(LIBDIR)/libzpaq.so
	ln -s $(SONAME) $(DESTDIR)$(LIBDIR)/libzpaq.so
	install -m 0755 -d $(DESTDIR)$(INCLUDEDIR)
	install -m 0644 -t $(DESTDIR)$(INCLUDEDIR) libzpaq.h
	install -m 0755 -d $(DESTDIR)$(BINDIR)
	install -m 0755 -t $(DESTDIR)$(BINDIR) zpaq
	install -m 0755 -d $(DESTDIR)$(MANDIR)/man1
	install -m 0644 -t $(DESTDIR)$(MANDIR)/man1 zpaq.1

clean:
	rm -f *.o *.so *.so.* zpaq *.1 archive.zpaq zpaq.new

check: zpaq $(SONAME)
	LD_LIBRARY_PATH=. ./zpaq add archive.zpaq zpaq
	LD_LIBRARY_PATH=. ./zpaq extract archive.zpaq zpaq -to zpaq.new
	cmp zpaq zpaq.new
	rm archive.zpaq zpaq.new
