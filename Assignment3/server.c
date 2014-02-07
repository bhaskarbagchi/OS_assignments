#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/ipc.h>
#include <sys/msg.h>

#define MSGSZ 256

typedef struct _msgBuf{
  long mtype;
  char mtext[MSGSZ];
} msgBuf;

void processMsg(char msg[], char proc[]){
  strcpy(proc, msg);
}

int main(int argc, char* argv[]){
  
  printf("Server created(PID = %d).\n", getpid());
  
  int UP, DOWN;
  key_t keyUP, keyDOWN;
  int msgflg = IPC_CREAT|0666;
  keyUP = 1234;
  keyDOWN = 4321;
  
  msgBuf sBuf, rBuf;
  size_t bufLen;
  
  time_t now;
  
  if((UP = msgget(keyUP, msgflg)) < 0){
    printf("ERROR!msgget() failed. UP queue could not be created.\n");
    exit(0);
  }
  if((DOWN = msgget(keyDOWN, msgflg)) < 0){
    printf("ERROR!msgget() failed. DOWN queue could not be created.\n");
    exit(0);
  }
  printf("Message queues (UP ID %d, DOWN ID %d) has been created.\n\n", UP, DOWN);
  
  while(1){
    /*
     * recieve message
     */
    if(msgrcv(UP, &rBuf, MSGSZ, 0, 0) < 0){
      printf("Error in recieving message.\n");
      exit(0);
    }
    else{
      time(&now);
      printf("Message recieved at time: %s", ctime(&now));
    }
    
    /*
     * process messege
     */
    sBuf.mtype = rBuf.mtype;
    processMsg(rBuf.mtext, sBuf.mtext);
    bufLen = strlen(sBuf.mtext);

    /*
     * send message
     */
    if(msgsnd(DOWN, &sBuf, bufLen, IPC_NOWAIT) < 0){
      printf("Messege sending failed!!!\n");
      exit(0);
    }
  }
  
  printf("THE END\n");
  
  return 0;
}
