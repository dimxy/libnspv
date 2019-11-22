./autogen.sh
./configure
cd src/tools/cryptoconditions
./autogen.sh
./configure --enable-androidso
make
cd ../../..
make

