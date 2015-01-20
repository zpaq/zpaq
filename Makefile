CCFLAGS= -O3 -c -DNDEBUG divsufsort.c
PPFLAGS= -O3 -march=native -Dunix zpaq.cpp libzpaq.cpp divsufsort.o -pthread
prefix=/usr/local
BIN=${prefix}/bin
INC=${prefix}/include

all: zpaq

zpaq: zpaq.cpp libzpaq.cpp libzpaq.h divsufsort.o
	g++ $(PPFLAGS) -o $@

divsufsort.o: divsufsort.c divsufsort.h
	gcc $(CCFLAGS)

install: all
	@echo installing executable file to ${BIN}
	@mkdir -p ${BIN}
	@cp -f zpaq ${BIN}
	@chmod 755 ${BIN}/zpaq

uninstall:
	@echo removing executable file from ${BIN}
	@rm -f ${BIN}/zpaq

clean:
	rm -vf zpaq divsufsort.o
