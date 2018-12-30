# dedup copy-on-write test

import os
import shutil
import sys
import stat
import time

os.mkdir('test')

if os.system('../fusecompress -o dedup,detach test') != 0:
  os.rmdir('test')
  sys.exit(2)	# dedup not available

shutil.copy('/bin/sh', 'test/sh1')
shutil.copy('/bin/sh', 'test/sh2')

os.system('fusermount -u test')
time.sleep(.1)

assert(os.stat('test/sh1').st_nlink == 2)

os.system('../fusecompress -o dedup,detach test')

open('test/sh1', 'w').write('foo')
time.sleep(.1)
# make sure the other file is still the same
assert(open('/bin/sh').read() == open('test/sh2').read())

os.system('fusermount -u test')
time.sleep(1)
# make sure the files are not linked (previous test could be false
# positive due to caching issues)
assert(os.lstat('test/sh1').st_nlink == 1)
assert(os.lstat('test/sh2').st_nlink == 1)

shutil.rmtree('test')
