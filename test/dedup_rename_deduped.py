# overwrite a file through rename that has previously been deduped
# and make sure that subsequent files written with the old (no longer
# existing) content are not linked to that new file

import os
import shutil
import sys
import time
import stat

# check if dedup is enabled
if not 'deduplication' in os.popen('../fsck.fusecompress --help 2>&1').read():
  sys.exit(2)

os.mkdir('test')

# create a file that will later be rename()d over
open('test/tempfile', 'w').write('oldcontent')

# compress the file and build a dedup DB
if os.system('../fusecompress_offline -c gz test') != 0:
  os.abort()
if os.system('../fsck.fusecompress -v -r test') != 0:
  os.abort()
if not os.path.exists('test/._fCdedup_db'):
  os.abort()

# mount the filesystem with dedup
if os.system('../fusecompress -o dedup,detach,gz test') != 0:
  os.abort()

# write a file with new content...
open('test/tempfile.new', 'w').write('newcontent')
# and rename() it over the old one
os.rename('test/tempfile.new', 'test/tempfile')

# now the old content is gone
assert(open('test/tempfile').read() == 'newcontent')

# a file created now with the old content should not be linked to 'tempfile'
open('test/unrelated', 'w').write('oldcontent')

os.system('fusermount -u test')
time.sleep(.1)

# make sure no links have been made
assert(os.stat('test/unrelated').st_nlink == 1)
assert(os.stat('test/tempfile').st_nlink == 1)
# make sure the contents of the files are actually different
assert(open('test/unrelated').read() != open('test/tempfile').read())

shutil.rmtree('test')

sys.exit(0)
