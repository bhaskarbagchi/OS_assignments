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

#define LEFT (id + N - 1) % N
#define RIGHT (id + 1) % N

void philospher(int id);
void take_fork(int id, int r_id);
void put_fork(int id, int r_id);
int deadlock();
void preempt();
void free_resources(int sig);

int N, t_time;
int *S;
key_t key;
int shm_id;

int *graph, *readmutex;

struct sembuf pop, vop;


int main()
{
    srand((unsigned int)time(NULL));
    signal(SIGINT, free_resources);

	// printf("Enter the number of philosophers: ");
	// scanf("%d", &N);
	// while(N < 2 || N > 10) {
	// 	printf("Error: Range should be [2-10]\n");
	// 	printf("Enter the number of philosophers: ");
	// 	scanf("%d", &N);
	// }
    N = 5;
    printf("Number of philosophers: %d\n", N);

	printf("Enter number of seconds to run the code: ");
	scanf("%d", &t_time);
    while(t_time < 10 || t_time > 30) {
        printf("Error: Range should be [10-30]\n");
        printf("Enter number of seconds to run the code for: ");
        scanf("%d", &t_time);
    }

    t_time += time(NULL);

	S = (int *) malloc(N * sizeof(int));
    readmutex = (int *) malloc(N * sizeof(int));

	int i, pid = 0;

	/* Set up the sembuf structure for P(s) and V(s) operations. */
	pop.sem_num = vop.sem_num = 0;
	pop.sem_flg = vop.sem_flg = 0;
	pop.sem_op = -1 ; vop.sem_op = 1;

	/* Setup the semaphores for each fork. */
	for (i = 0; i < N; ++i)
	{
		S[i] = semget(IPC_PRIVATE, 1, 0777|IPC_CREAT);
		semctl(S[i], 0, SETVAL, 1);
	}

    for (i = 0; i < N; ++i)
    {
        readmutex[i] = semget(IPC_PRIVATE, 1, 0777|IPC_CREAT);
        semctl(readmutex[i], 0, SETVAL, 1);  
    }

	/* Setup the resource graph shared array. */
	key = ftok("/usr/local/lib/", 1234);

	/* Create the segment. */
    if ((shm_id = shmget(key, N * sizeof(int), IPC_CREAT | 0777)) < 0) {
        perror("shmget");
        printf("You must have pressed Ctrl-C in previous execution.\n");
        printf("Please restart or change the key.\n");
        exit(1);
    }

    /* Attach the segment to process' data space. */
    if ((graph = shmat(shm_id, NULL, 0)) == (int *) -1) {
        perror("shmat");
        exit(1);
    }

    /* Initialize graph*/
    for (i = 0; i < N; ++i)
    {
        graph[i]  = -1;
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

    while(time(NULL) < t_time + 2 ) {
        if(deadlock()) {
            printf("Parent detects deadlock, going to initiate recovery.\n");
            preempt();
        }
        // else {
        //     printf("deadlock not found\n");
        //     for (i = 0; i < N; ++i)
        //     {
        //         printf("R: %d P: %d\n", i, graph[i]);
        //     }
        // }
        sleep(1);
    }

    int status;
    for (i = 0; i < N; ++i)
    {
        wait(&status);
    }

    /* Detach the shared memory segment. */
    if(shmdt(graph) < 0) {
        perror("shmdt");
        exit(1);
    }

    /* Free and delete the memory segment. */
    if(shmctl(shm_id, IPC_RMID, 0) < 0)
    {
        perror("shmctl");
        exit(1);
    }

    /* Free the semaphores. */
    for (i = 0; i < N; ++i)
    {
        semctl(readmutex[i], 0, IPC_RMID, 0);
    }

    for (i = 0; i < N; ++i)
    {
    	semctl(S[i], 0, IPC_RMID, 0);
    }

    /* Free allocated memories. */
    free(readmutex);
    free(S);
	return 0;
}

void philospher(int id) {
	 /* Attach the segment to process' data space. */
    if ((graph = shmat(shm_id, NULL, 0)) == (int *) -1) {
        perror("shmat");
        exit(1);
    }


    printf("Philosopher %d is thinking.\n", id);
    while(time(NULL) < t_time)
    {
        take_fork(id, LEFT);
        take_fork(id, id);

        P(readmutex[LEFT]);
        if(graph[LEFT] != -1) {
            V(readmutex[LEFT]);
            printf("Philosopher %d is eating.\n", id);
            put_fork(id, LEFT);
        }
        else V(readmutex[LEFT]);

        put_fork(id, id);

        printf("Philosopher %d is thinking.\n", id);
    }

    /* Detach the shared memory segment. */
    if(shmdt(graph) < 0) {
    	perror("shmdt");
        exit(1);
    }
}

void take_fork(int id, int r_id)
{
    P(S[r_id]);

    P(readmutex[r_id]);
    graph[r_id] = id; /* Assign the corresponding resource in the resource graph.*/
    V(readmutex[r_id]);

    printf("Philosopher %d takes fork %d", id, r_id);    
    if(r_id == LEFT) {
        printf(" (left).\n");    
    }
    else if(r_id == id) {
        printf(" (right).\n");
    }
    else {
        printf(".\n");
    }
    sleep(2);
}

void put_fork(int id, int r_id)
{
    V(S[r_id]);

    P(readmutex[r_id]);
    graph[r_id] = -1; /* Free the corresponding resource in the resource graph.*/
    V(readmutex[r_id]);

    printf("Philosopher %d puts fork %d", id, r_id);
    if(r_id == LEFT) {
        printf(" (left) down.\n");
    }
    else if(r_id == id) {
        printf(" (right) down.\n");
    }
    else {
        printf(" down.\n");
    }
}

int deadlock() {
    int id, result = 1;

    for (id = 0; id < N; ++id)
    {
        P(readmutex[id]);
        if(graph[id] != RIGHT) {
            V(readmutex[id]);
            result = 0;
            break;
        }
        else V(readmutex[id]);
    }

    return result;
}

void preempt() {
    int id;

    id = rand() % N;

    V(S[id]);

    P(readmutex[id]);
    graph[id] = -1;
    V(readmutex[id]);


    printf("Parent preempts Philosopher %d\n", RIGHT);
}

void free_resources(int sig) {
    int i;
    /* Detach the shared memory segment. */
    if(shmdt(graph) < 0) {
        perror("shmdt");
    }

    /* Free and delete the memory segment. */
    if(shmctl(shm_id, IPC_RMID, 0) < 0)
    {
        perror("shmctl");
    }

    /* Free the semaphores. */
    for (i = 0; i < N; ++i)
    {
        semctl(readmutex[i], 0, IPC_RMID, 0);
    }

    for (i = 0; i < N; ++i)
    {
        semctl(S[i], 0, IPC_RMID, 0);
    }

    /* Free allocated memories. */
    free(readmutex);
    free(S);
    exit(0);
}