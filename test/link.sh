#!/bin/bash -e
cc -o link link.c
mkdir test
../fusecompress -d test
cd test
../link
cd ..
fusermount -u test
rm -fr test
rm link
