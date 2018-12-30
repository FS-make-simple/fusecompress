#!/bin/bash -e
mkdir test1
mkdir test2
../fusecompress test1 test2 -o cache_skipped,detach
fusermount -u test2
../fusecompress test1 -o cache_skipped,detach
fusermount -u test1
../fusecompress -o cache_skipped,detach test1 test2
fusermount -u test2
../fusecompress -o cache_skipped,detach test1
fusermount -u test1
rmdir test1
rmdir test2
