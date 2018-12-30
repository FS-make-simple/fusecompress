# mixed compression dedup testing: are all identical files properly linked?

import os
import shutil
import sys
import stat
import time

if not 'deduplication' in os.popen('../fsck.fusecompress --help 2>&1').read():
  sys.exit(2)

compressors = [None, 'gz', 'lzo', 'bz2', 'lzma']

os.mkdir('test')

for c in compressors:
  shutil.copy('/bin/sh', 'test/sh_' + str(c))
  if c != None:
    assert(os.system('../fusecompress_offline -vc ' + c + ' test/sh_' + c) == 0)

# test offline
assert(os.system('../fsck.fusecompress -vrl test') == 0)
assert(os.path.exists('test/._fCdedup_db'))

for (dir, subdirs, files) in os.walk('test'):
  for f in files:
    if f.startswith('._fC'): continue
    s = os.lstat(dir + '/' + f)
    assert(s.st_nlink == len(compressors))

# test online
for c in compressors:
  if c == None: continue
  assert(os.system('../fusecompress -o detach,dedup,' + c + ' test') == 0)
  time.sleep(.1)
  shutil.copy('/bin/sh' , 'test/sh2_' + c)
  assert(os.system('fusermount -u test') == 0)
  # main thread can take a moment to notice there are no more pending
  # comp/dedup jobs
  time.sleep(2)
  assert(os.lstat('test/sh2_' + c).st_nlink > len(compressors))

shutil.rmtree('test')
sys.exit(0)
