# (re)build dedup DB and deduplicate files using fsck

import os
import shutil
import sys

# check if dedup is enabled
if not 'deduplication' in os.popen('../fsck.fusecompress --help 2>&1').read():
  sys.exit(2)

os.mkdir('test')

shutil.copy('/bin/sh', 'test/sh1')
shutil.copy('/bin/sh', 'test/sh2')
shutil.copy('/bin/ls', 'test/ls')

# compress the files
assert(os.system('../fusecompress_offline -c gz test') == 0)
# rebuild the dedup database and dedup files using fsck
assert(os.system('../fsck.fusecompress -vrl test') == 0)
# check if the DB is there
assert(os.path.exists('test/._fCdedup_db'))
# check if files have been deduped
assert(os.lstat('test/sh1').st_nlink == 2)
assert(os.lstat('test/sh2').st_nlink == 2)
assert(os.lstat('test/ls').st_nlink == 1)

shutil.rmtree('test')

sys.exit(0)
