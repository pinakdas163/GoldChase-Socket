#include "fancyRW.h"
#include<iostream>
#include <stdlib.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/wait.h>
using namespace std;

int main() {
	  const char* myfifo="pinak.txt";
	  mkfifo(myfifo, 0666);

	  pid_t id=fork();
	  //cout<<"fork returned: "<<(int)id<<endl;
	  if(id<0)
	  {
	    perror("Fork failed");
	    exit(1);
	  }
	  if(id==0)
	  {
	    int chorbifd=open(myfifo,O_RDONLY);
	    char arr[2];  char arr1[3]; char arr2[5]; char arp[1];
	    //READ(chorbifd,arr, 11);
			read(chorbifd, arr, 2);
			read(chorbifd, arr1, 3);
			read(chorbifd, arr2, 5);
			read(chorbifd, arp, sizeof(arp));
	    cout<<"Received message: "<<arr<<arr1<<arr2<<endl;
			cout<<arp<<endl;
	    close(chorbifd);
	    cout<<"Leaving child process"<<endl;
	  }
	  else
	  {
	    int golufd=open(myfifo, O_WRONLY);
			// write(golufd, "To", 2);
			// write(golufd, "DD ", 3);
			// write(golufd, "Gibso", 5);
			// write(golufd, "n", 1);
			WRITE(golufd, "ToDD Gibson", 11);

	    close(golufd);
	    waitpid(id,NULL,0);
	    unlink(myfifo);
	    cout<<"Parent process finishing after waiting for child to be over "<<endl;
	  }
	  return 0;

}
