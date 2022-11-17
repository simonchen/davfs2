#!/bin/sh
echo "\n================ ENVIROMENT ========================\n"
env

echo "\n================ Extracting dependent packages =====\n"
tar -xzvf ./musl-1.2.3.tar.gz -C . &&
tar -xzvf ./zlib-1.2.11.tar.gz -C . &&
tar -xzvf ./expat-2.5.0.tar.gz -C . &&
tar -xzvf ./neon-0.31.0.tar.gz -C . &&

echo "\n================ MUSL COMPLATION =================\n"
./musl-1.2.3/configure --host=$ENV_HOST && make install &&

echo "\n================ ZLIB COMPLATION =================\n"
./zlib-1.2.11/configure --includedir="/usr/local/musl/include" --libdir="/usr/local/musl/lib" --static --sharedlibdir=. && make install &&

echo "\n================ EXPAT COMPLATION =================\n"
cd ./expat-2.5.0 && ./configure --enable-static --host=$ENV_HOST && make install && cd .. && 

echo "\n================ neon COMPLATION =================\n"
export LDFLAGS=-Wl,-L/usr/local/musl/lib,-lexpat &&
export CPPFLAGS=-I/usr/local/musl/include &&
cd ./neon-0.31.0 && ./configure --prefix=/usr/local/musl --with-expat --enable-static --host=$ENV_HOST && make install && cd .. &&
ln -s /usr/local/musl/bin/neon-config /usr/local/bin/neon-config &&

echo "\n================ DAVFS2 COMPLATION =================\n"
chmod +x ./configure
./configure CC="/usr/local/musl/bin/musl-gcc" CFLAGS="-I/user/local/musl/include -I/usr/local/musl/include/neon" CPPFLAGS="-I/usr/local/musl/include -I/usr/local/musl/include/neon" LDFLAGS="-Wl,-Bstatic -L/usr/local/musl/lib" --prefix=/usr/local/musl --host=$ENV_HOST && make install

echo "\n================ DONE =============================\n"
