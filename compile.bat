g++ -O2 -fomit-frame-pointer -s -march=pentiumpro zpaq.cpp libzpaq.cpp libzpaqo.cpp -o zpaq.exe -DNDEBUG
g++ -O2 -fomit-frame-pointer -s -march=pentiumpro zpipe.cpp libzpaq.cpp libzpaqo.cpp -o zpipe.exe -DNDEBUG
g++ -O2 -fomit-frame-pointer -s -march=pentiumpro zpsfx.cpp libzpaq.cpp libzpaqo.cpp -DNDEBUG -o zpsfx.exe
g++ -O2 -fomit-frame-pointer -s -march=pentiumpro lzppre.cpp -DNDEBUG -o lzppre.exe
g++ -O2 -s -march=native -c -DNDEBUG libzpaq.cpp
g++ -O2 -s -march=native -c -DNDEBUG -DOPT zpaq.cpp
g++ -O2 -s -march=native -c -DNDEBUG zpsfx.cpp
upx -qq -9 zpaq.exe zpipe.exe zpsfx.exe lzppre.exe
