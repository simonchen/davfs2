# davfs2 (v1.7.0) ongoing cross-platform compliation
Mount a WebDAV resource as a regular file system.
the current source has pulled from http://download.savannah.nongnu.org/releases/davfs2/

# dockcross - compile issues

## mips-unknown-linux-gnu
Linking libs: expat-2.5.0

## mipsel-unknown-linux-gnu
Linking libs: zlib-1.2.11, libxml2-2.9.11

Finally, when running ./configure , it raised errors as follows:
```
configure:7809: checking linking against neon
configure:7826: /usr/xcc/mipsel-unknown-linux-gnu/bin/mipsel-unknown-linux-gnu-gcc -o conftest -g -O2 -I/usr/local/include/neon -D_LARGEFILE64_SOURCE -DNE_LFS   conftest.c  -L/usr/local/lib -lneon -L/usr/local/lib -lxml2 >&5
/usr/xcc/mipsel-unknown-linux-gnu/lib/gcc/mipsel-unknown-linux-gnu/8.5.0/../../../../mipsel-unknown-linux-gnu/bin/ld: warning: libz.so.1, needed by /usr/local/lib/libxml2.so, not found (try using -rpath or -rpath-link)
/usr/xcc/mipsel-unknown-linux-gnu/lib/gcc/mipsel-unknown-linux-gnu/8.5.0/../../../../mipsel-unknown-linux-gnu/bin/ld: /usr/local/lib/libxml2.so: undefined reference to `inflateEnd'
/usr/xcc/mipsel-unknown-linux-gnu/lib/gcc/mipsel-unknown-linux-gnu/8.5.0/../../../../mipsel-unknown-linux-gnu/bin/ld: /usr/local/lib/libxml2.so: undefined reference to `gzdirect@ZLIB_1.2.2.3'
/usr/xcc/mipsel-unknown-linux-gnu/lib/gcc/mipsel-unknown-linux-gnu/8.5.0/../../../../mipsel-unknown-linux-gnu/bin/ld: /usr/local/lib/libxml2.so: undefined reference to `gzclose'
/usr/xcc/mipsel-unknown-linux-gnu/lib/gcc/mipsel-unknown-linux-gnu/8.5.0/../../../../mipsel-unknown-linux-gnu/bin/ld: /usr/local/lib/libxml2.so: undefined reference to `deflate'
/usr/xcc/mipsel-unknown-linux-gnu/lib/gcc/mipsel-unknown-linux-gnu/8.5.0/../../../../mipsel-unknown-linux-gnu/bin/ld: /usr/local/lib/libxml2.so: undefined reference to `inflateInit2_'
/usr/xcc/mipsel-unknown-linux-gnu/lib/gcc/mipsel-unknown-linux-gnu/8.5.0/../../../../mipsel-unknown-linux-gnu/bin/ld: /usr/local/lib/libxml2.so: undefined reference to `inflate'
/usr/xcc/mipsel-unknown-linux-gnu/lib/gcc/mipsel-unknown-linux-gnu/8.5.0/../../../../mipsel-unknown-linux-gnu/bin/ld: /usr/local/lib/libxml2.so: undefined reference to `gzwrite'
/usr/xcc/mipsel-unknown-linux-gnu/lib/gcc/mipsel-unknown-linux-gnu/8.5.0/../../../../mipsel-unknown-linux-gnu/bin/ld: /usr/local/lib/libxml2.so: undefined reference to `crc32'
/usr/xcc/mipsel-unknown-linux-gnu/lib/gcc/mipsel-unknown-linux-gnu/8.5.0/../../../../mipsel-unknown-linux-gnu/bin/ld: /usr/local/lib/libxml2.so: undefined reference to `deflateEnd'
/usr/xcc/mipsel-unknown-linux-gnu/lib/gcc/mipsel-unknown-linux-gnu/8.5.0/../../../../mipsel-unknown-linux-gnu/bin/ld: /usr/local/lib/libxml2.so: undefined reference to `gzdopen'
/usr/xcc/mipsel-unknown-linux-gnu/lib/gcc/mipsel-unknown-linux-gnu/8.5.0/../../../../mipsel-unknown-linux-gnu/bin/ld: /usr/local/lib/libxml2.so: undefined reference to `deflateInit2_'
/usr/xcc/mipsel-unknown-linux-gnu/lib/gcc/mipsel-unknown-linux-gnu/8.5.0/../../../../mipsel-unknown-linux-gnu/bin/ld: /usr/local/lib/libxml2.so: undefined reference to `gzopen64@ZLIB_1.2.3.3'
/usr/xcc/mipsel-unknown-linux-gnu/lib/gcc/mipsel-unknown-linux-gnu/8.5.0/../../../../mipsel-unknown-linux-gnu/bin/ld: /usr/local/lib/libxml2.so: undefined reference to `gzread'
collect2: error: ld returned 1 exit status
configure:7826: $? = 1
configure: failed program was:
| /* confdefs.h */
| #define PACKAGE_NAME "davfs2"
| #define PACKAGE_TARNAME "davfs2"
| #define PACKAGE_VERSION "1.7.0"
| #define PACKAGE_STRING "davfs2 1.7.0"
| #define PACKAGE_BUGREPORT "http://savannah.nongnu.org/projects/davfs2"
| #define PACKAGE_URL ""
| #define PACKAGE "davfs2"
| #define VERSION "1.7.0"
| #define ENABLE_NLS 1
| #define HAVE_GETTEXT 1
| #define HAVE_DCGETTEXT 1
| /* end confdefs.h.  */
| #include <ne_utils.h>
| int
| main (void)
| {
| ne_version_match(0, 0);
|   ;
|   return 0;
| }
configure:7835: result: no
configure:8014: incompatible neon library version 0.31.0: wanted 0.27 28 29 30 31 32
configure:8026: error: could not find neon
```
**solution**

Since it was stuck on the last error, but exactly it was raised by previous warning at
```
warning: libz.so.1, needed by /usr/local/lib/libxml2.so, not found (try using -rpath or -rpath-link)
```
furthermore, the bash script failed to check next steps incorrectly:
```
configure:8014: incompatible neon library version 0.31.0: wanted 0.27 28 29 30 31 32
```
obviously, it's missing the libz.so.1 to link, the reason is that 'libz' path has not been added via the bash commmand:
```
/usr/local/bin/neon-config --libs
```
after changed the following lines to add "-L/usr/local -lz", the ./configure is bypassed!
```
    --libs)
        LIBS="-lneon  -L/usr/local/lib -lxml2 -L/usr/local/lib -lz"
