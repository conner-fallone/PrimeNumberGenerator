/***********************************************************************
 * Author: Conner Fallone
 * CS-452 Fall 2013
 *
 * This is a pipelined version of the well-known Sieve of Eratosthenes 
 * algorithm for generating prime numbers. This program demonstrates
 * concurrent stream processing, in that it uses multiple processes
 * combined with piped IPC to implement a parallel, pipelined version
 * of this algorithm. A stream of incrementing numbers is constantly
 * being fed through all of the processes. A process is spawned for each
 * prime number that is printed.
 *
 ***********************************************************************/

#include <stdio.h> 
#include <stdlib.h> 
#include <unistd.h>
#include <signal.h>

#define READ 0
#define WRITE 1
#define READ_BEFORE_PIPE 2
#define READ_AFTER_PIPE 3

pid_t pid;
pid_t pid2 = 0;
int numStream = 2,processCount = 0;
int prime,numPrimes,parentPID,maxNum,operationMode;
int fd[4];

void sigHandler(int);

int main() 
{
	// Initialize sigHandler for parent 
	signal(SIGUSR1,sigHandler);
	parentPID = getpid();

	// Prompt user
	printf("Which mode of operation would you like?\n");
	printf("1 : Request the first k primes.\n");
	printf("2 : Request all of the prime numbers between 2 and k.\n");
	printf(" Please enter either a '1' or a '2' >> ");
	scanf("%d",&operationMode);

	// Get proper input based on operation mode selected
	if (operationMode == 1){
		printf("Number of primes desired >> ");
		scanf("%d",&numPrimes);
	}
	else if (operationMode == 2){
		printf("Please enter a value for k >> ");
		scanf("%d",&maxNum);
	}

	printf("Retrieving %d primes...\n",numPrimes);

	// Create initial pipe and the first process that will read from it
	if (pipe (fd) < 0)
		perror ("Error with pipe");
	if ((pid = fork()) < 0){
		perror("fork failure");
		exit(1);
	}
	
	if (pid == 0){
		// Child
		// Keep track of children processes spawned - initial increment
		processCount++;

		while (1){
			// Forked process is a child - it must print a prime. Else process is parent, it has already printed a prime - resume iterations
			if (pid2 == 0){
				// Restore the read pointer from the previous pipe since it was overwritten	
				if (processCount >= 2)
					fd[READ] = fd[READ_AFTER_PIPE];

				// Read continuous stream of numbers from parent
				read(fd[READ],&numStream,sizeof(numStream));
				// Keep track of children processes spawned - all other children
				processCount++;

				// Prime is the first number read by the process.
				prime = numStream;

				// If the current prime is greater than the max limit entered by the user, we are done
				if (operationMode == 2){
					if (prime > maxNum){
						kill (parentPID,SIGUSR1);
						// Sleep until killed, just to prevent unwanted code from executing
						sleep(1);
					}
				}
				printf("%d) PID #%d : Prime Number: %d\n",processCount,getpid(),prime);
				fflush(stdout);

				// If number of processes is equal to number of primes requested, we are done
				if (processCount == numPrimes){
					kill (parentPID,SIGUSR1);
					// Sleep until killed, just to prevent unwanted code from executing
					sleep(1);
				}

				// Save the read pointer before we pipe - It will get overwritten by pipe
				fd[READ_BEFORE_PIPE] = fd[READ];

				// Create pipe				
				if (pipe(fd) < 0)
					perror ("Error with pipe");

				// Store new read pointer before we fork - It will get overwritten when we restore the old pointer
				fd[READ_AFTER_PIPE] = fd[READ];

				// Restore the read pointer saved previously
				fd[READ] = fd[READ_BEFORE_PIPE];
				
				// Fork
				if ((pid2 = fork()) < 0){
					perror("fork failure");
					exit(1);
				}

			}else{
				// Parent
				// Read continuous stream of numbers from parent	
				read(fd[READ],&numStream,sizeof(numStream));
			}

			//Write to the pipe numbers that aren't divisible by the prime
			if (numStream % prime != 0){
				write(fd[WRITE],&numStream,sizeof(numStream));
			}
		}
	}
	else{
		// Parent	
		// Initial parent never reads - close it
		close(fd[READ]);
		// Counter is essentially the first prime - print it
		prime = numStream;
		printf("%d) PID #%d : Prime Number: %d\n",processCount + 1,getpid(),prime);
		fflush(stdout);

		// The user only wanted the first prime
		if (processCount + 1  == numPrimes)
			return 0;

		// Send infinite stream of incrementing integers to child
		while(1){
			numStream++;
			// Write to pipe numbers that aren't divisble by 2
			if (numStream % prime != 0){
				write(fd[WRITE], &numStream,sizeof(numStream));
			}
		}
	}
	return 0;
}

void sigHandler(int sigUsr1){
	printf("All primes retrieved... Ending all processes...\n");
	// Kill the process group - thus ending all processes	
	killpg(getpid(),SIGKILL);
}
