# check if we get up-to-date data renaming over an existing cached file

import os
import sys
import shutil
import time
import stat

os.mkdir('test')
os.system('../fusecompress -o lzma,cache_skipped,detach test')
a = open('test/a','w')
a.write('blafwpegfjwegwegherjhj32r0grobfn23t-=wefopjweofewfjwopefjp' * 1000)
a.close()
b = open('test/b','w')
b.write('1111111111111111111111111111111111111111111111111111111111' * 1000)
b.close()
time.sleep(1)
os.system('fusermount -u test')
time.sleep(1)
os.system('../fusecompress -s log -o lzma,cache_skipped,detach test')

a = open('test/a','r+')
a.seek(10000)
a.read(100) # fusecompress caches skipped part
a.seek(0)
a.close()
os.rename('test/b', 'test/a')
a = open('test/a','r')
if not '111111' in a.read(4096):
  os.abort()
a.close()

os.system('fusermount -u test')
time.sleep(1)
shutil.rmtree('test')
sys.exit(0)
