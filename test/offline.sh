#!/bin/bash -e
# fusecompress_offline test

mkdir test
cp /bin/sh test/sh1
cp /bin/sh test/sh2

# compress entire directory
../fusecompress_offline -v -c lzma test
# reference file
cp /bin/sh test/sh_ref
# compressed file should be smaller than reference
test `filesize test/sh1` -lt `filesize test/sh_ref`
# uncompress all files again
../fusecompress_offline -v test
# now they should be the same size
test `filesize test/sh1` -eq `filesize test/sh_ref`
# compress with different levels
../fusecompress_offline -l 1 -c lzma test/sh1
../fusecompress_offline -l 6 -c lzma test/sh2
# should be sh_ref > sh1 > sh2
test `filesize test/sh1` -lt `filesize test/sh_ref`
test `filesize test/sh2` -lt `filesize test/sh1`

rm -r test
