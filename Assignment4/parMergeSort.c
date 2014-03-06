#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <time.h>

#define BUFF_SZ 2

void parallelMergeSort(int* shm_arr,  int shm_id, int l, int ML, int ms, int i, int j);
void bubbleSort(int* shm_arr, int i, int j);

int main(int argc, char *argv[])
{
	int i, n, ms, ML, shm_id, *shm_arr;
	key_t key;

	printf("Enter size of the array (n): ");
	scanf("%d", &n);
	while(n < 0) {
		printf("\tError: n should be negative!!\n");
		printf("Enter size of the array (n): ");
		scanf("%d", &n);
	}

	printf("Enter maximum length of the tree (ML): ");
	scanf("%d", &ML);
	while(ML < 0) {
		printf("\tError: ML should be negative!!\n");
		printf("Enter maximum length of the tree (ML): ");
		scanf("%d", &ML);
	}

	printf("Enter maximum size of the array chunk (ms): ");
	scanf("%d", &ms);
	while(ms < 0) {
		printf("\tError: ms should be negative!!\n");
		printf("Enter maximum size of the array chunk (ms): ");
		scanf("%d", &ms);
	}

	key = ftok("/usr/local/lib/", 0);

	 /* Create the segment. */
    if ((shm_id = shmget(key, n, IPC_CREAT | 0666)) < 0) {
        perror("shmget");
        exit(1);
    }

    /* Attach the segment to parent process' data space. */
    if ((shm_arr = shmat(shm_id, NULL, 0)) == (int *) -1) {
        perror("shmat");
        exit(1);
    }

    printf("Unsorted array:\n");
    /* Randomly generate n integers. */
    srand(time(NULL));
    for (i = 0; i < n; ++i)
    {
    	shm_arr[i] = rand() % 10000;
    	printf("%d ", shm_arr[i]);
    }
    printf("\n");

    /* Sort the array. */
	// printf("Processes: \n");
	// printf("\t root: [%d]\n", getpid());
    parallelMergeSort(shm_arr, shm_id, 0, ML, ms, 0, n-1);

    /* Print the array values. */
    printf("Sorted array:\n");
    for (i = 0; i < n; ++i)
    {
    	printf("%d ", shm_arr[i]);
    }
    printf("\n");

    /* Detach the shared memory segment. */
    if(shmdt(shm_arr) < 0) {
    	perror("shmdt");
        exit(1);
    }

    /* Free and delete the memory segment. */
    if(shmctl(shm_id, IPC_RMID, 0) < 0)
    {
        perror("shmctl");
        exit(1);
    }

	return 0;
}

