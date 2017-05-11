#include<iostream>
#include<vector>
#include<stdlib.h>
#include<fstream>
#include "goldchase.h"
#include "Map.h"
#include<unistd.h>
#include<errno.h>
#include<sys/types.h>
#include <fcntl.h>           /* For O_* constants */
#include <sys/stat.h>        /* For mode constants */
#include <semaphore.h>
#include <sys/mman.h>
#include <signal.h>
#include <mqueue.h>
#include <sstream>
#include <string>
#include <cstring>
#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>
#include "fancyRW.h"
#include "mapboard.h"
using namespace std;
extern sem_t *sem;
extern string msgquename[];
string serquename;
mqd_t msgquefd2[5];
int clientfd;
unsigned char *local_map2;
extern mapboard *map_pointer;
string msgqstrname;

void clientsigusr2handler(int) {
  unsigned char sendtoserver;
  //char* playermsg;
  for(int i=0;i<5;i++)
  {
    int err;
    char msg[250];
    memset(msg, 0, 250);
    while((err=mq_receive(msgquefd2[i], msg, 250, NULL))!=-1) {
      sendtoserver=G_SOCKMSG;
      if(i==0)
      {
        sendtoserver |= G_PLR0;
      }
      else if(i==1)
      {
        sendtoserver|=G_PLR1;
      }
      else if(i==2)
      {
        sendtoserver|=G_PLR2;
      }
      else if(i==3)
      {
        sendtoserver|=G_PLR3;
      }
      else if(i==4)
      {
        sendtoserver|=G_PLR4;
      }
      short msgsize= strlen(msg);
      //playermsg=msg;
      WRITE(clientfd,&sendtoserver,sizeof(sendtoserver));
      WRITE(clientfd,&msgsize,sizeof(msgsize));
      WRITE(clientfd,&msg,msgsize);
    }
  }
}

void clientsighubhandler(int sig) {
  unsigned char activeplayers=G_SOCKPLR;
  if(map_pointer->players[0]!=0)
    activeplayers |= G_PLR0;
  if(map_pointer->players[1]!=0)
    activeplayers |= G_PLR1;
  if(map_pointer->players[2]!=0)
    activeplayers |= G_PLR2;
  if(map_pointer->players[3]!=0)
    activeplayers |= G_PLR3;
  if(map_pointer->players[4]!=0)
    activeplayers |= G_PLR4;
  WRITE(clientfd, &activeplayers, sizeof(activeplayers));
  if(activeplayers==G_SOCKPLR)
  {
    shm_unlink("/TAG_mymap");
    sem_close(sem);
    sem_unlink("/mySem");
    exit(0);
  }
}

void clientsigusr1handler(int sig)
{
  std::vector<std::pair<short, unsigned char> > sockmap;
  for(short i=0; i<map_pointer->rows*map_pointer->cols;i++)
  {
    if(local_map2[i]!=map_pointer->map[i])
    {
      sockmap.push_back(std::make_pair(i, map_pointer->map[i]));
      local_map2[i]=map_pointer->map[i];
    }
  }
  if(sockmap.size()>0)
  {
    short n = sockmap.size();
    unsigned char byt = 0;
    WRITE(clientfd, &byt, sizeof(byt));
    WRITE(clientfd, &n, sizeof(n));
    for(short i=0; i< n; i++)
    {
      short offset= sockmap[i].first;
      unsigned char value = sockmap[i].second;
      WRITE(clientfd, &offset, sizeof(offset));
      WRITE(clientfd, &value, sizeof(value));
    }
  }

}

