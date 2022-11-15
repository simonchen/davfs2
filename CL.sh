HOST=$1
tar -xzvf ./musl-1.2.3.tar.gz -C . &&
tar -xzvf ./zlib-1.2.11.tar.gz -C . &&
tar -xzvf ./expat-2.5.0.tar.gz -C . &&
tar -xzvf ./neon-0.31.0.tar.gz -C . &&
./musl-1.2.3/configure && make install && 
./zilib-1.2.11/configure --includedir="/usr/local/musl/include" --libdir="/usr/local/musl/lib" --static --sharedlibdir=. && make install &&
export LDFLAGS=-Wl,-L/usr/local/musl/lib,-lexpat &&
export CPPFLAGS=-I/usr/local/musl/include &&
./expat-2.5.0/configure --prefix=/usr/local/musl --with-expat --enable-static --host=$HOST && make install &&
ln -s /usr/local/musl/bin/neon-config /usr/local/bin/neon-config &&
./configure CC="/usr/local/musl/bin/musl-gcc" CFLAGS="-I/user/local/musl/include -I/usr/local/musl/include/neon" CPPFLAGS="-I/usr/local/musl/include -I/usr/local/musl/include/neon" LDFLAGS="-Wl,-Bstatic -L/usr/local/musl/lib" --prefix=/usr/local/musl --host=$HOST && makeinstall
