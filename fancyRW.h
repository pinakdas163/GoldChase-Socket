/*
 * write template functions that are guaranteed to read and write the
 * number of bytes desired
 */

#ifndef fancyRW_h
#define fancyRW_h
#include <unistd.h>
#include <errno.h>
template<typename T>
int READ(int fd, T* obj_ptr, int count)
{
  char* addr=(char*)obj_ptr;
  int rem=count;
  int n=0;
  //loop. Read repeatedly until count bytes are read in
  while(rem>0) {
    n=read(fd, addr, rem);
    if(n<0) {
      if(errno==EINTR) // handle signal interrupt
        n=0;
      else
        return -1;
    }
    else if(n==0)
      break;          // end of file
    addr+=n;
    rem-=n;
  }
  return count-rem;
}

template<typename T>
int WRITE(int fd, T* obj_ptr, int count)
{
  char* addr=(char*)obj_ptr;
  int rem=count;
  int n=0;
  //loop. Write repeatedly until count bytes are written out
  while(rem>0) {
    if((n=write(fd, addr, rem)) <= 0)
    {
      if(errno==EINTR)
        n=0;
      else
        return -1;
    }
    addr+=n;
    rem-=n;
  }
  return count;
}
#endif
