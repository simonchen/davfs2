# davfs2 (based on v1.7.0) 
Mount a WebDAV resource as a regular file system.
the current source has pulled from http://download.savannah.nongnu.org/releases/davfs2/

# Adaptes for mulsel Arch.
Here I've revised some codes and adding some missing sources such as fstab.c / rsmatch.
thus, alonging to change config.h.in and src/Makefile.in

# New features special for wierless router firmware (on linux)
1. the owner of secrets file doesn't have to be same as current user (permission check is removed)
2. davfs2.conf, the variable dav_user / dav_group can be loaded from user config file (as system wide), therefore, the mount hasn't to be run at 'davfs2' group. 

# 当前Release bin只支持musel 架构（例如MT7620/MT7621 soc)
注意：要使用davfs2， 必须确认linux kernel包含/sys/fs/fuse.ko (例如：Padavan 需要在编译时包含fuse)

## 编译/链接库 musl-gcc wrapper insted of Toolchain gcc compiler
Refers to https://www.musl-libc.org/faq.html

when we've copied the gcc / c runtime static libs in /usr/local/lib, appending LDFLAGS="-L/usr/local/lib" that tells gcc to link libs in the directory as priority.

### musl
```
./configure --host=mipsel-unknown-linux-gnu && make install
```

### zlib
```
./configure --prefix=/usr/local/musl --includedir="/usr/local/musl/include" --libdir="/usr/local/musl/lib" --static --sharedlibdir=.
```

### expat (alternative to libxml2)
```
./configure --prefix=/usr/local/musl --enable-static --host=mipsel-unknown-linux-gnu
```

### neno
```
./configure LDFLAGS="-Wl,-L/usr/local/musl/lib,-lexpat" CPPFLAGS=-I/usr/local/musl/include --prefix=/usr/local/musl --with-expat --enable-static --host=mipsel-unknown-linux-gnu
ln -s /usr/local/musl/bin/neon-config /usr/local/bin/neon-config
```

### davfs2
```
./configure CC="/usr/local/musl/bin/musl-gcc" CFLAGS="-I/user/local/musl/include -I/usr/local/musl/include/neon" CPPFLAGS="-I/usr/local/musl/include -I/usr/local/musl/include/neon" LDFLAGS="-Wl,-Bstatic -L/usr/local/musl/lib" --prefix=/usr/local/musl --host=mipsel-unknown-linux-gnu
```
