//Socket server
#include <sys/types.h>
#include<sys/socket.h>
#include<netdb.h>
#include<unistd.h> //for read/write
#include<string.h> //for memset
#include<stdio.h> //for fprintf, stderr, etc.
#include<stdlib.h> //for exit

int main()
{
  int sockfd; //file descriptor for the socket
  int status; //for error checking


  //change this # between 2000-65k before using
  const char* portno="42424"; 
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
  if((new_sockfd=accept(sockfd, (struct sockaddr*) &client_addr, &clientSize))==-1)
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
  return(0);
}
