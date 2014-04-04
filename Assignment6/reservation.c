#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <pthread.h>
#include <unistd.h>

#define NO_OF_TRAINS 100
#define CAPACITY_OF_TRAIN 500
#define MAX_CONCURRENT_QUERIES 5
#define NO_OF_WORKER_THREADS 20

#define INQUIRY 0
#define BOOKING 1
#define CANCELLATION 2
#define NONE 3

typedef struct _train{
	int train_no;
	int capacity;
	int no_available_seats;
	int seats[CAPACITY_OF_TRAIN];
} train;

typedef struct _booked_tickets{
	int train_no;
	int seat_no;
	struct _booked_tickets *next;
} booked_tickets;

typedef struct _table{
	int train_no;
	int query_type;
	int no_of_threads;
} table;

/* Function prototypes */
void initialize_trains();
void initialize_mutexes();
void *worker(void *arg);
void create_worker_threads();

/* Global variables */
table Table[MAX_CONCURRENT_QUERIES];
train Train[NO_OF_TRAINS];
pthread_mutex_t conc_query_count_mutex;
int conc_query_count = 0;
pthread_mutex_t query_mutex[MAX_CONCURRENT_QUERIES];
pthread_t threads[NO_OF_WORKER_THREADS];
pthread_mutex_t table_mutex;
pthread_cond_t max_cond;
pthread_cond_t is_cond[MAX_CONCURRENT_QUERIES];

/* Master Thread */
int main(int argc, char* argv[]){

	srand(time(NULL));
	/* Initialize trains */
	printf("Initializing train information.\n");
	initialize_trains();

	/* Initialize mutexes */
	printf("Creating mutexes.\n");
	initialize_mutexes();

	/* Create worker threads */
	printf("Creating worker threads.\n");
	create_worker_threads();

	/* Wait for all threads to complete */
	printf("Joining workers to master.\n");
	printf("Waiting for workers to complete.\n");
	int i;
	for(i = 0; i < NO_OF_WORKER_THREADS; i++){
		pthread_join(threads[i], NULL);
	}
	return 0;
}

void initialize_trains(){
	int i, j;
	for(i = 0; i<NO_OF_TRAINS; i++){
		Train[i].train_no = 10000 + i;
		Train[i].capacity = CAPACITY_OF_TRAIN;
		Train[i].no_available_seats = CAPACITY_OF_TRAIN;
		for(j = 0; j<CAPACITY_OF_TRAIN; j++){
			Train[i].seats[j] = 0; 							/* 0 indicates available and 1 indicates booked */
		}
	}
	for(i = 0; i < MAX_CONCURRENT_QUERIES; i++){
		Table[i].train_no = -1;
		Table[i].query_type = NONE;
		Table[i].no_of_threads = 0;
	}
	return;
}

void initialize_mutexes(){
	int i;
	for(i = 0; i<MAX_CONCURRENT_QUERIES; i++){
		pthread_mutex_init(&query_mutex[i], NULL);
		pthread_mutex_trylock(&query_mutex[i]);
		pthread_mutex_unlock(&query_mutex[i]);
	}
	pthread_mutex_init(&conc_query_count_mutex, NULL);
	pthread_mutex_trylock(&conc_query_count_mutex);
	pthread_mutex_unlock(&conc_query_count_mutex);
	pthread_mutex_init(&table_mutex, NULL);
	pthread_mutex_trylock(&table_mutex);
	pthread_mutex_unlock(&table_mutex);
	pthread_cond_init(&max_cond, NULL);
	for(i = 0; i<MAX_CONCURRENT_QUERIES; i++){
		pthread_cond_init(&is_cond[i], NULL);
	}
}

