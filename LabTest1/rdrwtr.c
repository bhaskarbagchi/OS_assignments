// Name: Utkarsh Jaiswal
// Roll No.: 11CS30038

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

struct sembuf pop, vop; /* sembuf structure. */
char* FILENAME = "buffer.txt"; // Filename for the shared buffer.
int MAX; // Maximum value which the writer can write.

int mutex_wrt; // mutex for writing.
int mutex_read; // mutex for accessing readcount.
int shmid_readcount; // shmid for readcount
int* readptr; // pointer for readcount.
#define READCOUNT (*readptr)
int VAR;

void reader();
void writer();

int main(int argc, char const *argv[])
{
	
	printf("Enter the Maximum value (MAX): ");
	scanf("%d", &MAX);
	while( MAX < 1 || MAX > 50) {
		printf("Range should be [1-50]\n");
		printf("Enter the Maximum value (MAX): ");
		scanf("%d", &MAX);
	}

    int pid1 = 0, pid2 = 0;

	/* Set up the sembuf structure for P(s) and V(s) operations. */
	pop.sem_num = vop.sem_num = 0;
	pop.sem_flg = vop.sem_flg = 0;
	pop.sem_op = -1 ; vop.sem_op = 1;

	// Setup the semaphores.
	if((mutex_wrt = semget(IPC_PRIVATE, 1, 0777|IPC_CREAT)) < 0) {
		perror("semget");
		exit(1);
	}
	semctl(mutex_wrt, 0, SETVAL, 1); // Initialize mutex_read = 1;

	if((mutex_read = semget(IPC_PRIVATE, 1, 0777|IPC_CREAT)) < 0) {
		perror("semget");
		exit(1);
	}
	semctl(mutex_read, 0, SETVAL, 1); // Initialize mutex_read = 1;

	/* Setup the readcount shared memory. */
	key_t key;
	key = ftok("/tmp/", 1234);

	/* Create the segment. */
    if ((shmid_readcount = shmget(key, sizeof(int), IPC_CREAT | 0777)) < 0) {
        perror("shmget");
        exit(1);
    }

    /* Attach the segment to process' data space. */
    if ((readptr = shmat(shmid_readcount, NULL, 0)) == (int *) -1) {
        perror("shmat");
        exit(1);
    }

    READCOUNT = 0; // Initialize the readcount to 0.

    /* Detach the shared memory segment. */
    if(shmdt(readptr) < 0) {
        perror("shmdt");
        exit(1);
    }

    if((pid1 = fork()) == 0) {
    	reader();
    	exit(0);
    }
    if((pid2 = fork()) == 0) {
    	reader();
    	exit(0);
    }

    writer();	

    int status;
    wait(&status);
    wait(&status);

    // Free the semaphores.
    semctl(mutex_wrt, 0, IPC_RMID, 0);
    semctl(mutex_read, 0, IPC_RMID, 0);

    // Free the shared memory.
    shmctl(shmid_readcount, IPC_RMID, 0);

	return 0;
}

void reader() {
	usleep(1000);
	int pid = getpid();

	// Shared buffer for reading (file).
	FILE *fp;

	/* Attach the segment to process' data space. */
    if ((readptr = shmat(shmid_readcount, NULL, 0)) == (int *) -1) {
        perror("shmat");
        exit(1);
    }

	VAR = 0;
	while(VAR != MAX) {
		P(mutex_read);
		READCOUNT++;
		if(READCOUNT == 1) {
			printf("First Reader enters.\n");
			P(mutex_wrt);
		}
		V(mutex_read);
		// do reading

		fp = fopen(FILENAME, "r");
		fscanf(fp, " %d", &VAR);
    	fclose(fp);
		printf("Reader (%d) reads the value %d\n", pid, VAR);
		usleep(1000);
		P(mutex_read);
		READCOUNT--;
		if(READCOUNT == 0) {
			printf("Last Reader leaves.\n");
			V(mutex_wrt);
		}
		V(mutex_read);
		sleep(1);
	}

	/* Detach the shared memory segment. */
    if(shmdt(readptr) < 0) {
        perror("shmdt");
        exit(1);
    }

}

void writer() {
	int pid = getpid();

	// Shared buffer for reading (file).
	FILE *fpw, *fpr;

	VAR = 0;
	fpw = fopen(FILENAME, "w");
	fprintf(fpw, "%d\n", VAR);
	fclose(fpw);
	while(VAR != MAX) {
		P(mutex_wrt);
		printf("Writer enters.\n");
		// do writing

		fpw = fopen(FILENAME, "w");
		fpr = fopen(FILENAME, "r");
		fscanf(fpr, " %d", &VAR);
		usleep(10000);
		VAR++;
		fprintf(fpw, "%d\n", VAR);
		printf("Writer (%d) writes the value %d\n", pid, VAR);
		fclose(fpw);
		fclose(fpr);
		printf("Writer leaves.\n");
		V(mutex_wrt);

		sleep(3);
	}

}