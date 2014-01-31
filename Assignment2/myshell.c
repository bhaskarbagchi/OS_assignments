#include <sys/cdefs.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <pwd.h>
#include <grp.h>
#include <time.h>
#include <wait.h>

#define BUFFSIZ 1024

void execute(char **argv);

int main(int argn, char *arg[])
{
	int i, argc; char** argv;
	char buffer[BUFFSIZ], *p;

	argv = (char**) malloc(BUFFSIZ * sizeof(char*) );

	while(1) {
		printf("myshell> ");
		argc = 0;

		if(scanf(" %[^\n]", buffer) == EOF) {
			printf("\n");
			return 0;
		}

		p = strtok(buffer, " ");
		while(p != NULL) {
			argv[argc] = (char*) malloc((strlen(p) + 1) * sizeof(char));
			strcpy(argv[argc], p);
			argc++;
			p = strtok(NULL, " ");
		}

		if(strcmp(argv[0], "cd") == 0) {
			if(chdir(argv[1]) != 0) {
				printf("Error: No such directory.\n");
			}
		}
		else if(strcmp(argv[0], "pwd") == 0) {
			getcwd(buffer, BUFFSIZ - 1);
			printf("%s\n", buffer);
		}
		else if(strcmp(argv[0], "mkdir") == 0) {
			if(mkdir(argv[1], 0775) != 0) {
				printf("Error: Cannot create directory.\n");
			}
		}
		else if(strcmp(argv[0], "rmdir") == 0) {
			if(rmdir(argv[1]) != 0) {
				printf("Error: Cannot remove the directory.\n");
			}
		}
		else if(strcmp(argv[0], "ls") == 0) {
			if(argc > 1) {
				if(strcmp(argv[1], "-l") == 0) {
					DIR *mydir;
				    struct dirent *myfile;
				    struct stat mystat;

				    getcwd(buffer, BUFFSIZ - 1);
				    mydir = opendir(buffer);
				    while((myfile = readdir(mydir)) != NULL)
				    {
				    	if(myfile->d_name[0] == '.') {
				        	continue;
				        }
				        stat(myfile->d_name, &mystat);
				        // Permissions
				        printf( (S_ISDIR(mystat.st_mode)) ? "d" : "-");
					    printf( (mystat.st_mode & S_IRUSR) ? "r" : "-");
					    printf( (mystat.st_mode & S_IWUSR) ? "w" : "-");
					    printf( (mystat.st_mode & S_IXUSR) ? "x" : "-");
					    printf( (mystat.st_mode & S_IRGRP) ? "r" : "-");
					    printf( (mystat.st_mode & S_IWGRP) ? "w" : "-");
					    printf( (mystat.st_mode & S_IXGRP) ? "x" : "-");
					    printf( (mystat.st_mode & S_IROTH) ? "r" : "-");
					    printf( (mystat.st_mode & S_IWOTH) ? "w" : "-");
					    printf( (mystat.st_mode & S_IXOTH) ? "x" : "-");
					    printf(" ");
					    // Username and group name
					    printf("%s ", getpwuid(mystat.st_uid)->pw_name);
					    printf("%s ", getgrgid(mystat.st_gid)->gr_name); 
					    // Last access
					    struct tm* tm_info;
					    tm_info = localtime(&(mystat.st_mtime));
					    strftime(buffer, 25, "%Y:%m:%d %H:%M:%S", tm_info);
    					printf("%s ", buffer);
					    //Size   
				        printf("%d ", (int) mystat.st_size);
				        // File name
				        printf(" %s\n", myfile->d_name);
				    }
				    closedir(mydir);
				}
				else {
					printf("Error: Arguments not supported.\n");
				}
			}
			else {
				DIR *mydir;
			    struct dirent *myfile;

			    getcwd(buffer, BUFFSIZ - 1);
			    mydir = opendir(buffer);
			    while((myfile = readdir(mydir)) != NULL)
			    {
			        if(myfile->d_name[0] == '.') {
			        	continue;
			        }
			        printf("%s ", myfile->d_name);
			    }
			    closedir(mydir);
			    printf("\n");
			}
		}
		else if(strcmp(argv[0], "exit") == 0) {
			for (i = 0; i < argc; ++i)
			{
				free(argv[i]);
			}
			break;
		}
		else {
			execute(argv);
		}

		for (i = 0; i < argc; ++i)
		{
			free(argv[i]);
		}
	}

	free(argv);
	return 0;
}

void execute(char **argv)
{
     pid_t  pid;
     int    status;

     if ((pid = fork()) < 0) {     /* fork a child process           */
          printf("Error: forking child process failed\n");
          exit(1);
     }
     else if (pid == 0) {          /* for the child process:         */
          if (execvp(*argv, argv) < 0) {     /* execute the command  */
               printf("Error: exec failed\n");
               exit(1);
          }
          exit(0);
     }
     else {                                  /* for the parent:      */
          while (wait(&status) != pid)       /* wait for completion  */
               ;
     }
}
