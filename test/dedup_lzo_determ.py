# verify that LZO compression is deterministic when using dedup

import os
import shutil
import sys
import time

filecount = 100

os.mkdir('test')
if os.system('../fusecompress -o dedup,lzo,detach test') != 0:
  os.rmdir('test')
  sys.exit(2)	# dedup not available

for i in range(0, filecount):
  shutil.copy('/bin/sh', 'test/sh' + str(i))
os.system('fusermount -u test')
time.sleep(1)
sh_data = open('test/sh0').read()
for i in range(1, filecount):
  if sh_data != open('test/sh' + str(i)).read():
    os.abort()
shutil.rmtree('test')
sys.exit(0)
