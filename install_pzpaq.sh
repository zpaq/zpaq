#!/bin/bash
# To install pzpaq: sudo ./install.sh
# You will also need g++, pod2man, gzip

# Set for your C++ compiler
CC="g++"
CCFLAGS="-O3 -march=native -s"

# Installation directories, modify if needed
BIN=/usr/bin         # zpaq executable will go here
OBJ=/usr/lib         # zpaq/*.o support files will go here
INC=/usr/include     # libzpaq.h will go here
MAN=/usr/share/man   # man1/pzpaq.1.gz will go here

# Check that all files exist
for i in pzpaq.cpp libzpaq.cpp libzpaqo.cpp libzpaq.h pzpaq.1.pod ; do
  if [[ ! -r $i ]] ; then
    echo "File $i not found."
    echo Get pzpaq and libzpaq from http://mattmahoney.net/dc/zpaq.html
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
pod2man --center="User Commands" pzpaq.1.pod > $MAN/man1/pzpaq.1
gzip -9 -f $MAN/man1/pzpaq.1
if [ ! -r $MAN/man1/pzpaq.1.gz ] ; then
  echo "Warning $MAN/man1/pzpaq.1.gz not created"
fi

# Compile
$CC $CCFLAGS -DNDEBUG -lpthread pzpaq.cpp libzpaq.cpp libzpaqo.cpp -o pzpaq \
  -DOPT="\"$CC $CCFLAGS -lpthread \$1.cpp $OBJ/zpaq/pzpaq.o $OBJ/zpaq/libzpaq.o -o \$1.exe\""
$CC $CCFLAGS -DNDEBUG -c pzpaq.cpp libzpaq.cpp
for i in pzpaq pzpaq.o libzpaq.o ; do
  if [[ ! -r $i ]] ; then
    echo "Compile of $i failed"
    exit
  fi
done

# Put files in their places
mv -f pzpaq $BIN
mv -f pzpaq.o libzpaq.o $OBJ/zpaq
cp -f libzpaq.h $INC

# Test if files are installed
for i in $BIN/pzpaq $OBJ/zpaq/pzpaq.o $OBJ/zpaq/libzpaq.o $INC/libzpaq.h ; do
  if [[ ! -r $i ]] ; then
    echo "Installation of $i failed"
  fi
done
echo "Finished installing pzpaq"

