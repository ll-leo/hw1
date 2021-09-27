#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>

// Place any necessary global variables here

int main(int argc, char *argv[]){

	/*
	 *
	 * Implement Here
	 *
	 *
	 */
    struct timval start, end;
    gettimeofday(&start, NULL);
    for(int i=0;i<100000;i++){
            getpid();
            }
    gettimeofday(&end, NULL);
    long micros = ((seconds * 1000000) + end.tv_usec) - (start.tv_usec);
    printf("Syscalls Performed: XXXX");
    printf("Total Elapsed Time: %d microseconds\n",micros);
    printf("Average Time Per Syscall: %d microseconds",micros/100000);
	return 0;

}
