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
    if(arg < 2) 
    {
        perror("Eroare la linia de comanda! Numar insuficient de argumente!\n");
        exit(1);
    }

    int fd = open(argv[1], O_RDWR);
    if(fd == -1)
    {
        perror("Eroare la deschiderea fisierului argument!\n");
        exit(2);
    }
    struct stat st;
    fstat(fd, &st);
    printf("The file %s was modified %ld seconds ago\n", argv[1], st.st_mtime);
    close(fd);
    exit(0);
}