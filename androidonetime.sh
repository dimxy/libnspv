./autogen.sh
./configure
cd src/tools/cryptoconditions
./autogen.sh
./configure --enable-androidlog
make
cd ../../..
make

