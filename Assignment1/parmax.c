#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int findmax(int A[], int L, int R);
int findMaxByProcess(int A[], int L, int R);
void printArray(int A[], int size);

int main(int argc, char* argv[]){
  if(argc < 2){
    printf("USAGE: ./parmax <size of array>\n");
    getchar();
    return 1;
  }
  int i, max, size = atoi(argv[1]);
  int* A;
  A = (int *)malloc(size*sizeof(int));
  srand(time(NULL));
  for(i = 0; i<size; i++){
    A[i] = rand()%127;
  }
  max = findmax(A, 0, size-1);
  printArray(A, size);
  printf("Maximum = %d\n", max);
  getchar();
  return 0;
}

int findmax(int A[], int L, int R){
  if((R-L)<10){
    int max, iterator;
    max = A[L];
    for(iterator = L; iterator<=R; iterator++){
      if(max<A[iterator])
        max = A[iterator];
    }
    return max;
  }
  else{
    int max = findMaxByProcess(A, L, R);
    return max;
  }
}

int findMaxByProcess(int A[], int L, int R){
  int PID, PID1, mid, maxLeft, maxRight;
  mid = (L+R)/2;
  PID = fork();
  if(PID == 0){
    maxLeft = findmax(A, L, mid);
    exit(maxLeft);
  }
  else{
    PID1 = fork();
    if(PID1 == 0){
      maxRight = findmax(A, mid+1, R);
      exit(maxRight);
    }
    else{
      wait(&maxLeft);
      maxLeft = maxLeft>>8;
      wait(&maxRight);
      maxRight = maxRight>>8;
      return (maxLeft>maxRight? maxLeft:maxRight);
    }
  }
}

void printArray(int A[], int size){
  int i;
  for(i = 0; i< size; i++){
    printf("%d ", A[i]);
  }
  printf("\n");
}
