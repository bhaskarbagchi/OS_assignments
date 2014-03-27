#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/wait.h>
#include <time.h>

#define P(s) semop(s, &pop, 1)  /* pop is the structure we pass for doing
				   the P(s) operation */
#define V(s) semop(s, &vop, 1)  /* vop is the structure we pass for doing
				   the V(s) operation */

#define THINKING 0
#define HUNGRY 1
#define EATING 2
#define LEFT (id + N - 1) % N
#define RIGHT (id + 1) % N

void philospher(int id);
void take_fork(int id);
void test(int id);
void put_fork(int id);


int mutex;
int N, count;
int *S;
key_t key;
int shm_id, *state;

struct sembuf pop, vop;

int main()
{

	printf("Enter the number of philosophers: ");
	scanf("%d", &N);
	while(N < 2 || N > 10) {
		printf("Error: Range should be [2-10]\n");
		printf("Enter the number of philosophers: ");
		scanf("%d", &N);
	}

	printf("Enter number of iterations: ");
	scanf("%d", &count);

	while(count < 1 || count > 10) {
		printf("Error: Range should be [1-10]\n");
		printf("Enter the number of iterations: ");
		scanf("%d", &count);	
	}

	S = (int *) malloc(N * sizeof(int));

	int i, pid = 0 , status;

	/* Set up the sembuf structure for P(s) and V(s) operations. */
	pop.sem_num = vop.sem_num = 0;
	pop.sem_flg = vop.sem_flg = 0;
	pop.sem_op = -1 ; vop.sem_op = 1;

	/* Setup the mutex semaphore. */
	mutex = semget(IPC_PRIVATE, 1, 0777|IPC_CREAT);
	semctl(mutex, 0, SETVAL, 1);

	/* Setup the semaphores for each philosopher. */
	for (i = 0; i < N; ++i)
	{
		S[i] = semget(IPC_PRIVATE, 1, 0777|IPC_CREAT);
		semctl(S[i], 0, SETVAL, 1);
	}

	/* Setup the state shared array. */
	key = ftok("/usr/local/lib/", 4321);

	/* Create the segment. */
    if ((shm_id = shmget(key, N * sizeof(int), IPC_CREAT | 0777)) < 0) {
        perror("shmget");
        printf("You must have pressed Ctrl-C in previous execution.\n");
        printf("Please restart or change the key.\n");
        exit(1);
    }


    for (i = 0; i < N; ++i)
    {
    	pid = fork();
    	if(pid == 0) {
    		// Child process
    		philospher(i);
    		printf("Philosopher %d left the table.\n", i);
    		exit(0);
    	}	
    }

    for (i = 0; i < N; ++i)
    	wait(&status);


    /* Free and delete the memory segment. */
    if(shmctl(shm_id, IPC_RMID, 0) < 0)
    {
        perror("shmctl");
        exit(1);
    }

    /* Free the semaphores. */
    for (i = 0; i < N; ++i)
    {
    	semctl(S[i], 0, IPC_RMID, 0);
    }
    semctl(mutex, 0, IPC_RMID, 0);

    /* Free allocated memories. */
    free(S);
	return 0;
}

void philospher(int id) {
	int i;

	 /* Attach the segment to process' data space. */
    if ((state = shmat(shm_id, NULL, 0)) == (int *) -1) {
        perror("shmat");
        exit(1);
    }


    printf("Philosopher %d is thinking.\n", id);
    for (i = 0; i < count; ++i)
    {
        sleep(1);
        take_fork(id);
        sleep(0);
        put_fork(id);
    }

    /* Detach the shared memory segment. */
    if(shmdt(state) < 0) {
    	perror("shmdt");
        exit(1);
    }
}

void take_fork(int id)
{
    P(mutex);
    state[id] = HUNGRY;
    printf("Philosopher %d is hungry.\n",id);
    test(id);
    V(mutex);
    P(S[id]);
    sleep(1);
}

void test(int id)
{
    if (state[id] == HUNGRY && state[LEFT] != EATING && state[RIGHT] != EATING)
    {
        state[id] = EATING;
        sleep(2);
        printf("Philosopher %d takes fork %d and %d.\n", id, LEFT, id);
        printf("Philosopher %d is eating.\n",id);
        V(S[id]);
    }
}

void put_fork(int id)
{
    P(mutex);
    state[id] = THINKING;
    printf("Philosopher %d puts fork %d and %d down.\n",id,LEFT,id);
    printf("Philosopher %d is thinking.\n", id);
    test(LEFT);
    test(RIGHT);
    V(mutex);
}