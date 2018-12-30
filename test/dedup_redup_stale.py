# check that redup doesn't drop temp files

import os
import sys
import shutil
import time
import glob

numlinks = 100

if not 'deduplication' in os.popen('../fsck.fusecompress --help 2>&1').read():
  sys.exit(2)

os.mkdir('test')
shutil.copy('/bin/sh', 'test/sh_ref')
assert(os.system('../fusecompress_offline -v -c lzo test/sh_ref') == 0)
for i in range(0, numlinks):
  os.link('test/sh_ref', 'test/sh' + str(i))

assert(os.system('../fusecompress -o detach,dedup,redup test') == 0)
time.sleep(.2)
os.stat('test/sh_ref')
for i in range(0, numlinks):
  os.stat('test/sh' + str(i))

assert(os.system('fusermount -u test') == 0)
time.sleep(3)

assert(glob.glob('test/._fCt*') == [])

shutil.rmtree('test')
sys.exit(0)
