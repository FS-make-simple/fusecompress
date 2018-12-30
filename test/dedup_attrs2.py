# attribute file test: deduped files must be able to have different attributes

import os
import shutil
import sys
import time
import stat

os.mkdir('test')
if os.system('../fusecompress -o dedup,detach test') != 0:
  os.rmdir('test')
  sys.exit(2)

text1 = 'foo'
text2 = 'bar'

open('test/1', 'w').write(text1)
os.chmod('test/1', 0647)
open('test/2', 'w').write(text1)
os.chmod('test/2', 0600)

os.system('fusermount -u test')
time.sleep(1)

assert(os.lstat('test/1').st_nlink == 2)

os.system('../fusecompress -o dedup,detach test')
time.sleep(.1)

stat1 = os.lstat('test/1')
stat2 = os.lstat('test/2')
assert(stat1.st_nlink == 1)
assert(stat2.st_nlink == 1)
assert(stat.S_IMODE(stat1.st_mode) == 0647)
assert(stat.S_IMODE(stat2.st_mode) == 0600)

os.system('fusermount -u test')
time.sleep(1)

shutil.rmtree('test')
sys.exit(0)
