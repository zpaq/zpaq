CCFLAGS = -O3 -march=native -s -fomit-frame-pointer -Wall -pedantic -DNDEBUG

all: zpaq.exe libzpaq.o zpaq.o divsufsort.o

libzpaq.o: libzpaq.cpp libzpaq.h
	g++ $(CCFLAGS) -c libzpaq.cpp

zpaq.o: zpaq.cpp libzpaq.h
	g++ $(CCFLAGS) -c -DOPT zpaq.cpp

divsufsort.o: divsufsort.c divsufsort.h
	gcc $(CCFLAGS) -c divsufsort.c

zpaq.exe: zpaq.cpp libzpaq.o libzpaq.h divsufsort.o divsufsort.h
	g++ $(CCFLAGS) zpaq.cpp libzpaq.o divsufsort.o -o zpaq
