# try to put files in the background queue and
# rename them while in there
# was triggering assertion failure in direct_rename()
# see also issue #2

import os
import random
from threading import Timer
import shutil
import sys
import time

size = 100000
files = 100

os.mkdir('test')
os.system('../fusecompress -d test')

#os.mkdir('test')

def fn(i): return 'test/file'+str(i)
def rf(): return fn(random.randint(0,files))

quit = False

def out_ok():
  global quit
  quit = True
  time.sleep(.1)
  os.system('fusermount -z -u test')
  while True:
    try:
      shutil.rmtree('test')
      break
    except: pass
  sys.exit(0)

def out_barf(e):
  print 'Exception:',e
  os.abort()
    
Timer(30,out_ok).start()

try:
  count = 0
  while not quit:
    print count
    # create a random file
    a = open(rf(),'w')
    a.write('b'*size)
    a.flush()
    # overwrite part of it to force decompression
    a.seek(0, os.SEEK_SET)
    a.write('seier')
    a.close() # now it is put in the background queue
    # rename a random file
    i = rf()
    if os.path.exists(i): os.rename(i,rf())
    # read a random file
    i = rf()
    if os.path.exists(i):
      b = open(i)
      if len(b.read()) != size: out_barf()
      b.close()
    count += 1
except OSError, e:
  out_barf(e)
