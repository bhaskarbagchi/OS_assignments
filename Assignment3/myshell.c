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
#include <termios.h>
#include <fcntl.h>

#define BUFFSIZ 4096
#define FD_DEFAULT 0
#define FD_STDIN 1
#define FD_STDOUT 2
#define FD_STDERR 3

char buffer[BUFFSIZ], userInput;
int RUN, SAVED_INFILE, SAVED_OUTFILE, SAVED_ERRFILE;

/* Keep track of attributes of the shell.  */
pid_t shell_pgid;
struct termios shell_tmodes;
int shell_terminal;
int shell_is_interactive;

/* A process is a single process.  */
typedef struct process
{
	struct process *next;       /* next process in pipeline */
	char **argv;                /* for exec */
	pid_t pid;                  /* process ID */
	char completed;             /* true if process has completed */
	char stopped;               /* true if process has stopped */
	int status;                 /* reported status value */
} process;

/* A job is a pipeline of processes.  */
typedef struct job
{
	struct job *next;           /* next active job */
	char *command;              /* command line, used for messages */
	process *first_process;     /* list of processes in this job */
	pid_t pgid;                 /* process group ID */
	char notified;              /* true if user told about stopped job */
	struct termios tmodes;      /* saved terminal modes */
	int stdin, stdout, stderr;  /* standard i/o channels */
	int foreground;
} job;

/* The active jobs are linked into a list.  This is its head.   */
job *first_job = NULL;

void executeJob();
void getTextLine();
void handleUserCommand();
void init_shell();
void launch_job (job *j);
void launch_process (process *p, pid_t pgid, int infile, int outfile, int errfile, int foreground);
job* find_job (pid_t pgid);
int job_is_stopped (job *j);
int job_is_completed (job *j);
void put_job_in_foreground (job *j, int cont);
void put_job_in_background (job *j, int cont);
int mark_process_status (pid_t pid, int status);
void update_status (void);
void wait_for_job (job *j);
void format_job_info (job *j, const char *status);
void do_job_notification (int p);
void mark_job_as_running (job *j);
void continue_job (job *j, int foreground);
job* parse();
void windup(job* j);
int recollect(job* j, process* p, int* fdType, char* tempBuff, char* start, int length, int whitespace, int* cmd_index);
int checkBuiltInCommands(char** argv, int infile, int outfile, int errfile);
void setupRedirection(int infile, int outfile, int errfile);
void resetRedirection();

int main(int argn, char *arg[])
{
	RUN = 1;

	init_shell();
	printf("myshell> ");
	while (RUN) {
            userInput = getchar();
            switch (userInput) {
            case '\n':
				printf("myshell> ");
                break;
            default:
                getTextLine();
                handleUserCommand();
                if(RUN) printf("myshell> ");
                break;
            case -1:
            	RUN = 0;
    			printf("\n");
            	break;
            }
    }
    return 0;
}

/* Make sure the shell is running interactively as the foreground job before proceeding. */
void
init_shell ()
{

	/* See if we are running interactively.  */
	shell_terminal = STDIN_FILENO;
	shell_is_interactive = isatty (shell_terminal);

if (shell_is_interactive)
	 {
	   		/* Loop until we are in the foreground.  */
			while (tcgetpgrp (shell_terminal) != (shell_pgid = getpgrp ()))
			 kill (- shell_pgid, SIGTTIN);

			/* Ignore interactive and job-control signals.  */
			signal (SIGINT, SIG_IGN);
           	signal (SIGQUIT, SIG_IGN);
           	signal (SIGTSTP, SIG_IGN);
           	signal (SIGTTIN, SIG_IGN);
           	signal (SIGTTOU, SIG_IGN);
           	signal (SIGCHLD, &do_job_notification);

			/* Put ourselves in our own process group.  */
			shell_pgid = getpid ();
			if (setpgid (shell_pgid, shell_pgid) < 0)
				{
			   		perror ("Couldn't put the shell in its own process group");
			   		exit (1);
			 	}

			/* Grab control of the terminal.  */
			tcsetpgrp (shell_terminal, shell_pgid);

			/* Save default terminal attributes for shell.  */
			tcgetattr (shell_terminal, &shell_tmodes);
	 }
}

