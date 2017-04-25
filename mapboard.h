#ifndef MAPBOARD_H
#define MAPBOARD_H
//#include "serverdaemon.cpp"
struct mapboard {
  int rows;//8
  int cols;//8
  pid_t daemonID;  //processid of daemon
  pid_t players[5];
  unsigned char map[0]; //0
};

//sem_t *sem;   // semaphore
void create_server_daemon();
//create_server_daemon();
#endif
