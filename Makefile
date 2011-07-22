CCFLAGS = -O3 -march=native -s -Wall -pedantic -DNDEBUG

all: zpaq libzpaq.o zpaq.o divsufsort.o

libzpaq.o: libzpaq.cpp libzpaq.h
	g++ $(CCFLAGS) -c libzpaq.cpp

zpaq.o: zpaq.cpp libzpaq.h
	g++ $(CCFLAGS) -c -DOPT zpaq.cpp

divsufsort.o: divsufsort.c divsufsort.h
	gcc $(CCFLAGS) -fopenmp -c divsufsort.c

zpaq: zpaq.cpp libzpaq.o libzpaq.h divsufsort.o divsufsort.h
	g++ $(CCFLAGS) -lpthread -fopenmp zpaq.cpp libzpaq.o divsufsort.o -o zpaq