```


# ./configure

## adding --static --enable-static --enable-static=yes --disable-shared for the compliation for all libs.

## Toolchain - the existing static lib paths
that's gcc / c runtime libs, suggest to copy all the static libs in /usr/local/lib/ , so gcc compiler will be able to link those as priority.

### gcc runtime
```
/usr/xcc/mipsel-unknown-linux-gnu/lib/gcc/mipsel-unknown-linux-gnu/8.5.0/libgcc.a
```

### c runtime
```
#ls -lt /usr/xcc/mipsel-unknown-linux-gnu/mipsel-unknown-linux-gnu/sysroot/usr/lib/lib*.a
-r--r--r-- 1 root root    55768 Nov  1 18:40 mipsel-unknown-linux-gnu/sysroot/usr/lib/libutil.a
-r--r--r-- 1 root root   406270 Nov  1 18:40 mipsel-unknown-linux-gnu/sysroot/usr/lib/librpcsvc.a
-r--r--r-- 1 root root   222936 Nov  1 18:40 mipsel-unknown-linux-gnu/sysroot/usr/lib/libanl.a
-r--r--r-- 1 root root   217408 Nov  1 18:40 mipsel-unknown-linux-gnu/sysroot/usr/lib/libcrypt.a
-r--r--r-- 1 root root   598156 Nov  1 18:40 mipsel-unknown-linux-gnu/sysroot/usr/lib/libresolv.a
-r--r--r-- 1 root root  5091134 Nov  1 18:40 mipsel-unknown-linux-gnu/sysroot/usr/lib/libpthread.a
-r--r--r-- 1 root root   723434 Nov  1 18:40 mipsel-unknown-linux-gnu/sysroot/usr/lib/librt.a
-r--r--r-- 1 root root 32290512 Nov  1 18:40 mipsel-unknown-linux-gnu/sysroot/usr/lib/libc.a
-r--r--r-- 1 root root   115094 Nov  1 18:40 mipsel-unknown-linux-gnu/sysroot/usr/lib/libc_nonshared.a
-r--r--r-- 1 root root     2730 Nov  1 18:40 mipsel-unknown-linux-gnu/sysroot/usr/lib/libg.a
-r--r--r-- 1 root root     9168 Nov  1 18:40 mipsel-unknown-linux-gnu/sysroot/usr/lib/libmcheck.a
-r--r--r-- 1 root root   305662 Nov  1 18:40 mipsel-unknown-linux-gnu/sysroot/usr/lib/libdl.a
-r--r--r-- 1 root root  3930458 Nov  1 18:40 mipsel-unknown-linux-gnu/sysroot/usr/lib/libm.a
-r--r--r-- 1 root root    24278 Nov  1 18:40 mipsel-unknown-linux-gnu/sysroot/usr/lib/libBrokenLocale.a
```

## Using musl-gcc wrapper
Refers to https://www.musl-libc.org/faq.html

when we've copied the gcc / c runtime static libs in /usr/local/lib, appending LDFLAGS="-L/usr/local/lib" that tells gcc to link libs in the directory as priority.

### zlib
```
./configure --includedir="/usr/local/musl/include" --libdir="/usr/local/musl/lib" --static --sharedlibdir=.
```

### libxml2
```
./configure CC="/usr/local/musl/bin/musl-gcc" --includedir="/usr/local/musl/include /usr/local/include" --libdir="/usr/local/musl/lib" --with-zlib --without-python --host=mipsel-unknown-linux-gnu
```

### davfs2
```
./configure LDFLAGS="-L/usr/local/lib" LIBS="-lz -lm -lc" --enable-static=yes --host=mipsel-unknown-linux-gnu
```
