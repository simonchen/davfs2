# davfs2 (v1.7.0) ongoing cross-platform compliation
Mount a WebDAV resource as a regular file system.
the current source has pulled from http://download.savannah.nongnu.org/releases/davfs2/

# Revision for adapting mulsel Arch.
Here I've revised some codes and adding some missing sources such as fstab.c / rsmatch.

## Using musl-gcc wrapper insted of Toolchain gcc compiler
Refers to https://www.musl-libc.org/faq.html

when we've copied the gcc / c runtime static libs in /usr/local/lib, appending LDFLAGS="-L/usr/local/lib" that tells gcc to link libs in the directory as priority.

### zlib
```
./configure --includedir="/usr/local/musl/include" --libdir="/usr/local/musl/lib" --static --sharedlibdir=.
```

### expat (alternative to libxml2)
```
export LDFLAGS=-Wl,-L/usr/local/musl/lib,-lexpat
export CPPFLAGS=-I/usr/local/musl/include
./configure --prefix=/usr/local/musl --with-expat --enable-static --host=mipsel-unknown-linux-gnu
```

### davfs2
```
ln -s /usr/local/musl/bin/neon-config /usr/local/bin/neon-config
./configure CC="/usr/local/musl/bin/musl-gcc" CFLAGS="-I/user/local/musl/include -I/usr/local/musl/include/neon" CPPFLAGS="-I/usr/local/musl/include -I/usr/local/musl/include/neon" LDFLAGS="-Wl,-Bstatic -L/usr/local/musl/lib" --prefix=/usr/local/musl --host=mipsel-unknown-linux-gnu
```
