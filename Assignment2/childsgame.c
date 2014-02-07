#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#define ON_C_WINS SIGUSR1
#define ON_D_WINS SIGUSR2
#define ON_REQUEST SIGIO
#define MAX 1
#define MIN 0
#define BUFFSIZ 1000

/* Function prototypes */
void C_wins_Handler(int n);
void D_wins_Handler(int n);
void C_request_Handler(int n);
void D_request_Handler(int n);

/* Global variables */
int C_WINS = 0;
int D_WINS = 0;
int fd_C[2];
int fd_D[2];

/* Main */
int main(int argc, char *argv[])
{
	int pid_C, pid_D;

	if(pipe(fd_C) == -1) {
		printf("Sorry!!The game cannot start without proper communication with child C.\n");
		return 0;
	}
	if(pipe(fd_D) == -1) {
		printf("Sorry!!The game cannot start without proper communication with child D.\n");
		return 0;
	}

	switch(pid_C = fork()) {
		case 0: // Child C
			// Register necessary signals
			signal(ON_C_WINS, C_wins_Handler);
			signal(ON_D_WINS, D_wins_Handler);
			signal(ON_REQUEST, C_request_Handler);

			srand((long int) &pid_C);

			close(fd_D[0]);close(fd_D[1]); // Do not interfere with the communication of parent with pipe D.
			close(fd_C[0]); // Close the reading end.
			// Play game until someone wins
			while( (!C_WINS) && (!D_WINS) ) {
			}
			if(C_WINS) {
				printf("C (PID=%d): Yeah!! I won.\n", getpid());
			}
			else {
				printf("C (PID=%d): Congratulations D!! You won.\n", getpid());
			}
			close(fd_C[1]); // Close the writing end.
			break;
		default:
			switch(pid_D = fork()) {
				case 0: // Child D
					// Register necessary signals
					signal(ON_C_WINS, C_wins_Handler);
					signal(ON_D_WINS, D_wins_Handler);
					signal(ON_REQUEST, D_request_Handler);

					close(fd_C[0]);close(fd_C[1]); // Do not interfere with the communication of parent with pipe C.
					close(fd_D[0]); // Close the reading end.

					srand((long int) &pid_D);

					// Play game until someone wins
					while( (!C_WINS) && (!D_WINS) ) {
					}
					if(D_WINS) {
						printf("D (PID=%d): Yeah!! I won.\n", getpid());
					}
					else {
						printf("D (PID=%d): Congratulations C!! You won.\n", getpid());
					}
					close(fd_D[1]); // Close the writing end.
					break;
				default: // Parent
					close(fd_C[1]);close(fd_D[1]); // Close all writing ends.

					usleep(5000); // Wait for child processes to register signals.

					int points_C = 0, points_D = 0, flag, value_C, value_D, num;
					char buffer[1024];

					srand(time(NULL));

					while( (points_C < 10) && (points_D < 10)) {
						// Choose a flag.
						flag = rand() % 2;
						// Get the values from children.
						kill(pid_C, ON_REQUEST);
						kill(pid_D, ON_REQUEST);
						num = read(fd_C[0], buffer, BUFFSIZ);
						buffer[num] = '\0';
						value_C = atoi(buffer);
						num = read(fd_D[0], buffer, BUFFSIZ);
						buffer[num] = '\0';
						value_D = atoi(buffer);
						printf("Parent (PID=%d): C chose %d and D chose %d.\n", getpid(), value_C, value_D);
						if(flag == MAX) {
							printf("Parent (PID=%d): Flag for this round is MAX.\n", getpid());
							if(value_C > value_D) {
								points_C++;
								printf("Parent (PID=%d): C wins this round.\n", getpid());
							}
							else if(value_D > value_C) {
								points_D++;
								printf("Parent (PID=%d): D wins this round.\n", getpid());
							} 
						}
						else {
							printf("Parent (PID=%d): Flag for this round is MIN.\n", getpid());
							if(value_C < value_D) {
								points_C++;
								printf("Parent (PID=%d): C wins this round.\n", getpid());
							}
							else if(value_D < value_C) {
								points_D++;
								printf("Parent (PID=%d): D wins this round.\n", getpid());
							}
						}
					}
					// Decide the winner.
					printf("Parent (PID=%d): Final score is C: %d and D: %d\n", getpid(), points_C, points_D);
					if(points_C == 10) {
						kill(pid_C, ON_C_WINS);
						kill(pid_D, ON_C_WINS);
						printf("Parent (PID=%d): Congratulations C!! You win.\n", getpid());
					}
					else {
						kill(pid_C, ON_D_WINS);
						kill(pid_D, ON_D_WINS);
						printf("Parent (PID=%d): Congratulations D!! You win.\n", getpid());
					}
					close(fd_C[0]);close(fd_D[0]); // Close all reading ends.
					// Remove the child processes.
					wait(&num);
					wait(&num);
					break;
				case -1:
					printf("Sorry!!The game cannot start without child D.\n");
					break;
			}
			break;
		case -1:
			printf("Sorry!!The game cannot start without child C.\n");
			break;
	}



	return 0;
}

void C_wins_Handler(int n) {
	C_WINS = 1;
}

void D_wins_Handler(int n) {
	D_WINS = 1;
}

void C_request_Handler(int n) {
	char buffer[1024];
	sprintf(buffer, "%d", rand());
	printf("C (PID=%d): I choose %s.\n", getpid(), buffer);
	write(fd_C[1], buffer, strlen(buffer));
}

void D_request_Handler(int n) {
	char buffer[1024];
	sprintf(buffer, "%d", rand());
	printf("D (PID=%d): I choose %s.\n", getpid(), buffer);
	write(fd_D[1], buffer, strlen(buffer));
}
