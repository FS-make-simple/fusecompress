Here's a little introduction on how to use FuseCompress.

## Basic use ##

There are two basic ways to use FuseCompress. The first is

```
fusecompress /foo
```

This starts FuseCompress and mounts `/foo` as a compressed filesystem, meaning that (almost) all files you write to `/foo` will be compressed and transparently decompressed when you read them again. Once you unmount the compressed filesystem using

```
fusermount -u /foo
```

you will see the compressed files in directory `/foo` as they are actually stored on disk.

The other way to use FuseCompress is

```
fusecompress /foo /bar
```

This mounts a compressed filesystem at `/bar` where you can store and retrieve transparently compressed data, but stores the actual files on disk in directory `/foo` and thus allows you to see the compressed data while the filesystem is mounted. There are not many cases where this is useful, and you will usually want to use the first method.

**_WARNING_:** Mounting FuseCompress filesystems over their backing directory (first method) can lead to lockups when using libfuse 2.7.x. (See [issue #14](https://code.google.com/p/fusecompress/issues/detail?id=#14).) This problem does not seem to occur when using separate mount points and backing directories (second method) or with libfuse 2.8.0pre1.

## Choosing a compression method ##

By default, FuseCompress runs using [LZO](http://en.wikipedia.org/wiki/LZO) compression. It is also possible to use different methods:

```
fusecompress -c lzma /foo
```

This mounts a compressed filesystem at `/foo` using [LZMA](http://en.wikipedia.org/wiki/LZMA) compression. This means that all new files created (and all files that had to be uncompressed for some reason) will be compressed using LZMA. Existing files will retain whatever compression method they were created with. The other methods available are [Deflate](http://en.wikipedia.org/wiki/Deflate) (`-c gz`), [bzip2](http://en.wikipedia.org/wiki/bzip2) (`-c bz2`), and null (`-c null`, only useful for debugging).

It is also possible to give the compression level as a filesystem option (see below), which is helpful when mounting FuseCompress filesystems from /etc/fstab.

**_WARNING_:** The LZMA Utils library used in FuseCompress is not stable yet, and different revisions may create mutually incompatible data streams, potentially leaving you unable to read your previously written data in case of an update.

## Compression level ##

Some compression methods (LZMA, Deflate, and bzip2) support different compression levels (1 to 9), with the lower levels being faster with lower compression ratio, and the higher levels slower (sometimes dramatically so) with higher compression ratio. The defaults are 4 for LZMA, and 6 for Deflate and bzip2. If you want to use a different level, you can use the `-l` option:

```
fusecompress -c lzma -l 7 /foo
```

There is a wiki page that tries to help you in [choosing your compression method and level](ChoosingYourCompressionLevel.md).

## FUSE options ##

FuseCompress allows you to pass options to FUSE using the `-o` option. Commonly used options are:

  * `allow_other` - allows other users to access the files inside; by default, only the user mounting the filesystem can see the files inside. For the root user, `allow_other` is on by default.
  * `suid` and `dev` - allows SUID binaries and device files to be used inside your compressed filesystem (default when mounting as root)
  * `lzo`, `gz`, `lzma`, and `bzip2` select the corresponding compression method. This way of specifying it works better in /etc/fstab.
  * `cache` or `cache_skipped` causes FuseCompress to cache the decompressed data it comes along while seeking for another bit of data. This avoids repeated decompression (and possibly recompression) of files on random read accesses. This is a new feature and has not yet been extensively tested, but its use is highly recommended anyway, especially in combination with advanced algorithms such as LZMA, where decompression and especially recompression take an awful long time. The maximum cache size defaults to 100 MB and can be changed using the `cache_size` option. For instance, `cache_size=32` reduces the maximum size to 32 MB.
  * `uncompressed_binaries` tells FuseCompress not to compress files in the standard binary directories (such as `/bin`, `/usr/bin`, `/sbin`, etc.) or shared objects (files with `.so` in their names). This option is probably only useful if you do not want to use `cache_skipped`. The idea is to prevent the permanent decompression and recompression of frequently used binaries, which will not usually occur when using `cache_skipped`.
  * `detach` and `nodetach` force FuseCompress to run in the background or in the foreground, respectively. If these options are not given, release builds will detach, while debug builds will not.
  * `noterm` keeps FuseCompress from exiting when it receives a SIGTERM signal. This option is vital if you are using FuseCompress as your root filesystem. Most (if not all) distributions send a SIGTERM to all processes when shutting down. If FuseCompress heeded that signal you would no longer have access to your root filesystem and your system would hang. `noterm` is the default when mounting a filesystem as root. If you want the terminating behavior instead, use the option `term`.
> (There is a related issue with the SIGKILL signal that is usually sent to processes that won't die voluntarily when sent a SIGTERM. SIGKILL cannot be handled and will in all likelihood result in data corruption if it catches FuseCompress - or indeed any other FUSE filesystem - with its pants down. This issue has to be [fixed in your distributions init system](http://bugs.debian.org/cgi-bin/bugreport.cgi?bug=476698) before you can use FUSE filesystems for `/` safely.)
  * `maxcompress` specifies the file size above which FuseCompress will no longer try to compress a file. There is no such limit by default. Note that FuseCompress cannot know the final size of a file that is created from scratch, so it is still possible that newly created files above that size will be written with compression enabled.
  * `nocompext` allows the user to extend the list of uncompressible file types built into FuseCompress. For instance, `nocompext=foo,nocompext=bla` will keep files like `file.foo` and `image.bla` from being compressed.

Example:

```
fusecompress -o gz,cache_skipped /
```

This mounts a compressed root filesystem using Deflate compression and with caching of uncompressed data. (**Caveat**: Do not do this unless you can make sure that your system automatically executes that command on the next reboot, _before_ using _any_ of the files in the root filesystem.)

## Access Control ##

FuseCompress filesystems mounted by the root user exhibit the same default behavior as regular kernel filesystems. For instance, they allow SUID binaries and devices. When mounting a filesystem as a regular user, however, none of these are enabled by default, and no other user (including root!) can access the filesystem. If you want to enable other users to see your mounted filesystem, you need to give the option `allow_other`. This is only possible if it has been explicitly enabled in `/etc/fuse.conf`. SUID binaries and devices cannot be enabled by regular users.