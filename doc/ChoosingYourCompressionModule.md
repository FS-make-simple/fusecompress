Here are some hints on what compression module and what compression level suits your application best.

## LZO ##
LZO is fast. And I mean fast. LZO provides such an enormous throughput that it can actually speed up systems (even those with fast disks) in some cases by cutting down on I/O load, depending on what exactly you are doing. Compression ratio is so-so. LZO does not support different compression levels, it is one-size-fits-all. Use LZO for your $HOME directory, or even your root filesystem.

## LZMA ##
LZMA offers the best compression ratios, even at compression level 1, where it is also reasonably fast. Higher levels have steep CPU and memory requirements for compression and are probably unsuitable for workloads with frequent write or random read access, such as $HOME directories or binaries. If you don't want to grow old waiting for your data to shrink, stay away from any levels higher than 4 (which is the default). Use LZMA for data that hardly ever changes. `/usr/share/doc` comes to mind.

The LZMA Utils library used to be unstable; different older revisions used to create mutally incompatible streams. 4.999.3 and 4.999.5 did, for instance. The format has stabilized since, and format-related problems should be history by now.

## gzip (deflate) ##
Deflate is a good general-purpose compression algorithm, and levels up to 6 offer good compression ratios at very affordable CPU cost. It is generally slower than LZO but produces much better results. For slow disks (USB, for instance) or network-mounted filesystems, the increased compression ratio may actually make it faster than LZO. Compression levels higher than 6 are both slower and produce worse results than LZMA level 1 and are thus not recommended. The default is 6.

## bzip2 ##
bzip2 produces worse results at higher CPU cost than LZMA level 1 in all cases. For non-text data (especially binaries), the results are worse than with deflate, while requiring about ten times as many CPU cycles. Not recommended for anything. Default compression level is 6. Not that that matters much...

## null ##
The null module does not do compression and is only useful for debugging.