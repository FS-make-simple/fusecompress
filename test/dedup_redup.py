# check redup functionality

import os
import sys
import shutil
import time

if not 'deduplication' in os.popen('../fsck.fusecompress --help 2>&1').read():
  sys.exit(2)

os.mkdir('test')
shutil.copy('/bin/sh', 'test/sh_ref')
shutil.copy('/bin/sh', 'test/sh1')
assert(os.system('../fusecompress_offline -v -c lzo test/sh1') == 0)
assert(os.system('../fusecompress -o detach,dedup,redup test') == 0)
time.sleep(.2)
os.stat('test/sh_ref')
os.stat('test/sh1')
shutil.copy('/bin/sh', 'test/sh2')
assert(os.system('fusermount -u test') == 0)
time.sleep(3)
assert(os.path.exists('test/._fCdedup_db'))
for i in ['sh_ref', 'sh1', 'sh2']:
  assert(os.lstat('test/' + i).st_nlink == 3)
shutil.rmtree('test')
sys.exit(0)
