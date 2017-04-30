#include<iostream>
#include<vector>
#include <sys/socket.h>
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
#include <netdb.h>
#include <stdio.h>
#include "fancyRW.h"
#include "mapboard.h"
#include <sys/wait.h>
using namespace std;

Map *goldchasecopy;
sem_t *sem;   // semaphore
mqd_t playermsgque;
mapboard *map_pointer;
int currLoc=0;
mqd_t playerwrtque;
int player;
int clientdpipe[2];
int clientfd;
unsigned char *local_map2;
int input(int item, unsigned char *mp, int row, int col)
{
  int randnum=0;
  while(1)
  {
    randnum=rand()%(row*col);
    //cout<<randnum<<endl;
    if(mp[randnum]==G_WALL || mp[randnum] == G_GOLD || mp[randnum] == G_FOOL ||
      mp[randnum] == G_PLR0 || mp[randnum] == G_PLR1 || mp[randnum] == G_PLR2 ||
    mp[randnum] == G_PLR3 || mp[randnum] == G_PLR4)
    {continue;}
    mp[randnum]=item;
    break;
  }
  return randnum;
}

vector<string> createMap(string file, int &row, int &col, int &gold)
{
  ifstream mymap;
  mymap.open(file.c_str());
  string str;
  vector<string> map;
  if(!mymap)
  {
    cout << "Could not open mymap.txt" << endl;
    exit(1);
  }
  else {
    getline(mymap, str);
    gold = atoi(str.c_str());
    getline(mymap, str);
    map.push_back(str);
    row++;
    col = str.length();
    while(getline(mymap, str))
    {
      map.push_back(str);
      row++;
    }
    mymap.close();
  }
  return map;
}

void readMap(vector<string> mapstring) {
  unsigned char *str;
  str=map_pointer->map;
  for(std::vector<string>::iterator it=mapstring.begin(); it!=mapstring.end();it++)
  {
    string line=*it;
    char *ptr=&line[0];
    while(*ptr!='\0')
    {
      if(*ptr==' ') {*str=0; }
      else if(*ptr=='*') {*str=G_WALL; }
      else if(*ptr=='1') {*str=G_PLR0; }
      else if(*ptr=='2') {*str=G_PLR1; }
      else if(*ptr=='3') {*str=G_PLR2; }
      else if(*ptr=='4') {*str=G_PLR3; }
      else if(*ptr=='5') {*str=G_PLR4; }
      else if(*ptr=='G') {*str=G_GOLD; }
      else if(*ptr=='F') {*str=G_FOOL; }
      ptr++; str++;
    }
  }
}

void maskPlayer() {
  sem_wait(sem);
  if(map_pointer->players[0]==0)
  {
    map_pointer->players[0]=getpid();
    player = G_PLR0;
  }
  else if(map_pointer->players[1]==0)
  {
    map_pointer->players[1]=getpid();
    player = G_PLR1;
  }
  else if(map_pointer->players[2]==0)
  {
    map_pointer->players[2]=getpid();
    player = G_PLR2;
  }
  else if(map_pointer->players[3]==0)
  {
    map_pointer->players[3]=getpid();
    player = G_PLR3;
  }
  else if(map_pointer->players[4]==0)
  {
    map_pointer->players[4]=getpid();
    player = G_PLR4;
  }
  else {
    cout<<"No space for more players"<<endl;
    sem_post(sem);
    exit(1);
  }
  sem_post(sem);
}

void searchgold(unsigned char* str_map,Map* goldchasecopy, int currLoc, bool &goldfound)
{
  if(str_map[currLoc] & G_FOOL)
  {
    goldchasecopy->postNotice("Not a real gold, keep searching..");
    goldchasecopy->drawMap();
  }
  else if(str_map[currLoc] & G_GOLD)
  {
    goldchasecopy->postNotice("You found the real gold!");
		str_map[currLoc]&=~G_GOLD;
    goldfound=true;
    goldchasecopy->drawMap();
  }
}