void parallelMergeSort(int* shm_arr, int shm_id, int l, int ML, int ms, int i, int j) {

	if( l > ML || j - i + 1 < ms ) {
		bubbleSort(shm_arr, i, j);
		return;
	}

	int k, pid_P = 0, pid_Q = 0, fd_P_Q_1[2], fd_Q_P_1[2], fd_P_Q_2[2], fd_Q_P_2[2];

	/* Pipes for communication between process P and process Q. */
	if(pipe(fd_P_Q_1) < 0) {
		perror("pipe");
		exit(1);
	}

	if(pipe(fd_Q_P_1) < 0) {
		perror("pipe");
		exit(1);
	}

	if(pipe(fd_P_Q_2) < 0) {
		perror("pipe");
		exit(1);
	}

	if(pipe(fd_Q_P_2) < 0) {
		perror("pipe");
		exit(1);
	}

	/* Get the middle index. */
	k = i + ( j - i ) / 2;

	switch(pid_P = fork()) {
		case 0: // Child process P
		{
			/* Close appropriate read and write ends. */
			close(fd_P_Q_1[0]);
			close(fd_Q_P_1[1]);
			close(fd_P_Q_2[0]);
			close(fd_Q_P_2[1]);

			/* Buffer for reading and writing. */
			char buffer_P[BUFF_SZ]; 
			int num_P;

			/* Attach the segment to process P's data space. */
		    if ((shm_arr = shmat(shm_id, NULL, 0)) == (int *) -1) {
		        perror("shmat");
		        exit(1);
		    }

		    /* Sort first half of the array. */
		    parallelMergeSort(shm_arr, shm_id, l + 1, ML, ms, i, k);

		    /* Inform Q that the first half has been sorted. */
		    sprintf(buffer_P, "1");							// 1 => Sorting half array is complete.
		    write(fd_P_Q_1[1], buffer_P, 1);

		    /* Wait for process Q to complete sorting. */
		    buffer_P[0] = 0;
		    num_P = read(fd_Q_P_1[0], buffer_P, BUFF_SZ);
		    buffer_P[num_P] = '\0';
		    while(*buffer_P != '1') {
		    	num_P = read(fd_Q_P_1[0], buffer_P, BUFF_SZ);
		    	buffer_P[num_P] = '\0';
		    }

		    /* Merge from left to right in a local array. */
		    int u, v, w, arr_P[k - i + 1];
		    u = i; v = k + 1; w = 0;

		    while( w <= k - i ) {
		    	if( v <= j && shm_arr[v] < shm_arr[u]) {
		    		arr_P[w] = shm_arr[v];
		    		w++;
		    		v++;
		    	}
		    	else {
		    		arr_P[w] = shm_arr[u];
		    		w++;
		    		u++;
		    	}
		    }

		    /* Inform Q that the first half is ready to get merged. */
		    sprintf(buffer_P, "2");							// 2 => Sorted first half array is ready.
		    write(fd_P_Q_2[1], buffer_P, 1);

		    /* Wait till process Q completes the merging process. */
		    buffer_P[0] = 0;
		    num_P = read(fd_Q_P_2[0], buffer_P, BUFF_SZ);
		    buffer_P[num_P] = '\0';
		    while(*buffer_P != '2') {
		    	num_P = read(fd_Q_P_2[0], buffer_P, BUFF_SZ);
		    	buffer_P[num_P] = '\0';
		    }

		    /* Copy the local array to shared memory. */
		    for (w = 0; w <= k - i; ++w)
		    {
		    	shm_arr[i + w] = arr_P[w];
		    }

		    /* Detach the shared memory segment. */
		    if(shmdt(shm_arr) < 0) {
		    	perror("shmdt");
		        exit(1);
		    }

		    /* Close the file descriptors. */
		    close(fd_P_Q_1[1]);
			close(fd_Q_P_1[0]);
		    close(fd_P_Q_2[1]);
			close(fd_Q_P_2[0]);


		    /* Exit process P. */
			exit(0);
		}
		case -1:
		{
			perror("fork");
			exit(1);
		}
		default:
		{
			switch(pid_Q = fork()) {
				case 0: // Child process Q
				{
					/* Close appropriate read and write ends. */
					close(fd_Q_P_1[0]);
					close(fd_P_Q_1[1]);
					close(fd_Q_P_2[0]);
					close(fd_P_Q_2[1]);

					/* Buffer for reading and writing. */
					char buffer_Q[BUFF_SZ];
					int num_Q;

					/* Attach the segment to process Q's data space. */
					if ((shm_arr = shmat(shm_id, NULL, 0)) == (int *) -1) {
				        perror("shmat");
				        exit(1);
				    }

				    /* Sort second half of the array. */
		    		parallelMergeSort(shm_arr, shm_id, l + 1, ML, ms, k + 1, j);

		    		/* Inform P that the second half has been sorted.*/
				    sprintf(buffer_Q, "1");							// 1 => Sorting half array is complete.
				    write(fd_Q_P_1[1], buffer_Q, 1);

				    /* Wait for process P to complete sorting. */
				    buffer_Q[0] = 0;

				    num_Q = read(fd_P_Q_1[0], buffer_Q, BUFF_SZ);
				    buffer_Q[num_Q] = '\0';

				    while(*buffer_Q != '1') {
				    	num_Q = read(fd_P_Q_1[0], buffer_Q, BUFF_SZ);
				    	buffer_Q[num_Q] = '\0';
				    }

				    /* Merge from right to left in a local array. */
				    int a, b, c, arr_Q[j - k];
				    a = k; b = j; c = j - k - 1;
				    
				    while ( c >= 0 ) {
				    	if( b >= (k + 1) && shm_arr[b] > shm_arr[a] ) {
				    		arr_Q[c] = shm_arr[b];
				    		b--;
				    		c--;
				    	}
				    	else {
				    		arr_Q[c] = shm_arr[a];
				    		a--;
				    		c--;
				    	}
				    }

				    /* Inform P that the second half is sorted.*/
				    sprintf(buffer_Q, "2");							// 2 => Sorted first half array is ready.
				    write(fd_Q_P_2[1], buffer_Q, 1);

					/* Wait till process P completes the merging process. */
					buffer_Q[0] = 0;

				    num_Q = read(fd_P_Q_2[0], buffer_Q, BUFF_SZ);
				    buffer_Q[num_Q] = '\0';

				    while(*buffer_Q != '2') {
				    	num_Q = read(fd_P_Q_2[0], buffer_Q, BUFF_SZ);
				    	buffer_Q[num_Q] = '\0';
				    }

				    /* Copy the local array to shared memory. */
				    for (c = j - k - 1; c >= 0; --c)
				    {
				    	shm_arr[c + k + 1] = arr_Q[c];
				    }

				    /* Detach the shared memory segment. */
				    if(shmdt(shm_arr) < 0) {
				    	perror("shmdt");
				        exit(1);
				    }

				    /* Close the file descriptors. */
				    close(fd_Q_P_1[1]);
					close(fd_P_Q_1[0]);
					close(fd_Q_P_2[1]);
					close(fd_P_Q_2[0]);
				    
				    /* Exit process Q. */
					exit(0);
				}	
				case -1:
				{
					perror("fork");
					exit(1);
				}
				default: // Parent
				{
					close(fd_P_Q_1[0]);close(fd_P_Q_1[1]);
					close(fd_Q_P_1[0]);close(fd_Q_P_1[1]);
					close(fd_P_Q_2[0]);close(fd_P_Q_2[1]);
					close(fd_Q_P_2[0]);close(fd_Q_P_2[1]);

					int status;
					// printf("\t[%d] [%d]\n", pid_P, pid_Q);
					/* Wait for the child processes to end. */
					waitpid(pid_P, &status, 0);
					waitpid(pid_Q, &status, 0);
				}
				break;
			}	
		}
		break;
	}
}

void bubbleSort(int* arr, int i, int j)
{
 
 	int c, d, t;
	for (c = 0 ; c < j - i + 1; c++)
	{
		for (d = i ; d < j - c; d++)
		{
			if (arr[d] > arr[d+1])
			{
				/* Swapping */
				t = arr[d];
				arr[d] = arr[d+1];
				arr[d+1] = t;
			}
		}
	}
}