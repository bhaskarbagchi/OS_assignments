#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct _LL{
	int data;
	struct _LL *next;
}LL;

typedef struct _LL_count{
	int data;
	int count;
	struct _LL_count *next;
}LL_count;

int simulateFIFO(char* file){
	FILE *fp = fopen(file, "r");
	int no_pages, n, next_page;
	int count = 0, page_fault = 0, found = 0;
	LL *head = NULL, *tail = NULL, *temp;
	fscanf(fp, "%d", &no_pages);
	fscanf(fp, "%d", &n);
	int i;
	for(i = 0; i<no_pages; i++){
		fscanf(fp, "%d", &next_page);
		found = 0;
		if(head == NULL && tail == NULL){
			temp = (LL *)malloc(sizeof(LL));
			temp->data = next_page;
			temp->next = NULL;
			count++;
			page_fault++;
			head = tail = temp;
			//printf("Initializing LL\n");
		}
		else{
			//printf("Loop %d \n", i+1);
			temp = head;
			while(temp != NULL){
				if(temp->data == next_page){
					found = 1;
					break;
				}
				temp = temp->next;
			}
			if(found != 1){
				//printf("Not Found in LL\n");
				temp = (LL *)malloc(sizeof(LL));
				temp->data = next_page;
				temp->next = NULL;
				if(count < n){
					//printf("LL not full\n");
					tail->next = temp;
					tail = temp;
					count++;
					page_fault++;
				}
				else{
					//printf("LL full\n");
					LL* t = head;
					head = head->next;
					tail->next = temp;
					tail = temp;
					free(t);
					page_fault++;
				}
			}
			else{
				//printf("Found\n");
				continue;
			}
		}
	}
	fclose(fp);
	return page_fault;
}

int simulateLRU(char* file){
	FILE *fp = fopen(file, "r");
	int no_pages, n, next_page;
	int count = 0, page_fault = 0, found = 0;
	LL *head = NULL, *tail = NULL, *temp;
	fscanf(fp, "%d", &no_pages);
	fscanf(fp, "%d", &n);
	int i;
	for(i = 0; i<no_pages; i++){
		fscanf(fp, "%d", &next_page);
		found = 0;
		if(head == NULL && tail == NULL){
			temp = (LL *)malloc(sizeof(LL));
			temp->data = next_page;
			temp->next = NULL;
			count++;
			page_fault++;
			head = tail = temp;
			//printf("Initializing LL\n");
		}
		else{
			//printf("Loop %d \n", i+1);
			//printf("Head = %d  Tail = %d Next to search = %d\n", head->data, tail->data, next_page);
			temp = head;
			if(count != 1 && head->data == next_page){
				LL *t = head;
				head = head->next;
				tail->next = t;
				tail = t;
				tail->next = NULL;
				found = 1;
			}
			else if(tail->data == next_page){
				found = 1;
				continue;
			}
			else{
				while(temp != NULL){
					if(temp->next != NULL && temp->next->data == next_page){
						LL *t = temp->next;
						temp->next = temp->next->next;
						tail->next = t;
						tail = t;
						t->next = NULL;
						found = 1;
						break;
					}
					temp = temp->next;
				}
			}
			if(found != 1){
				//printf("Not Found in LL\n");
				temp = (LL *)malloc(sizeof(LL));
				temp->data = next_page;
				temp->next = NULL;
				if(count < n){
					//printf("LL not full\n");
					tail->next = temp;
					tail = temp;
					count++;
					page_fault++;
				}
				else{
					//printf("LL full\n");
					LL* t = head;
					head = head->next;
					tail->next = temp;
					tail = temp;
					free(t);
					page_fault++;
				}
			}
			else{
				//printf("Found\n");
				continue;
			}
		}
	}
	fclose(fp);
	return page_fault;	
}

