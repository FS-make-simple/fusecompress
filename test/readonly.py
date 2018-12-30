import os
import stat
import time
import shutil

os.mkdir('test')

bigsize = 1000

os.system('../fusecompress -o detach test')
f = open('test/big', 'w')
for i in range(0, bigsize):
  f.write('abcd'*256)
f.close()
shutil.copy('/bin/bash', 'test/bashc')
os.system('fusermount -u test')
shutil.copy('/bin/bash', 'test/bash')
assert(os.stat('test/bashc')[stat.ST_SIZE] < os.stat('test/bash')[stat.ST_SIZE])

os.system('../fusecompress -o detach,ro test')

# try to create a file (expect failure)
passd = False
try:
  open('test/1','w')
except:
  passd = True
if not passd:
  print 'create did not fail as expected'
  os.abort()

# look at file to get it into the bg compression queue
# (should not happen)
os.stat('test/bash')
# wait until unmount to check if it has been compressed

# try to trigger decompression (should not happen)
f = open('test/big', 'r')
off1 = 0
off2 = bigsize * 256 * 4 - 100
for i in range(0,10):
  f.seek(off1)
  f.read(100)
  off1 += 4096
  f.seek(off2)
  f.read(100)
  off2 -= 4096
f.close()

os.system('fusermount -u test')
time.sleep(1)

if os.stat('test/big')[stat.ST_SIZE] > bigsize * 256 * 2:
  print 'big file uncompressed'
  os.abort()

if os.stat('test/bash')[stat.ST_SIZE] < os.stat('/bin/bash')[stat.ST_SIZE]:
  print 'bash compressed'
  os.abort()

shutil.rmtree('test')
