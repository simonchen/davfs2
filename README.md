# davfs2 (v1.7.0)
Mount a WebDAV resource as a regular file system
the current source has pulled from http://download.savannah.nongnu.org/releases/davfs2/

# dockcross - compile issue
# mips-unknown-linux-gnu
Linking libs: expat-2.5.0
## for mipsel-unknown-linux-gnu
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
it's stuck on last error, exactly it's raised by previous warning at
```
warning: libz.so.1, needed by /usr/local/lib/libxml2.so, not found (try using -rpath or -rpath-link)
```
furthermore, the bash script failed to check next steps incorrectly:
```
configure:8014: incompatible neon library version 0.31.0: wanted 0.27 28 29 30 31 32
```
