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
./zlib-1.2.11/configure --prefix=/usr/local/musl --includedir="/usr/local/musl/include" --libdir="/usr/local/musl/lib" --static --sharedlibdir=. && make install &&

echo "\n================ EXPAT COMPLATION =================\n"
cd ./expat-2.5.0 && ./configure --prefix=/usr/local/musl --enable-static --host=$ENV_HOST && make install && cd .. && 

echo "\n================ neon COMPLATION =================\n"
cd ./neon-0.31.0 && ./configure LDFLAGS="-Wl,-L/usr/local/musl/lib,-lexpat" CPPFLAGS=-I/usr/local/musl/include --prefix=/usr/local/musl --with-expat --enable-static --host=$ENV_HOST && make install && cd .. &&
ln -s /usr/local/musl/bin/neon-config /usr/local/bin/neon-config &&

echo "\n================ DAVFS2 COMPLATION =================\n"
chmod +x ./configure
./configure dav_user=admin dav_group=root dav_syscachedir=/tmp CC="/usr/local/musl/bin/musl-gcc" CFLAGS="-I/user/local/musl/include -I/usr/local/musl/include/neon" CPPFLAGS="-I/usr/local/musl/include -I/usr/local/musl/include/neon" LDFLAGS="-Wl,-Bstatic -L/usr/local/musl/lib" --prefix=/usr/local/musl --sysconfdir=/etc --host=$ENV_HOST && make install &&
echo "\n================ Release =============================\n"
mkdir ./$ENV_HOST
cp /usr/local/musl/sbin/*davfs ./$ENV_HOST/
mkdir ./$ENV_HOST/davfs2
cp ./etc/davfs2.conf ./$ENV_HOST/davfs2/
cp ./etc/secrets ./$ENV_HOST/davfs2/
ls -lt ./$ENV_HOST/
