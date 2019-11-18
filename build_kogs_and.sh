
gcc -shared ./src/Unity/cc/kogswrapper.c -o ../libnspv-bin/libkogswrapper.so -L ./.libs -Wl,-l:libnspv.so -I ./include/ -I ./src/logdb/include/ -fPIC