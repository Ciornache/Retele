#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <signal.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <semaphore.h>
#include <time.h>

#define BUFFER_SIZE 4096


int main(int arg, char * argv[])
{
     int pid = fork();
     if(pid == 0) {
         printf("Copil: Am intrat in copil!\n"); 
     }
     else {
         printf("Tata: Am intrat in tata!\n");
     }
     exit(0);
}