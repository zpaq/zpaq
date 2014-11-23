zpaq: zpaq.cpp libzpaq.cpp libzpaq.h
	g++ -O3 -Dunix zpaq.cpp libzpaq.cpp -pthread -o zpaq
