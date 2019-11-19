
gcc -shared ./src/tools/nspv.c -I ./src -DLIBNSPV_BUILD ./.libs/libbtc.a -o ./.libs/libnspv.so -O2  -Wno-unused-parameter -Wl,-L./src/secp256k1/.libs -Wl,-L./src/tools/cryptoconditions/.libs -Xlinker -levent -Wl,-soname,libcryptoconditions.so -Wl,-soname,libsecp256k1.so -Wl,-lsodium -Wl,-lpthread -fPIC

gcc -shared ./src/Unity/cc/kogswrapper.c -o ../libnspv-bin/libkogsplugin.so -L ./.libs -Wl,-soname,libnspv.so -I ./include/ -I ./src/logdb/include/ -fPIC
# gcc -shared ./src/Unity/cc/kogswrapper.c -o ../libnspv-bin/libkogsplugin.so ./.libs/libnspv.so -I ./include/ -I ./src/logdb/include/ -fPIC
# static link:
# gcc -shared ./src/Unity/cc/kogswrapper.c -o ../libnspv-bin/libkogsplugin.so ./.libs/libnspv.a ./src/secp256k1/.libs/libsecp256k1.a -I ./include/ -I ./src/logdb/include/ -fPIC
# gcc -shared ./src/Unity/cc/kogswrapper.c -o ../libnspv-bin/libkogsplugin.so ./.libs/libnspv.a -I ./include/ -I ./src/logdb/include/ -fPIC
# gcc -shared ./src/Unity/cc/kogswrapper.c -o ../libnspv-bin/libkogsplugin.so -L ./.libs -Wl,-soname,libnspv.so  -L ./src/secp256k1/.libs -Wl,-soname,libsecp256k1.so  -L ./src/tools/cryptoconditions/.libs -Wl,-soname,libcryptoconditions.so  -I ./include/ -I ./src/logdb/include/ -fPIC