void insertGold(unsigned char *mp, int gold, int row, int col)
{
  for(int i=0; i<gold; i++)
  {
    if(i==0)
    {
      input(G_GOLD, mp, row, col);
    }
    else {
      input(G_FOOL, mp, row, col);
    }
  }
}

bool movePlayer(unsigned char* str_map, int &currLoc,int row, int col, char key,
  Map* goldchasecopy, int player, bool &goldfound) {

    int player_row=currLoc/col;
    int player_col=currLoc%col;
    switch(key)
    {
      case 'h':
      {
        if(player_col<=0 && goldfound==false)
        {
          return false;
        }
        if(str_map[currLoc-1]==G_WALL) {
            return false;
        }
        if(player_col<=0 && goldfound==true)
        {
          str_map[currLoc]&=~player;
          return true;
        }
        str_map[currLoc]&=~player;
        currLoc--;
        str_map[currLoc]|=player;
        searchgold(str_map,goldchasecopy, currLoc, goldfound);
      }
      break;

      case 'l':
      {
        if(((player_row*col)+player_col+1)>=(player_row+1)*col && goldfound==true)
        {
          //cerr<<"Came inside gold found part"<<endl;
          str_map[currLoc]&=~player;
          return true;
        }
        if(((player_row*col)+player_col+1)>=(player_row+1)*col && goldfound==false)
          return false;
        if(str_map[currLoc+1]==G_WALL) {
          return false;
        }
        str_map[currLoc]&=~player;
        currLoc++;
        str_map[currLoc]|=player;
        searchgold(str_map,goldchasecopy, currLoc, goldfound);
      }
      break;

      case 'j':
      {
        if(((player_row*col)+player_col+col)>=(col*row) && goldfound==true)
        {
          str_map[currLoc]&=~player;
          return true;
        }
        if(((player_row*col)+player_col+col)>=(col*row) && goldfound==false) {
          return false;
        }
        if(str_map[(player_row*col)+(player_col+col)]==G_WALL)
        {
          return false;
        }
        str_map[currLoc]&=~player;
        currLoc=(player_row*col)+(player_col+col);
        str_map[currLoc]|=player;
        searchgold(str_map,goldchasecopy, currLoc, goldfound);
      }
      break;

      case 'k':
      {
        if(player_row<=0 && goldfound==true)
        {
          str_map[currLoc]&=~player;
            return true;
        }
        if(player_row<=0 && goldfound==false)
          return false;
        if(str_map[(player_row*col)+(player_col-col)]==G_WALL)
        {
          return false;
        }
        str_map[currLoc]&=~player;
        currLoc=(player_row*col)+(player_col-col);
        str_map[currLoc]|=player;
        searchgold(str_map,goldchasecopy, currLoc, goldfound);
      }
      break;
  }
  return false;
}

std::string mqname(int id) {
  std::stringstream ss;
  ss << "/mqueue-"<<id;
  return ss.str();
}

void exitfunc() {
  int id=getpid();
  sem_wait(sem);
  if(map_pointer->players[0]==id) {
    map_pointer->map[currLoc] &=~ G_PLR0;
    map_pointer->players[0]=0;
  }
  else if(map_pointer->players[1]==id) {
    map_pointer->map[currLoc] &=~ G_PLR1;
    map_pointer->players[1]=0;
  }
  else if(map_pointer->players[2]==id) {
    map_pointer->map[currLoc] &=~ G_PLR2;
    map_pointer->players[2]=0;
  }
  else if(map_pointer->players[3]==id) {
    map_pointer->map[currLoc] &=~ G_PLR3;
    map_pointer->players[3]=0;
  }
  else if(map_pointer->players[4]==id) {
    map_pointer->map[currLoc] &=~ G_PLR4;
    map_pointer->players[4]=0;
  }
  for(int i=0;i<5;i++) {
    if(map_pointer->players[i]!=id && map_pointer->players[i]!=0)
    {
      kill(map_pointer->players[i], SIGUSR1);
    }
  }
  if(map_pointer->daemonID!=0) {
    kill(map_pointer->daemonID, SIGUSR1);
  }
  sem_post(sem);

  // bool playercheck=true;
  //
  // for(int i=0; i<5; i++) {
  //   if(map_pointer->players[i]!=0)
  //   {
  //     playercheck=false;
  //   }
  // }
  // if(playercheck==true)
  // {
  //   shm_unlink("/TAG_mymap");
  //   sem_close(sem);
  //   sem_unlink("/mySem");
  // }
  std::string qname=mqname(getpid());
  const char* cnam=qname.c_str();
  mq_close(playermsgque);
  mq_unlink(cnam);
  delete goldchasecopy;
}

