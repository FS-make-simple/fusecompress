from threading import Thread,Timer
import dircache
import random
import os
import stat
import time 
import thread
import sys
import shutil

os.mkdir('test')
ret = -1
def fusecompress_thread():
  global ret
  if os.system('../fusecompress -f test 2>/dev/null') != 0:
    os.system('fusermount -z -u test')
    shutil.rmtree('test')
    ret = 1

os.system('../fusecompress -d test')
# The number of files and their size has been picked carefully.
# Think before you change anything!
for i in range(0,10):
  a = open('test/file'+str(i),'w')
  a.write('bla'*10000000)
  a.close()
os.system('fusermount -u test')

Thread(target=fusecompress_thread).start()

def done_thread():
  global ret
  ret = 0

Timer(20, done_thread).start()

def load_thread():
  a = dircache.listdir('test')
  f = 'test/' + random.sample(a,1)[0]
  s = os.stat(f)
  if stat.S_ISREG(s[stat.ST_MODE]):
    try:
      b = open(f)
      b.seek(random.randint(0,s[stat.ST_SIZE]))
      c = b.read(random.randint(0,131072))
      b.close()
    except: pass

count = 0
while ret == -1:
  try: 
    print count
    count += 1
    Thread(target=load_thread).start()
  except thread.error: time.sleep(.1)

if ret == 0:
  os.system('fusermount -z -u test')
  while True:
    try:
      shutil.rmtree('test')
      break
    except: pass

sys.exit(ret)
