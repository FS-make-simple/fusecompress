# check for race-free setting of timestamps

import os
import time
import sys
import shutil

data = 'foo' * 100
numfiles = 10000

os.mkdir('test')
os.system('../fusecompress -d test')
for i in range(0, numfiles):
  open('test/a' + str(i), 'w').write(data)
  os.utime('test/a' + str(i), (0, 0))

time.sleep(1)

for i in range(0, numfiles):
  if os.lstat('test/a' + str(i))[8] != 0:
    print 'file', i, 'bad timestamp'
    os.system('fusermount -z -u test')
    os.abort()

os.system('fusermount -u test')
shutil.rmtree('test')
sys.exit(0)
