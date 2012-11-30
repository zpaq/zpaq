g++ -O2 -fomit-frame-pointer -s -march=pentiumpro zpaq.cpp libzpaq.cpp libzpaqo.cpp -o zpaq.exe -DNDEBUG
g++ -O2 -fomit-frame-pointer -s -march=pentiumpro zpipe.cpp libzpaq.cpp libzpaqo.cpp -o zpipe.exe -DNDEBUG
g++ -O2 -fomit-frame-pointer -s -march=pentiumpro zpsfx.cpp libzpaq.cpp libzpaqo.cpp -DNDEBUG
copy/b a.exe+zpsfx.tag zpsfx.exe
del a.exe
g++ -O2 -s -march=native -c -DNDEBUG libzpaq.cpp
g++ -O2 -s -march=native -c -DNDEBUG -DOPT zpaq.cpp
upx zpaq.exe
upx zpipe.exe
upx zpsfx.exe
