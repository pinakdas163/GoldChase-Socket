#include<unistd.h>
#include<stdlib.h>
#include<fcntl.h>
#include<sys/stat.h>

int main()
{
  //initialize game for first player
  if(fork()==0)
    create_server_daemon();
  //continue on to the loop that has getKey()
}

void create_server_daemon()
{
  if(fork()>0)
    exit(0);

  if(setsid()==-1)//child obtains its own SID & Process Group
    exit(1);
  for(int i=0; i<sysconf(_SC_OPEN_MAX); ++i)
    close(i);
  open("/dev/null", O_RDWR); //fd 0
  open("/dev/null", O_RDWR); //fd 1
  open("/home/tgibson/project4/myfifo", O_RDWR); //fd 2
  umask(0);
  chdir("/");
  //now do whatever you want the daemon to do
}
