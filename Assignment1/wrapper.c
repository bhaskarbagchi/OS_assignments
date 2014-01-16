#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>

int main(int argc, char* argv[]){
  if(argc<2){
    printf("USAGE: ./wrapper <size of array>");
    getchar();
    return 1;
  }
  execl("/usr/bin/xterm", "/usr/bin/xterm", "-e", "./parmax", argv[1], (void*)NULL);        //execl() function calls xTerm window and runs parmax on that. It exits when execution of parmax is over.
  printf("If execl function is called properly this sentence should not be printed.\n");    //Since the process is already over when parmax is executed, this sentence should never be printed on the terminal.
  return 0;
}
