#include <fcntl.h>
#include <stdlib.h>

#define DATA "lullerbullertruller"
#define DATA2 "seier"

int main(int argc, char** argv)
{
  int a;
  char buf[sizeof(DATA2)];
  a = open("a",O_RDWR|O_TRUNC|O_CREAT,0644);
  write(a, DATA, sizeof(DATA));
  close(a);
  a = open("a",O_RDWR);
  ftruncate(a,0);
  write(a, DATA2, sizeof(DATA2));
  close(a);
  a = open("a",O_RDONLY);
  if(a < 0) abort();
  if(read(a, buf, sizeof(DATA2)) < 0) abort();
  if(strncmp(buf, DATA2, sizeof(DATA2))) abort();
  close(a);
  return 0;
}
