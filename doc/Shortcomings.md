# Disk images and database files #

FuseCompress compresses files en bloc, without breaking them up into many small pieces. It is this feature that allows it to take advantage of modern compression algorithms, such as LZMA (or even bzip2), which would perform substantially worse if applied to small blocks. This approach does, however, have a big disadvantage as well: It makes random accesses into the compressed data impossible. For small files, FuseCompress either caches uncompressed data (see the section on executable binaries below), or re-reads those files from the beginning. For large files, however, FuseCompress has to uncompress (and later re-compress) files on which non-linear accesses are performed to disk, causing excessive CPU and I/O load and low performance. It is thus not recommended to use FuseCompress for large (read/write) disk images or databases. It is possible to prevent compression of such files in two ways:

## Don't compress files above a certain size ##

Using the `maxcompress` option, it is possible to specify a size (in MB) above which FuseCompress will not attempt to compress files any more:

```
fusecompress -o maxcomp=128 /mount/point
```

This will prevent compression of all files larger than 128 MB. (Note that it is not possible for FuseCompress to tell how large a file will be at the time of creation, so it is still possible that a file larger than that size is initially written with compression enabled.)

## Exclude files with certain extensions from compression ##

FuseCompress has a built-in list of file extensions it will not try to compress, ever. This list can be expanded by the user with the `nocompext` option:

```
fusecompress -o nocompext=foo,nocompext=bar /mount/point
```

In this FuseCompress filesystem, files with names such as `file.foo` or `image.bar` will not be compressed.

# Executable binaries #

Binary executables and shared libraries are memory-mapped when run and thus result in very random access patterns. Normally, this means that these kinds of files are either uncompressed to disk when run or repeatedly read (and thus repeatedly decompressed) from the beginning. This can be avoided by enabling the `cache` option:

```
fusecompress -o cache /mount/point
```

This option will cause fusecompress to cache data not currently requested by the kernel when encountering it and serving it from cache when the kernel wants it, avoiding repeated decompression as well as decompression to disk. The maximum cache size can be specified using the `cache_size` option and defaults to 100 MB:

```
fusecompress -o cache,cache_size=32 /mount/point
```

This will mount a FuseCompress filesystem using a maximum of 32 MB for its cache.