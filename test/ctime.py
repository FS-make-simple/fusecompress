# check that reading a file won't modify ctime
import os
import time

os.mkdir('test')
assert(os.system('../fusecompress -o detach test') == 0)
a = open('test/foo', 'w')
a.write('foobar')
a.close()
time.sleep(1)
ctime = os.lstat('test/foo').st_ctime
a = open('test/foo').read()
time.sleep(1)
assert(os.lstat('test/foo').st_ctime == ctime)
os.system('fusermount -u test')
os.unlink('test/foo')
os.rmdir('test')
