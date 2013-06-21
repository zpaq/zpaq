zpaq: zpaq.cpp libzpaq.cpp libzpaq.h divsufsort.c divsufsort.h
	g++ -O3 -s -Dunix -DNDEBUG zpaq.cpp libzpaq.cpp divsufsort.c -fopenmp -o zpaq
