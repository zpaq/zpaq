g++ -O2 -s -march=native -DNDEBUG -Ic:\zpaq c:\zpaq\zpsfx.o c:\zpaq\libzpaq.o zpaqopt.cpp -o zpsfxopt.exe
upx -qqq -9 zpsfxopt.exe
