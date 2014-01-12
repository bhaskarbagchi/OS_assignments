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
  execl("/usr/bin/xterm", "/usr/bin/xterm", "-e", "./parmax", argv[1], (void*)NULL);
  printf("If execl function is called properly this sentence should not be printed.\n");
  return 0;
}
