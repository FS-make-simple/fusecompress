import os
import time
import sys

data = 'bla' * 10000

os.mkdir('test')
os.system('../fusecompress -d test')
b = open('test/b','w')
b.write(data)
b.flush()
os.utime('test/b',(0,0))
time.sleep(1)
b.close()
time.sleep(1)
if os.lstat('test/b')[8] != 0:
  os.system('fusermount -z -u test')
  os.system('rm -fr test')
  os.abort()
os.system('fusermount -u test')
os.system('rm -fr test')
sys.exit(0)