void handleUserCommand()
{
	job* j = parse();

	if(j == NULL) {
		return;
	}

	/* Add job to job list. */
	j->next = first_job;
	first_job = j;

	/* Launch job */
	launch_job(j);
}

void
launch_job (job *j)
{
	process *p;
	pid_t pid;
	int mypipe[2], infile, outfile;

	infile = j->stdin;
	for (p = j->first_process; p; p = p->next)
	 {
	   /* Set up pipes, if necessary.  */
	   if (p->next)
	     {
	       if (pipe (mypipe) < 0)
	         {
	           perror ("pipe");
	           exit (1);
	         }
	       outfile = mypipe[1];
	     }
	   else
	     outfile = j->stdout;

	 /* Check built-in commands */
	 if(!checkBuiltInCommands(p->argv, infile, outfile, j->stderr))
	 {
	   /* Fork the child processes.  */
	   pid = fork ();
	   if (pid == 0)
	     /* This is the child process.  */
	     launch_process (p, j->pgid, infile,
	                     outfile, j->stderr, j->foreground);
	   else if (pid < 0)
	     {
	       /* The fork failed.  */
	       perror ("fork");
	       exit (1);
	     }
	   else
	     {
	       /* This is the parent process.  */
	       p->pid = pid;
	       if (shell_is_interactive)
	         {
	           if (!j->pgid)
	             j->pgid = pid;
	           setpgid (pid, j->pgid);
	         }
	     }
	 }

	   /* Clean up after pipes.  */
	   if (infile != j->stdin)
	     close (infile);
	   if (outfile != j->stdout)
	     close (outfile);
	   infile = mypipe[0];
	 }

	//format_job_info (j, "launched");

	if (!shell_is_interactive)
	 wait_for_job (j);
	else if (j->foreground)
	 put_job_in_foreground (j, 0);
	else
	 put_job_in_background (j, 0);
}

void
launch_process (process *p, pid_t pgid, int infile, int outfile, int errfile, int foreground)
{
	pid_t pid;

	if (shell_is_interactive)
	 {
	   /* Put the process into the process group and give the process group
	      the terminal, if appropriate.
	      This has to be done both by the shell and in the individual
	      child processes because of potential race conditions. */
	   pid = getpid ();
	   if (pgid == 0) pgid = pid;
	   setpgid (pid, pgid);
	   if (foreground)
	     tcsetpgrp (shell_terminal, pgid);

	   /* Set the handling for job control signals back to the default.  */
	   signal (SIGINT, SIG_DFL);
	   signal (SIGQUIT, SIG_DFL);
	   signal (SIGTSTP, SIG_DFL);
	   signal (SIGTTIN, SIG_DFL);
	   signal (SIGTTOU, SIG_DFL);
	   signal (SIGCHLD, SIG_DFL);
	 }

	/* Set the standard input/output channels of the new process.  */
	if (infile != STDIN_FILENO)
	 {
	   dup2 (infile, STDIN_FILENO);
	   close (infile);
	 }
	if (outfile != STDOUT_FILENO)
	 {
	   dup2 (outfile, STDOUT_FILENO);
	   close (outfile);
	 }
	if (errfile != STDERR_FILENO)
	 {
	   dup2 (errfile, STDERR_FILENO);
	   close (errfile);
	 }

	DIR *mydir;
    struct dirent *myfile;
    int flag = 0;
    
    getcwd(buffer, BUFFSIZ - 1);
    mydir = opendir(buffer);
    while((myfile = readdir(mydir)) != NULL)
    {
            if(strcmp(myfile->d_name, p->argv[0]) == 0) {
                    flag = 1;
                    break;
            }
    }
    closedir(mydir);
	if(flag) {
         char* t = (char*) malloc((strlen(p->argv[0]) + 3) * sizeof(char));
         t[0] = '.';
         t[1] = '/';
         t[2] = '\0';
         strcat(t, p->argv[0]);
         free(p->argv[0]);
         p->argv[0] = t;
    }
/* Exec the new process.  Make sure we exit.  */
	execvp (p->argv[0], p->argv);
	perror ("execvp");
	exit (1);
}

void getTextLine()
{
	int bufferChars = 0;
	while ((userInput != '\n') && (bufferChars < BUFFSIZ)) {
	        buffer[bufferChars++] = userInput;
	        userInput = getchar();
	}
	buffer[bufferChars] = 0x00;
}

 /* Find the active job with the indicated pgid.  */
