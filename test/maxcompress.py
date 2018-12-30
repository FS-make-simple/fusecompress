# check if maxcompress ensures files larger than X won't be compressed

import os
import sys
import stat
import shutil
import time

os.mkdir('test')
a = open('test/a', 'w')
a.write('1234567890abcdef' * 1024 * 1024) # write 16 MB
a.close()
os.system('../fusecompress -o maxcompress=1 -d test')
open('test/a').read() # read the file (and thus enqueue it for bg compression)
os.system('fusermount -u test')
time.sleep(.5)
if os.stat('test/a')[stat.ST_SIZE] < 16 * 1024 * 1024:
  os.abort()
shutil.rmtree('test')
sys.exit(0)
