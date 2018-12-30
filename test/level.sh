#!/bin/bash -e

try()
{
          \time -f "$c $l %E" cp -a /bin test/
          diff -r /bin test/bin
          rm -fr test/bin
          fusermount -u test
}

mkdir test

echo --- without fusecompress
\time -f "plain %E" cp -a /bin test/
diff -r /bin test/bin
rm -fr test/bin

for c in lzo null gz bz2 lzma
do
  echo --- $c
  for l in 1 3 7
  do
          ../fusecompress -d -c $c -l $l test
          try
  done
  l=default
  ../fusecompress -d -c $c test
  try
done
rm -fr test