job *
find_job (pid_t pgid)
{
	job *j;

	for (j = first_job; j; j = j->next)
	 if (j->pgid == pgid)
	   return j;
	return NULL;
}

/* Return true if all processes in the job have stopped or completed.  */
int
job_is_stopped (job *j)
{
	process *p;

	for (p = j->first_process; p; p = p->next)
	 if (!p->completed && !p->stopped)
	   return 0;
	return 1;
}

/* Return true if all processes in the job have completed.  */
int
job_is_completed (job *j)
{
	process *p;

	for (p = j->first_process; p; p = p->next)
	 if (!p->completed)
	   return 0;
	return 1;
}

/* Put job j in the foreground.  If cont is nonzero,
restore the saved terminal modes and send the process group a
SIGCONT signal to wake it up before we block.  */
void
put_job_in_foreground (job *j, int cont)
{
	/* Put the job into the foreground.  */
	tcsetpgrp (shell_terminal, j->pgid);

	/* Send the job a continue signal, if necessary.  */
	if (cont)
	 {
	   tcsetattr (shell_terminal, TCSADRAIN, &j->tmodes);
	   if (kill (- j->pgid, SIGCONT) < 0)
	     perror ("kill (SIGCONT)");
	 }

	/* Wait for it to report.  */
	wait_for_job (j);

	/* Put the shell back in the foreground.  */
	tcsetpgrp (shell_terminal, shell_pgid);

	/* Restore the shell's terminal modes.  */
	tcgetattr (shell_terminal, &j->tmodes);
	tcsetattr (shell_terminal, TCSADRAIN, &shell_tmodes);
}

/* Put a job in the background.  If the cont argument is true, send
the process group a SIGCONT signal to wake it up. */   
void
put_job_in_background (job *j, int cont)
{
	/* Send the job a continue signal, if necessary.  */
	if (cont)
	 if (kill (-j->pgid, SIGCONT) < 0)
	   perror ("kill (SIGCONT)");
}

/* Store the status of the process pid that was returned by waitpid.
        Return 0 if all went well, nonzero otherwise.  */
     
int
mark_process_status (pid_t pid, int status)
{
	job *j;
	process *p;

	if (pid > 0)
	 {
	   /* Update the record for the process.  */
	   for (j = first_job; j; j = j->next)
	     for (p = j->first_process; p; p = p->next)
	       if (p->pid == pid)
	         {
	           p->status = status;
	           if (WIFSTOPPED (status))
	             p->stopped = 1;
	           else
	             {
	               p->completed = 1;
	               if (WIFSIGNALED (status)) {
	                 //fprintf (stderr, "%d: Terminated by signal %d.\n", (int) pid, WTERMSIG (p->status));
	             	}
	             }
	           return 0;
	          }
	   //fprintf (stderr, "No child process %d.\n", pid);
	   return -1;
	 }
	else if (pid == 0 || errno == ECHILD)
	 /* No processes ready to report.  */
	 return -1;
	else {
	 /* Other weird errors.  */
	 perror ("waitpid");
	 return -1;
	}
}

/* Check for processes that have status information available,
without blocking.  */

void
update_status (void)
{
	int status;
	pid_t pid;

	do
	 pid = waitpid (WAIT_ANY, &status, WUNTRACED|WNOHANG);
	while (!mark_process_status (pid, status));
}

/* Check for processes that have status information available,
blocking until all processes in the given job have reported.  */

void
wait_for_job (job *j)
{
	int status;
	pid_t pid;

	do
	 pid = waitpid (WAIT_ANY, &status, WUNTRACED);
	while (!mark_process_status (pid, status)
	      && !job_is_stopped (j)
	      && !job_is_completed (j));
}

/* Format information about job status for the user to look at.  */

void
format_job_info (job *j, const char *status)
{
fprintf (stderr, "%ld : (%s)\n", (long)j->pgid, status);
}

/* Notify the user about stopped or terminated jobs.
Delete terminated jobs from the active job list.  */
     