int simulateLFU(char* file){
	FILE *fp = fopen(file, "r");
	int no_pages, n, next_page;
	int count = 0, page_fault = 0, found = 0;
	LL_count *head = NULL, *tail = NULL, *temp;
	fscanf(fp, "%d", &no_pages);
	fscanf(fp, "%d", &n);
	int i;
	for(i = 0; i<no_pages; i++){
		fscanf(fp, "%d", &next_page);
		found = 0;
		if(head == NULL && tail == NULL){
			temp = (LL_count *)malloc(sizeof(LL_count));
			temp->data = next_page;
			temp->count = 1;
			temp->next = NULL;
			count++;
			page_fault++;
			head = tail = temp;
			//printf("Initializing LL\n");
		}
		else{
			//printf("Loop %d \n", i+1);
			temp = head;
			while(temp != NULL){
				if(temp->data == next_page){
					temp->count++;
					found = 1;
					break;
				}
				temp = temp->next;
			}
			if(found != 1){
				//printf("Not Found in LL\n");
				temp = (LL_count *)malloc(sizeof(LL_count));
				temp->data = next_page;
				temp->count = 1;
				temp->next = NULL;
				if(count < n){
					//printf("LL not full\n");
					tail->next = temp;
					tail = temp;
					count++;
					page_fault++;
				}
				else{
					//printf("LL full\n");
					int minimum = head->count;
					LL_count *t = head;
					temp = head;
					while(temp != NULL){
						if(temp->count < minimum){
							t = temp;
							minimum = temp->count;
						}
						temp = temp->next;
					}
					t->data = next_page;
					t->count = 1;
					page_fault++;
				}
			}
			else{
				//printf("Found\n");
				continue;
			}
		}
	}
	fclose(fp);
	return page_fault;	
}

int main(int argc, char* argv[]){
	int count = 0;
	switch(argc){
		case 1:	{
					printf("Command line arguments missing.\n");
					printf("USAGE: ./page <filename> [[FF] [LF] [LR]]\n");
					exit(0);
				}
		case 2:	{
					printf("Page faults in FIFO = %d\n", simulateFIFO(argv[1]));
					printf("Page faults in LRU = %d\n", simulateLRU(argv[1]));
					printf("Page faults in LFU = %d\n", simulateLFU(argv[1]));	
					break;
				}
		case 3:	{
					if(strcmp(argv[2], "FF") == 0){
						printf("Page faults in FIFO = %d\n", simulateFIFO(argv[1]));
					}
					else if(strcmp(argv[2], "LF") == 0){
						printf("Page faults in LFU = %d\n", simulateLFU(argv[1]));	
					}
					else if(strcmp(argv[2], "LR") == 0){
						printf("Page faults in LRU = %d\n", simulateLRU(argv[1]));
					}
					else{
						printf("Command line arguments missing.\n");
						printf("USAGE: ./page <filename> [[FF] [LF] [LR]]\n");
						exit(0);
					}
					break;
				}
		case 4:	{
					if(strcmp(argv[2], "FF") == 0 || strcmp(argv[3], "FF") == 0){
						printf("Page faults in FIFO = %d\n", simulateFIFO(argv[1]));
						count++;
					}
					if(strcmp(argv[2], "LF") == 0 || strcmp(argv[3], "LF") == 0){
						printf("Page faults in LFU = %d\n", simulateLFU(argv[1]));
						count++;
					}
					if(strcmp(argv[2], "LR") == 0 || strcmp(argv[3], "LR") == 0){
						printf("Page faults in LRU = %d\n", simulateLRU(argv[1]));
						count++;
					}
					if(count != 2){
						printf("Command line arguments missing.\n");
						printf("USAGE: ./page <filename> [[FF] [LF] [LR]]\n");
					}
					break;
				}
		case 5:	{
					if(strcmp(argv[2], "FF") == 0 || strcmp(argv[3], "FF") == 0 || strcmp(argv[4], "FF") == 0){
						printf("Page faults in FIFO = %d\n", simulateFIFO(argv[1]));
						count++;
					}
					if(strcmp(argv[2], "LF") == 0 || strcmp(argv[3], "LF") == 0 || strcmp(argv[4], "LF") == 0){
						printf("Page faults in LFU = %d\n", simulateLFU(argv[1]));
						count++;
					}
					if(strcmp(argv[2], "LR") == 0 || strcmp(argv[3], "LR") == 0 || strcmp(argv[4], "LR") == 0){
						printf("Page faults in LRU = %d\n", simulateLRU(argv[1]));
						count++;
					}
					if(count != 3){
						printf("Command line arguments missing.\n");
						printf("USAGE: ./page <filename> [[FF] [LF] [LR]]\n");
					}
					break;
				}
		default:{
					printf("Command line arguments missing.\n");
					printf("USAGE: ./page <filename> [[FF] [LF] [LR]]\n");
					exit(0);
				}
	}
	return 0;
}