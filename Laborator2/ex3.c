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
        int eid = execl("/usr/bin/ls", "ls", "-a", "-l", NULL);
        if(eid == -1) 
        {
            perror("Eroare la executia comenzii execlp!\n");
            exit(1);
        }

     }
     else {
        wait(NULL);
        printf("%s: Tata: Comanda a fost executata cu succes de catre fiu!\n", argv[0]);
     }
     exit(0);
}