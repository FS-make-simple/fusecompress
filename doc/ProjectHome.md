### What is FuseCompress? ###
FuseCompress provides a mountable Linux filesystem which transparently compresses its content.

Files stored in this filesystem are compressed on the fly and FUSE allows to create a transparent interface between compressed files and user applications.

FuseCompress supports different compression methods: LZO, gzip, bzip2, and LZMA.

### Isn't there a new version? ###
This is the legacy C implementation of FuseCompress (0.9.x tree). It performs slightly less well than the current one implemented in C++ (1.99.x tree), but does not exhibit flaws such as data corruption or files growing larger and larger that make the current code (as of July 17 2008) unusable in a production environment. You can find the new version at [Milan Svoboda's website](http://miio.net/fusecompress/). Because Milan is concentrating on the new version, I am now maintaining the old tree.

### How well does it work? ###
It works well enough that I have trusted it to handle the vast majority of my private data. I am also using it for my $HOME directory, source trees, and I have (with some really evil hacks) succeeded in installing an [openSUSE](http://www.opensuse.org/) 11.0 system with FuseCompress as its root filesystem. (That, however, is not something you should currently try at home. At least not on a real system.)

LZMA support should be reasonably stable. The LZMA format has changed between versions 4.999.3 and 4.999.5 of the LZMA library, but it is stable now.

I am still occasionally fixing slight deviations from the expected behavior of a Unix filesystem. These problems are all minor, but may still cause data loss, depending on how robust your applications are.

FuseCompress now has experimental support for Mac OS X and [MacFUSE](http://code.google.com/p/macfuse/).

### Where to get it ###
  * [Packages for openSUSE](http://download.opensuse.org/repositories/home:/Ulih/)
  * Source code can be found in the [SVN repository](http://code.google.com/p/fusecompress/source/checkout).

### More information ###
  * [Build HOWTO](BuildHOWTO.md)
  * [How to use FuseCompress](Usage.md)
  * [What compression method should I use?](ChoosingYourCompressionModule.md)