#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>

#define MSGSZ 128

typedef struct _msgBuf{
  long mtype;
  char mtext[MSGSZ];
} msgBuf;

int main(int argc, char* argv[]){
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
  
  /*
   * send a message
   */
  sBuf.mtype = getpid();
  printf("Insert messege to send to server: ");
  scanf("%s", sBuf.mtext);
  bufLen = strlen(sBuf.mtext);
  if(msgsnd(UP, &sBuf, bufLen, IPC_NOWAIT) < 0){
    printf("Messege sending failed!!!\n");
    exit(0);
  }
  
  /*
   * recieve message
   */
  if(msgrcv(DOWN, &rBuf, MSGSZ, getpid(), 0) < 0){
    printf("Error in recieving message.\n");
    exit(0);
  }
  else{
    printf("Processed messege from server: %s\n", rBuf.mtext);
  }
  
  return 0;
}
