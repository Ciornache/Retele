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
     int value = 6;
     int pid = fork();
     if(pid == 0) {
         printf("Valoare din copil: %d\n", value); 
         value += 2;
     }
     else {
        printf("Valoare din tata: %d\n", value);
        sleep(2);
        printf("Valoare din tata: %d\n", value);
     }
     exit(0);
}