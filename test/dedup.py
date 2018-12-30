# basic dedup testing: are all identical files properly linked?

import os
import shutil
import sys
import stat
import time

numcopies = 3

os.mkdir('test')

# use gzip, LZO is tested elsewhere
if os.system('../fusecompress -o dedup,detach,gz test') != 0:
  os.rmdir('test')
  sys.exit(2)	# dedup not available

for i in range(0, numcopies):
  os.system('cp -a /bin test/bin' + str(i))

os.system('fusermount -u test')
time.sleep(2)

for (dir, subdirs, files) in os.walk('test'):
  for f in files:
    if f.startswith('._fC'): continue
    s = os.lstat(dir + '/' + f)
    if (not stat.S_ISLNK(s.st_mode)) and s.st_nlink < numcopies:
      print dir + '/' + f, s.st_nlink, 'links'
      os.abort();

shutil.rmtree('test')
sys.exit(0)