void *worker(void *arg){
	sleep(3);
	int tno = (int)arg + 1;
	booked_tickets *booked = NULL, *head, *temp;
	booked = (booked_tickets *)malloc(sizeof(booked_tickets));
	booked->train_no = 0;
	booked->seat_no = 0;
	booked->next = NULL;
	head = booked;
	int no_seats_booked = 0;
	while(1){
		int query = rand()%3;
		int train_no = rand()%100 + 10000;
		int no_of_seats = rand()%5 + 1;
		int cancel;
		int seat_no;
		if(no_seats_booked>0){
			cancel = (rand()%no_seats_booked) + 1;
		}
		switch(query){
			case INQUIRY: 		{
									int i, found = 0;
									pthread_mutex_lock(&table_mutex);
									for(i = 0; i<MAX_CONCURRENT_QUERIES; i++){
										if(Table[i].train_no == train_no){
											found = 1;
											break;
										}
									}
									if(found == 1 && Table[i].query_type == INQUIRY){
										Table[i].no_of_threads++;
										pthread_mutex_unlock(&table_mutex);
										no_of_seats = Train[train_no - 10000].no_available_seats;
										sleep(rand()%5);
										printf("[%d]\tQuery: INQUIRY\tTrain no = %d\n", tno, train_no);
										printf("[%d]\tTrain no %d has %d available seats.\n", tno, train_no, no_of_seats);
										pthread_mutex_lock(&table_mutex);
										Table[i].no_of_threads--;
										if(Table[i].no_of_threads == 0){
											Table[i].train_no = -1;
											Table[i].query_type = NONE;
											pthread_mutex_lock(&conc_query_count_mutex);
											conc_query_count--;
											pthread_cond_signal(&max_cond);
											pthread_cond_signal(&is_cond[i]);
											pthread_mutex_unlock(&conc_query_count_mutex);
										}
										pthread_mutex_unlock(&table_mutex);
									}
									else{
										pthread_mutex_unlock(&table_mutex);
										//Use conditional wait for Max_count_conc_query and create a new table entry
										pthread_mutex_lock(&conc_query_count_mutex);
										if(conc_query_count >= MAX_CONCURRENT_QUERIES){
											printf("[%d]\tWaiting because MAX_CONCURRENT_QUERIES running.\n", tno);
											pthread_cond_wait(&max_cond, &conc_query_count_mutex);
										}
										pthread_mutex_lock(&table_mutex);
										for (i = 0; i < MAX_CONCURRENT_QUERIES; i++){
											if(Table[i].train_no = -1)
												break;
										}
										Table[i].train_no = train_no;
										Table[i].query_type = INQUIRY;
										Table[i].no_of_threads = 1;
										pthread_mutex_unlock(&table_mutex);
										conc_query_count++;
										pthread_mutex_unlock(&conc_query_count_mutex);
										no_of_seats = Train[train_no - 10000].no_available_seats;
										printf("[%d]\tSearching database for train no %d\n", tno, train_no);
										sleep(rand()%5);
										printf("[%d]\tQuery: INQUIRY\tTrain no = %d\n", tno, train_no);
										printf("[%d]\tTrain no %d has %d available seats.\n", tno, train_no, no_of_seats);
										pthread_mutex_lock(&table_mutex);
										Table[i].no_of_threads--;
										if(Table[i].no_of_threads == 0){
											Table[i].train_no = -1;
											Table[i].query_type = NONE;
											pthread_mutex_lock(&conc_query_count_mutex);
											conc_query_count--;
											pthread_cond_signal(&max_cond);
											pthread_cond_signal(&is_cond[i]);
											pthread_mutex_unlock(&conc_query_count_mutex);
										}
										pthread_mutex_unlock(&table_mutex);
									}
									break;
								}

			case BOOKING:		{
									int i;
									pthread_mutex_lock(&table_mutex);
									for (i = 0; i < MAX_CONCURRENT_QUERIES; ++i){
										if(Table[i].train_no == train_no){
											printf("[%d]\tWaiting beacuse some transaction is already running on train no %d\n", tno, train_no);
											pthread_cond_wait(&is_cond[i], &table_mutex);
										}
									}
									pthread_mutex_unlock(&table_mutex);
									//booking(train_no, no_of_seats, booked);
									printf("[%d]\tQuery: BOOKING\tTrain no = %d\tNo of seats = %d\n", tno, train_no, no_of_seats);
									pthread_mutex_lock(&conc_query_count_mutex);
									if(conc_query_count >= MAX_CONCURRENT_QUERIES){
										printf("[%d]\tWaiting for booking beacuse MAX_CONCURRENT_QUERIES are running.\n", tno);
										pthread_cond_wait(&max_cond, &conc_query_count_mutex);
									}
									pthread_mutex_lock(&table_mutex);
									for(i = 0; i<MAX_CONCURRENT_QUERIES; i++){
										if(Table[i].train_no == -1)
											break;
									}
									Table[i].train_no = train_no;
									Table[i].query_type = BOOKING;
									Table[i].no_of_threads = 1;
									pthread_mutex_unlock(&table_mutex);
									conc_query_count++;
									pthread_mutex_unlock(&conc_query_count_mutex);
									int k,j = 0;
									if(Train[train_no-10000].no_available_seats < no_of_seats){
										printf("[%d]\tBooking request %d seats in train no %d.\nThread %d : Only %d seats are available.\nThread %d : Abort\n", tno, seat_no,train_no,tno, Train[train_no].no_available_seats, tno);
									}
									char pp[1024];
									sprintf(pp, "[%d]\t\tBooking seat no ", tno);
									booked = head;
									while(booked->next != NULL)
										booked = booked->next;
									for(k = 0; k < CAPACITY_OF_TRAIN; k++){
										if(Train[train_no-10000].seats[k] == 0){
											Train[train_no-10000].no_available_seats--;
											Train[train_no-10000].seats[k] = 1;
											temp = (booked_tickets *)malloc(sizeof(booked_tickets));
											temp->train_no = train_no;
											temp->seat_no = k;
											temp->next = NULL;
											booked->next = temp;
											booked = temp;
											j++;
											no_seats_booked++;
											sprintf(pp, "%s%d\t", pp, k+1);
											if(j == no_of_seats)
												break;
										}
									}
									printf("%s\n", pp);
									printf("[%d]\t\tWainitg for bank transaction.\n", tno);
									sleep(rand()%5);
									printf("[%d]\tBooking completed.\n", tno);
									pthread_mutex_lock(&table_mutex);
									Table[i].train_no = -1;
									Table[i].query_type = NONE;
									Table[i].no_of_threads = 0;
									pthread_mutex_lock(&conc_query_count_mutex);
									conc_query_count--;
									pthread_cond_signal(&max_cond);
									pthread_cond_signal(&is_cond[i]);
									pthread_mutex_unlock(&conc_query_count_mutex);
									pthread_mutex_unlock(&table_mutex);
									break;
								}

			case CANCELLATION:	{
									printf("[%d]\tQuery: CANCELLATION\n", tno);
									if(no_seats_booked == 0){
										printf("[%d]\tNo seats booked by this thread yet!.\n", tno);
										break;
									} 
									int i;
									temp = head;
									for(i = 1; i<cancel-1; i++)
										temp = temp->next;
									train_no = temp->next->train_no;
									seat_no = temp->next->seat_no;
									printf("[%d]\tQuery: CANCELLATION\tTrain no = %d\tSeat no = %d\n", tno, train_no, seat_no);
									//cancellation(train_no, seat_no);
									pthread_mutex_lock(&table_mutex);
									for(i = 0; i<MAX_CONCURRENT_QUERIES; i++){
										if(Table[i].train_no == train_no){
											printf("[%d]\tWainitg because some other query is already running in onn train no %d\n", tno, train_no);
											pthread_cond_wait(&is_cond[i], &table_mutex);
										}
									}
									pthread_mutex_unlock(&table_mutex);
									printf("[%d]\tQuery: CANCELLATION\tTrain no = %d\tSeat no = %d\n", tno, train_no, seat_no);
									pthread_mutex_lock(&conc_query_count_mutex);
									if(conc_query_count >= MAX_CONCURRENT_QUERIES){
										printf("[%d]\tWaiting for cancellation beacuse MAX_CONCURRENT_QUERIES are running.\n", tno);
										pthread_cond_wait(&max_cond, &conc_query_count_mutex);
									}
									pthread_mutex_lock(&table_mutex);
									for(i = 0; i<MAX_CONCURRENT_QUERIES; i++){
										if(Table[i].train_no == -1)
											break;
									}
									Table[i].train_no = train_no;
									Table[i].query_type = BOOKING;
									Table[i].no_of_threads = 1;
									pthread_mutex_unlock(&table_mutex);
									conc_query_count++;
									no_seats_booked--;
									booked_tickets *x;
									x = temp->next;
									temp->next = temp->next->next;
									free(x);
									pthread_mutex_unlock(&conc_query_count_mutex);
									int j;
									if(Train[train_no-10000].seats[seat_no] == 1){
										Train[train_no-10000].seats[seat_no] = 0;
									}
									printf("[%d]\t\tWaiting for bank refund.\n", tno);
									sleep(rand()%5);
									printf("[%d]\tTicket cancelled. Train no. %d Seat no. %d\n", tno, train_no, seat_no);
									pthread_mutex_lock(&table_mutex);
									Table[i].train_no = -1;
									Table[i].query_type = NONE;
									Table[i].no_of_threads = 0;
									pthread_mutex_lock(&conc_query_count_mutex);
									conc_query_count--;
									pthread_cond_signal(&max_cond);
									pthread_cond_signal(&is_cond[i]);
									pthread_mutex_unlock(&conc_query_count_mutex);
									pthread_mutex_unlock(&table_mutex);
									break;
								}
		}
		sleep(5);
	}
}

void create_worker_threads(){
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

	int i;

	for(i = 0; i<NO_OF_WORKER_THREADS; i++){
		if(pthread_create(&threads[i], &attr, worker, (void *)i)){
			printf("Master couldn't create worker thread. Exiting...\n");
			exit(0);
		}
		printf("Worker %d created.\n", i+1);
	}
}

