
gcc -shared ./src/Unity/cc/kogswrapper.c -o ../libnspv-bin/libkogsplugin.so -L ./.libs -Wl,-soname,libnspv.so -I ./include/ -I ./src/logdb/include/ -fPIC
# gcc -shared ./src/Unity/cc/kogswrapper.c -o ../libnspv-bin/libkogsplugin.so ./.libs/libnspv.so -I ./include/ -I ./src/logdb/include/ -fPIC
