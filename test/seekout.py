from threading import Timer
import os
import sys
import time

pid = 0

def crapout():
  os.kill(pid,9)
  time.sleep(1)
  os.system('fusermount -z -u test')
  os.system('rm -fr test')
  os.abort()

os.mkdir('test')
pid = os.spawnlp(os.P_NOWAIT,'../fusecompress','../fusecompress','-f','test')
time.sleep(1)
os.system('cp /etc/fstab test')
a = open('test/fstab')
t = Timer(5,crapout)
t.start()
a.seek(100000)
b = a.read(5)
a.close()
os.system('fusermount -u test')
os.wait()
os.system('rm -fr test')
t.cancel()
sys.exit(0)
