CCFLAGS = -O3 -march=native -s -fomit-frame-pointer

all: zpaq zpaq.o libzpaq.o

zpaq: zpaq.cpp libzpaq.cpp libzpaqo.cpp libzpaq.h
	g++ $(CCFLAGS) -DNDEBUG zpaq.cpp libzpaq.cpp libzpaqo.cpp -o zpaq \
	-DOPT="\"g++ $(CCFLAGS) zpaq.o libzpaq.o zpaqopt.cpp -o zpaqopt.exe\""

zpaq.o: zpaq.cpp libzpaq.h
	g++ $(CCFLAGS) -DNDEBUG -c zpaq.cpp

libzpaq.o: libzpaq.cpp libzpaq.h
	g++ $(CCFLAGS) -DNDEBUG -c libzpaq.cpp

