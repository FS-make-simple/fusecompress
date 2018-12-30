#include <fcntl.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <stdio.h>

int main(int argc, char** argv)
{
  int a,b;
  char buf[5] = "";
  
  a = open("a",O_CREAT|O_TRUNC|O_RDWR,0644);
  write(a, "HUHU", 4);
  link("a", "b");
  b = open("b",O_RDONLY);
  if(read(b, buf, 4) < 4) abort();
  if(strncmp(buf,"HUHU",4)) abort();
  close(b);
  close(a);
  
#ifdef STRICT
  a = open("a",O_RDWR);
  b = open("b",O_RDONLY);
  write(a,"HO",2);
  if(read(b, buf, 4) < 4) { printf("read error\n"); abort(); }
  if(strncmp(buf,"HOHU",4)) { printf("bad data %s\n",buf); abort(); }
#endif
  
  return 0;
}
