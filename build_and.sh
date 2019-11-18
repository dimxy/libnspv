
gcc -shared ./src/Unity/cc/kogswrapper.c -o ../libnspv-bin/libkogswrapper.so -L ./.libs -Wl,-lnspv -I ./include/ -I ./src/logdb/include/ -fPIC