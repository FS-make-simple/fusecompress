#!/bin/bash
# must be run as root
test $EUID == 0 || exit 2

dd if=/dev/zero of=test.ext2 bs=1M count=1
mkfs.ext2 -F test.ext2
mount -o loop test.ext2 /mnt
mkdir /mnt/test
../fusecompress -d -c lzma /mnt/test
echo "--- creating empty file larger than FS (should be OK)"
dd if=/dev/zero bs=2M count=1 of=/mnt/test/x
echo "--- remounting to prevent cache hit"
fusermount -u /mnt/test
sleep 1
../fusecompress -d -c lzma /mnt/test
echo "--- random read on file (should yield ENOSPC)"
dd if=/mnt/test/x bs=1k count=1 skip=200 of=/dev/null && false || true
echo "--- write random data until disk full (should yield ENOSPC)"
dd if=/dev/urandom bs=2M count=1 of=/mnt/test/y && false || true
rm /mnt/test/y
echo "--- ending fusecompress in order to write uncompressed file"
fusermount -u /mnt/test
sleep 1
echo "--- creating uncompressed file barely smaller than FS"
dd if=/dev/zero bs=800k count=1 of=/mnt/test/z
echo "--- restarting fusecompress"
../fusecompress -d -c lzma /mnt/test
echo "--- reading file to get it into the background compression queue"
cat /mnt/test/z >/dev/null
sleep 1
echo "--- umounting; should leave file uncompressed and not hang"
fusermount -u /mnt/test
sleep 1
ls -l /mnt/test
umount /mnt
rm test.ext2
