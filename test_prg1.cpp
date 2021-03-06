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

Map *goldchasecopy;
sem_t *sem;   // semaphore
mqd_t playermsgque;
mapboard *map_pointer;
int currLoc=0;
mqd_t playerwrtque;
int shm_fd; // fd for shared mm
int player;

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
  sem_post(sem);

  bool playercheck=true;

  for(int i=0; i<5; i++) {
    if(map_pointer->players[i]!=0)
    {
      playercheck=false;
    }
  }
  if(playercheck==true)
  {
    shm_unlink("/TAG_mymap");
    sem_close(sem);
    sem_unlink("/mySem");
  }
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

int main(int argc, char *argv[])
{
  bool firstProcess=false;
  // close(2);
  // int fd=open("mypipe", O_WRONLY);
  struct sigaction sig_struct; // signal struct
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
  unsigned char *str_map;
  srand(time(NULL));
  player=G_PLR0;
  bool goldfound=false;
  bool endOfmap=false;
  const string file="mymap.txt";
  vector<string> mapstring;

  sem = sem_open("/mySem", O_RDWR,
                       S_IRUSR| S_IWUSR| S_IRGRP| S_IWGRP| S_IROTH| S_IWOTH,
                       1);
// first player
  if(sem==SEM_FAILED)
  {
    if(argv[1]==NULL) {
      firstProcess=true;
    }
    sem = sem_open("/mySem", O_CREAT,
                         S_IRUSR| S_IWUSR| S_IRGRP| S_IWGRP| S_IROTH| S_IWOTH,1);
    mapstring=createMap(file, row, col, gold);
    sem_wait(sem);
    shm_fd=shm_open("/TAG_mymap",O_CREAT|O_RDWR|O_EXCL, S_IRUSR|S_IWUSR);
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
    shm_fd=shm_open("/TAG_mymap",O_RDWR, S_IRUSR|S_IWUSR);
    int rowp, colp;
    read(shm_fd, &rowp, sizeof(int));
    read(shm_fd, &colp, sizeof(int));
    map_pointer=(mapboard*)mmap(NULL, (rowp*colp)+sizeof(mapboard), PROT_READ|PROT_WRITE,
    MAP_SHARED, shm_fd, 0);
    str_map=map_pointer->map;
    row=map_pointer->rows;
    col=map_pointer->cols;
    sem_post(sem);
    //maskPlayer();
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

    // if you are first player daemon creation here...

  int did=getpid();
  for(int i=0;i<5;i++) {
    if(map_pointer->players[i]!=did && map_pointer->players[i]!=0)
    {
      kill(map_pointer->players[i], SIGUSR1);
    }
  }

  // if(firstProcess) {
  //   if(fork()==0) {
  //     create_server_daemon();
  //   }
  // }

  // if(map_pointer->daemonID!=0) {
  //   kill(map_pointer->daemonID, SIGHUP);
  // }

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
  kill(map_pointer->daemonID, SIGHUP);
  sem_post(sem);
  bool playercheck=true;
  for(int i=0; i<5; i++) {
    if(map_pointer->players[i]!=0)
    {
      playercheck=false;
    }
  }


  if(playercheck==true)
  {
    shm_unlink("/TAG_mymap");
    sem_close(sem);
    sem_unlink("/mySem");
  }
  //std::string qname=mqname(getpid());
  //const char* cnam=qname.c_str();
  mq_close(playermsgque);
  mq_unlink(cnam);
  delete goldchasecopy;
  return 0;
}
