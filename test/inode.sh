#!/bin/bash -e
# check if stat returns same data for hard links
mkdir test
../fusecompress -d test
dd if=/dev/urandom of=test/a bs=10k count=1
ln test/a test/b
s1=`stat -t test/a|cut -d" " -f2-`
s2=`stat -t test/b|cut -d" " -f2-`
test "$s1" == "$s2"
fusermount -u test
rm -fr test
