
# gcc -shared ./src/Unity/cc/kogswrapper.c -o ../libnspv-bin/libkogsplugin.so -L ./.libs -Wl,-soname,libnspv.so -I ./include/ -I ./src/logdb/include/ -fPIC
# gcc -shared ./src/Unity/cc/kogswrapper.c -o ../libnspv-bin/libkogsplugin.so ./.libs/libnspv.so -I ./include/ -I ./src/logdb/include/ -fPIC
# static link:
# gcc -shared ./src/Unity/cc/kogswrapper.c -o ../libnspv-bin/libkogsplugin.so ./.libs/libnspv.a ./src/secp256k1/.libs/libsecp256k1.a -I ./include/ -I ./src/logdb/include/ -fPIC
gcc -shared ./src/Unity/cc/kogswrapper.c -o ../libnspv-bin/libkogsplugin.so ./.libs/libnspv.a -I ./include/ -I ./src/logdb/include/ -fPIC
