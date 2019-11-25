./autogen.sh
./configure
cd src/tools/cryptoconditions
./autogen.sh
./configure --enable-androidso=yes
make
cd ../../..
make

