import shutil
import os
import sys
import magic

max_bytes_per_type = 100000000

m = magic.open(magic.MAGIC_MIME)
m.load()

compressors = ['lzo', 'gz', 'bz2', 'lzma']

avgratio = dict()
for c in compressors:
  avgratio[c] = dict()

try:
  for fd in os.walk(sys.argv[1]):
    for ff in fd[2]:
      f = fd[0] + '/' + ff
      try:
        base = f[f.rindex('/') + 1:]
      except:
        base = f
      try:
        filetype = base[base.rindex('.') + 1:]
        assert(len(filetype) <= 4)
      except:
        filetype = m.file(f)
      
      try:
        osize = os.stat(f).st_size
      except:
        sys.stderr.write('WARNING could not open ' + f + '\n')
        continue
      
      if osize < 1 or osize > max_bytes_per_type * 2:
        continue

      try:
        if avgratio[compressors[0]][filetype][1] > max_bytes_per_type:
          continue
      except KeyError:
        pass
      
      for i in compressors:
        try:
          shutil.copy(f, 'ratio')
        except IOError:
          break
        assert(os.system('fusecompress_offline -c ' + i + ' ratio') == 0)
        csize = os.stat('ratio').st_size
        ratio = 1.0 * csize / osize
        sys.stderr.write(f + '\t' + i + '\t' + filetype + '\t' + str(100 * ratio) + '/' + str(osize) + '\n')
        os.unlink('ratio')
        try:
          (score_csize, score_osize) = avgratio[i][filetype]
        except:
          score_csize = float(0)
          score_osize = float(0)
        score_csize += csize
        score_osize += osize
        avgratio[i][filetype] = (score_csize, score_osize)
except (KeyboardInterrupt, AssertionError):
  pass

filetypes = avgratio[compressors[0]].keys()
filetypes.sort()

for f in filetypes:
  print '-----', f, '-----'
  for c in compressors:
    print c + ':',
    try:
      (csize, osize) = avgratio[c][f]
    except KeyError:
      print 'no data'
      continue
    if osize < 131072:
      print 'insufficient data, skipping'
      continue
    print str(100.0 * csize / osize) + '%', '('+ str(osize), 'bytes',
    if csize / osize > 0.95:
      print 'INCOMPRESSIBLE',
    print ')'
