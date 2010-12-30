#!/bin/bash
# To install zpaq: sudo ./install.sh
# You will also need g++, pod2man, gzip

# Set for your C++ compiler
CC="g++"
CCFLAGS="-O3 -march=native -s"

# Installation directories, modify if needed
BIN=/usr/bin         # zpaq executable will go here
OBJ=/usr/lib         # zpaq/*.o support files will go here
INC=/usr/include     # libzpaq.h will go here
MAN=/usr/share/man   # man1/zpaq.1.gz will go here

# Check that all files exist
for i in zpaq.cpp libzpaq.cpp libzpaqo.cpp libzpaq.h zpaq.1.pod ; do
  if [[ ! -r $i ]] ; then
    echo "File $i not found."
    echo Get zpaq and libzpaq from http://mattmahoney.net/dc/zpaq.html
    exit
  fi
done

# Check that installation directories exist
# and you have permission to put files in them.
for i in $BIN $OBJ $INC $MAN $MAN/man1 ; do
  if [[ ! -d $i ]] ; then
    echo "Directory $i does not exist."
    exit
  elif [[ ! -w $i ]] ; then
    echo "You need write permission for directory $i"
    echo "Try: sudo $0"
    exit
  fi
done

# Create directory for *.o files
mkdir -p $OBJ/zpaq
chmod 755 $OBJ/zpaq
if [[ ! -d $OBJ/zpaq || ! -w $OBJ/zpaq ]] ; then
  echo "Creation of writable $OBJ/zpaq failed"
  exit
fi

# Create man page
pod2man zpaq.1.pod > $MAN/man1/zpaq.1
gzip -9 -f $MAN/man1/zpaq.1
if [ ! -r $MAN/man1/zpaq.1.gz ] ; then
  echo "Warning $MAN/man1/zpaq.1.gz not created"
fi

# Compile
$CC $CCFLAGS -DNDEBUG zpaq.cpp libzpaq.cpp libzpaqo.cpp -o zpaq \
  -DOPT="\"$CC $CCFLAGS zpaqopt.cpp $OBJ/zpaq/zpaq.o $OBJ/zpaq/libzpaq.o -o zpaqopt.exe\""
$CC $CCFLAGS -DNDEBUG -c zpaq.cpp libzpaq.cpp
for i in zpaq zpaq.o libzpaq.o ; do
  if [[ ! -r $i ]] ; then
    echo "Compile of $i failed"
    exit
  fi
done

# Put files in their places
mv -f zpaq $BIN
mv -f zpaq.o libzpaq.o $OBJ/zpaq
cp -f libzpaq.h $INC

# Test if files are installed
for i in $BIN/zpaq $OBJ/zpaq/zpaq.o $OBJ/zpaq/libzpaq.o $INC/libzpaq.h ; do
  if [[ ! -r $i ]] ; then
    echo "Installation of $i failed"
  fi
done
echo "Finished installing zpaq"

