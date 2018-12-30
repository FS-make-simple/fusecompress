# create a broken file and check if read() returns an error

import os
import sys
import shutil
import time
import stat

# It may be worth noting here that even for the null module there is a
# difference between "compressed" and "uncompressed" files: the former
# have a header specifying the file size, and if they are truncated,
# they count as invalid and must return EIO on read().

for comp in ['lzo','lzma','gz','bz2','null']:
  print comp
  os.mkdir('test')
  os.system('../fusecompress -c ' + comp + ' -d test')
  a = open('test/a','w')
  a.write('blafwpegfjwegwegherjhj32r0grobfn23t-=wefopjweofewfjwopefjp' * 1000)
  a.close()
  time.sleep(1)
  os.system('fusermount -u test')
  time.sleep(1)
  print 'test/a is',os.stat('test/a')[stat.ST_SIZE],'bytes compressed'
  # break the file
  a = open('test/a','a')
  a.truncate(142)	# smallest compressed file is 180 bytes (LZMA)
  a.close()
  os.system('../fusecompress -slog.' + comp + ' -c ' + comp + ' -d test')
  a = open('test/a')
  try:
    a.read()
  except:
    # error as expected
    a.close()
    os.system('fusermount -u test')
    shutil.rmtree('test')
    try: os.unlink('log.'+comp)
    except: pass
    continue

  os.abort()

sys.exit(0)
