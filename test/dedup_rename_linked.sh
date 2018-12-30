#!/bin/bash -e

# check rename() semantics for deduped files

../fsck.fusecompress --help 2>&1 | grep -q deduplication || exit 2

mkdir test
cp /bin/sh test/sh1
ln test/sh1 test/sh2
test `stat -c %h test/sh1` == 2
../fusecompress -o dedup,detach test
test `stat -c %h test/sh1` == 1
# While mv usually works around the weirdo rename() semantics, it is still
# valid as a test here because fusecompress returns different inodes for sh1
# and sh2, so mv actually uses rename() instead of unlinking the source
# file.
test `stat -c %i test/sh1` != `stat -c %i test/sh2`
mv test/sh2 test/sh1
fusermount -u test
test `stat -c %h test/sh1` == 1
test ! -e test/sh2
rm -r test
