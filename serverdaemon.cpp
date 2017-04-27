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

int sockfd; //file descriptor for the socket
int status; //for error checking
int new_sockfd; // file descriptor for each connection
unsigned char *local_map; // local copy of map for server
mapboard *servmap_pointer;
//close(2);
//int fd=open("/home/pinakdas163/611myfiles/project4/pinakfifo", O_WRONLY);

unsigned char activePlayers() {
  unsigned char activplayers=0;
  if(servmap_pointer->players[0]!=0)
    activplayers |= G_PLR0;
  if(servmap_pointer->players[1]!=0)
    activplayers |= G_PLR1;
  if(servmap_pointer->players[2]!=0)
    activplayers |= G_PLR2;
  if(servmap_pointer->players[3]!=0)
    activplayers |= G_PLR3;
  if(servmap_pointer->players[4]!=0)
    activplayers |= G_PLR4;

  return activplayers;
}

void serversighandler(int handler)
{
  if(handler==SIGHUP) {
    unsigned char activeplayer = activePlayers();
    unsigned char socketplayer = G_SOCKPLR|activeplayer;
    //WRITE(2, &socketplayer, sizeof(char));
    WRITE(new_sockfd, &socketplayer, sizeof(socketplayer));
    if(socketplayer==G_SOCKPLR)
    {
      //WRITE(2, "All\n", 3);
      //close(shm_fd);
      shm_unlink("/TAG_mymap");
      //sem_close(sem);
      sem_unlink("/mySem");
      exit(0);
    }
  }
  else if(handler==SIGUSR1)
  {
    std::vector<std::pair<short, unsigned char> > sockmap;
    for(short i=0; i< servmap_pointer->rows*servmap_pointer->cols;i++)
    {
      if(local_map[i]!=servmap_pointer->map[i])
      {
        sockmap.push_back(std::make_pair(i, servmap_pointer->map[i]));
        local_map[i]=servmap_pointer->map[i];
      }
    }
    if(sockmap.size()>0)
    {
      short n = sockmap.size();
      unsigned char byt = 0;
      WRITE(new_sockfd, &byt, sizeof(byt));
      WRITE(new_sockfd, &n, sizeof(n));
      for(short i=0; i< n; i++)
      {
        short offset= sockmap[i].first;
        unsigned char value = sockmap[i].second;
        WRITE(new_sockfd, &offset, sizeof(offset));
        WRITE(new_sockfd, &value, sizeof(value));
      }
    }
  }
}
void createServer() {
  //  close(2);
  //  int fd=open("/home/pinakdas163/611myfiles/project4/pinakfifo", O_WRONLY);
  // server daemon start up
  int shm_fd1=shm_open("/TAG_mymap",O_RDWR, S_IRUSR|S_IWUSR|S_IRWXU);
  int rowp, colp;
  read(shm_fd1, &rowp, sizeof(int));
  read(shm_fd1, &colp, sizeof(int));
  servmap_pointer=(mapboard*)mmap(NULL, (rowp*colp)+sizeof(mapboard), PROT_READ|PROT_WRITE,
  MAP_SHARED, shm_fd1, 0);

  servmap_pointer->daemonID=getpid();
  local_map = new unsigned char(rowp*colp);

  memcpy(local_map, servmap_pointer->map, rowp*colp);

  unsigned char servplayers = activePlayers();
  unsigned char playersock = G_SOCKPLR|servplayers;

  struct sigaction serversig_struct; // signal struct
  sigemptyset(&serversig_struct.sa_mask);
  serversig_struct.sa_flags=0;
  serversig_struct.sa_restorer=NULL;
  serversig_struct.sa_handler=serversighandler;
  //
  sigaction(SIGHUP, &serversig_struct, NULL);
  sigaction(SIGUSR1, &serversig_struct, NULL);
  //sigaction(SIGUSR2, &sig_struct, NULL);
  // ending of setup
  // server connection establishment

  //change this # between 2000-65k before using
  const char* portno="62010";
  struct addrinfo hints;
  memset(&hints, 0, sizeof(hints)); //zero out everything in structure
  hints.ai_family = AF_UNSPEC; //don't care. Either IPv4 or IPv6
  hints.ai_socktype=SOCK_STREAM; // TCP stream sockets
  hints.ai_flags=AI_PASSIVE; //file in the IP of the server for me
  struct addrinfo *servinfo;
  if((status=getaddrinfo(NULL, portno, &hints, &servinfo))==-1)
  {
    fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
    exit(1);
  }
  sockfd=socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);

  /*avoid "Address already in use" error*/
  int yes=1;
  if(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int))==-1)
  {
    perror("setsockopt");
    exit(1);
  }
  //We need to "bind" the socket to the port number so that the kernel
  //can match an incoming packet on a port to the proper process
  if((status=bind(sockfd, servinfo->ai_addr, servinfo->ai_addrlen))==-1)
  {
    perror("bind");
    exit(1);
  }
  //when done, release dynamically allocated memory
  freeaddrinfo(servinfo);

  if(listen(sockfd,1)==-1)
  {
    perror("listen");
    exit(1);
  }
  //printf("Blocking, waiting for client to connect\n");
  struct sockaddr_in client_addr;
  socklen_t clientSize=sizeof(client_addr);

  do {
    new_sockfd=accept(sockfd, (struct sockaddr*) &client_addr, &clientSize);
  }while(new_sockfd==-1 && errno==EINTR);
  if(new_sockfd==-1 && errno!=EINTR)
  {
    perror("accept");
    exit(1);
  }
  // read and write to the socket will be here
  WRITE(new_sockfd, &rowp, sizeof(rowp));
  WRITE(new_sockfd, &colp, sizeof(colp));
  for(int i=0;i<rowp*colp;i++)
  {
    WRITE(new_sockfd, &local_map[i], sizeof(local_map));
  }
  WRITE(new_sockfd, &playersock, sizeof(playersock));

  while(true) {
    unsigned char protocol, newmapbyte;
    short noOfchangedmap, offset;
    unsigned char playerbit[5]= {G_PLR0, G_PLR1, G_PLR2, G_PLR3, G_PLR4};
    READ(new_sockfd, &protocol, sizeof(protocol));

    if(protocol & G_SOCKPLR)
    {
      for(int i=0;i<5; i++)
      {
        if(protocol & playerbit[i] && servmap_pointer->players[i]==0)
        {
          servmap_pointer->players[i]=getpid();
        }
        else if((protocol & playerbit[i])==false && servmap_pointer->players[i]!=0)
        {
          servmap_pointer->players[i]=0;
        }
      }
      if(protocol==G_SOCKPLR)
      {
        shm_unlink("/TAG_mymap");
        sem_unlink("/mySem");
        exit(0);
      }
    }

    else if(protocol==0) {

      READ(new_sockfd, &noOfchangedmap, sizeof(noOfchangedmap));
      for(short i=0;i<noOfchangedmap; i++)
      {
        READ(new_sockfd, &offset, sizeof(offset));
        READ(new_sockfd, &newmapbyte, sizeof(newmapbyte));
        servmap_pointer->map[offset]=newmapbyte;
        local_map[offset]=newmapbyte;
      }
      for(int i=0;i<5; i++)
      {
        if(servmap_pointer->players[i]!=0 && servmap_pointer->players[i]!=getpid())
        {
          kill(servmap_pointer->players[i], SIGUSR1);
        }
      }
    }
  }

  close(sockfd);
  close(new_sockfd);
  delete local_map;
}

void create_server_daemon()
{
  if(fork() > 0)
  {
    return;
  }
  if(fork()>0)
    exit(0);

  if(setsid()==-1) //child obtains its own SID & Process Group
    exit(1);
  for(int i=0; i<sysconf(_SC_OPEN_MAX); ++i)
    close(i);
  open("/dev/null", O_RDWR); //fd 0
  open("/dev/null", O_RDWR); //fd 1
  //open("/dev/null", O_RDWR); //fd 2
  int fd=open("/home/pinakdas163/611myfiles/project4/pinakfifo", O_WRONLY);
  if(fd==-1)
  {
    exit(99);
  }
  umask(0);
  chdir("/");
  //now do whatever you want the daemon to do
  createServer();
}
