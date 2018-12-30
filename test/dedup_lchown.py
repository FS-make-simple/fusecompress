# check that lchown actually does lchown, not chown

import os
import sys
import shutil
import time

if not 'deduplication' in os.popen('../fsck.fusecompress --help 2>&1').read():
  sys.exit(2)

os.mkdir('test')
assert(os.system('../fusecompress -o detach,dedup test') == 0)
time.sleep(.2)
os.symlink('foo', 'test/link')
os.lchown('test/link', os.getuid(), os.getgid())
assert(os.system('fusermount -u test') == 0)
time.sleep(1)
shutil.rmtree('test')
sys.exit(0)
