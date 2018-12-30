# (re)build dedup DB using fsck

import os
import shutil
import sys
import time
import stat

# check if dedup is enabled
if not 'deduplication' in os.popen('../fsck.fusecompress --help 2>&1').read():
  sys.exit(2)

os.mkdir('test')

shutil.copy('/bin/sh', 'test/sh1')
shutil.copy('/bin/sh', 'test/sh2')
shutil.copy('/bin/ls', 'test/ls')

# compress the files
if os.system('../fusecompress_offline -c gz test') != 0:
  os.abort()

# build the dedup database using fsck
if os.system('../fsck.fusecompress -v -r test') != 0:
  os.abort()

# check if it's there
if not os.path.exists('test/._fCdedup_db'):
  os.abort()

# mount the filesystem with dedup
if os.system('../fusecompress -o dedup,detach,gz test') != 0:
  os.abort()

# copy another redundant file into the FS
shutil.copy('/bin/sh', 'test/sh_dup')
os.system('fusermount -u test')
time.sleep(1)

# check if it has been linked properly
if os.stat('test/sh_dup').st_nlink < 2:
  os.abort()

shutil.rmtree('test')

sys.exit(0)
