#include "structs.h"
#include "compress_lzma.h"
#include "compress_lzo.h"
#include "compress_bz2.h"
#include "compress_gz.h"
#include "compress_null.h"
#include <fcntl.h>
#include <stdlib.h>

char compresslevel[3] = "wb4";

#define BUF_SIZE 4096
int compress_testcancel(void* v)
{
  return 0;
}
int main(int argc, char** argv)
{
  int a = open("in",O_RDONLY);
  int b = open("out",O_WRONLY|O_CREAT|O_TRUNC,0644); 
  char buf[BUF_SIZE];
  void* bf = MODULE.open(b,compresslevel);
  if(!bf) {
    perror("MODULE.open");
    exit(1);
  }
  int r;
  while((r = read(a, buf, BUF_SIZE)) > 0) {
    if ((r = MODULE.write(bf, buf, r)) < 0)
      exit(r);
  }
  MODULE.close(bf);
  close(b);
  close(a);
  
  a = open("out", O_RDONLY);
  b = open("back_in", O_WRONLY|O_CREAT|O_TRUNC,0644);
  void* af = MODULE.open(a,"rb");
  while((r = MODULE.read(af, buf, BUF_SIZE)) > 0) {
    write(b, buf, r);
  }
  close(b);
  MODULE.close(af);
  close(a);

  a = open("back_in", O_RDONLY);
  b = open("back_out", O_WRONLY|O_CREAT|O_TRUNC,0644);
  MODULE.compress(NULL,a,b);
  close(a);
  close(b);
  
  a = open("back_out", O_RDONLY);
  b = open("and_in_again", O_WRONLY|O_CREAT|O_TRUNC,0644);
  MODULE.decompress(a, b);
  close(a);
  close(b);
  
  return 0;
}