void handleplayermovement(int handler)
{
  if(handler==SIGUSR1) {
      goldchasecopy->drawMap();
  }
  else if(handler == SIGINT || handler == SIGTERM || handler == SIGHUP)
  {
      exitfunc();
      exit(1);
  }
}

void msghandler(int handle)
{
  struct sigevent mq_notification_event; //create a structure object
  mq_notification_event.sigev_notify=SIGEV_SIGNAL; //signal notification
  mq_notification_event.sigev_signo=SIGUSR2; //using the SIGUSR2 signal
  mq_notify(playermsgque, &mq_notification_event); //register it!

  char msg[250]; //a char array for the message
  memset(msg,0,250); //zero it out (if necessary)
  while(mq_receive(playermsgque, msg, 250, NULL)!=-1)
  //call postNotice(msg) for your Map object;
    goldchasecopy->postNotice(msg);
  if(errno!=EAGAIN) //EAGAIN expected after all messages are read
  {
    perror("mq_receive error"); //if errno != EAGAIN
    exitfunc();
    exit(1);  //clean up message queue. Remove shared memory
      //and semaphore if last player and exit.
  }
}

std::string playerNo()
{
  int playno;
  for(int i=0;i<5;i++)
  {
    if(map_pointer->players[i]==getpid())
    {
      playno=i+1;
      break;
    }
  }
  std::stringstream ss;
  ss<<"Player "<<playno;
  return ss.str();
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

void create_client_daemon(string ipaddr) {
  WRITE(2, "daemon created1\n", sizeof("daemon created1 "));
  int status;
  int clientrows, clientcols;

  unsigned char *clientsidemap;
  unsigned char clientPlayerSoc;
  //pipe(clientdpipe);
  if(fork()>0) {
    // close(clientdpipe[1]);
    // int msg;
    // READ(clientdpipe[0], &msg, sizeof(msg));
    // if(msg==0)
    //   WRITE(2, "SHM creation failed\n", sizeof("SHM creation failed "));
    // else
    //   WRITE(2, "SHM creation success\n", sizeof("SHM creation success "));
    return;
  }

  if(fork>0) {
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
  open("/dev/null", O_RDWR); //fd 2
  // int clfd=open("/home/binit/GoldChase-Socket/binitfifo", O_WRONLY);
  // if(clfd==-1)
  // {
  //   exit(99);
  // }
  umask(0);
  chdir("/");

  //now do whatever you want the daemon to do
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
    READ(clientfd,&square, sizeof(char));
    clientsidemap[i]=square;
  }
  memcpy(local_map2, clientsidemap, clientrows*clientcols);

  sem=sem_open("/mySem", O_CREAT,
                       S_IRUSR| S_IWUSR| S_IRGRP| S_IWGRP| S_IROTH| S_IWOTH,1);
  if(sem==SEM_FAILED)
  {
    WRITE(2, "semaphore creation in client failed\n", sizeof("semaphore creation in client failed "));
  }
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

  READ(clientfd, &clientPlayerSoc, sizeof(clientPlayerSoc));

  unsigned char playerbit[5]={G_PLR0, G_PLR1, G_PLR2, G_PLR3, G_PLR4};
  for(int i=0; i<5; i++)
  {
    if((clientPlayerSoc & playerbit[i]) && map_pointer->players[i]==0)
    {
      map_pointer->players[i]=getpid();
    }
  }
  WRITE(2, "client demon completed setup memory\n", sizeof("client demon completed setup memory "));
  sem_post(sem);

  // int msg=1;
  // WRITE(clientdpipe[1], &msg, sizeof(msg));
  while(true) {
    unsigned char protocol, newmapbyte;
    short noOfchangedmap, offset;
    READ(clientfd, &protocol, sizeof(protocol));

    if(protocol & G_SOCKPLR)
    {
      for(int i=0;i<5; i++)
      {
        if(protocol & playerbit[i] && map_pointer->players[i]==0)
        {
          map_pointer->players[i]=getpid();
        }
        else if((protocol & playerbit[i])==false && map_pointer->players[i]!=0)
        {
          map_pointer->players[i]=0;
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
  }
  close(clientfd);
  delete local_map2;
}

int main(int argc, char *argv[])
{
  //bool firstProcess=false;
  // close(2);
  // int fd=open("mypipe", O_WRONLY);
  struct sigaction sig_struct;
  sig_struct.sa_handler=handleplayermovement;
  sigemptyset(&sig_struct.sa_mask);
  sig_struct.sa_flags=0;
  sig_struct.sa_restorer=NULL;

  sigaction(SIGUSR1, &sig_struct, NULL);
  sigaction(SIGINT, &sig_struct, NULL);
  sigaction(SIGTERM, &sig_struct, NULL);
  sigaction(SIGHUP, &sig_struct, NULL);

  sig_struct.sa_handler=msghandler;

  sigaction(SIGUSR2, &sig_struct, NULL);

  int gold=0; int row=0; int col=0;
  char key='a';
  unsigned char *str;
  unsigned char *str_map;
  srand(time(NULL));
  player=G_PLR0;
  bool goldfound=false;
  bool endOfmap=false;
  const string file="mymap2.txt";
  vector<string> mapstring;

  sem = sem_open("/mySem", O_RDWR,
                       S_IRUSR| S_IWUSR| S_IRGRP| S_IWGRP| S_IROTH| S_IWOTH,
                       1);
   if(argv[1]!=NULL) {
     string ipaddr;
     ipaddr=argv[1];
     if(sem==SEM_FAILED)
     {
       create_client_daemon(ipaddr);
       while(sem_open("/mySem", O_RDWR,
                            S_IRUSR| S_IWUSR| S_IRGRP| S_IWGRP| S_IROTH| S_IWOTH,
                            1)==SEM_FAILED) {
         WRITE(2, "looping inside\n",sizeof("looping inside "));
         sleep(4);
       }
       WRITE(2, "shared memory found\n", sizeof("shared memory found "));
     }
    //  WRITE(2, "shared memory found\n", sizeof("shared memory found "));
   }
// first player
  if(sem==SEM_FAILED)
  {
    sem = sem_open("/mySem", O_CREAT,
                         S_IRUSR| S_IWUSR| S_IRGRP| S_IWGRP| S_IROTH| S_IWOTH,1);
    mapstring=createMap(file, row, col, gold);
    sem_wait(sem);
    int shm_fd=shm_open("/TAG_mymap",O_CREAT|O_RDWR, S_IRUSR|S_IWUSR);
    ftruncate(shm_fd, (row*col)+sizeof(mapboard));
    map_pointer=(mapboard*)mmap(NULL, (row*col)+sizeof(mapboard), PROT_READ|PROT_WRITE,
      MAP_SHARED, shm_fd, 0);

      map_pointer->rows=row;
      map_pointer->cols=col;
      for(int i=0;i<5;i++) {
       map_pointer->players[i]=0;
      }
    readMap(mapstring);

    str_map=map_pointer->map;
    insertGold(str_map, gold, row, col);
    sem_post(sem);
  }
  // subsequent players
  else {
    sem_wait(sem);
    WRITE(2, "client player got the shared memory\n", sizeof("client player got the shared memory "));
    int file_desc=shm_open("/TAG_mymap",O_RDWR, S_IRUSR|S_IWUSR);
    int rowp, colp;
    read(file_desc, &rowp, sizeof(int));
    read(file_desc, &colp, sizeof(int));
    map_pointer=(mapboard*)mmap(NULL, (rowp*colp)+sizeof(mapboard), PROT_READ|PROT_WRITE,
    MAP_SHARED, file_desc, 0);
    str_map=map_pointer->map;
    row=map_pointer->rows;
    col=map_pointer->cols;
    sem_post(sem);
  }

  maskPlayer();

  goldchasecopy=new Map(str_map, row, col);

  sem_wait(sem);
  currLoc=input(player, str_map, row, col);

  sem_post(sem);

  mq_attr queueval;
  queueval.mq_maxmsg=10;
  queueval.mq_msgsize=250;
  std::string qname=mqname(getpid());
  const char* cnam=qname.c_str();
  playermsgque = mq_open(cnam, O_RDONLY|O_CREAT|O_EXCL|O_NONBLOCK, S_IRUSR|S_IWUSR, &queueval);
  if(playermsgque==-1)
  {
    if(errno==EEXIST) {
      std::cerr << "Message queue already exist"<<endl;
      mq_unlink(cnam);
    }
  }

  struct sigevent mq_notification_event; //create a structure object
  mq_notification_event.sigev_notify=SIGEV_SIGNAL; //signal notification
  mq_notification_event.sigev_signo=SIGUSR2; //using the SIGUSR2 signal
  mq_notify(playermsgque, &mq_notification_event); //register it!

  goldchasecopy->drawMap();

  int did=getpid();
  for(int i=0;i<5;i++) {
    if(map_pointer->players[i]!=did && map_pointer->players[i]!=0)
    {
      kill(map_pointer->players[i], SIGUSR1);
    }
  }

  if(map_pointer->daemonID==0)
  {
    //WRITE(2,"daemonID equals zero\n",sizeof("daemonid equals zero "));
    create_server_daemon();
    wait(NULL);
  }
  else {
    kill(map_pointer->daemonID, SIGHUP);
    kill(map_pointer->daemonID, SIGUSR1);
  }

  while(key!='Q') {
    //key=goldmine.getKey();
    key=goldchasecopy->getKey();
    if(key==-1) {
      continue;
    }
    if(key=='m') {
      unsigned int playermask=0;
      int id=getpid();
      if(map_pointer->players[0]!=0 && map_pointer->players[0]!=id)
        playermask |= G_PLR0;
      if(map_pointer->players[1]!=0 && map_pointer->players[1]!=id)
        playermask |= G_PLR1;
      if(map_pointer->players[2]!=0 && map_pointer->players[2]!=id)
        playermask |= G_PLR2;
      if(map_pointer->players[3]!=0 && map_pointer->players[3]!=id)
        playermask |= G_PLR3;
      if(map_pointer->players[4]!=0 && map_pointer->players[4]!=id)
        playermask |= G_PLR4;
      unsigned int pl= goldchasecopy->getPlayer(playermask);
      std::string nam, msg;
      if(pl==G_PLR0) {
        nam=mqname(map_pointer->players[0]);
      }
      else if(pl==G_PLR1) {
        nam=mqname(map_pointer->players[1]);
      }
      else if(pl==G_PLR2) {
        nam=mqname(map_pointer->players[2]);
      }
      else if(pl==G_PLR3) {
        nam=mqname(map_pointer->players[3]);
      }
      else if(pl==G_PLR4) {
        nam=mqname(map_pointer->players[4]);
      }

      msg=playerNo();
      msg+=" says: ";
      msg+=goldchasecopy->getMessage();
      const char* c_nam=nam.c_str();
      if((playerwrtque=mq_open(c_nam, O_WRONLY))==-1)
      {
        perror("mq_open failed");
        exit(1);
      }
      if(mq_send(playerwrtque, msg.c_str(), msg.length() > 250 ? 250 : msg.length(), 0)==-1)
      {
        perror("bad send");
        exit(1);
      }
      mq_close(playerwrtque);
    }

    if(key=='b') {
      std::string nam, msg;
      int id=getpid();
      msg=playerNo();
      msg+=" says: ";
      msg+=goldchasecopy->getMessage();
      for(int i=0;i<5;i++)
      {
        if(map_pointer->players[i]!=0 && map_pointer->players[i]!=id)
        {
          nam=mqname(map_pointer->players[i]);
          mqd_t playerbroadcast;
          if((playerbroadcast=mq_open(nam.c_str(), O_WRONLY))==-1)
          {
            perror("mq_open failed");
            exit(1);
          }
          if(mq_send(playerbroadcast, msg.c_str(), msg.length() > 250 ? 250 : msg.length(), 0)==-1)
          {
            perror("bad send");
            exit(1);
          }
          mq_close(playerbroadcast);
        }
      }
    }
    sem_wait(sem);
    endOfmap=movePlayer(str_map, currLoc, row, col, key, goldchasecopy, player, goldfound);
    sem_post(sem);
    if(endOfmap==true) {
      goldchasecopy->drawMap();
      goldchasecopy->postNotice("Game finished! You won :)");
      string res=playerNo();
      res+=" won!";
      int pid=getpid();
      for(int i=0;i<5;i++) {
        if(map_pointer->players[i]!=pid && map_pointer->players[i]!=0)
        {
          kill(map_pointer->players[i], SIGUSR1);

          string val=mqname(map_pointer->players[i]);
          mqd_t playerbroadcast;
          if((playerbroadcast=mq_open(val.c_str(), O_WRONLY))==-1)
          {
            perror("mq_open failed");
            exit(1);
          }
          if(mq_send(playerbroadcast, res.c_str(), res.length(), 0)==-1)
          {
            perror("bad send");
            exit(1);
          }
          mq_close(playerbroadcast);
        }
      }
      if(map_pointer->daemonID!=0) {
        kill(map_pointer->daemonID, SIGUSR1);
      }
      break;
    }
    goldchasecopy->drawMap();
    // code will be here to send signal
    int pid=getpid();
    for(int i=0;i<5;i++) {
      if(map_pointer->players[i]!=pid && map_pointer->players[i]!=0)
      {
        kill(map_pointer->players[i], SIGUSR1);
      }
    }
    if(map_pointer->daemonID!=0) {
      kill(map_pointer->daemonID, SIGUSR1);
    }
  }
  sem_wait(sem);
  map_pointer->map[currLoc]&=~player;
  if(player==G_PLR0)
    map_pointer->players[0]=0;
  else if(player==G_PLR1)
      map_pointer->players[1]=0;
  else if(player==G_PLR2)
      map_pointer->players[2]=0;
  else if(player==G_PLR3)
      map_pointer->players[3]=0;
  else if(player==G_PLR4)
      map_pointer->players[4]=0;
  if(map_pointer->daemonID!=0) {
    kill(map_pointer->daemonID, SIGHUP);
    //kill(map_pointer->daemonID, SIGUSR1);
  }
  sem_post(sem);
  // bool playercheck=true;
  // for(int i=0; i<5; i++) {
  //   if(map_pointer->players[i]!=0)
  //   {
  //     playercheck=false;
  //   }
  // }
  // daemon is now responsible for removing shared memory and semaphore, check daemon id
  // if(playercheck==true)
  // {
  //   shm_unlink("/TAG_mymap");
  //   sem_close(sem);
  //   sem_unlink("/mySem");
  // }
  //std::string qname=mqname(getpid());
  //const char* cnam=qname.c_str();
  mq_close(playermsgque);
  mq_unlink(cnam);
  delete goldchasecopy;
  return 0;
}
