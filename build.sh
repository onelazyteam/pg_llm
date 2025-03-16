mkdir build
cd build
rm -rf *
cmake -DCMAKE_BUILD_TYPE=Debug ..
make -j8
make -j8 install
