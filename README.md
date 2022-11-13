# davfs2 (based on v1.7.0) 
Mount a WebDAV resource as a regular file system.
the current source has pulled from http://download.savannah.nongnu.org/releases/davfs2/

# Adapting for mulsel Arch.
Here I've revised some codes and adding some missing sources such as fstab.c / rsmatch.
thus, alonging to change config.h.in and src/Makefile.in

# 当前Release bin只支持musel 架构（例如MT7620/MT7621 soc)
注意：要使用davfs2， 必须确认linux kernel包含/sys/fs/fuse.ko (例如：Padavan 需要在编译时包含fuse)

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
