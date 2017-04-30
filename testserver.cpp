#include<iostream>
#include<stdlib.h>
#include<fstream>
#include<unistd.h>
#include<errno.h>
#include<sys/types.h>
#include <fcntl.h>           /* For O_* constants */
#include <sys/stat.h>        /* For mode constants */
#include <sys/mman.h>
#include <sstream>
#include <string.h>
#include <cstring>
#include <netdb.h>
#include <stdio.h>
#include "fancyRW.h"
#include "mapboard.h"
#include <sys/wait.h>
#include<sys/socket.h>
using namespace std;

void serverdaemon() {

  int sockfd; //file descriptor for the socket
  int status; //for error checking


  //change this # between 2000-65k before using
  const char* portno="42425";
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
  printf("Blocking, waiting for client to connect\n");

  struct sockaddr_in client_addr;
  socklen_t clientSize=sizeof(client_addr);
  int new_sockfd;
  do {
    new_sockfd=accept(sockfd, (struct sockaddr*) &client_addr, &clientSize);
  }while(new_sockfd==-1 && errno==EINTR);
  if(new_sockfd==-1 && errno!=EINTR)
  {
    perror("accept");
    exit(1);
  }

  //read & write to the socket
  char buffer[100];
  memset(buffer,0,100);
  int n;
  if((n=read(new_sockfd, buffer, 99))==-1)
  {
    perror("read is failing");
    printf("n=%d\n", n);
  }
  printf("The client said: %s\n", buffer);

  const char* message="These are the times that try men's souls.";
  write(new_sockfd, message, strlen(message));
  close(sockfd);
  close(new_sockfd);
}

int main(int argc, char* argv[]) {
	serverdaemon();
	return 0;
}