void
do_job_notification (int p)
{
	job *j, *jlast, *jnext;
	//process *p;

	/* Update status information for child processes.  */
	update_status ();

	jlast = NULL;
	for (j = first_job; j; j = jnext)
	 {
	   jnext = j->next;

	   /* If all processes have completed, tell the user the job has
	      completed and delete it from the list of active jobs.  */
	   if (job_is_completed (j)) {
	     //format_job_info (j, "completed");
	     if (jlast)
	       jlast->next = jnext;
	     else
	       first_job = jnext;
	     windup(j);
	   }

	   /* Notify the user about stopped jobs,
	      marking them so that we won't do this more than once.  */
	   else if (job_is_stopped (j) && !j->notified) {
	     //format_job_info (j, "stopped");
	     j->notified = 1;
	     jlast = j;
	   }

	   /* Don't say anything about jobs that are still running.  */
	   else
	     jlast = j;
 }
}

/* Mark a stopped job J as being running again.  */    
void
mark_job_as_running (job *j)
{
	process *p;

	for (p = j->first_process; p; p = p->next)
	 p->stopped = 0;
	j->notified = 0;
}

/* Continue the job J.  */
void
continue_job (job *j, int foreground)
{
	mark_job_as_running (j);
	if (foreground)
	 put_job_in_foreground (j, 1);
	else
	 put_job_in_background (j, 1);
}

job* parse() {
	int failure = 0;
	job* j = NULL;
	process* p;

	j = (job*) malloc(sizeof(job));
	p = (process*) malloc(sizeof(process));
	p->next = NULL;
	p->argv = (char**) malloc(BUFFSIZ * sizeof(char*));
	p->argv[0] = NULL;
	j->pgid = 0;
	j->first_process = p;
	j->foreground = 1;
	j->stdin = STDIN_FILENO;
	j->stdout = STDOUT_FILENO;
	j->stderr = STDERR_FILENO;
	
	int fdType = FD_DEFAULT, length = 0, cmd_index = 0, whitespace = 0;
	char* c;
	char* start = buffer;
	char* tempBuff = (char*) malloc(BUFFSIZ * sizeof(char));

	for (c = buffer; *c ; ++c)
	{
		switch(*c) {
			case '|':
				failure = recollect(j, p, &fdType, tempBuff, start, length, whitespace, &cmd_index);

				p->next = (process*) malloc(sizeof(process));
				p = p->next;
				p->next = NULL;
				p->argv = (char**) malloc(BUFFSIZ * sizeof(char*));

				cmd_index = 0;
				whitespace = 1;
				fdType = FD_DEFAULT;
				start = c+1;
				length = 0;
				break;
			case '>':
				failure = recollect(j, p, &fdType, tempBuff, start, length, whitespace, &cmd_index);

				whitespace = 1;
				fdType = FD_STDOUT;
				start = c+1;
				length = 0;
				break;
			case '<':
				failure = recollect(j, p, &fdType, tempBuff, start, length, whitespace, &cmd_index);

				whitespace = 1;
				fdType = FD_STDIN;
				start = c+1;
				length = 0;
				break;
			case ' ':
				failure = recollect(j, p, &fdType, tempBuff, start, length, whitespace, &cmd_index);

				whitespace = 1;
				start = c+1;
				length = 0;
				break;
			case '&':
				if(fdType != FD_DEFAULT) {
					failure = 1;
				}
				else {
					j->foreground = 0;
				}

				whitespace = 1;
				start = c+1;
				length = 0;
				break;
			default:
				whitespace = 0;
				length++;
				break;
		}
	}

	failure = recollect(j, p, &fdType, tempBuff, start, length, whitespace, &cmd_index);

	free(tempBuff);

	if(failure) {
		windup(j);
		j = NULL;
	}

	return j;
}

