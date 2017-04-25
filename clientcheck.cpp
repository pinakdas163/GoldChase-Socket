#include <iostream>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "goldchase.h"
#include "fancyRW.h"
#include <unistd.h>
using namespace std;

void readsocketmsg(const string);
void writesocketmsg(const string);
void writesocketplayer(const string);
void writesocketmap(const string);

int main(int argc, char *argv[])
{
  string value;
  const string file="test.txt";
  const string newfile="test2.txt";
  const string file2="test3.txt";
  //value="client side";
  //if(argv[1]!=NULL)
  //readsocketmsg(file);
  //writesocketmsg(file);
  //writesocketplayer(newfile);
  writesocketmap(file2);
  return 0;
}

void writesocketmap(const string file) {
  int fd=open(file.c_str(), O_CREAT|O_RDWR, S_IRUSR|S_IWUSR);
  if(fd==-1) {
    cerr<<"Error opening the file"<<endl;
  }
  else {
    int byt=0;
    WRITE(fd, &byt, sizeof(char));
    short val=2;
    WRITE(fd, &val, sizeof(short));
    for(short i=1; i<=val; i++)
    {
      int value=G_PLR1;
      WRITE(fd, &i, sizeof(short));
      WRITE(fd, &value, sizeof(char));
    }
  }
  close(fd);
}

void writesocketplayer(const string file) {
  int fd=open(file.c_str(), O_CREAT|O_RDWR, S_IRUSR|S_IWUSR);
  if(fd==-1) {
    cerr<<"Error opening the file"<<endl;
  }
  else {
    int player=G_SOCKPLR|G_PLR2;
    WRITE(fd, &player, sizeof(char));
  }
  close(fd);
}

void readsocketmsg(const string file) {
  int fd=open(file.c_str(), O_RDWR, S_IRUSR|S_IWUSR);
  if(fd==-1) {
    cerr<<"Error opening the file"<<endl;
  }
  else {
    char arr;
    READ(fd, &arr, 1);
    cout<<arr<<endl;
  }
  close(fd);
}

void writesocketmsg(const string file) {

  int fd=open(file.c_str(), O_RDWR, S_IRUSR|S_IWUSR);
  if(fd==-1) {
    cerr<<"Error opening the file"<<endl;
  }
  else {
    int player=G_SOCKMSG|G_PLR3;
    WRITE(fd, &player, sizeof(char));
    short noBytes=11;
    WRITE(fd, &noBytes, sizeof(short));
    WRITE(fd, "todd gibson", 11);
  }
  close(fd);
}
