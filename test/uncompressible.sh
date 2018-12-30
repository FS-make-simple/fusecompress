#!/bin/bash -e
c=".exe .txt .tiff"
u=".lzma .gz .tar.bz2 .mp3 .blubb .bla .rar.txt .zip.me"
size=`stat -c %s /bin/bash`
mkdir test
../fusecompress -d -c lzo -o nocompext=blubb,nocompext=bla test
for i in $c; do
  cp /bin/bash test/bash${i}
  s="$((`stat -c "%B * %b" test/bash${i}`))"
  test $s -lt $size
done
for i in $u; do
  cp /bin/bash test/bash${i}
  s="$((`stat -c "%B * %b" test/bash${i}`))"
  test $s -ge $size
done
fusermount -u test
sleep 1
rm -fr test
