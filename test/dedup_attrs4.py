import sys
import os
import shutil
import time
import stat

if not 'deduplication' in os.popen('../fsck.fusecompress --help 2>&1').read():
  sys.exit(2)

os.mkdir('test')
open('test/a', 'w').write('foo')
os.chmod('test/a', 0600)
open('test/b', 'w').write('foo')
os.chmod('test/b', 0700)

# compress and dedupe
assert(os.system('../fusecompress_offline -c lzo test/a test/b') == 0)
assert(os.system('../fsck.fusecompress -rlv test') == 0)

# check that the setup is the way we want it
assert(os.lstat('test/a').st_nlink == 2)
assert(os.lstat('test/b').st_mode == os.lstat('test/a').st_mode)
if os.path.exists('test/._fCat_b'):
  assert(os.lstat('test/._fCat_b').st_mode != os.lstat('test/a').st_mode)
else:
  assert(os.lstat('test/._fCat_a').st_mode != os.lstat('test/b').st_mode)

# mount filesystem
assert(os.system('../fusecompress -o detach,dedup test') == 0)

# undedupe test/b
if os.path.exists('test/._fCat_b'):
  open('test/b', 'r+').write('bar')
else:
  open('test/a', 'r+').write('bar')

assert(os.system('fusermount -u test') == 0)
time.sleep(2)

# check if attributes have been preserved
if os.path.exists('test/._fCat_b'):
  assert(stat.S_IMODE(os.lstat('test/._fCat_b').st_mode) == 0700)
else:
  assert(stat.S_IMODE(os.lstat('test/b').st_mode) == 0700)

if os.path.exists('test/._fCat_a'):
  assert(stat.S_IMODE(os.lstat('test/._fCat_a').st_mode) == 0600)
else:
  assert(stat.S_IMODE(os.lstat('test/a').st_mode) == 0600)

shutil.rmtree('test')
sys.exit(0)
