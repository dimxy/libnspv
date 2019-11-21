./autogen.sh
./configure
cd src/tools/cryptoconditions
./autogen.sh
./configure --enable-android-log
make
cd ../../..
make

