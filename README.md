# davfs2
Mount a WebDAV resource as a regular file system.
The current source has pulled from http://download.savannah.nongnu.org/releases/davfs2/ (original v1.7.0)

# Prerequisite
Your kernel must include either the [coda](https://docs.kernel.org/filesystems/coda.html) file system or the [fuse](https://www.kernel.org/doc/html/latest/filesystems/fuse.html) file system or both. They may be loadable modules. Almost all Linux kernels support at least one of them.

当前只支持mipsel 架构（例如 MT7620/MT7621 soc)
注意：要使用davfs2， 必须确认linux内核已经集成coda或者fuse文件系统(/sys/fs/fuse.ko)

# Installation
## TODO

# Default system configure files
- /etc/davfs2/davfs2.conf
- /etc/davfs2/secrets

# Default permission with user / group
- dav_user: admin
- dav_group: root

# Customized configuration
The default system configure file can be overridden by command option for example:
```
mount.davfs2 <server-url> <mountpoint> -o conf=/tmp/davfs2.conf
```
Here `/tmp/davfs2.conf` is the customized configure file that have overwrited the default system config,

**Note** more options such as `secret`, `dav_user`, `dav_grou` etc. can be overridden in `davfs2.conf`
      you should know that `/tmp/davfs2.conf` could be lost once you restart system, please make sure that you put `davfs2conf` in a permanent directory.

# Remove permission limitation 
- **davfs2_secrets** the owner of secrets file doesn't have to be same as current user (permission check is removed)
- **davfs2.conf**  natively, dav_user / dav_group was customized as system wide, the group name must be 'davfs2' 

# For developer: Using musl-gcc wrapper instead of Toolchain gcc compiler
Refers to https://www.musl-libc.org/faq.html

Adding some missing sources such as fstab.c / rsmatch that musl didn't provide,
hence alonging to change config.h.in and src/Makefile.in

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
./configure dav_user=admin dav_group=root dav_syscachedir=/tmp/cache_davfs2 CC="/usr/local/musl/bin/musl-gcc" CFLAGS="-I/user/local/musl/include -I/usr/local/musl/include/neon" CPPFLAGS="-I/usr/local/musl/include -I/usr/local/musl/include/neon" LDFLAGS="-Wl,-Bstatic -L/usr/local/musl/lib" --prefix=/usr/local/musl --sysconfdir=/etc  --host=mipsel-unknown-linux-gnu
```