int recollect(job *j, process* p, int* fdType, char* tempBuff, char* start, int length, int whitespace, int* cmd_index) {
	int failure = 0;

	switch(*fdType) {
		case FD_STDOUT:
			if(!whitespace) {
				strncpy(tempBuff, start, length);
				tempBuff[length] = '\0';
				j->stdout = open(tempBuff, O_CREAT | O_WRONLY, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
				if(j->stdout < 0) {
					printf("Error: Cannot redirect output.\n");
					failure = 1;
				}
				*fdType = FD_DEFAULT;
			}
			break;
		case FD_STDIN:
			if(!whitespace) {
				strncpy(tempBuff, start, length);
				tempBuff[length] = '\0';
				j->stdin = open(tempBuff, O_RDONLY);
				if(j->stdin < 0) {
					printf("Error: Cannot redirect input.\n");
					failure = 1;
				}
				*fdType = FD_DEFAULT;
			}
			break;
		default:
			if(!whitespace) {
				p->argv[*cmd_index] = (char*) malloc((length + 1) * sizeof(char));
				strncpy(p->argv[*cmd_index], start, length);
				p->argv[*cmd_index][length] = '\0';
				(*cmd_index)++;
				p->argv[*cmd_index] = NULL;
				*fdType = FD_DEFAULT;
			}
			break;
	}

	return failure;
}

void windup(job* j) {
	process *curr;
	process* pre;
	int i;

	curr = j->first_process;
	while(curr) {
		pre = curr;
		curr = curr->next;

		i = 0;
		while(pre->argv[i]) {
			free(pre->argv[i]);
			i++;
		}

		free(pre->argv);
		free(pre);
	}

	free(j);
}

int checkBuiltInCommands(char** argv, int infile, int outfile, int errfile) {

	int BUILT_IN = 0;

	if(strcmp(argv[0], "cd") == 0) {
		BUILT_IN = 1;
		setupRedirection(infile, outfile, errfile);

		if(argv[1] == NULL){
                            chdir(getenv("HOME"));
                    }
                    else if(chdir(argv[1]) != 0) {
			printf("Error: No such directory.\n");
		}

		resetRedirection();
	}
	/*else if(strcmp(argv[0], "pwd") == 0) {
		BUILT_IN = 1;
		setupRedirection(infile, outfile, errfile);

		getcwd(buffer, BUFFSIZ - 1);
		printf("%s\n", buffer);

		resetRedirection();
	}
	else if(strcmp(argv[0], "mkdir") == 0) {
		BUILT_IN = 1;
		setupRedirection(infile, outfile, errfile);

		if(mkdir(argv[1], 0775) != 0) {
			printf("Error: Cannot create directory.\n");
		}

		resetRedirection();
	}
	else if(strcmp(argv[0], "rmdir") == 0) {
		BUILT_IN = 1;
		setupRedirection(infile, outfile, errfile);

		if(rmdir(argv[1]) != 0) {
			printf("Error: Cannot remove the directory.\n");
		}

		resetRedirection();
	}
	else if(strcmp(argv[0], "ls") == 0) {
		if(argv[1] != NULL) {
			if(strcmp(argv[1], "-l") == 0) {
				BUILT_IN = 1;
				setupRedirection(infile, outfile, errfile);

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

			    resetRedirection();
			}
		}
		else {
			BUILT_IN = 1;
			setupRedirection(infile, outfile, errfile);

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

		    resetRedirection();
		}
	}*/
	else if(strcmp(argv[0], "exit") == 0) {
		BUILT_IN = 1;
		RUN = 0;
	}

	return BUILT_IN;
}

void setupRedirection(int infile, int outfile, int errfile) {

	SAVED_INFILE = -1;
	SAVED_OUTFILE = -1;
	SAVED_ERRFILE = -1;

	/* Set the standard input/output channels of the new process.  */
	if (infile != STDIN_FILENO)
	 {
	 	SAVED_INFILE = dup(STDIN_FILENO);
	   dup2 (infile, STDIN_FILENO);
	   close (infile);
	 }
	if (outfile != STDOUT_FILENO)
	 {
	 	SAVED_OUTFILE = dup(STDOUT_FILENO);
	   dup2 (outfile, STDOUT_FILENO);
	   close (outfile);
	 }
	if (errfile != STDERR_FILENO)
	 {
	 	SAVED_ERRFILE = dup(STDERR_FILENO);
	   dup2 (errfile, STDERR_FILENO);
	   close (errfile);
	 }
}

void resetRedirection() {
	if(SAVED_INFILE != -1) {
		dup2 (SAVED_INFILE, STDIN_FILENO);
		close(SAVED_INFILE);
	}
	if(SAVED_OUTFILE != -1) {
		dup2 (SAVED_OUTFILE, STDOUT_FILENO);
		close(SAVED_OUTFILE);
	}
	if(SAVED_ERRFILE != -1) {
		dup2 (SAVED_ERRFILE, STDERR_FILENO);
		close(SAVED_ERRFILE);
	}
}