void createClient(string ipaddr) {
  int status;
  int clientrows, clientcols;

  unsigned char *clientsidemap;
  unsigned char clientPlayerSoc;
  WRITE(2, "daemon creation finished\n", sizeof("daemon creation finished "));
  const char* portno="62010";
  struct addrinfo hints;
  memset(&hints, 0, sizeof(hints)); //zero out everything in structure
  hints.ai_family = AF_UNSPEC; //don't care. Either IPv4 or IPv6
  hints.ai_socktype=SOCK_STREAM; // TCP stream sockets

  struct addrinfo *servinfo;
  //instead of "localhost", it could by any domain name
  if((status=getaddrinfo(ipaddr.c_str(), portno, &hints, &servinfo))==-1)
  {
    fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
    exit(1);
  }
  clientfd=socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);

  if((status=connect(clientfd, servinfo->ai_addr, servinfo->ai_addrlen))==-1)
  {
    perror("connect");
    exit(1);
  }
  //release the information allocated by getaddrinfo()
  //newclientfd=clientfd;
  freeaddrinfo(servinfo);
  READ(clientfd, &clientrows, sizeof(int));
  READ(clientfd, &clientcols, sizeof(int));
  unsigned char square;
  local_map2=new unsigned char[clientrows*clientcols];
  clientsidemap=new unsigned char[clientrows*clientcols];
  for(int i=0;i<clientrows*clientcols;i++)
  {
    READ(clientfd, &square, sizeof(char));
    clientsidemap[i]=square;
  }
  memcpy(local_map2, clientsidemap, clientrows*clientcols);

  sem=sem_open("/mySem", O_CREAT,
                       S_IRUSR| S_IWUSR| S_IRGRP| S_IWGRP| S_IROTH| S_IWOTH,1);
  sem_wait(sem);

  WRITE(2, "client demon creating shared memory\n", sizeof("client demon creating shared memory "));
  int shm_fd2=shm_open("/TAG_mymap",O_CREAT|O_RDWR, S_IRUSR|S_IWUSR);
  ftruncate(shm_fd2, (clientrows*clientcols)+sizeof(mapboard));
  map_pointer=(mapboard*)mmap(NULL, (clientrows*clientcols)+sizeof(mapboard), PROT_READ|PROT_WRITE,
    MAP_SHARED, shm_fd2, 0);
  map_pointer->daemonID=getpid();
  map_pointer->rows=clientrows;
  map_pointer->cols=clientcols;
  memcpy(map_pointer->map, local_map2, clientrows*clientcols);

  struct sigaction clientsig_struct;
  sigemptyset(&clientsig_struct.sa_mask);
  clientsig_struct.sa_flags=0;
  clientsig_struct.sa_restorer=NULL;
  clientsig_struct.sa_handler=clientsighubhandler;
  sigaction(SIGHUP, &clientsig_struct, NULL);

  clientsig_struct.sa_handler=clientsigusr1handler;
  sigaction(SIGUSR1, &clientsig_struct, NULL);

  struct mq_attr attributes;
  attributes.mq_flags=0;
  attributes.mq_maxmsg=10;
  attributes.mq_msgsize=250;
  clientsig_struct.sa_handler=clientsigusr2handler;
  sigaction(SIGUSR2, &clientsig_struct, NULL);

  READ(clientfd, &clientPlayerSoc, sizeof(clientPlayerSoc));

  unsigned char playerbit[5]={G_PLR0, G_PLR1, G_PLR2, G_PLR3, G_PLR4};
  for(int i=0; i<5; i++)
  {
    if((clientPlayerSoc & playerbit[i]) && map_pointer->players[i]==0)
    {
      map_pointer->players[i]=getpid();
      serquename=msgquename[i];
      if((msgquefd2[i]=mq_open(serquename.c_str(), O_CREAT|O_RDONLY|O_EXCL|O_NONBLOCK,
            S_IRUSR|S_IWUSR, &attributes))==-1)
      {
        perror("mq_open");
        exit(1);
      }
      struct sigevent mq_notification_event;
      mq_notification_event.sigev_notify=SIGEV_SIGNAL;
      mq_notification_event.sigev_signo=SIGUSR2;
      mq_notify(msgquefd2[i], &mq_notification_event);
    }
  }
  WRITE(2, "client demon completed setup memory\n", sizeof("client demon completed setup memory "));
  sem_post(sem);

  // int msg=1;
  // WRITE(clientdpipe[1], &msg, sizeof(msg));
  while(true) {
    unsigned char protocol, newmapbyte;
    short noOfchangedmap, offset;
    short msglength;
    char msgreceived[250];
    READ(clientfd, &protocol, sizeof(protocol));

    if(protocol & G_SOCKPLR)
    {
      for(int i=0;i<5; i++)
      {
        if(protocol & playerbit[i] && map_pointer->players[i]==0)
        {
          struct mq_attr attributes;
          attributes.mq_flags=0;
          attributes.mq_maxmsg=10;
          attributes.mq_msgsize=250;

          serquename=msgquename[i];
          if((msgquefd2[i]=mq_open(serquename.c_str(), O_CREAT|O_RDONLY|O_EXCL|O_NONBLOCK,
                S_IRUSR|S_IWUSR, &attributes))==-1)
          {
            perror("mq_open");
            exit(1);
          }
          struct sigevent mq_notification_event;
          mq_notification_event.sigev_notify=SIGEV_SIGNAL;
          mq_notification_event.sigev_signo=SIGUSR2;
          mq_notify(msgquefd2[i], &mq_notification_event);
          map_pointer->players[i]=getpid();
        }
        else if((protocol & playerbit[i])==false && map_pointer->players[i]!=0)
        {
          map_pointer->players[i]=0;
          serquename=msgquename[i];
          mq_close(msgquefd2[i]);
          mq_unlink(serquename.c_str());
        }
      }
      if(protocol==G_SOCKPLR)
      {
        shm_unlink("/TAG_mymap");
        sem_close(sem);
        sem_unlink("/mySem");
        exit(0);
      }
    }

    else if(protocol==0) {

      READ(clientfd, &noOfchangedmap, sizeof(noOfchangedmap));
      for(short i=0;i<noOfchangedmap; i++)
      {
        READ(clientfd, &offset, sizeof(offset));
        READ(clientfd, &newmapbyte, sizeof(newmapbyte));
        map_pointer->map[offset]=newmapbyte;
        local_map2[offset]=newmapbyte;
      }
      for(int i=0;i<5; i++)
      {
        if(map_pointer->players[i]!=0 && map_pointer->players[i]!=getpid())
        {
          kill(map_pointer->players[i], SIGUSR1);
        }
      }
    }

    else if(protocol & G_SOCKMSG)
    {
      unsigned char clientplayermsg;
      if(protocol & G_PLR0)
      {
        clientplayermsg=G_PLR0;
        msgqstrname=msgquename[0];
      }
      else if(protocol & G_PLR1)
      {
        clientplayermsg=G_PLR1;
        msgqstrname=msgquename[1];
      }
      else if(protocol & G_PLR2)
      {
        clientplayermsg=G_PLR2;
        msgqstrname=msgquename[2];
      }
      else if(protocol & G_PLR3)
      {
        clientplayermsg=G_PLR3;
        msgqstrname=msgquename[3];
      }
      else if(protocol & G_PLR4)
      {
        clientplayermsg=G_PLR4;
        msgqstrname=msgquename[4];
      }
      READ(clientfd, &msglength, sizeof(msglength));
      READ(clientfd, &msgreceived, msglength);

      mqd_t clientplayerwrite;
      if((clientplayerwrite=mq_open(msgqstrname.c_str(), O_WRONLY|O_NONBLOCK))==-1)
      {
        perror("mq_open error in server daemon");
        exit(1);
      }
      //cerr << "fd=" << writequeue_fd << endl;
      char message[250];
      memset(message, 0, 250);
      strncpy(message, msgreceived, 250);
      if(mq_send(clientplayerwrite, message, strlen(message), 0)==-1)
      {
        perror("mq_send error from daemon");
        exit(1);
      }
      mq_close(clientplayerwrite);
    }
  }
  close(clientfd);
  delete local_map2;
}

void create_client_daemon(string ipaddr) {

  if(fork()>0) {
    return;
  }

  if(fork()>0) {
    exit(0);
  }
  WRITE(2, "daemon creation started\n", sizeof("daemon creation started "));
  if(setsid()==-1) //child obtains its own SID & Process Group
    exit(1);
  for(int i=0; i<sysconf(_SC_OPEN_MAX); ++i) {
      //if(i!=clientdpipe[1])
      close(i);
  }
  open("/dev/null", O_RDWR); //fd 0
  open("/dev/null", O_RDWR); //fd 1
  //open("/dev/null", O_RDWR); //fd 2
  int clfd=open("/home/binit/GoldChase-Socket/binitfifo", O_WRONLY);
  if(clfd==-1)
  {
    exit(99);
  }
  umask(0);
  chdir("/");
  //now do whatever you want the daemon to do
  createClient(ipaddr);
}
