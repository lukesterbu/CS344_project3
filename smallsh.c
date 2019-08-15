#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>

#define LINE_MAX 2048
#define ARGS_MAX 512

int BgProcess = 0; // 0 indicates not background, 1 indicates it is
int BgAllowed = 1; // 0 indicates not allowed, 1 indicates allowed 

// Function prototypes
void printStatus(int childExitMethod);
void runCdCommand(char** args);
void parseString(char** args, char* buffer, char* delim, char** files);
void catchSIGTSTP(int signo);
void catchSIGINT(int signo);
void checkBackground();

void main()
{
	// Adding SIGINT to block list
	sigset_t set;
	sigemptyset(&set);
	sigaddset(&set, SIGINT);
	// Setting up signal handler for ^C SIGINT
	struct sigaction SIGINT_action = {0};
	SIGINT_action.sa_handler = catchSIGINT;
	SIGINT_action.sa_mask = set;
	//sigfillset(&SIGINT_action.sa_mask);
	SIGINT_action.sa_flags = 0;
	sigaction(SIGINT, &SIGINT_action, NULL);
	// Setting up signal handler for ^Z SIGSTP
	struct sigaction SIGTSTP_action = {0};
	SIGTSTP_action.sa_handler = catchSIGTSTP;
	sigfillset(&SIGTSTP_action.sa_mask);
	SIGTSTP_action.sa_flags = SA_RESTART;
	sigaction(SIGTSTP, &SIGTSTP_action, NULL);	

	char buffer[LINE_MAX];
	char* args[ARGS_MAX];
	// Set all elements of args to NULL
	int i = 0;
	char delim[] = " ";	
	int childExitMethod = -5;
	// for file IO
	char* files[2];
	int input, output = -5;
	BgAllowed = 1;
	while(1) { // Infinite loop if not terminated by exit
		sigprocmask(SIG_BLOCK, &set, NULL); // block signals in set 
		BgProcess = 0;
		files[0] = NULL;
		files[1] = NULL;
		int i = 0;
		// Clear out args and buffer
		for (i = 0; i < ARGS_MAX; i++) {
			args[i] = NULL;
		}
		for (i = 0; i < LINE_MAX; i++) {
			buffer[i] = '\0';
		}
		checkBackground(); // Checks for background processes
		fflush(stdout);
		printf(": ");
		fflush(stdout);
		fflush(stdin);
		// Get user input
		if (fgets(buffer, LINE_MAX, stdin) != NULL)
		{
			// Remove the new line character
			if (strlen(buffer) > 1)
				buffer[strlen(buffer) - 1] = '\0'; 
			// Checks if user entered blank line
			if (strcmp(&buffer[0], "\n") == 0)
				continue; // continue to next iteration
			// Checks if user entered a space first
			if (isspace(buffer[0]))
				continue; // continue to next iteration
			// Check for comment character #
			if (buffer[0] == '#')
				continue; // continue to next iteration
			int i = 0;
			while (buffer[i] != '\0') {
				if (strcmp(&buffer[i], "$$") == 0) {
					char* msg = malloc(10);
					sprintf(msg, "%d", getpid()); // Overwrite $$
					int x = 0;
					while (x < strlen(msg)) {
						strcpy(&buffer[i + x], &msg[x]); 
						x++;
					} 
					free(msg);
					break;
				}
				i++;
			}
		}
		// Break the string into words
		parseString(args, buffer, delim, files);
		// Handles exit
		if (strcmp(args[0], "exit") == 0) {
			exit(0);		
		}
		// Handles status
		else if (strcmp(args[0], "status") == 0) {
			printStatus(childExitMethod);
			fflush(stdout);
		}
		// Handles cd
		else if (strcmp(args[0], "cd") == 0) {
			runCdCommand(args);
		}
		else
		{
			// Fork variables
			pid_t spawnPid = -5;
			spawnPid = fork();
			switch (spawnPid) {
				case -1: // Error case
					perror("Hull Breach!");
					fflush(stderr);
					exit(1);
					break;
				case 0: // Child case
					if (!BgProcess) { // Can't stop with ^C
						sigprocmask(SIG_UNBLOCK, &set, NULL);	
					}
					// Input
					if (files[0] != NULL) {
						input = open(files[0], O_RDONLY);
						if (input == -1) {
							printf("cannot open %s for input\n", files[0]);
							fflush(stdout);
							_Exit(1);
						}
						// Set up redirection
						if (dup2(input, 0) == -1) {
							perror("dup2 input file\n");
							fflush(stderr);
							_Exit(1);
						}
						// This line triggers closing the file
						close(input);		
					}
					else if (BgProcess) {
						input = open("/dev/null", O_RDONLY);
						if (input == -1) {
							perror("open dev null\n");
							fflush(stderr);
							_Exit(1);
						}
						if (dup2(input, 0) == -1) {
							perror("dup2 dev null\n");
							fflush(stderr);
							_Exit(1);
						}
					}
					// Output
					if (files[1] != NULL) {
						output = open(files[1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
						if (output == -1) {
							printf("cannot open %s for output\n", files[1]);
							fflush(stdout);
							_Exit(1);
						}
						// Set up redirection
						if (dup2(output, 1) == -1) {
							perror("dup2 output file\n");
							fflush(stderr);
							_Exit(1);
						}
						// This line triggers closing the file
						close(output);
					}
					// not one of the built funcitons so pass to exec
					if (execvp(args[0], args) < 0) {
						printf("%s: no such file or directory\n", args[0]);
						fflush(stdout);
						_Exit(1);
					}
					break;		
				default: // Parent case	
					if (!BgProcess) { // if not background process
						waitpid(spawnPid, &childExitMethod, 0);
					}
					else { // if background process	
						printf("background pid %d\n", spawnPid);
						fflush(stdout);
						break;
					}
			}
		}
	}
}

// Prints the exits status
void printStatus(int childExitMethod) {
	if (childExitMethod == -5) { // no foreground process has been run yet
		printf("exit value 0\n");
	}
	else if (WIFEXITED(childExitMethod)) {
		printf("exit value %d\n", WEXITSTATUS(childExitMethod));
	}
	else {
		printf("terminated by signal %d\n", WTERMSIG(childExitMethod));
	}
}

// This function handles the cd command
void runCdCommand(char** args)
{
	// Check if the first argument is empty
	if (args[1] == NULL)
	{
		// If it is then go to the home directory
		chdir(getenv("HOME"));
		return;
	}
	else { // args[1] != NULL
		// Change to the directory in args[1]
		if (chdir(args[1]) == -1) {
			printf("%s: no such file or directory\n", args[1]);
			fflush(stdout);
		}
	}
}

/* Separates the string passed in through buffer by the deliminator
 * delim and places the output into args. */
void parseString(char** args, char* buffer, char* delim, char** files) {
	int i = 0;
	char* word;
	word = strtok(buffer, delim);
	// Checks for end of line and to see if too many arguments were entered
	while (word != NULL && i < ARGS_MAX) {
		// file input indicator
		if (strcmp(word, "<") == 0) {
			word = strtok(NULL, delim);
			files[0] = word;
			word = strtok(NULL, delim);
			if (word == NULL)
				break;
		}
		// file output indicator
		if (strcmp(word, ">") == 0) {
			word = strtok(NULL, delim);
			files[1] = word;
			word = strtok(NULL, delim);
			if (word == NULL)
				break;
		}
		args[i] = word; // Add current word to array
		word = strtok(NULL, delim); // Get next word
		i++; // Increment i
	}
	if (strcmp(args[i - 1], "&") == 0) {
		if (BgAllowed == 1)
			BgProcess = 1; // Mark as background process	
		args[i - 1] = NULL; // Erase the trailing &
	}	
}

// Handles ^Z aka SIGSTP. If flag is set to 1, set it to 0 and print the first
// message. If flag is set to 0, set it to 1 and print the second message.
// If flag is set to 1 then background processes are allowed. If it's set to 0
// then background processes are not allowed. 
void catchSIGTSTP(int signo) {
	if (BgAllowed == 1) {
		printf("\n");
		printf("Entering foreground-only mode (& is now ignored)\n");
		fflush(stdout);
		BgAllowed = 0;
	}
	else {
		printf("\n");
		printf("Exiting foreground-only mode\n");
		fflush(stdout);
		BgAllowed = 1;
	}
}

// This isn't working
void catchSIGINT(int signo) {
	printf("terminated by signal %d\n", signo);
	fflush(stdout);
	kill(getpid(), signo);
}

// Check for background process that have terminated	
void checkBackground() {
	int childExitMethod = -5;
	int spawnPid = waitpid(-1, &childExitMethod, WNOHANG);
	while(spawnPid > 0) {
		printf("background pid %d is done: ", spawnPid);
		printStatus(childExitMethod);
		spawnPid = waitpid(-1, &childExitMethod, WNOHANG);
	}
}
