#!/bin/sh -e
cp /etc/services in
for i in module_lzma module_gzip module_bz2 module_lzo module_null
do
	echo $i
	gcc -g -I.. -DMODULE=$i -o tc tc.c ../compress_*.c ../file.c ../minilzo/lzo.c ../globals.c -llzma -lbz2 -lz -llzo2
	./tc
	cmp in back_in
	test "$i" = "module_lzo" || cmp out back_out
	cmp in and_in_again
	rm back_in out back_out and_in_again
done
rm in tc
