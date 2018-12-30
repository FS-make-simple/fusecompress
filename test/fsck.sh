#!/bin/bash -e
mkdir test
../fusecompress -o detach test
dd if=/dev/urandom of=test/file bs=10k count=1
cp test/file test/file2
cp test/file test/file3
chmod 000 test/file3	# see if we can open a no permissions file
fusermount -u test
usleep 500000
dd if=/dev/null of=test/file bs=1k seek=5	# break test/file
# check if the files behave as expected
../fusecompress -o detach test
cat test/file >/dev/null && false || true
cat test/file2 >/dev/null
cat test/file3 && false || true
fusermount -u test
usleep 500000
set +e
../fsck.fusecompress test
ret=$?
set -e
test $ret == 4
../fsck.fusecompress test 2>&1 | grep "unable to open" && false || true
set +e
../fsck.fusecompress -d test
ret=$?
set -e
test $ret == 1
test ! -e test/file	# test/file is broken, it should have been removed
../fusecompress -o detach test
chmod 644 test/file3
cmp test/file2 test/file3
fusermount -u test
usleep 500000
rm -fr test
