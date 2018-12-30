This page tells you how to build FuseCompress from source.

## What you need ##
  * the [FUSE](http://fuse.sourceforge.net/) library, libfuse. I am using version 2.7.2 as shipped with openSUSE 11.0. Newer version should do, too.
  * on Mac OS X, [MacFUSE](http://code.google.com/p/macfuse/). I am using version 2.3.0,2. Newer version might do, too.

You will also want to have at least one of the following data compression libraries:

  * [zlib](http://www.zlib.net/). Any version should do.
  * the [bzip2](http://www.bzip.org/) library, libbz2. Just take what comes with your distribution.
  * [LZMA Utils](http://tukaani.org/lzma/) with liblzma. You need an "experimental" release (version 4.42 and up) as there is no support for compression in the older releases of the library.
  * [LZO](http://www.oberhumer.com/opensource/lzo/) version 2.02 or up. minilzo works, too, but you will have to make minor adjustments to the `#include` statements in `lzo.c` and `lzo.h`.

In openSUSE, you can get all these packages by running
```
zypper in fuse-devel zlib-devel libbz2-devel lzma-alpha-devel lzo-devel
```
as root.

On Mac OS X with [MacPorts](http://www.macports.org/) installed, you can run
```
sudo port install liblzma zlib bzip2 lzo2
```
to get the compression libraries. MacFUSE has to be installed separately; the version in MacPorts (1.7) is outdated and may not work.

(Note that even though there are FUSE implementations for several operating systems, I have only ever tested on Linux (thoroughly) and Mac OS X (barely). If you get it working on other OS's, leave a comment and the necessary patches.)

## Compiling it ##

Run `./autogen.sh` if there is no `configure` file in the source tree yet.

Depending on whether you want to create a debug or a release build, enter `./configure --enable-debug` or just `./configure`. You should end up with a binary called `fusecompress` as well as the tools `fsck.fusecompress` and `fusecompress_offline`.

The build system will autodetect the available compression libraries and enable the corresponding compression modules. You can explicitly disable compression modules you do not want included:

```
./configure --disable-gz --disable-bz2 --disable-lzma --disable-lzo
```

This example will result in a fusecompress binary that can only handle null-compressed files.

## Running the test suite ##

If you have made changes, be sure to run the test suite to check if they broke anything. Enter `cd test ; bash run_tests`. Be aware that some tests create considerable load, for instance by spawning as many threads as possible to stress the filesystem. Some tests only run as root and are automatically skipped if you are running the test suite as another user. Most tests are written in Python, some in BASH and/or C. The test suite will only pass successfully if FuseCompress has been compiled with all compression modules enabled. The test suite currently only works on Linux.